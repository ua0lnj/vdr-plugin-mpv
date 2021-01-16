//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#ifndef __MPV_PLAYER_H
#define __MPV_PLAYER_H

#include <string>
#include <vector>
#include <vdr/player.h>
#include <mpv/client.h>

using std::string;
using std::vector;

class cMpvPlayer:public cPlayer
{
  private:
    void PlayerStart();
    void SwitchOsdToMpv();
    void PlayerHideCursor();

    string PlayFilename;              // file to play
    bool PlayShuffle;                 // shuffle playlist
    pthread_t ObserverThreadHandle;   // player property observer thread
    mpv_handle *hMpv;                 // handle to mpv
    int OriginalFps;                  // fps before modeswitch
    bool IsPlaylist(string File);     // returns true if the given file is a playlist
    void ChangeFrameRate(int TargetRate);
    void HandlePropertyChange(mpv_event *event);  // handle change mpv property
    void HandleTracksChange();                    // handle mpv track information
    static void *ObserverThread(void *handle);
#ifdef USE_KEYPRESS
    pthread_t XEventThreadHandle;     // Xorg event thread
    static void *XEventThread(void *handle);
#endif
    // Player status variables
    int PlayerPaused;                 // player paused
    int PlayerDiscNav;                // discnav active
    int PlayerNumChapters;            // number of chapters
    int PlayerChapter;                // current chapter
    int PlayerFps;                    // frames per second
    int PlayerCurrent;                // current postion in seconds
    int PlayerTotal;                  // total length in seconds
    string PlayerFilename;            // filename currently playing
    int PlayerSpeed;                  // player playback speed
    vector<int> PlayerChapters;       // chapter start times
    vector<string> ChapterTitles;     // chapter titles
    string mediaTitle;                // title from meta data
    static cMpvPlayer *PlayerHandle;  // our player
    static volatile int running;
    int ListCurrent;                  // position in playlist
    int ListTotal;                    // number of positions in playlist

  public:
    cMpvPlayer(string Filename, bool Shuffle=false);
    virtual ~cMpvPlayer();
    void Activate(bool);                          // player attached/detached
    virtual void SetAudioTrack(eTrackType Type, const tTrackId *TrackId);
    virtual void SetSubtitleTrack(eTrackType Type, const tTrackId *TrackId);
    virtual bool GetReplayMode(bool &Play, bool &Forward, int &Speed);
    virtual bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
    virtual double FramesPerSecond();
    void OsdClose();                              // clear or close current OSD
    void Shutdown();
    static volatile int PlayerIsRunning() { return running; }
    static cMpvPlayer *Player() { return PlayerHandle; }

    // functions to send commands to mpv
    void SendCommand(const char *cmd, ...);
    void PlayNew(string Filename);
    void Seek(int Seconds); // seek n seconds
    void SetTimePos(int Seconds);
    void SetSpeed(int Speed);
    void SetAudio(int Audio);
    void SetSubtitle(int Subtitle);
    void SetChapter(int Chapter);
    void TogglePause();
    void QuitPlayer();
    void StopPlayer();
    void SavePosPlayer();
    void DiscNavUp();
    void DiscNavDown();
    void DiscNavLeft();
    void DiscNavRight();
    void DiscNavMenu();
    void DiscNavSelect();
    void DiscNavPrev();
    void PreviousChapter();
    void NextChapter();
    void NextPlaylistItem();
    void PreviousPlaylistItem();
    void SetVolume(int Volume);
    void ScaleVideo(int x, int y, int width, int height);

    // functions to get different status information about the current playback
    int IsPaused() { return PlayerPaused; }
    int DiscNavActive() { return PlayerDiscNav; }
    int NumChapters() { return PlayerNumChapters; }
    int CurrentChapter() { return PlayerChapter + 1; } // return the current value +1 since mpv begins at 0
    int CurrentPlayTime() { return PlayerCurrent; }
    int TotalPlayTime() { return PlayerTotal; }
    int CurrentListPos() { return ListCurrent; }
    int TotalListPos() { return ListTotal; }
    int CurrentFps() { return PlayerFps; }
    string CurrentFile() { return PlayerFilename; }
    int CurrentPlaybackSpeed() { return PlayerSpeed; }
    int ChapterStartTime(unsigned int Chapter) { if (Chapter <= PlayerChapters.size()) return PlayerChapters[Chapter-1]; else return 0; }
    string ChapterTitle(unsigned int Chapter) { if (Chapter <= ChapterTitles.size()) return ChapterTitles[Chapter-1]; else return ""; }
    string MediaTitle() { return mediaTitle; }
};

#endif
