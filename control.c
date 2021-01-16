//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#include "control.h"
#include "config.h"
#include <vdr/interface.h>

cMpvControl::cMpvControl(string Filename, bool Shuffle)
:cControl(Player = new cMpvPlayer(Filename.c_str(), Shuffle))
{
#ifdef DEBUG
  dsyslog("[mpv] %s\n", __FUNCTION__);
#endif
  DisplayReplay = NULL;
  VolumeStatus = NULL;
  if (!MpvPluginConfig->UsePassthrough)
    VolumeStatus = new cMpvStatus(Player);
  infoVisible = false;
  timeSearchActive = false;
  cStatus::MsgReplaying(this, Filename.c_str(), Filename.c_str(), true);
}

cMpvControl::~cMpvControl()
{
#ifdef DEBUG
  dsyslog("[mpv]%s\n", __FUNCTION__);
#endif
  if (VolumeStatus)
    delete VolumeStatus;
  Hide();
  Player->OsdClose();
  // Osd Workaround
  // osd is open, first send menu key, this will close the menu, if any other
  // osd was open this opens the menu, so send kBack afterwards, to close it again
  if (cOsd::IsOpen())
  {
    eKeys key = kMenu;
#ifdef DEBUG
    dsyslog ("[mpv] osd is open sending menu key followed by back key\n");
#endif
    cRemote::Put(key);
    key = kBack;
    cRemote::Put(key);
  }
  cStatus::MsgReplaying(this, NULL, NULL, false); // This has to be done before delete the player
  Player->Shutdown();
  delete Player;
  Player = NULL;
  player = NULL;
  if (remove ("/tmp/mpv.playlist")) dsyslog ("[mpv] Playlist was not used\n");
  cDevice::SetPrimaryDevice(cDevice::PrimaryDevice()->DeviceNumber() + 1);
}

void cMpvControl::ShowProgress(int playlist)
{
  if (!Player->IsPaused() && LastPlayerCurrent == Player->CurrentPlayTime())
    return;
  LastPlayerCurrent = Player->CurrentPlayTime();

  if (!DisplayReplay)
    DisplayReplay = Skins.Current()->DisplayReplay(false);

  if (!infoVisible)
  {
    UpdateMarks();
    infoVisible = true;
    timeoutShow = Setup.ProgressDisplayTime ? (time(0) + Setup.ProgressDisplayTime) : 0;
  }

  string TitleDisplay = Player->CurrentFile();

  if (MpvPluginConfig->TitleOverride != "")
    TitleDisplay = MpvPluginConfig->TitleOverride;
  else if (MpvPluginConfig->ShowMediaTitle && Player->MediaTitle() != "")
    TitleDisplay = Player->MediaTitle();

  if (Player->TotalListPos() > 1 && !playlist)
    TitleDisplay = std::string(itoa(Player->CurrentListPos())) + " " + TitleDisplay;

  if (Player->NumChapters() > 0)
  {
    if (Player->ChapterTitle(Player->CurrentChapter()) != "")
      TitleDisplay += " - " + Player->ChapterTitle(Player->CurrentChapter());

    char buffer[10];
    int err = snprintf (buffer, sizeof(buffer), " (%d/%d)", Player->CurrentChapter(), Player->NumChapters());
    if (err < 0)
    {
      esyslog ("[mpv] ShowProgress TitleDisplay error!\n");
      return;
    }
    TitleDisplay += buffer;
    DisplayReplay->SetMarks(Marks());
  }

  DisplayReplay->SetTitle(TitleDisplay.c_str());

  if (playlist)
    DisplayReplay->SetProgress(Player->CurrentListPos(), Player->TotalListPos());
  else
    DisplayReplay->SetProgress(Player->CurrentPlayTime(), Player->TotalPlayTime() != 0 ? Player->TotalPlayTime() : 1);

  int Speed = Player->CurrentPlaybackSpeed();
  if (Speed == 1)
    Speed = -1;
  DisplayReplay->SetMode(!Player->IsPaused(), true, Speed);

  if (playlist)
{
  DisplayReplay->SetCurrent(itoa(Player->CurrentListPos()));
  DisplayReplay->SetTotal(itoa(Player->TotalListPos()));
}
else
{
  DisplayReplay->SetCurrent(IndexToHMSF(Player->CurrentPlayTime(), false, 1));
  DisplayReplay->SetTotal(IndexToHMSF(Player->TotalPlayTime(), false, 1));
}
  SetNeedsFastResponse(true);
  Skins.Flush();
}

void cMpvControl::Hide()
{
  if (DisplayReplay)
  {
    delete DisplayReplay;
    DisplayReplay = NULL;
    SetNeedsFastResponse(false);
  }
}

eOSState cMpvControl::ProcessKey(eKeys key)
{
  eOSState state;
  static int count;
#ifdef DEBUG
  if (key != kNone)
  {
    dsyslog("[mpv]%s: key=%d\n", __FUNCTION__, key);
  }
#endif
  if (!Player->PlayerIsRunning()) // check if player is still alive
  {
    cControl::Shutdown();
    return osEnd;
  }

  if (timeSearchActive && key != kNone)
  {
    TimeSearchProcess(key);
    return osContinue;
  }

  if (timeoutShow && time(0) > timeoutShow)
  {
    timeoutShow = 0;
    infoVisible = false;
    Hide();
  }
  else if (infoVisible) // if RecordingInfo visible then update
    ShowProgress(count % 2);

  state = osContinue;
  switch ((int)key) // cast to shutup g++ warnings
  {
    case kUp:
      if (Player->DiscNavActive())
      {
        Player->DiscNavUp();
        break;
      }
    case kPlay:
      if (Player->CurrentPlaybackSpeed() != 1)
      {
        Player->SetSpeed(1);
      }
      if (Player->IsPaused())
      {
        Player->TogglePause();
      }
    break;

    case kDown:
      if (Player->DiscNavActive())
      {
        Player->DiscNavDown();
        break;
      }
    case kPause:
      Player->TogglePause();
    break;

    case kLeft:
      if (Player->DiscNavActive())
      {
        Player->DiscNavLeft();
        break;
      }
    case kFastRew:
      // reset to normal playback speed (if fastfwd is active currently) and then just go back 5 seconds since there is no fastrew in mpv
      Player->SetSpeed(1);
      Player->Seek(-5);
    break;
    case kRight:
      if (Player->DiscNavActive())
      {
        Player->DiscNavRight();
        break;
      }
    case kFastFwd:
      if (Player->CurrentPlaybackSpeed() < 32)
      {
        Player->SetSpeed(Player->CurrentPlaybackSpeed() * 2);
      }
    break;

    case kRed:
      TimeSearch();
    break;
    case kGreen | k_Repeat:
    case kGreen:
      Player->Seek(-60);
    break;
    case kYellow | k_Repeat:
    case kYellow:
      Player->Seek(+60);
    break;

    case kBlue:
      MpvPluginConfig->ShowOptions = 1;
      cRemote::CallPlugin("mpv");
    break;

    case kBack:
      if (Player->DiscNavActive())
      {
        Player->DiscNavPrev();
        break;
      }
      MpvPluginConfig->ShowOptions = 0;
      cRemote::CallPlugin("mpv");
      break;
    case kStop:
      if (!MpvPluginConfig->ExitAtEnd)
      {
        Hide();
        if (MpvPluginConfig->SavePos)
          Player->SavePosPlayer();
        Player->StopPlayer();
      }
      else
      {
        Hide();
        Player->QuitPlayer();
      }
    break;

    case kOk:
      if (Player->DiscNavActive())
      {
        Player->DiscNavSelect();
        break;
      }
      if (infoVisible && (count > 1))
      {
        count = 0;
        Hide();
        infoVisible = false;
      }
      else
      {
        if (Player->TotalListPos() <= 1) count++;
        count++;
        ShowProgress(count % 2);
      }
    break;

    case k5:
      Player->DiscNavMenu();
    break;

    case kNext:
      if (MpvPluginConfig->PlaylistOnNextKey || (MpvPluginConfig->PlaylistIfNoChapters && !Player->NumChapters()))
        Player->NextPlaylistItem();
      else
        Player->NextChapter();
    break;

    case kPrev:
      if (MpvPluginConfig->PlaylistOnNextKey || (MpvPluginConfig->PlaylistIfNoChapters && !Player->NumChapters()))
        Player->PreviousPlaylistItem();
      else
        Player->PreviousChapter();
    break;

    case k1 | k_Repeat:
    case k1:
      Player->Seek(-15);
    break;
    case k3 | k_Repeat:
    case k3:
      Player->Seek(+15);
    break;

    case k7:
      if (MpvPluginConfig->PlaylistOnNextKey)
        Player->PreviousChapter();
      else
        Player->PreviousPlaylistItem();
    break;

    case k9:
      if (MpvPluginConfig->PlaylistOnNextKey)
        Player->NextChapter();
      else
        Player->NextPlaylistItem();
    break;

    default:
      break;
  }
  return state;
}

void cMpvControl::UpdateMarks()
{
  ((cList<cMark> *)&ChapterMarks)->Clear();

  if (Player->NumChapters() > 1)
  {
    ChapterMarks.Add(0);
    for (int i=1; i <= Player->NumChapters(); i++)
    {
      ChapterMarks.Add(Player->ChapterStartTime(i) - 1);
      ChapterMarks.Add(Player->ChapterStartTime(i));
    }
  }
}

// TimeSearch code is copied from vdr menu.c and slightly adjusted to fit with our class
void cMpvControl::TimeSearch()
{
  timeSearchTime = timeSearchPos = 0;
  timeSearchHide = false;
  if (!infoVisible) {
     ShowProgress();
     if (infoVisible)
        timeSearchHide = true;
     else
        return;
     }
  timeoutShow = 0;
  TimeSearchDisplay();
  timeSearchActive = true;
}

void cMpvControl::TimeSearchDisplay()
{
  char buf[64];
  // TRANSLATORS: note the trailing blank!
  strcpy(buf, tr("Jump: "));
  int len = strlen(buf);
  char h10 = '0' + (timeSearchTime >> 24);
  char h1  = '0' + ((timeSearchTime & 0x00FF0000) >> 16);
  char m10 = '0' + ((timeSearchTime & 0x0000FF00) >> 8);
  char m1  = '0' + (timeSearchTime & 0x000000FF);
  char ch10 = timeSearchPos > 3 ? h10 : '-';
  char ch1  = timeSearchPos > 2 ? h1  : '-';
  char cm10 = timeSearchPos > 1 ? m10 : '-';
  char cm1  = timeSearchPos > 0 ? m1  : '-';
  sprintf(buf + len, "%c%c:%c%c", ch10, ch1, cm10, cm1);
  DisplayReplay->SetJump(buf);
}

void cMpvControl::TimeSearchProcess(eKeys Key)
{
#define STAY_SECONDS_OFF_END 10
  int Seconds = (timeSearchTime >> 24) * 36000 + ((timeSearchTime & 0x00FF0000) >> 16) * 3600 + ((timeSearchTime & 0x0000FF00) >> 8) * 600 + (timeSearchTime & 0x000000FF) * 60;
  int Current = Player->CurrentPlayTime();
  int Total = Player->TotalPlayTime();
  switch (Key) {
    case k0 ... k9:
         if (timeSearchPos < 4) {
            timeSearchTime <<= 8;
            timeSearchTime |= Key - k0;
            timeSearchPos++;
            TimeSearchDisplay();
            }
         break;
    case kFastRew:
    case kLeft:
    case kFastFwd:
    case kRight: {
         int dir = ((Key == kRight || Key == kFastFwd) ? 1 : -1);
         if (dir > 0)
#if VDRVERSNUM < 20305
            Seconds = min(Total - Current - STAY_SECONDS_OFF_END, Seconds);
#else
            Seconds = std::min(Total - Current - STAY_SECONDS_OFF_END, Seconds);
#endif
         else
            Seconds = 0-Seconds;
         Player->Seek(Seconds);
         timeSearchActive = false;
         }
         break;
    case kPlayPause:
    case kPlay:
    case kUp:
    case kPause:
    case kDown:
    case kOk:
         if (timeSearchPos > 0) {
#if VDRVERSNUM < 20305
            Seconds = min(Total - STAY_SECONDS_OFF_END, Seconds);
#else
            Seconds = std::min(Total - STAY_SECONDS_OFF_END, Seconds);
#endif
            Player->SetTimePos(Seconds);
            }
         timeSearchActive = false;
         break;
    default:
         if (!(Key & k_Flags)) // ignore repeat/release keys
            timeSearchActive = false;
         break;
    }

  if (!timeSearchActive) {
     if (timeSearchHide)
        Hide();
     else
        DisplayReplay->SetJump(NULL);
//     ShowProgress();
     }
}

void cMpvControl::PlayNew(string Filename)
{
  Player->PlayNew(Filename);
}

void cMpvControl::SeekTo(int seconds)
{
  Player->SetTimePos(seconds);
}

void cMpvControl::SeekRelative(int seconds)
{
  Player->Seek(seconds);
}

void cMpvControl::ScaleVideo(int x, int y, int width, int height)
{
  Player->ScaleVideo(x, y, width, height);
}
