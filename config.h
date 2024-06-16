//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#ifndef __MPV_CONFIG_H
#define __MPV_CONFIG_H

#include <string>
#include <vector>

#include "player.h"

using std::string;
using std::vector;

class cMpvPluginConfig
{
  private:
    string PlayListExtString;       // list of playlist extensions, we will convert this into a vector
    vector<string> ExplodeString(string Input); // creates a vector from the given string

  public:
    cMpvPluginConfig();             // define default values
    int ProcessArgs(int argc, char *const argv[]); // parse command line arguments
    const char *CommandLineHelp();  // return our command line help string
    bool SetupParse(const char *name, const char *value);  // parse setup.conf values

    // plugin setup variables
    int UseDeinterlace;             // enable deinterlace
    int UsePassthrough;             // enable passthrough
    int UseDtsHdPassthrough;        // enable DTS-HD passthrough
    int StereoDownmix;              // enable stereo downmix
    int HideMainMenuEntry;          // hide main menu entry
    int PlaylistOnNextKey;          // skip to next playlist item on next/previous keys
    int PlaylistIfNoChapters;       // skip to next playlist item if the file has no chapters
    int ShowMediaTitle;             // show title from media file instead of filename
    int ShowSubtitles;              // show subtitles
    int ExitAtEnd;                  // exit at the end
    int ShowAfterStop;              // show after stop: 0 - black screen, 1 - filebrowser
    int SavePos;                    // save position on quit
    int SoftVol;                    // software volume

    // plugin parameter variables
    string BrowserRoot;             // start dir for filebrowser
    int RefreshRate;                // enable modeline switching
    string VideoOut;                // video out device
    string HwDec;                   // hwdec codecs
    string GpuCtx;                  // gpu context
    string AudioOut;                // audio out device
    string DiscDevice;              // optical disc device
    string Languages;               // language string for audio and subtitle TODO move to Setup menu
    vector<string> PlaylistExtensions; // file extensions which are recognized as a playlist
    string MainMenuEntry;           // the text displayed in VDR main menu
    int NoScripts;                  // don't load mpv LUA scripts

    string X11Display;              // X11 display used for mpv
    string TitleOverride;           // title to display (ovveride used via service interface)
    string Geometry;                // X11 display geometry
    string DRMdev;                  // DRM device
    int Windowed;                   // windowed mode, not fullscreen

    int ShowOptions;                // switch show menu options or filebrowser menu
};

// only create one instance (done in mpv.c), all other calls will simply get the extern reference
#ifdef CREATE_CONFIG
  cMpvPluginConfig *MpvPluginConfig;    // create an instance of this class to have the config available if needed
#else
  extern cMpvPluginConfig *MpvPluginConfig;
#endif

#endif

