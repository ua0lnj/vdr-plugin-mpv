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
#include <vdr/plugin.h>

#include "player.h"
#include "config.h"
#include "osd.h"

#ifdef USE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

#ifdef USE_DRM
#include <xf86drmMode.h>
#endif

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
#define MPV_OBSERVE_VIA_NET 13
#define MPV_OBSERVE_TRACK_LIST 14

volatile int cMpvPlayer::running = 0;
cMpvPlayer *cMpvPlayer::PlayerHandle = NULL;
std::string LocaleSave;
int drm_ctx = 0;
const char *drm_dev = NULL;
Display *Dpy = NULL;
xcb_connection_t *Connect = NULL;
xcb_window_t VideoWindow = 0;
xcb_pixmap_t pixmap = XCB_NONE;
xcb_cursor_t cursor = XCB_NONE;
int is_softhddevice = 0;
extern volatile char ShownMenu;

#ifdef __cplusplus
extern "C"
{
#endif
extern void FeedKeyPress(const char *, const char *, int, int, const char *);
extern void RemoteStart();
extern void RemoteStop();
#ifdef __cplusplus
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

void set_deinterlace(mpv_handle *h)
{
    if (strstr(MpvPluginConfig->HwDec.c_str(),"vaapi"))
    {
      check_error(mpv_set_option_string(h, "vf", "vavpp=deint=auto"));
    }
    else if (strstr(MpvPluginConfig->HwDec.c_str(),"vdpau"))
    {
      check_error(mpv_set_option_string(h, "vf", "vdpaupp=deint=yes:deint-mode=temporal-spatial"));
    }
    else if (strstr(MpvPluginConfig->HwDec.c_str(),"cuda"))
    {
      check_error(mpv_set_option_string(h, "vd-lavc-o", "deint=adaptive"));
    }
    else
    {
      check_error(mpv_set_option_string(h, "deinterlace", "yes"));
    }
}

void *cMpvPlayer::XEventThread(void *handle)
{
  XEvent event;
  KeySym keysym;
  const char *keynam;
  char buf[64];
  char letter[64];
  int letter_len;
  static Time clicktime;
  static bool toggle;
  cMpvPlayer *Player = (cMpvPlayer*) handle;

  while (Player->PlayerIsRunning())
  {
    if(Dpy && Connect && VideoWindow) {
        XWindowEvent(Dpy, VideoWindow, KeyPressMask|ButtonPressMask|StructureNotifyMask|SubstructureNotifyMask, &event);
        switch (event.type) {
	  case ButtonPress:
	    if (is_softhddevice) {
		if (event.xbutton.button == 1) {
		    Time difftime = event.xbutton.time - clicktime;
		    if (difftime < 500) {
			check_error(mpv_set_option_string(Player->hMpv, "fullscreen", toggle ? "yes" : "no"));
			toggle = !toggle;
		    }
		    clicktime = event.xbutton.time;
		}
		else if (event.xbutton.button == 2) {
		    FeedKeyPress("XKeySym", "Ok", 0, 0, NULL);
		}
		else if (event.xbutton.button == 3) {
		    if (ShownMenu)
			FeedKeyPress("XKeySym", "Back", 0, 0, NULL);
		    else
			FeedKeyPress("XKeySym", "Menu", 0, 0, NULL);
		}
		if (event.xbutton.button == 4) {
		    if (ShownMenu)
			FeedKeyPress("XKeySym", "Up", 0, 0, NULL);
		    else
			FeedKeyPress("XKeySym", "Volume+", 0, 0, NULL);
		}
		if (event.xbutton.button == 5) {
		    if (ShownMenu)
			FeedKeyPress("XKeySym", "Down", 0, 0, NULL);
		    else
			FeedKeyPress("XKeySym", "Volume-", 0, 0, NULL);
		}
	    } else {
		check_error(mpv_set_option_string(Player->hMpv, "fullscreen", toggle ? "yes" : "no"));
		toggle = !toggle;
	    }
	    break;
	  case ButtonRelease:
	    break;
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
          case ConfigureNotify:
	    Player->windowWidth = event.xconfigure.width;
	    Player->windowHeight = event.xconfigure.height;
	    Player->windowX = event.xconfigure.x;
	    Player->windowY = event.xconfigure.y;
	    Player->OsdClose();
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
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_VIA_NET, "demuxer-via-network", MPV_FORMAT_FLAG);
  mpv_observe_property(Player->hMpv, MPV_OBSERVE_TRACK_LIST, "track-list", MPV_FORMAT_NODE);

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
        Player->PlayerIdle = 0;
        if (MpvPluginConfig->UseDeinterlace && !Player->Image()) set_deinterlace(Player->hMpv);
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
#if MPV_CLIENT_API_VERSION < MPV_MAKE_VERSION(2,0)
      case MPV_EVENT_TRACKS_CHANGED :
        Player->HandleTracksChange();
      break;
#endif
      case MPV_EVENT_VIDEO_RECONFIG :
          if(!drm_ctx) {
            Player->PlayerGetWindow("- mpv", &Connect, VideoWindow, Player->windowWidth, Player->windowHeight, Player->windowX, Player->windowY);
            Player->PlayerHideCursor();
          }
#ifdef USE_DRM
          else
            Player->PlayerGetDRM();
#endif
      break;

      case MPV_EVENT_IDLE :
        if (Player->PlayerIdle != -1)
            Player->PlayerIdle = 1;
      break;

      case MPV_EVENT_NONE :
      case MPV_EVENT_END_FILE :
#if MPV_CLIENT_API_VERSION < MPV_MAKE_VERSION(2,0)
      case MPV_EVENT_PAUSE :
      case MPV_EVENT_UNPAUSE :
      case MPV_EVENT_TRACK_SWITCHED :
      case MPV_EVENT_SCRIPT_INPUT_DISPATCH :
      case MPV_EVENT_METADATA_UPDATE :
      case MPV_EVENT_CHAPTER_CHANGE :
#endif
      case MPV_EVENT_FILE_LOADED :
      case MPV_EVENT_GET_PROPERTY_REPLY :
      case MPV_EVENT_SET_PROPERTY_REPLY :
      case MPV_EVENT_COMMAND_REPLY :
      case MPV_EVENT_START_FILE :
      case MPV_EVENT_TICK :
      case MPV_EVENT_CLIENT_MESSAGE :
      case MPV_EVENT_AUDIO_RECONFIG :
      case MPV_EVENT_SEEK :
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
  PlayerRecord = 0;
  ObserverThreadHandle = 0;
  XEventThreadHandle = 0;
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

void cMpvPlayer::PlayerGetWindow(string need, xcb_connection_t **connect, xcb_window_t &window, int &width, int &height, int &x, int &y)
{
  int screen_nr;
  int i;
  int len;
  xcb_screen_iterator_t screen_iter;
  xcb_screen_t *screen;
  xcb_query_tree_cookie_t  cookie;
  xcb_query_tree_reply_t *reply;
  xcb_window_t *child;
  xcb_window_t parent = 0;

  if (!Dpy)
    Dpy = XOpenDisplay(MpvPluginConfig->X11Display.c_str());
  if (!Dpy) return;

  if (!*connect)
    *connect = XGetXCBConnection(Dpy);
  if (!*connect) return;

  if(!window)
  {
    screen_nr = DefaultScreen(Dpy);
    //get root screen
    screen_iter = xcb_setup_roots_iterator(xcb_get_setup(*connect));
    for (i = 0; i < screen_nr; ++i)
    {
      xcb_screen_next(&screen_iter);
    }
    screen = screen_iter.data;
    //query child of root
    cookie = xcb_query_tree(*connect,screen->root);
    reply = xcb_query_tree_reply(*connect, cookie, 0);
    len = xcb_query_tree_children_length(reply);

    if (len)
    {
      xcb_get_property_cookie_t procookie;
      xcb_get_property_reply_t *proreply;

      xcb_atom_t property = XCB_ATOM_WM_NAME;
      //get children of root
      child = xcb_query_tree_children(reply);

      xcb_query_tree_cookie_t  c_cookie;
      xcb_query_tree_reply_t *c_reply;
      xcb_window_t *c_child = NULL;
      int c_len;

      for (i = 0; i < len; i++) {
        //query child of child
        c_cookie = xcb_query_tree(*connect,child[i]);
        c_reply = xcb_query_tree_reply(*connect, c_cookie, 0);
        c_len = xcb_query_tree_children_length(c_reply);

        if (c_len) {
          //get children of child
          c_child = xcb_query_tree_children(c_reply);
        }

        for (int o = 0; o < (c_len ? c_len : 1); o++){
          //get child property
          procookie = xcb_get_property(*connect, 0, c_len ? c_child[o] : child[i], property, XCB_GET_PROPERTY_TYPE_ANY, 0, 1000);
          if (proreply = xcb_get_property_reply(*connect, procookie, NULL)) {
            if (xcb_get_property_value_length(proreply) > 0) {
              string name = (char*)xcb_get_property_value(proreply);
              if (name.find(need) != string::npos) {
                  if (!c_len) window = child[i];
                  else {
                    window = c_child[o];
                    parent = child[i];
                  }
                  break;
              }
            }
          }
          free(proreply);
        }
        free(c_reply);
      }
    }
    free(reply);
  }

  if (window) {
    //get geometry
    xcb_get_geometry_cookie_t geocookie;
    xcb_get_geometry_reply_t *georeply;

    geocookie = xcb_get_geometry(*connect, window);
    georeply = xcb_get_geometry_reply(*connect, geocookie, NULL);
    if (georeply) {
      width = georeply->width;
      height = georeply->height;
      x = georeply->x;
      y = georeply->y;
      free(georeply);
    }
  }

  if (parent) {
    //get geometry
    xcb_get_geometry_cookie_t geocookie;
    xcb_get_geometry_reply_t *georeply;

    geocookie = xcb_get_geometry(*connect, parent);
    georeply = xcb_get_geometry_reply(*connect, geocookie, NULL);
    if (georeply) {
      x += georeply->x;
      y += georeply->y;
      free(georeply);
    }
  }
}

void cMpvPlayer::PlayerHideCursor()
{
  uint32_t values[4];

  if (VideoWindow && Connect) {
    pixmap = xcb_generate_id(Connect);
    xcb_create_pixmap(Connect, 1, pixmap, VideoWindow, 1, 1);
    cursor = xcb_generate_id(Connect);
    xcb_create_cursor(Connect, cursor, pixmap, pixmap, 0, 0, 0, 0, 0, 0, 1, 1);
    values[0] = cursor;
    xcb_change_window_attributes(Connect, VideoWindow, XCB_CW_CURSOR, values);
  }
  if (VideoWindow) {
    XSelectInput (Dpy, VideoWindow, KeyPressMask|ButtonPressMask|StructureNotifyMask|SubstructureNotifyMask);
    XMapWindow (Dpy, VideoWindow);
  }
  XFlush(Dpy);
}

#ifdef USE_DRM
int cMpvPlayer::PlayerTryDRM()
{
  int fd;
  if (strcmp(MpvPluginConfig->DRMdev.c_str(),"")) {
    drm_dev = MpvPluginConfig->DRMdev.c_str();
    return 1;
  }
  //card1 mean external card, card0 internal. First try external card
  fd = open("/dev/dri/card1", O_RDWR);
  if (fd < 0) {
    fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) return 0;
    else drm_dev = "/dev/dri/card0";
  } else drm_dev = "/dev/dri/card1";

  close(fd);
  return 1;
}

void cMpvPlayer::PlayerGetDRM()
{
  int fd, i;
  drmModeRes *resources;
  drmModeConnector *connector;
  drmModeModeInfo mode;
  drmModeEncoder *encoder;
  drmModeCrtc *crtc;

  fd = open(drm_dev, O_RDWR);
  if (fd < 0) return;

  resources = drmModeGetResources(fd);
  if (resources != NULL) {
    for(i = 0; i < resources->count_connectors; ++i) {
      connector = drmModeGetConnector(fd, resources->connectors[i]);
      if(connector != NULL) {
        if(connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0)
          break;
        drmModeFreeConnector(connector);
      }
    }
    if(i < resources->count_connectors) {

      encoder = drmModeGetEncoder(fd, connector->encoder_id);
      crtc = drmModeGetCrtc(fd, encoder->crtc_id);

      mode = crtc->mode;
      windowWidth = mode.hdisplay;
      windowHeight = mode.vdisplay;

      drmModeFreeCrtc(crtc);
      drmModeFreeEncoder(encoder);
      drmModeFreeConnector(connector);

    } else
      esyslog("No active connector found\n");

    esyslog("windowWidth %d windowHeight %d\n",windowWidth,windowHeight);
    drmModeFreeResources(resources);
    close(fd);
  }
}
#endif

void cMpvPlayer::PlayerStart()
{
  PlayerPaused = 0;
  PlayerIdle = 0;
  PlayerSpeed = 1;
  PlayerDiscNav = 0;
  isImage = 0;

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
  check_error(mpv_set_option_string(hMpv, "gpu-context", MpvPluginConfig->GpuCtx.c_str()));
  check_error(mpv_set_option_string(hMpv, "hwdec-codecs", "all"));
#ifdef USE_DRM
  if (!strcmp(MpvPluginConfig->GpuCtx.c_str(),"drm") || !strcmp(MpvPluginConfig->VideoOut.c_str(),"drm"))
  {
    drm_ctx = 1;
    if (!PlayerTryDRM()) return;
    check_error(mpv_set_option_string(hMpv, "drm-device", drm_dev));
  }
#endif
  //window geometry with x11, drm-mode with drm
  if (!drm_ctx) //x11
  {
    if (strcmp(MpvPluginConfig->Geometry.c_str(),""))
    {
      check_error(mpv_set_option_string(hMpv, "geometry", MpvPluginConfig->Geometry.c_str()));
    } else if (windowWidth && windowHeight) {
      char geo[25];
      sprintf(geo, "%dx%d+%d+%d", windowWidth, windowHeight, windowX, windowY);
      check_error(mpv_set_option_string(hMpv, "geometry", geo));
    }
    if (!MpvPluginConfig->Windowed)
    {
      check_error(mpv_set_option_string(hMpv, "fullscreen", "yes"));
    }
  }
#ifdef USE_DRM
  else //drm
  {
    if (strcmp(MpvPluginConfig->Geometry.c_str(),""))
    {
      check_error(mpv_set_option_string(hMpv, "drm-mode", MpvPluginConfig->Geometry.c_str()));
    }
  }
#endif
  if (MpvPluginConfig->UseDeinterlace)
  {
    set_deinterlace(hMpv);
  }
  check_error(mpv_set_option_string(hMpv, "audio-device", MpvPluginConfig->AudioOut.c_str()));
  check_error(mpv_set_option_string(hMpv, "slang", MpvPluginConfig->Languages.c_str()));
  check_error(mpv_set_option_string(hMpv, "alang", MpvPluginConfig->Languages.c_str()));
  check_error(mpv_set_option_string(hMpv, "cache", "no")); // video stutters if enabled
  check_error(mpv_set_option_string(hMpv, "sub-visibility", MpvPluginConfig->ShowSubtitles ? "yes" : "no"));
#if MPV_CLIENT_API_VERSION < MPV_MAKE_VERSION(2,2)
  check_error(mpv_set_option_string(hMpv, "sub-forced-only", "yes"));
#else
  check_error(mpv_set_option_string(hMpv, "sub-forced-events-only", "yes"));
#endif
  check_error(mpv_set_option_string(hMpv, "sub-auto", "all"));
  check_error(mpv_set_option_string(hMpv, "hr-seek", "yes"));
  check_error(mpv_set_option_string(hMpv, "write-filename-in-watch-later-config", "yes"));
  check_error(mpv_set_option_string(hMpv, "config-dir", config_dir.c_str()));
  check_error(mpv_set_option_string(hMpv, "config", "yes"));
  check_error(mpv_set_option_string(hMpv, "ontop", "yes"));
  check_error(mpv_set_option_string(hMpv, "cursor-autohide", "always"));
  check_error(mpv_set_option_string(hMpv, "input-cursor", "no"));
  check_error(mpv_set_option_string(hMpv, "stop-playback-on-init-failure", "no"));
  check_error(mpv_set_option_string(hMpv, "idle", MpvPluginConfig->ExitAtEnd ? "once" : "yes"));
  check_error(mpv_set_option_string(hMpv, "force-window", "immediate"));
  check_error(mpv_set_option_string(hMpv, "image-display-duration", "inf"));
  check_error(mpv_set_option(hMpv, "osd-level", MPV_FORMAT_INT64, &osdlevel));
#ifdef DEBUG
  check_error(mpv_set_option_string(hMpv, "log-file", "/var/log/mpv"));
#endif
  if (MpvPluginConfig->UsePassthrough)
  {
    check_error(mpv_set_option_string(hMpv, "audio-spdif", "ac3,dts"));
    if (MpvPluginConfig->UseDtsHdPassthrough)
    {
      check_error(mpv_set_option_string(hMpv, "ad-lavc-downmix", "no"));
      check_error(mpv_set_option_string(hMpv, "audio-channels", "7.1,5.1,stereo"));
      check_error(mpv_set_option_string(hMpv, "audio-spdif", "ac3,dts,dts-hd,truehd,eac3"));
    }
  }
  else
  {
    int64_t StartVolume = cDevice::CurrentVolume() / 2.55;
    if (MpvPluginConfig->SoftVol)
      check_error(mpv_set_option(hMpv, "volume", MPV_FORMAT_INT64, &StartVolume));
    else
      mpv_set_property_string(hMpv, "ao-volume", std::to_string(StartVolume).c_str());
    if (MpvPluginConfig->StereoDownmix)
    {
      check_error(mpv_set_option_string(hMpv, "ad-lavc-downmix", "yes"));
      check_error(mpv_set_option_string(hMpv, "audio-channels", "stereo"));
    }
    else
      check_error(mpv_set_option_string(hMpv, "audio-channels", "7.1,5.1,stereo"));
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

  if (!drm_ctx)
  {
    pthread_create(&XEventThreadHandle, NULL, XEventThread, this);
    RemoteStart();
  }
  if (cPluginManager::GetPlugin("softhddevice"))
  {
    is_softhddevice = 1;
  }
}

void cMpvPlayer::HandlePropertyChange(mpv_event *event)
{
  mpv_event_property *property = (mpv_event_property *) event->data;

  if (!property->data)
    return;

  // don't log on time-pos change since this floods the log
  if (event->reply_userdata != MPV_OBSERVE_TIME_POS
    && event->reply_userdata != MPV_OBSERVE_LENGTH)
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
        mpv_free_node_contents(&Node);
      }
    break;

    case MPV_OBSERVE_CHAPTER :
      PlayerChapter = (int)*(int64_t*)property->data;
    break;
#if MPV_CLIENT_API_VERSION >= MPV_MAKE_VERSION(2,0)
    case MPV_OBSERVE_TRACK_LIST :
        HandleTracksChange();
    break;
#endif
    case MPV_OBSERVE_PAUSE :
      PlayerPaused = (int)*(int64_t*)property->data;
    break;

    case MPV_OBSERVE_SPEED :
      PlayerSpeed = (int)*(double*)property->data;
    break;

    case MPV_OBSERVE_VIA_NET :
      isNetwork = (int)*(int64_t*)property->data;
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
          mpv_free_node_contents(&Node1);
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
      if (strcmp(Node.u.list->values[i].u.list->keys[j], "image") == 0)
      {
        isImage = Node.u.list->values[i].u.list->values[j].u.flag;
        if (isImage) check_error(mpv_set_option_string(hMpv, "deinterlace", "no"));
      }
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
  mpv_free_node_contents(&Node);
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
  RemoteStop();

  if(!drm_ctx) {
    if (XEventThreadHandle) {
      void *res;
      int s;
      pthread_cancel(XEventThreadHandle);
      while(res != PTHREAD_CANCELED) {
        s= pthread_join(XEventThreadHandle, &res);
        esyslog("cansel %d\n",s);
        if (s) break;
      }
    }
  }

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

  if (!drm_ctx) {
    if (cursor != XCB_NONE) {
      xcb_free_cursor(Connect, cursor);
      cursor = XCB_NONE;
    }
    if (pixmap != XCB_NONE) {
      xcb_free_pixmap(Connect, pixmap);
      pixmap = XCB_NONE;
    }
    if (Connect) {
      xcb_flush(Connect);
      Connect = NULL;
    }
    if (Dpy) {
      XFlush(Dpy);
      Dpy = NULL;
    }
  }

#if MPV_CLIENT_API_VERSION >= MPV_MAKE_VERSION(1,29)
  mpv_destroy(hMpv);
#else
  mpv_detach_destroy(hMpv);
#endif
  hMpv = NULL;
  cOsdProvider::Shutdown();
  // set back locale
  setlocale(LC_NUMERIC, LocaleSave.c_str());
  if (MpvPluginConfig->RefreshRate)
  {
    ChangeFrameRate(OriginalFps);
    OriginalFps = -1;
  }
  Setup.CurrentVolume  = cDevice::CurrentVolume();
  Setup.Save();
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
  if (!MpvPluginConfig->RefreshRate)
    return;

  int RefreshRate = 0;
#ifdef USE_XRANDR
  if (!drm_ctx)
  {
    Display *Dpl;

    XRRScreenConfiguration *CurrInfo;

    if (TargetRate == 25)
      TargetRate = 50; // fix DVD audio and since this is doubled it's ok

    if (TargetRate == 23)
      TargetRate = 24;

    Dpl = XOpenDisplay(MpvPluginConfig->X11Display.c_str());

    if (Dpl)
    {
      short *Rates;
      int NumberOfRates;
      SizeID CurrentSizeId;
      Rotation CurrentRotation;
      int RateFound = 0;

      CurrInfo = XRRGetScreenInfo(Dpl, DefaultRootWindow(Dpl));
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
        XRRSetScreenConfigAndRate(Dpl, CurrInfo, DefaultRootWindow(Dpl),
        CurrentSizeId, CurrentRotation, TargetRate, CurrentTime);
      }

      XRRFreeScreenConfigInfo(CurrInfo);
      XCloseDisplay(Dpl);
    }
  }
#endif
}

void cMpvPlayer::ScaleVideo(int x, int y, int width, int height)
{
  char buffer[20];
  int osdWidth, osdHeight;
  double Aspect;
  cDevice::PrimaryDevice()->GetOsdSize(osdWidth, osdHeight, Aspect);
  mpv_get_property(hMpv, "video-params/aspect", MPV_FORMAT_DOUBLE, &Aspect);
  if (Aspect > 1.77) Aspect = 1.77;

  if(!x && !y && !width && !height)
  {
    mpv_set_property_string(hMpv, "video-pan-x", "0.0");
    mpv_set_property_string(hMpv, "video-pan-y", "0.0");
#if MPV_CLIENT_API_VERSION >= MPV_MAKE_VERSION(2,0)
    mpv_set_property_string(hMpv, "video-scale-x", "1.0");
    mpv_set_property_string(hMpv, "video-scale-y", "1.0");
#else
    mpv_set_property_string(hMpv, "video-zoom", "0");
#endif
  }
  else
  {
    int err;
#if MPV_CLIENT_API_VERSION >= MPV_MAKE_VERSION(2,0)
    err = snprintf (buffer, sizeof(buffer), "%d:%d", width, osdWidth);
    if (err > 0)
      mpv_set_property_string(hMpv, "video-scale-x", buffer);
    err = snprintf (buffer, sizeof(buffer), "%d:%d", height, osdHeight);
    if (err > 0)
      mpv_set_property_string(hMpv, "video-scale-y", buffer);
#else
    if (osdWidth > width)
      err = snprintf (buffer, sizeof(buffer), "-%f", ((float)osdWidth/width)/2.0);
    else
      err = snprintf (buffer, sizeof(buffer), "%f", ((float)width/osdWidth)/2.0);
    if (err > 0)
      mpv_set_property_string(hMpv, "video-zoom", buffer);
#endif
    err = snprintf (buffer, sizeof(buffer), "%d:%d", x - (int)((osdWidth - width) * Aspect / 3.54), width);
    if (err > 0)
      mpv_set_property_string(hMpv, "video-pan-x", buffer);
    err = snprintf (buffer, sizeof(buffer), "%d:%d", y - (osdHeight - height) / 2,height);
    if (err > 0)
      mpv_set_property_string(hMpv, "video-pan-y", buffer);
  }
}

uint8_t *cMpvPlayer::GrabImage(int *size, int *width, int *height)
{
  uint8_t *data = NULL;
  mpv_byte_array *ba = NULL;
  mpv_node Node;
  const char *cmd[] = {"screenshot-raw", "window", NULL};

  dsyslog("[mpv] %s %d %d %d\n", __FUNCTION__, *size, *width, *height);

  mpv_command_ret(hMpv, cmd, &Node);
  if(Node.format == MPV_FORMAT_NODE_MAP)
  {

    for (int i=0; i<Node.u.list->num; i++)
    {
      if (strcmp(Node.u.list->keys[i], "w") == 0) *width = Node.u.list->values[i].u.int64;
      if (strcmp(Node.u.list->keys[i], "h") == 0) *height = Node.u.list->values[i].u.int64;
      if (strcmp(Node.u.list->keys[i], "data") == 0) ba = Node.u.list->values[i].u.ba;
    }

    data = (uint8_t*)malloc(ba->size);
    memcpy(data, ba->data, ba->size);
    *size = ba->size;

    mpv_free_node_contents(&Node);
    return data;
  }

  return NULL;
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
  if (MpvPluginConfig->SavePos && !isNetwork)
    SavePosPlayer();
  mpv_command(hMpv, cmd);
}

void cMpvPlayer::Record(string Filename)
{
  mpv_set_option_string(hMpv, "stream-record", Filename.c_str());
  if (Filename == "")
    PlayerRecord = 0;
  else
    PlayerRecord = 1;
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
  if (MpvPluginConfig->SoftVol)
    SendCommand("set volume %d\n", Volume);
  else if (PlayerIsRunning())
    mpv_set_property_string(hMpv, "ao-volume", std::to_string(Volume).c_str());
}
