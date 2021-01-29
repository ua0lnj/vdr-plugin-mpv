//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#include <locale.h>
#include <string>
#include <vector>

#include "player.h"
#include "config.h"
#include "osd.h"

#ifdef USE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>

using std::vector;

#define MPV_OBSERVE_TIME_POS 1
#define MPV_OBSERVE_DISC_MENU 2
#define MPV_OBSERVE_FPS 3
#define MPV_OBSERVE_FILENAME 4
#define MPV_OBSERVE_LENGTH 5
#define MPV_OBSERVE_CHAPTERS 6
#define MPV_OBSERVE_CHAPTER 7
#define MPV_OBSERVE_PAUSE 8
#define MPV_OBSERVE_SPEED 9
#define MPV_OBSERVE_MEDIA_TITLE 10
#define MPV_OBSERVE_LIST_POS 11
#define MPV_OBSERVE_LIST_COUNT 12

volatile int cMpvPlayer::running = 0;
cMpvPlayer *cMpvPlayer::PlayerHandle = NULL;
std::string LocaleSave;

Display *Dpy = NULL;
xcb_connection_t *Connect;
xcb_window_t VideoWindow;

#ifdef USE_KEYPRESS

#ifdef __cplusplus
extern "C"
{
#endif
extern void FeedKeyPress(const char *, const char *, int, int, const char *);
#ifdef __cplusplus
}
#endif

void *cMpvPlayer::XEventThread(void *handle)
{
  XEvent event;
  KeySym keysym;
  const char *keynam;
  char buf[64];
  char letter[64];
  int letter_len;
  cMpvPlayer *Player = (cMpvPlayer*) handle;

  while (Player->PlayerIsRunning())
  {
    if(Dpy && Connect && VideoWindow) {
        XWindowEvent(Dpy, VideoWindow, KeyPressMask, &event);
        switch (event.type) {
          case KeyPress:
	    letter_len =
		XLookupString(&event.xkey, letter, sizeof(letter) - 1, &keysym, NULL);
	    if (letter_len < 0) {
		letter_len = 0;
	    }
	    letter[letter_len] = '\0';
	    if (keysym == NoSymbol) {
		dsyslog("video/event: No symbol for %d\n", event.xkey.keycode);
		break;
	    }
	    keynam = XKeysymToString(keysym);
	    // check for key modifiers (Alt/Ctrl)
	    if (event.xkey.state & (Mod1Mask | ControlMask)) {
		if (event.xkey.state & Mod1Mask) {
		    strcpy(buf, "Alt+");
		} else {
		    buf[0] = '\0';
		}
		if (event.xkey.state & ControlMask) {
		    strcat(buf, "Ctrl+");
		}
		strncat(buf, keynam, sizeof(buf) - 10);
		keynam = buf;
	    }
	    FeedKeyPress("XKeySym", keynam, 0, 0, letter);
            break;
          default:
            break;
        }
    }
    usleep(1000);
  }
#ifdef DEBUG
  dsyslog("[mpv] XEvent thread ended\n");
#endif
  return handle;
}
#endif

// check mpv errors and send them to log
static inline void check_error(int status)
{
  if (status < 0)
  {
    esyslog("[mpv] API error: %s\n", mpv_error_string(status));
  }
}

void *cMpvPlayer::ObserverThread(void *handle)
{
  cMpvPlayer *Player = (cMpvPlayer*) handle;
  struct mpv_event_log_message *msg;

  // set properties which should be observed
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_TIME_POS, "time-pos", MPV_FORMAT_DOUBLE);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_DISC_MENU, "disc-menu-active", MPV_FORMAT_FLAG);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_FPS, "container-fps", MPV_FORMAT_DOUBLE);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_FILENAME, "filename", MPV_FORMAT_STRING);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_LENGTH, "duration", MPV_FORMAT_DOUBLE);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_CHAPTERS, "chapters", MPV_FORMAT_INT64);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_CHAPTER, "chapter", MPV_FORMAT_INT64);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_PAUSE, "pause", MPV_FORMAT_FLAG);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_SPEED, "speed", MPV_FORMAT_DOUBLE);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_MEDIA_TITLE, "media-title", MPV_FORMAT_STRING);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_LIST_POS, "playlist-pos-1", MPV_FORMAT_INT64);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_LIST_COUNT, "playlist-count", MPV_FORMAT_INT64);

  while (Player->PlayerIsRunning())
  {
    mpv_event *event = mpv_wait_event(Player->hMpv, 5);
    switch (event->event_id)
    {
      case MPV_EVENT_SHUTDOWN :
        Player->running = 0;
      break;

      case MPV_EVENT_PROPERTY_CHANGE :
        Player->HandlePropertyChange(event);
      break;

      case MPV_EVENT_PLAYBACK_RESTART :
        Player->ChangeFrameRate(Player->CurrentFps()); // switching directly after the fps event causes black screen
      break;

      case MPV_EVENT_LOG_MESSAGE :
        msg = (struct mpv_event_log_message *)event->data;
        // without DEBUG log to error since we only request error messages from mpv in this case
#ifdef DEBUG
          dsyslog("[mpv]: %s\n", msg->text);
#else
          esyslog("[mpv]: %s\n", msg->text);
#endif
      break;

      case MPV_EVENT_TRACKS_CHANGED :
        Player->HandleTracksChange();
      break;

      case MPV_EVENT_VIDEO_RECONFIG :
          Player->PlayerHideCursor();
      break;

      case MPV_EVENT_NONE :
      case MPV_EVENT_END_FILE :
      case MPV_EVENT_PAUSE :
      case MPV_EVENT_UNPAUSE :
      case MPV_EVENT_FILE_LOADED :
      case MPV_EVENT_GET_PROPERTY_REPLY :
      case MPV_EVENT_SET_PROPERTY_REPLY :
      case MPV_EVENT_COMMAND_REPLY :
      case MPV_EVENT_START_FILE :
      case MPV_EVENT_TRACK_SWITCHED :
      case MPV_EVENT_IDLE :
      case MPV_EVENT_TICK :
      case MPV_EVENT_SCRIPT_INPUT_DISPATCH :
      case MPV_EVENT_CLIENT_MESSAGE :
      case MPV_EVENT_AUDIO_RECONFIG :
      case MPV_EVENT_METADATA_UPDATE :
      case MPV_EVENT_SEEK :
      case MPV_EVENT_CHAPTER_CHANGE :
      default :
        dsyslog("[mpv]: event: %d %s\n", event->event_id, mpv_event_name(event->event_id));
      break;
    }
  }
#ifdef DEBUG
  dsyslog("[mpv] Observer thread ended\n");
#endif
  return handle;
}

cMpvPlayer::cMpvPlayer(string Filename, bool Shuffle)
:cPlayer(pmExtern_THIS_SHOULD_BE_AVOIDED)
{
  PlayerHandle = this;
  PlayFilename = Filename;
  PlayShuffle = Shuffle;
  running = 0;
  OriginalFps = -1;
}

cMpvPlayer::~cMpvPlayer()
{
#ifdef DEBUG
  dsyslog("[mpv]%s: end\n", __FUNCTION__);
#endif
  Detach();
  PlayerHandle = NULL;
}

void cMpvPlayer::Activate(bool on)
{
  if (on)
    PlayerStart();
}

void cMpvPlayer::SetAudioTrack(eTrackType Type, const tTrackId *TrackId)
{
  SetAudio(TrackId->id);
}

void cMpvPlayer::SetSubtitleTrack(eTrackType Type, const tTrackId *TrackId)
{
  if (Type == ttNone)
  {
    check_error(mpv_set_option_string(hMpv, "sub-forced-only", "yes"));
    return;
  }
  check_error(mpv_set_option_string(hMpv, "sub-forced-only", "no"));
  SetSubtitle(TrackId->id);
}

bool cMpvPlayer::GetReplayMode(bool &Play, bool &Forward, int &Speed)
{
  Speed = CurrentPlaybackSpeed();
  if (Speed == 1)
    Speed = -1;
  Forward = true;
  Play = !IsPaused();

  return true;
}

bool cMpvPlayer::GetIndex(int& Current, int& Total, bool SnapToIFrame __attribute__((unused)))
{
	Total = TotalPlayTime() * FramesPerSecond();
	Current = CurrentPlayTime() * FramesPerSecond();
	return true;
}

double cMpvPlayer::FramesPerSecond()
{
  return CurrentFps();
}

void cMpvPlayer::PlayerHideCursor()
{
  int screen_nr;
  int i;
  int len;
  xcb_screen_iterator_t screen_iter;
  xcb_screen_t *screen;
  xcb_query_tree_cookie_t  cookie;
  xcb_query_tree_reply_t *reply;
  xcb_window_t *child;

  if (!Dpy)
    Dpy = XOpenDisplay(MpvPluginConfig->X11Display.c_str());
  if (!Dpy) return;

  if (!Connect)
    Connect = XGetXCBConnection(Dpy);
  if (!Connect) return;

  if(!VideoWindow)
  {
    screen_nr = DefaultScreen(Dpy);

    screen_iter = xcb_setup_roots_iterator(xcb_get_setup(Connect));
    for (i = 0; i < screen_nr; ++i)
    {
      xcb_screen_next(&screen_iter);
    }
    screen = screen_iter.data;

    cookie = xcb_query_tree(Connect,screen->root);
    reply = xcb_query_tree_reply(Connect, cookie, 0);
    len = xcb_query_tree_children_length(reply);

    if (len)
    {
      uint32_t values[4];
      xcb_pixmap_t pixmap;
      xcb_cursor_t cursor;

      xcb_get_property_cookie_t procookie;
      xcb_get_property_reply_t *proreply;

      xcb_atom_t property = XCB_ATOM_WM_NAME;

      child = xcb_query_tree_children(reply);
      pixmap = xcb_generate_id(Connect);

      for (i = 0; i < len; i++) {
        procookie = xcb_get_property(Connect, 0, child[i], property, XCB_GET_PROPERTY_TYPE_ANY, 0, 1000);
        if (proreply = xcb_get_property_reply(Connect, procookie, NULL)) {
            if (xcb_get_property_value_length(proreply) > 0) {
              string name = (char*)xcb_get_property_value(proreply);
              if (name.find("mpv") != string::npos) {
                  VideoWindow = child[i];
                  break;
              }
            }
        }
        free(proreply);
      }

      if (VideoWindow) {
        xcb_create_pixmap(Connect, 1, pixmap, VideoWindow, 1, 1);
        cursor = xcb_generate_id(Connect);
        xcb_create_cursor(Connect, cursor, pixmap, pixmap, 0, 0, 0, 0, 0, 0, 1, 1);

        values[0] = cursor;
        xcb_change_window_attributes(Connect, VideoWindow, XCB_CW_CURSOR, values);
      }
    }
    free(reply);
  }
#ifdef USE_KEYPRESS
  if (VideoWindow) {
    XSelectInput (Dpy, VideoWindow, KeyPressMask);
    XMapWindow (Dpy, VideoWindow); 
  }
#endif
  XFlush(Dpy);
}

void cMpvPlayer::PlayerStart()
{
  PlayerPaused = 0;
  PlayerSpeed = 1;
  PlayerDiscNav = 0;

  SwitchOsdToMpv();

  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // we are cheating here with mpv since it checks for LC_NUMERIC=C at startup
  // this can cause unforseen issues with mpv
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  LocaleSave = setlocale(LC_NUMERIC, NULL);
#ifdef DEBUG
  dsyslog ("get locale %s\n", LocaleSave.c_str());
#endif
  setlocale(LC_NUMERIC, "C");
  hMpv = mpv_create();
  if (!hMpv)
  {
    esyslog("[mpv] failed to create context\n");
    cControl::Shutdown();
  }

  int64_t osdlevel = 0;
  string config_dir = PLGRESDIR;

  check_error(mpv_set_option_string(hMpv, "vo", MpvPluginConfig->VideoOut.c_str()));
  check_error(mpv_set_option_string(hMpv, "hwdec", MpvPluginConfig->HwDec.c_str()));
  if (MpvPluginConfig->UseGlx)
  {
    check_error(mpv_set_option_string(hMpv, "gpu-context", "x11"));
  }
  if (strcmp(MpvPluginConfig->Geometry.c_str(),""))
  {
    check_error(mpv_set_option_string(hMpv, "geometry", MpvPluginConfig->Geometry.c_str()));
  }
  if (!MpvPluginConfig->Windowed)
  {
    check_error(mpv_set_option_string(hMpv, "fullscreen", "yes"));
  }
  if (MpvPluginConfig->UseDeinterlace)
  {
    if (!strcmp(MpvPluginConfig->HwDec.c_str(),"vaapi"))
    {
      check_error(mpv_set_option_string(hMpv, "vf", "vavpp=deint=auto"));
    }
    if (!strcmp(MpvPluginConfig->HwDec.c_str(),"vdpau"))
    {
      check_error(mpv_set_option_string(hMpv, "vf", "vdpaupp=deint=yes:deint-mode=temporal-spatial"));
    }
    if (!strcmp(MpvPluginConfig->HwDec.c_str(),"cuda"))
    {
      check_error(mpv_set_option_string(hMpv, "hwdec-codecs", "all"));
      check_error(mpv_set_option_string(hMpv, "vd-lavc-o", "deint=adaptive"));
    }
  }
  check_error(mpv_set_option_string(hMpv, "audio-device", MpvPluginConfig->AudioOut.c_str()));
  check_error(mpv_set_option_string(hMpv, "slang", MpvPluginConfig->Languages.c_str()));
  check_error(mpv_set_option_string(hMpv, "alang", MpvPluginConfig->Languages.c_str()));
  check_error(mpv_set_option_string(hMpv, "cache", "no")); // video stutters if enabled
  check_error(mpv_set_option_string(hMpv, "sub-visibility", MpvPluginConfig->ShowSubtitles ? "yes" : "no"));
  check_error(mpv_set_option_string(hMpv, "sub-forced-only", "yes"));
  check_error(mpv_set_option_string(hMpv, "sub-auto", "all"));
  check_error(mpv_set_option_string(hMpv, "hr-seek", "yes"));
  check_error(mpv_set_option_string(hMpv, "write-filename-in-watch-later-config", "yes"));
  check_error(mpv_set_option_string(hMpv, "config-dir", config_dir.c_str()));
  check_error(mpv_set_option_string(hMpv, "config", "yes"));
  check_error(mpv_set_option_string(hMpv, "ontop", "yes"));
  check_error(mpv_set_option_string(hMpv, "cursor-autohide", "always"));
  check_error(mpv_set_option_string(hMpv, "stop-playback-on-init-failure", "no"));
  check_error(mpv_set_option_string(hMpv, "idle", MpvPluginConfig->ExitAtEnd ? "once" : "yes"));
  check_error(mpv_set_option_string(hMpv, "force-window", "yes"));
  check_error(mpv_set_option_string(hMpv, "image-display-duration", "inf"));
  check_error(mpv_set_option(hMpv, "osd-level", MPV_FORMAT_INT64, &osdlevel));
#ifdef DEBUG
  check_error(mpv_set_option_string(hMpv, "log-file", "/var/log/mpv"));
#endif

  if (MpvPluginConfig->UsePassthrough)
  {
    check_error(mpv_set_option_string(hMpv, "audio-spdif", "ac3,dts"));
    if (MpvPluginConfig->UseDtsHdPassthrough)
      check_error(mpv_set_option_string(hMpv, "audio-spdif", "ac3,dts,dts-hd,truehd,eac3"));
  }
  else
  {
    int64_t StartVolume = cDevice::CurrentVolume() / 2.55;
    check_error(mpv_set_option(hMpv, "volume", MPV_FORMAT_INT64, &StartVolume));
    if (MpvPluginConfig->StereoDownmix)
    {
      check_error(mpv_set_option_string(hMpv, "ad-lavc-downmix", "yes"));
      check_error(mpv_set_option_string(hMpv, "audio-channels", "2"));
    }
  }

  if (PlayShuffle && IsPlaylist(PlayFilename))
    check_error(mpv_set_option_string(hMpv, "shuffle", "yes"));

  if (MpvPluginConfig->NoScripts)
    check_error(mpv_set_option_string(hMpv, "load-scripts", "no"));

#ifdef DEBUG
  mpv_request_log_messages(hMpv, "info");
#else
  mpv_request_log_messages(hMpv, "error");
#endif

  if (mpv_initialize(hMpv) < 0)
  {
    esyslog("[mpv] failed to initialize\n");
    return;
  }

  running = 1;

  isyslog("[mpv] playing %s\n", PlayFilename.c_str());
  if (!IsPlaylist(PlayFilename))
  {
    const char *cmd[] = {"loadfile", PlayFilename.c_str(), NULL};
    mpv_command(hMpv, cmd);
  }
  else
  {
    const char *cmd[] = {"loadlist", PlayFilename.c_str(), NULL};
    mpv_command(hMpv, cmd);
  }

  // start thread to observe and react on mpv events
  pthread_create(&ObserverThreadHandle, NULL, ObserverThread, this);
#ifdef USE_KEYPRESS
  pthread_create(&XEventThreadHandle, NULL, XEventThread, this);
#endif
}

void cMpvPlayer::HandlePropertyChange(mpv_event *event)
{
  mpv_event_property *property = (mpv_event_property *) event->data;

  if (!property->data)
    return;

  // don't log on time-pos change since this floods the log
  if (event->reply_userdata != MPV_OBSERVE_TIME_POS)
  {
    dsyslog("[mpv]: property %s \n", property->name);
  }

  switch (event->reply_userdata)
  {
    case MPV_OBSERVE_TIME_POS :
      PlayerCurrent = (int)*(double*)property->data;
    break;

    case MPV_OBSERVE_DISC_MENU :
      PlayerDiscNav = (int)*(int64_t*)property->data;
    break;

    case MPV_OBSERVE_FPS :
      PlayerFps = (int)*(double*)property->data;
    break;

    case MPV_OBSERVE_FILENAME :
      PlayerFilename = *(char**)property->data;
    break;

    case MPV_OBSERVE_LENGTH :
      PlayerTotal = (int)*(double*)property->data;
    break;

    case MPV_OBSERVE_CHAPTERS :
      PlayerNumChapters = (int)*(int64_t*)property->data;
      mpv_node Node;
      mpv_get_property(hMpv, "chapter-list", MPV_FORMAT_NODE, &Node);
      ChapterTitles.clear();
      PlayerChapters.clear();
      if (Node.format == MPV_FORMAT_NODE_ARRAY)
      {
        for (int i=0; i<Node.u.list->num; i++)
        {
          ChapterTitles.push_back (Node.u.list->values[i].u.list->values[0].u.string);
          PlayerChapters.push_back (Node.u.list->values[i].u.list->values[1].u.double_);
        }
      }
    break;

    case MPV_OBSERVE_CHAPTER :
      PlayerChapter = (int)*(int64_t*)property->data;
    break;

    case MPV_OBSERVE_PAUSE :
      PlayerPaused = (int)*(int64_t*)property->data;
    break;

    case MPV_OBSERVE_SPEED :
      PlayerSpeed = (int)*(double*)property->data;
    break;

    case MPV_OBSERVE_MEDIA_TITLE :
      mediaTitle = *(char**)property->data;
    break;

    case MPV_OBSERVE_LIST_POS :
      ListCurrent = (int)*(int64_t*)property->data;
    break;

    case MPV_OBSERVE_LIST_COUNT :
      ListTotal = (int)*(int64_t*)property->data;
      mpv_node Node1;
      if (mpv_get_property(hMpv, "playlist", MPV_FORMAT_NODE, &Node1) >= 0)
      {
        ListTitles.clear();
        ListFilenames.clear();
        if (Node1.format == MPV_FORMAT_NODE_ARRAY)
        {
          for (int i=0; i<Node1.u.list->num; i++)
          {
            ListFilenames.push_back (Node1.u.list->values[i].u.list->values[0].u.string);
            for (int a=1;a<Node1.u.list->values[i].u.list->num;a++)
            {
              if(Node1.u.list->values[i].u.list->values[a].format == MPV_FORMAT_STRING)
              {
                ListTitles.push_back (Node1.u.list->values[i].u.list->values[a].u.string);
                break;
              }
            }
            // push filename if no title
            std::string title = Node1.u.list->values[i].u.list->values[0].u.string;
            if ((int)ListTitles.size() < i + 1) ListTitles.push_back (title.substr(title.find_last_of("/") + 1));
//            dsyslog("%d %s ---- %s\n",i,ListFilename(i+1).c_str(),ListTitle(i+1).c_str());
          }
        }
      }
    break;
  }
}

void cMpvPlayer::HandleTracksChange()
{
  mpv_node Node;
  mpv_get_property(hMpv, "track-list", MPV_FORMAT_NODE, &Node);
  if (Node.format != MPV_FORMAT_NODE_ARRAY)
    return;

  // loop though available tracks
  for (int i=0; i<Node.u.list->num; i++)
  {
    int TrackId = 0;
    string TrackType;
    string TrackLanguage = "undefined";
    string TrackTitle = "";
    for (int j=0; j<Node.u.list->values[i].u.list->num; j++)
    {
      if (strcmp(Node.u.list->values[i].u.list->keys[j], "id") == 0)
        TrackId = Node.u.list->values[i].u.list->values[j].u.int64;
      if (strcmp(Node.u.list->values[i].u.list->keys[j], "type") == 0)
        TrackType = Node.u.list->values[i].u.list->values[j].u.string;
      if (strcmp(Node.u.list->values[i].u.list->keys[j], "lang") == 0)
        TrackLanguage = Node.u.list->values[i].u.list->values[j].u.string;
      if (strcmp(Node.u.list->values[i].u.list->keys[j], "title") == 0)
        TrackTitle = Node.u.list->values[i].u.list->values[j].u.string;
    }
    if (TrackType == "audio")
    {
      eTrackType type = ttAudio;
      DeviceSetAvailableTrack(type, i, TrackId, TrackLanguage.c_str(), TrackTitle.c_str());
    }
    else if (TrackType == "sub")
    {
      eTrackType type = ttSubtitle;
      DeviceSetAvailableTrack(type, 0, 0, "Off");
      DeviceSetAvailableTrack(type, i, TrackId, TrackLanguage.c_str(), TrackTitle.c_str());
    }
  }
}

void cMpvPlayer::OsdClose()
{
#ifdef DEBUG
  dsyslog("[mpv] %s\n", __FUNCTION__);
#endif
  SendCommand ("overlay-remove 1");
}

void cMpvPlayer::Shutdown()
{
#ifdef USE_KEYPRESS
  if (XEventThreadHandle){
    void *res;
    int s;
    pthread_cancel(XEventThreadHandle);
    while(res != PTHREAD_CANCELED){
      s= pthread_join(XEventThreadHandle, &res);
      esyslog("cansel %d\n",s);
      if (s) break;
    }
  }
#endif

  mediaTitle = "";
  running = 0;
  MpvPluginConfig->TitleOverride = "";
  ChapterTitles.clear();
  PlayerChapters.clear();
      ListTitles.clear();
      ListFilenames.clear();

  if (ObserverThreadHandle)
    pthread_cancel(ObserverThreadHandle);

  VideoWindow = 0;

  if (Connect) {
    xcb_flush(Connect);
    Connect = NULL;
  }

  if (Dpy) {
    XFlush(Dpy);
#ifndef USE_KEYPRESS
    XCloseDisplay(Dpy);
#endif
    Dpy = NULL;
  }

  mpv_destroy(hMpv);
  hMpv = NULL;
  cOsdProvider::Shutdown();
  // set back locale
  setlocale(LC_NUMERIC, LocaleSave.c_str());
  if (MpvPluginConfig->RefreshRate)
  {
    ChangeFrameRate(OriginalFps);
    OriginalFps = -1;
  }
}

void cMpvPlayer::SwitchOsdToMpv()
{
#ifdef DEBUG
  dsyslog("[mpv] %s\n", __FUNCTION__);
#endif
  cOsdProvider::Shutdown();
  new cMpvOsdProvider(this);
}

bool cMpvPlayer::IsPlaylist(string File)
{
  for (unsigned int i=0;i<MpvPluginConfig->PlaylistExtensions.size();i++)
  {
    if (File.substr(File.find_last_of(".") + 1) == MpvPluginConfig->PlaylistExtensions[i])
      return true;
  }

  return false;
}

void cMpvPlayer::ChangeFrameRate(int TargetRate)
{
#ifdef USE_XRANDR
  if (!MpvPluginConfig->RefreshRate)
    return;

  int RefreshRate;
  XRRScreenConfiguration *CurrInfo;

  if (TargetRate == 25)
    TargetRate = 50; // fix DVD audio and since this is doubled it's ok

  if (TargetRate == 23)
    TargetRate = 24;

  if (Dpy)
  {
    short *Rates;
    int NumberOfRates;
    SizeID CurrentSizeId;
    Rotation CurrentRotation;
    int RateFound = 0;

    CurrInfo = XRRGetScreenInfo(Dpy, DefaultRootWindow(Dpy));
    RefreshRate = XRRConfigCurrentRate(CurrInfo);
    CurrentSizeId =
    XRRConfigCurrentConfiguration(CurrInfo, &CurrentRotation);
    Rates = XRRConfigRates(CurrInfo, CurrentSizeId, &NumberOfRates);

    while (NumberOfRates-- > 0)
    {
      if (TargetRate == *Rates++)
        RateFound = 1;
    }

    if ((RefreshRate != TargetRate) && (RateFound == 1))
    {
      OriginalFps = RefreshRate;
      XRRSetScreenConfigAndRate(Dpy, CurrInfo, DefaultRootWindow(Dpy),
      CurrentSizeId, CurrentRotation, TargetRate, CurrentTime);
    }

    XRRFreeScreenConfigInfo(CurrInfo);
  }
#endif
}

void cMpvPlayer::ScaleVideo(int x, int y, int width, int height)
{
  char buffer[20];
  int osdWidth, osdHeight;
  double Aspect;
  cDevice::PrimaryDevice()->GetOsdSize(osdWidth, osdHeight, Aspect);

  if(!x && !y && !width && !height)
  {
    mpv_set_property_string(hMpv, "video-pan-x", "0.0");
    mpv_set_property_string(hMpv, "video-pan-y", "0.0");
    mpv_set_property_string(hMpv, "video-scale-x", "1.0");
    mpv_set_property_string(hMpv, "video-scale-y", "1.0");
  }
  else
  {
    int err = snprintf (buffer, sizeof(buffer), "%d:%d", width, osdWidth);
    if (err > 0)
      mpv_set_property_string(hMpv, "video-scale-x", buffer);
    err = snprintf (buffer, sizeof(buffer), "%d:%d", height, osdHeight);
    if (err > 0)
      mpv_set_property_string(hMpv, "video-scale-y", buffer);
    err = snprintf (buffer, sizeof(buffer), "%d:%d", x - (osdWidth - width) / 2, width);
    if (err > 0)
      mpv_set_property_string(hMpv, "video-pan-x", buffer);
    err = snprintf (buffer, sizeof(buffer), "%d:%d", y - (osdHeight - height) / 2,height);
    if (err > 0)
      mpv_set_property_string(hMpv, "video-pan-y", buffer);
  }
}

void cMpvPlayer::SendCommand(const char *cmd, ...)
{
  if (!PlayerIsRunning())
    return;
  va_list va;
  char buf[256];
  va_start(va, cmd);
  vsnprintf(buf, sizeof(buf), cmd, va);
  va_end(va);
  mpv_command_string(hMpv, buf);
}

void cMpvPlayer::PlayNew(string Filename)
{
  const char *cmd[] = {"loadfile", Filename.c_str(), NULL};
  if (MpvPluginConfig->SavePos)
    SavePosPlayer();
  mpv_command(hMpv, cmd);
}

void cMpvPlayer::Seek(int Seconds)
{
  SendCommand("seek %d\n", Seconds);
}

void cMpvPlayer::SetTimePos(int Seconds)
{
  SendCommand("set time-pos %d\n", Seconds);
}

void cMpvPlayer::SetSpeed(int Speed)
{
  SendCommand("set speed %d\n", Speed);
}

void cMpvPlayer::SetAudio(int Audio)
{
  SendCommand("set audio %d\n", Audio);
}

void cMpvPlayer::SetSubtitle(int Subtitle)
{
  SendCommand("set sub %d\n", Subtitle);
}

void cMpvPlayer::SetChapter(int Chapter)
{
  SendCommand("set chapter %d\n", Chapter-1);
}

void cMpvPlayer::PlayIndex(int Index)
{
  SendCommand("playlist-play-index %d\n", Index-1);
}

void cMpvPlayer::TogglePause()
{
  SendCommand("cycle pause\n");
}

void cMpvPlayer::QuitPlayer()
{
  if (MpvPluginConfig->SavePos)
    SendCommand("quit-watch-later\n");
  else
    SendCommand("quit\n");
}

void cMpvPlayer::StopPlayer()
{
  SendCommand("stop\n");
}

void cMpvPlayer::SavePosPlayer()
{
  SendCommand("write-watch-later-config\n");
}

void cMpvPlayer::DiscNavUp()
{
  SendCommand("discnav up\n");
}

void cMpvPlayer::DiscNavDown()
{
  SendCommand("discnav down\n");
}

void cMpvPlayer::DiscNavLeft()
{
  SendCommand("discnav left\n");
}

void cMpvPlayer::DiscNavRight()
{
  SendCommand("discnav right\n");
}

void cMpvPlayer::DiscNavMenu()
{
  SendCommand("discnav menu\n");
}

void cMpvPlayer::DiscNavSelect()
{
  SendCommand("discnav select\n");
}

void cMpvPlayer::DiscNavPrev()
{
  SendCommand("discnav prev\n");
}

void cMpvPlayer::PreviousChapter()
{
  SendCommand("cycle chapter -1\n");
}

void cMpvPlayer::NextChapter()
{
  SendCommand("cycle chapter 1\n");
}

void cMpvPlayer::NextPlaylistItem()
{
  SendCommand("playlist-next\n");
}

void cMpvPlayer::PreviousPlaylistItem()
{
  SendCommand("playlist-prev\n");
}

void cMpvPlayer::SetVolume(int Volume)
{
  SendCommand("set volume %d\n", Volume);
}
