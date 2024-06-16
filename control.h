//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#ifndef __MPV_CONTROL_H
#define __MPV_CONTROL_H

#include <vdr/player.h>
#include "player.h"
#include "status.h"

class cMpvControl:public cControl
{
  private:
    cMpvPlayer *Player;                 // our player
    cSkinDisplayReplay *DisplayReplay;  // our osd display
    void ShowProgress(int playlist = 0);    // display progress bar
    void Hide();                        // hide replay control
    bool infoVisible;                   // RecordingInfo visible
    int LastPlayerCurrent;              // the lasz cuurent time the osd was rendered with
    cMarks ChapterMarks;                // chapter marks
    time_t timeoutShow;                 // timeout shown control
    cMpvStatus *VolumeStatus;           // observe hte VDR volume and adjust mpv volume

    void TimeSearch();
    void TimeSearchDisplay();
    void TimeSearchProcess(eKeys Key);
    bool timeSearchActive, timeSearchHide;
    int timeSearchTime, timeSearchPos;

  public:
    cMpvControl(string filename, bool Shuffle=false);
    virtual ~cMpvControl();
    virtual eOSState ProcessKey(eKeys); // handle keyboard input
    cMarks *Marks() { return &ChapterMarks; }
    void UpdateMarks();
    void PlayNew(string Filename);
    void SeekTo(int seconds);
    void SeekRelative(int seconds);
    void ScaleVideo(int x, int y, int width, int height);
    uint8_t *GrabImage(int *size, int *width, int *height);
};

#endif
