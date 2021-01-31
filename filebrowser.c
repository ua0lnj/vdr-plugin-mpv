#include <vector>
#include <string>
#include <algorithm>

#ifdef USE_DISC
#include <cdio/cdio.h>
#include <sys/mount.h>
#include <libmount/libmount.h>
#endif

#include "filebrowser.h"
#include "mpv_service.h"
#include "config.h"

using std::string;
using std::vector;
using std::sort;

string cMpvFilebrowser::currentDir = "";
string cMpvFilebrowser::currentItem = "";

cMpvFilebrowser::cMpvFilebrowser(string RootDir, string DiscDevice)
:cOsdMenu(tr("Filebrowser"))
{
  rootDir = RootDir;
  discDevice = DiscDevice;
  if (currentDir == "")
    currentDir = rootDir;
  ShowDirectory(currentDir);
}

void cMpvFilebrowser::ShowDirectory(string Path)
{
  Clear();
  vector<string> Directories;
  vector<string> Files;

  DIR *hDir;
  struct dirent *Entry;

  hDir = opendir(Path.c_str());

  // current directory deleted?
  if (!hDir && Path != rootDir)
  {
    currentDir = rootDir;
    Path = rootDir;
    hDir = opendir(Path.c_str());
  }
  // do not crash if browser root directory absent
  if (!hDir)
  {
    esyslog("[mpv] No browser root directory!\n");
    rootDir = "/";
    currentDir = rootDir;
    MpvPluginConfig->BrowserRoot = rootDir;
    Path = rootDir;
    hDir = opendir(Path.c_str());
  }
  while ((Entry = readdir(hDir)) != NULL)
  {
    if (!Entry || Entry->d_name[0] == '.')
      continue;

    struct stat Stat;
    string Filename = Path + "/" + Entry->d_name;
    stat(Filename.c_str(), &Stat);

    if (S_ISDIR(Stat.st_mode))
      Directories.push_back(Entry->d_name);

    else if (S_ISREG(Stat.st_mode))
      Files.push_back(Entry->d_name);
  }
  closedir(hDir);

  sort(Directories.begin(), Directories.end());
  sort(Files.begin(), Files.end());

  for (unsigned int i=0; i<Directories.size(); i++)
    AddItem(Path, Directories[i], true);

  for (unsigned int i=0; i<Files.size(); i++)
    AddItem(Path, Files[i], false);

  string MenuTitle = tr("Filebrowser");
  if (rootDir != Path)
    MenuTitle += " (" + Path.substr(rootDir.size() + 1, string::npos) + ")";
  SetTitle(MenuTitle.c_str());
#ifdef USE_DISC
  SetHelp(tr("Disc"), Directories.size() ? tr("PlayDir") : NULL, tr("Shuffle"), NULL);
#else
  SetHelp(NULL, Directories.size() ? tr("PlayDir") : NULL, tr("Shuffle"), NULL);
#endif
  Display();
}

int cMpvFilebrowser::PlayListCreate(string Path, FILE *fdPl)
{
  Clear();
  vector<string> PlayDirectories;
  vector<string> PlayFiles;

  DIR *playDir;
  struct dirent *Entry;

  playDir = opendir(Path.c_str());
  // do not crash if browser play directory absent
  if (!playDir)
  {
    esyslog("[mpv] No play directory!\n");
    return -1;
  }
  while ((Entry = readdir(playDir)) != NULL)
  {
    if (!Entry || Entry->d_name[0] == '.')
      continue;

    //excluding resume files
    string ex = Entry->d_name;
    if (ex.find("resume") != string::npos) continue;

    struct stat Stat;
    string Filename = Path + "/" + Entry->d_name;
    stat(Filename.c_str(), &Stat);

    if (S_ISDIR(Stat.st_mode))
      PlayDirectories.push_back(Entry->d_name);

    else if (S_ISREG(Stat.st_mode))
      PlayFiles.push_back(Entry->d_name);
  }
  closedir(playDir);

  if (!PlayDirectories.size() && !PlayFiles.size()) return -1;

  sort(PlayDirectories.begin(), PlayDirectories.end());
  sort(PlayFiles.begin(), PlayFiles.end());

  for (unsigned int i=0; i<PlayDirectories.size(); i++)
  {
    string Filedir = Path + "/" + PlayDirectories[i];
    PlayListCreate(Filedir, fdPl);
  }

  for (unsigned int i=0; i<PlayFiles.size(); i++)
  {
    string Filename = Path + "/" + PlayFiles[i];
    fprintf(fdPl, "%s\n", Filename.c_str());
  }
  return 0;
}

void cMpvFilebrowser::AddItem(string Path, string Text, bool IsDir)
{
  bool Current = false;
  if (currentItem == Text)
    Current = true;
  Add(new cMpvFilebrowserMenuItem(Path, Text, IsDir), Current);
}

eOSState cMpvFilebrowser::ProcessKey(eKeys Key)
{
  string newPath = "";
  cMpvFilebrowserMenuItem *item;
  eOSState State;
  switch (Key)
  {
    case kOk:
      item = (cMpvFilebrowserMenuItem *) Get(Current());
      if (!item) break;
      newPath = item->Path() + "/" + item->Text();
      if (item->IsDirectory())
      {
        currentDir = newPath;
        currentItem = "";
        ShowDirectory(newPath);
        return osContinue;
      }
      else
      {
        currentItem = item->Text();
        PlayFile(newPath);
        return osEnd;
      }
    break;

    case kBack:
      // we reached our root, so don't go back further and let VDR handle this (close submenu)
      if (currentDir == rootDir)
        return cOsdMenu::ProcessKey(Key);
      currentItem = currentDir.substr(currentDir.find_last_of("/")+1, string::npos);
      currentDir = currentDir.substr(0,currentDir.find_last_of("/"));
      ShowDirectory(currentDir);
      return osContinue;

    case kLeft:
    case kRight:
    case kUp:
    case kDown:
      // first let VDR handle those keys or we get the previous item
      State = cOsdMenu::ProcessKey(Key);
      item = (cMpvFilebrowserMenuItem *) Get(Current());
      currentItem = item->Text();
#ifdef USE_DISC
      SetHelp(tr("Disc"), item->IsDirectory() ? tr("PlayDir") : NULL, tr("Shuffle"), NULL);
#else
      SetHelp(NULL, item->IsDirectory() ? tr("PlayDir") : NULL, tr("Shuffle"), NULL);
#endif
    return State;

    case kRed:
      if (PlayDisc())
        return osEnd;
    break;

    case kYellow:
      item = (cMpvFilebrowserMenuItem *) Get(Current());
      if (!item) break;
      newPath = item->Path() + "/" + item->Text();
      if (!item->IsDirectory())
      {
        currentItem = item->Text();
        PlayFile(newPath, true);
        return osEnd;
      }
      return osContinue;

    case kGreen:
      item = (cMpvFilebrowserMenuItem *) Get(Current());
      if (!item) break;
      newPath = item->Path() + "/" + item->Text();
      if (item->IsDirectory())
      {
        int res;
        FILE *fdPl = fopen ("/tmp/mpv.playlist", "w");
        if (!fdPl)
        {
          Skins.Message(mtError, tr("Playlist cannot be created!"));
          esyslog ("[mpv] Playlist creation error!\n");
          return osEnd;
        }
        res = PlayListCreate(newPath, fdPl);
        fclose (fdPl);
        if (!res)
        {
          PlayFile("/tmp/mpv.playlist");
          return osEnd;
        }
        else
        {
          ShowDirectory(currentDir);
          Skins.Message(mtError, tr("Playlist cannot be created!"));
          return osContinue;
        }
      }
    break;

    default:
    break;
  }

  return cOsdMenu::ProcessKey(Key);
}

bool cMpvFilebrowser::PlayFile(string Filename, bool Shuffle)
{
  cPlugin *p;
  p = cPluginManager::GetPlugin("mpv");
  if (!p)
    return false;

  if (!Shuffle)
  {
    Mpv_PlayFile playFile;
    playFile.Filename = (char *)Filename.c_str();
    return p->Service("Mpv_PlayFile", &playFile);
  }
  else
  {
    Mpv_PlaylistShuffle shuffleFile;
    shuffleFile.Filename = (char *)Filename.c_str();
    return p->Service("Mpv_PlaylistShuffle", &shuffleFile);
  }

  // should never be reached, but silence a compiler warning
  return false;
}

// returns true if play is started, false otherwise
bool cMpvFilebrowser::PlayDisc()
{
#ifdef USE_DISC
  CdIo_t *hCdio = cdio_open(discDevice.c_str(), DRIVER_DEVICE);
  discmode_t DiscMode = cdio_get_discmode(hCdio);
  cdio_destroy(hCdio);
  switch (DiscMode)
  {
    case CDIO_DISC_MODE_CD_DA:
      PlayFile("cdda://");
    break;

    case CDIO_DISC_MODE_DVD_ROM:
    case CDIO_DISC_MODE_DVD_RAM:
    case CDIO_DISC_MODE_DVD_R:
    case CDIO_DISC_MODE_DVD_RW:
    case CDIO_DISC_MODE_HD_DVD_ROM:
    case CDIO_DISC_MODE_HD_DVD_RAM:
    case CDIO_DISC_MODE_HD_DVD_R:
    case CDIO_DISC_MODE_DVD_PR:
    case CDIO_DISC_MODE_DVD_PRW:
    case CDIO_DISC_MODE_DVD_PRW_DL:
    case CDIO_DISC_MODE_DVD_PR_DL:
    case CDIO_DISC_MODE_DVD_OTHER:
      if (!Mount(discDevice))
        break;
      struct stat DirInfo;
      if (stat("tmp/mpv_mount/VIDEO_TS", &DirInfo) == 0)
      {
        Unmount();
        return PlayFile("dvd://");
      }
      else if (stat("tmp/mpv_mount/BDMV", &DirInfo) == 0)
      {
        Unmount();
        return PlayFile("bd://");
      }
      Unmount();
    break;

    case CDIO_DISC_MODE_CD_XA:
    case CDIO_DISC_MODE_CD_MIXED:
    case CDIO_DISC_MODE_NO_INFO:
    case CDIO_DISC_MODE_ERROR:
    case CDIO_DISC_MODE_CD_I:
    default:
    break;
  }
#endif
return false;
}

bool cMpvFilebrowser::Mount(string Path)
{
#ifdef USE_DISC
  Unmount();
  string MountPoint = "/tmp/mpv_mount";
  if (mkdir(MountPoint.c_str(), S_IRWXU))
  {
    dsyslog("[mpv] create mount point failed");
    return false;
  }

  struct libmnt_context *hMount = mnt_new_context();
  mnt_context_set_source(hMount, Path.c_str());
  mnt_context_set_target(hMount, MountPoint.c_str());
  mnt_context_set_mflags(hMount, MS_RDONLY);

  int res = mnt_context_mount(hMount);
  mnt_free_context(hMount);
  if (res == 0)
    return true;
#endif
  return false;
}

bool cMpvFilebrowser::Unmount()
{
#ifdef USE_DISC
  string MountPoint = "/tmp/mpv_mount";
  int res = umount(MountPoint.c_str());
  rmdir(MountPoint.c_str());
  if (res == 0)
    return true;
#endif
  return false;
}

cMpvFilebrowserMenuItem::cMpvFilebrowserMenuItem(string Path, string Item, bool IsDir)
{
  isDir = IsDir;
  path = Path;
  SetText(Item.c_str());
}

