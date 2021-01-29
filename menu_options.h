#ifndef __MPV_MENU_OPTIONS
#define __MPV_MENU_OPTIONS

#include <string>
#include <vdr/plugin.h>
#include "player.h"

using std::string;

class cMpvMenuOptions:public cOsdMenu
{
  private:
    cMpvPlayer *player;
    void ShowOptions();

  public:
    cMpvMenuOptions(cMpvPlayer *Player);
    ~cMpvMenuOptions();
    virtual eOSState ProcessKey(eKeys Key);
};

//Chapters
class cMpvMenuChapters:public cOsdMenu
{
  private:
    cMpvPlayer *player;

    void AddItem(string Title, int Number);

  public:
    cMpvMenuChapters(cMpvPlayer *Player);
    virtual eOSState ProcessKey(eKeys Key);
};

class cMpvMenuChapterItem:public cOsdItem
{
  private:
    int number;

  public:
    cMpvMenuChapterItem(string Title, int Number);
    int Number() { return number; }
};

//Playlist
class cMpvMenuPlaylist:public cOsdMenu
{
  private:
    cMpvPlayer *player;

    void AddItem(string Title, int Number);

  public:
    cMpvMenuPlaylist(cMpvPlayer *Player);
    virtual eOSState ProcessKey(eKeys Key);
};

class cMpvMenuPlaylistItem:public cOsdItem
{
  private:
    int number;

  public:
    cMpvMenuPlaylistItem(string Title, int Number);
    int Number() { return number; }
};

#endif

