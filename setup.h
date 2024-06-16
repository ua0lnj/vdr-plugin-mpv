//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#ifndef __MPV_SETUP_H
#define __MPV_SETUP_H

#include <vdr/plugin.h>

class cMpvPluginSetup:public cMenuSetupPage
{
  private:
    void Setup();
    virtual void Store();

    int SetupHideMainMenuEntry;
    int SetupUseDeinterlace;
    int SetupUsePassthrough;
    int SetupUseDtsHdPassthrough;
    int SetupStereoDownmix;
    int SetupPlaylistOnNextKey;
    int SetupPlaylistIfNoChapters;
    int SetupShowMediaTitle;
    int SetupShowSubtitles;
    int SetupExitAtEnd;
    int SetupShowAfterStop;
    int SetupSavePos;
    int SetupSoftVol;

  public:
    cMpvPluginSetup();
    virtual eOSState ProcessKey(eKeys);
};

#endif

