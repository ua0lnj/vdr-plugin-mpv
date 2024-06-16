#include "menu_options.h"

cMpvMenuOptions::cMpvMenuOptions(cMpvPlayer *Player)
:cOsdMenu(tr("Options"))
{
  SetDisplayMenu();
  player = Player;
  ShowOptions();
  Show();
  Display();
}

cMpvMenuOptions::~cMpvMenuOptions()
{
#ifdef DEBUG
  dsyslog ("dekonstruktor\n");
#endif
}

void cMpvMenuOptions::ShowOptions()
{
  // display chapter select menu if the file has named named chapters
  if (player->NumChapters() && player->ChapterTitle(1) != "")
    Add(new cOsdItem(tr("Show chapters"), osUser1));

  if (player->TotalListPos() > 1)
    Add(new cOsdItem(tr("Show playlist"), osUser2));
}

eOSState cMpvMenuOptions::ProcessKey(eKeys Key)
{
  eOSState State = cOsdMenu::ProcessKey(Key);

  if (!HasSubMenu())
  {
    switch (Key)
    {
      case kOk:
        if(State == osUser1)
          AddSubMenu(new cMpvMenuChapters(player));
        if(State == osUser2)
          AddSubMenu(new cMpvMenuPlaylist(player));
      break;

      default:
      break;
    }
  }

  return State;
}

//Chapters
cMpvMenuChapters::cMpvMenuChapters(cMpvPlayer *Player)
:cOsdMenu(tr("Chapters"))
{
  player = Player;
  for (int i=1; i<=player->NumChapters(); i++)
    AddItem(player->ChapterTitle(i), i);

  Display();
}

eOSState cMpvMenuChapters::ProcessKey(eKeys Key)
{
  eOSState State = cOsdMenu::ProcessKey(Key);
  if (State == osUnknown)
  {
    switch (Key)
    {
      case kOk:
        cMpvMenuChapterItem *item = (cMpvMenuChapterItem *) Get(Current());
        player->SetChapter(item->Number());
      return osEnd;
    }
  }

  return State;
}

void cMpvMenuChapters::AddItem(string Title, int Number)
{
  bool Current = false;
  if (player->ChapterTitle(player->CurrentChapter()) == Title)
    Current = true;
  Add(new cMpvMenuChapterItem(Title, Number), Current);
}

cMpvMenuChapterItem::cMpvMenuChapterItem(string Title, int Number)
{
  number = Number;
  Title = std::to_string(number) + "  " + Title;
  SetText(Title.c_str());
}

//Playlist
cMpvMenuPlaylist::cMpvMenuPlaylist(cMpvPlayer *Player)
:cOsdMenu(tr("Playlist"))
{
  player = Player;
  for (int i=1; i<=player->TotalListPos(); i++)
    AddItem(player->ListTitle(i), i);

  Display();
}

eOSState cMpvMenuPlaylist::ProcessKey(eKeys Key)
{
  eOSState State = cOsdMenu::ProcessKey(Key);
  if (State == osUnknown)
  {
    switch (Key)
    {
      case kOk:
        if (player->IsRecord())
        {
          Skins.Message(mtError, tr("Recording - can't play!"));
          break;
        }
        cMpvMenuPlaylistItem *item = (cMpvMenuPlaylistItem *) Get(Current());
        player->PlayIndex(item->Number());
      return osEnd;
    }
  }

  return State;
}

void cMpvMenuPlaylist::AddItem(string Title, int Number)
{
  bool Current = false;
  if (player->ListTitle(player->CurrentListPos()) == Title)
    Current = true;
  Add(new cMpvMenuPlaylistItem(Title, Number), Current);
}

cMpvMenuPlaylistItem::cMpvMenuPlaylistItem(string Title, int Number)
{
  number = Number;
  Title = std::to_string(number) + "  " + Title;
  SetText(Title.c_str());
}
