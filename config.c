//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <vdr/tools.h>

#include "config.h"

cMpvPluginConfig::cMpvPluginConfig()
{
  HideMainMenuEntry = 0;
  UseDeinterlace = 1;
  UsePassthrough = 1;
  UseDtsHdPassthrough = 1;
  StereoDownmix = 0;
  PlaylistOnNextKey = 0;
  PlaylistIfNoChapters = 1;
  ShowMediaTitle = 0;
  ShowSubtitles = 0;
  ExitAtEnd = 1;
  SavePos = 0;
  SoftVol = 0;

  BrowserRoot = "/";
  RefreshRate = 0;
  VideoOut = "vdpau";
  HwDec = "vdpau";
  UseGlx = 0;
  AudioOut = "alsa:device=default";
  DiscDevice = "/dev/dvd";
  Languages = "deu,de,ger,eng,en";
  PlayListExtString = "m3u,ini,pls,txt,playlist";
  MainMenuEntry = "MPV";
  NoScripts = 0;

  X11Display = ":0.0";
  Geometry = "";
  Windowed = 0;
  ShowOptions = 0;
}

vector<string> cMpvPluginConfig::ExplodeString(string Input)
{
  vector<string> Result;

  std::stringstream Data(Input);
  string Item;
  while(std::getline(Data, Item, ','))
  {
    if (Item != "")
      Result.push_back(Item);
  }
  return Result;
}

int cMpvPluginConfig::ProcessArgs(int argc, char *const argv[])
{
  char *s;
  if ((s = getenv("DISPLAY")))
  {
    X11Display = s;
  }
  else
    setenv("DISPLAY", X11Display.c_str(), 1);

  for (;;)
  {
    switch (getopt(argc, argv, "a:v:h:d:b:l:x:rm:swg:"))
    {
      case 'a': // audio out
        AudioOut = optarg;
      continue;
      case 'v': // video out
        VideoOut = optarg;
      continue;
      case 'h': // hwdec-codecs
        HwDec = optarg;
      continue;
      case 'd': // dvd-device
        DiscDevice = optarg;
      continue;
      case 'b': // browser root
        BrowserRoot = optarg;
      continue;
      case 'l':  // languages
        Languages = optarg;
      continue;
      case 'x':  // playlist extensions
        PlayListExtString = optarg;
      continue;
      case 'r': // refresh rate
        RefreshRate = 1;
      continue;
      case 'm':
        MainMenuEntry = optarg;
      continue;
      case 's':
        NoScripts = 1;
      continue;
      case 'w':
        Windowed = 1;
      continue;
      case 'g': // glx with x11 and geometry
        Geometry = optarg;
        UseGlx = 1;
      continue;
      case EOF:
      break;
      case ':':
        esyslog("[mpv]: missing argument for option '%c'\n", optopt);
        return 0;
      default:
        if (optopt == 'g') //glx with x11
        {
          UseGlx = 1;
          continue;
        }
        esyslog("[mpv]: unknown option '%c'\n", optopt);
        return 0;
    }
    break;
  }
  while (optind < argc)
  {
    esyslog("[mpv]: unhandled argument '%s'\n", argv[optind++]);
  }

  //convert the playlist extension string to a vector
  PlaylistExtensions = ExplodeString(PlayListExtString);

  return 1;
}

const char *cMpvPluginConfig::CommandLineHelp(void)
{
  return
    "  -a audio\tmpv --ao (Default: alsa:device=default)\n"
    "  -v video\tmpv --vo (Default: vdpau)\n"
    "  -h hwdec\tmpv --hwdec-codecs (Default: vdpau)\n"
    "  -d device\tmpv optical disc device (Default: /dev/dvd)\n"
    "  -l languages\tlanguages for audio and subtitles (Default: deu,de,ger,eng,en)\n"
    "  -b /dir\tbrowser root directory\n"
    "  -x extensions\tfiles with this extensions are handled as playlists\n"
    "\t\t(Default: m3u,ini,pls,txt,playlist)\n"
#ifdef USE_XRANDR
    "  -r\t\tswitch modeline to refresh rate of played file\n"
#endif
    "  -m text\ttext displayed in VDR main menu (Default: MPV)\n"
    "  -s\t\tdon't load mpv LUA scripts\n"
    "  -g\t\tuse GLX with X11\n"
    "  -g geometry\t X11 geometry [W[xH]][+-x+-y][/WS] or x:y\n"
    ;
}

bool cMpvPluginConfig::SetupParse(const char *name, const char *value)
{
  if (!strcasecmp(name, "HideMainMenuEntry"))
    HideMainMenuEntry = atoi(value);
  else if (!strcasecmp(name, "UseDeinterlace"))
    UseDeinterlace = atoi(value);
  else if (!strcasecmp(name, "UsePassthrough"))
    UsePassthrough = atoi(value);
  else if (!strcasecmp(name, "StereoDownmix"))
    StereoDownmix = atoi(value);
  else if (!strcasecmp(name, "UseDtsHdPassthrough"))
    UseDtsHdPassthrough = atoi(value);
  else if (!strcasecmp(name, "PlaylistOnNextKey"))
    PlaylistOnNextKey = atoi(value);
  else if (!strcasecmp(name, "PlaylistIfNoChapters"))
    PlaylistIfNoChapters = atoi(value);
  else if (!strcasecmp(name, "ShowMediaTitle"))
    ShowMediaTitle = atoi(value);
  else if (!strcasecmp(name, "ShowSubtitles"))
    ShowSubtitles = atoi(value);
  else if (!strcasecmp(name, "ExitAtEnd"))
    ExitAtEnd = atoi(value);
  else if (!strcasecmp(name, "SavePos"))
    SavePos = atoi(value);
  else if (!strcasecmp(name, "SoftVol"))
    SoftVol = atoi(value);
  else
    return false;
  return true;
}

