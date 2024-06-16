//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#include <string>
#include <vdr/plugin.h>

// set this define to create the instance of the config class
#define CREATE_CONFIG 1
#include "config.h"
#include "control.h"
#include "filebrowser.h"
#include "setup.h"
#include "player.h"
#include "menu_options.h"
#include "mpv_service.h"

static const char *VERSION = "1.7.1"
#ifdef GIT_REV
    "-GIT" GIT_REV
#endif
;
volatile char ShownMenu = 0;

using std::string;

static const char *DESCRIPTION = trNOOP("mpv player plugin");

class cMpvPlugin:public cPlugin
{
  private:
    void PlayFile(string Filename, bool Shuffle=false);
    bool IsIsoImage(string Filename);
    void PlayFileHandleType(string Filename, bool Shuffle=false);

  public:
    cMpvPlugin(void);
    virtual ~cMpvPlugin(void);
    virtual const char *Version(void) { return VERSION; }
    virtual const char *Description(void) { return tr(DESCRIPTION); }
    virtual const char *CommandLineHelp(void) { return MpvPluginConfig->CommandLineHelp(); }
    virtual bool ProcessArgs(int argc, char *argv[]) { return MpvPluginConfig->ProcessArgs(argc, argv); }
    virtual bool Initialize(void);
    virtual void MainThreadHook(void) {}
    virtual const char *MainMenuEntry(void);
    virtual cOsdObject *MainMenuAction(void);
    virtual cMenuSetupPage *SetupMenu(void) { return new cMpvPluginSetup; }
    virtual bool SetupParse(const char *Name, const char *Value) { return MpvPluginConfig->SetupParse(Name, Value); }
    virtual bool Service(const char *Id, void *Data = NULL);
    virtual const char **SVDRPHelpPages(void) { return NULL; }
    virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplayCode);
};

bool cMpvPlugin::Initialize(void)
{
  if (MpvPluginConfig->SavePos)
  {
    DIR *hDir;
    string watch_later = PLGRESDIR;
    watch_later += "/watch_later";
    hDir = opendir(watch_later.c_str());
    if (!hDir)
      esyslog("[mpv] No directory %s, mpv can't resume play!\n", watch_later.c_str());
    else 
      closedir(hDir);
  }
  return true;
}

void cMpvPlugin::PlayFile(string Filename, bool Shuffle)
{
  if (cPluginManager::GetPlugin("softhddevice") && !cMpvPlayer::PlayerIsRunning())
    cControl::Launch(new cMpvControl(Filename.c_str(), Shuffle));
  else
  {
    cMpvControl* control = dynamic_cast<cMpvControl*>(cControl::Control(true));
    if(control)
      control->PlayNew(Filename.c_str());
  }
}

cMpvPlugin::cMpvPlugin(void)
{
  MpvPluginConfig = new cMpvPluginConfig();
}

cMpvPlugin::~cMpvPlugin(void)
{
  delete MpvPluginConfig;
}

const char *cMpvPlugin::MainMenuEntry(void)
{
  if (cMpvPlayer::PlayerIsRunning())
    ShownMenu = 1;

  return MpvPluginConfig->HideMainMenuEntry ? NULL : MpvPluginConfig->MainMenuEntry.c_str();
}

cOsdObject *cMpvPlugin::MainMenuAction(void)
{
  if (cMpvPlayer::PlayerIsRunning())
  {
    if (!MpvPluginConfig->ShowOptions)
      return new cMpvFilebrowser(MpvPluginConfig->BrowserRoot, MpvPluginConfig->DiscDevice);
    else
      if (cMpvPlayer::Player()->NetworkPlay())
        return new cMpvMenuPlaylist(cMpvPlayer::Player());
      else
        return new cMpvMenuOptions(cMpvPlayer::Player());
  }
  return new cMpvFilebrowser(MpvPluginConfig->BrowserRoot, MpvPluginConfig->DiscDevice);
}

bool cMpvPlugin::Service(const char *id, void *data)
{
  if (strcmp(id, "Mpv_Seek") == 0)
  {
    Mpv_Seek *seekInfo = (Mpv_Seek *)data;

    cMpvControl* control = dynamic_cast<cMpvControl*>(cControl::Control(true));
    if(control)
    {
      if(seekInfo->SeekRelative != 0)
      {
          control->SeekRelative(seekInfo->SeekRelative);
      }
      else if(seekInfo->SeekAbsolute >= 0)
      {
        control->SeekTo(seekInfo->SeekAbsolute);
      }
    }
    return true;
  }
  if (strcmp(id, "ScaleVideo") == 0)
  {
    Mpv_ScaleVideo *scaleVideo = (Mpv_ScaleVideo *)data;
    cMpvControl* control = dynamic_cast<cMpvControl*>(cControl::Control(true));
    if(control)
    {
        control->ScaleVideo(scaleVideo->x, scaleVideo->y, scaleVideo->width, scaleVideo->height);
    }
    return true;
  }
  if (strcmp(id, "GrabImage") == 0)
  {
    Mpv_GrabImage *grabImage = (Mpv_GrabImage *)data;
    cMpvControl* control = dynamic_cast<cMpvControl*>(cControl::Control(true));
    if(control)
    {
        grabImage->image = control->GrabImage(&grabImage->size, &grabImage->width, &grabImage->height);
    }
    return true;
  }
  if (strcmp(id, "Mpv_PlayFile") == 0)
  {
    Mpv_PlayFile *playFile = (Mpv_PlayFile *)data;
    PlayFileHandleType(playFile->Filename);
    return true;
  }
  if (strcmp(id, "Mpv_PlaylistShuffle") == 0)
  {
    Mpv_PlaylistShuffle *shuffleFile = (Mpv_PlaylistShuffle *)data;
    PlayFileHandleType(shuffleFile->Filename, true);
    return true;
  }
  if (strcmp(id, "Mpv_SetTitle") == 0)
  {
    Mpv_SetTitle *setTitle = (Mpv_SetTitle *) data;
    MpvPluginConfig->TitleOverride = setTitle->Title;
    return true;
  }
  if (strcmp(id, MPV_START_PLAY_SERVICE) == 0)
  {
    Mpv_StartPlayService_v1_0_t *r = (Mpv_StartPlayService_v1_0_t *) data;
    PlayFileHandleType(r->Filename);
    return true;
  }
  if (strcmp(id, MPV_SET_TITLE_SERVICE) == 0)
  {
    Mpv_SetTitleService_v1_0_t *r = (Mpv_SetTitleService_v1_0_t *) data;
    MpvPluginConfig->TitleOverride = r->Title;
    return true;
  }
  return false;
}

cString cMpvPlugin::SVDRPCommand(const char *Command, const char *Option, int &ReplayCode)
{
#ifdef DEBUG
  dsyslog ("Command %s Option %s\n", Command, Option);
#endif
  if (strcasecmp(Command, "PLAY") == 0)
  {
    PlayFileHandleType(Option);
    cControl::Attach();
    return "starting player";
  }
  return NULL;
}

bool cMpvPlugin::IsIsoImage(string Filename)
{
  vector<string> IsoExtensions;
  IsoExtensions.push_back("bin");
  IsoExtensions.push_back("dvd");
  IsoExtensions.push_back("img");
  IsoExtensions.push_back("iso");
  IsoExtensions.push_back("mdf");
  IsoExtensions.push_back("nrg");
  for (unsigned int i=0;i<IsoExtensions.size();i++)
  {
    if (Filename.substr(Filename.find_last_of(".") + 1) == IsoExtensions[i])
      return true;
  }

  return false;
}

void cMpvPlugin::PlayFileHandleType(string Filename, bool Shuffle)
{
  if (IsIsoImage(Filename.c_str())) // handle dvd iso images
  {
    Filename = "dvd://menu/" + Filename;
  }
  PlayFile(Filename, Shuffle);
}

VDRPLUGINCREATOR(cMpvPlugin); // Don't touch this!
