//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licenced under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#ifndef __MPV_SERVICE_H
#define __MPV_SERVICE_H

#define MPV_START_PLAY_SERVICE "Mpv-StartPlayService_v1_0"
#define MPV_SET_TITLE_SERVICE "Mpv-SetTitleService_v1_0"

// Deprecated will be removed in a future release, please use Mpv_Playfile instead
typedef struct
{
  char* Filename;
  char* Title;
} Mpv_StartPlayService_v1_0_t;

// Deprecated will be removed in a future release, please use Mpv_SetTitle instead
typedef struct
{
  char* Title;
} Mpv_SetTitleService_v1_0_t;

// play the given Filename, this can be a media file or a playlist
typedef struct
{
  char *Filename;
} Mpv_PlayFile;

// start the given playlist in shuffle mode
typedef struct
{
  char *Filename;
} Mpv_PlaylistShuffle;

// Overrides the displayed title in replay info
typedef struct
{
  char *Title;
} Mpv_SetTitle;

typedef struct
{
  int SeekAbsolute;
  int SeekRelative;
} Mpv_Seek;

typedef struct
{
  int x;
  int y;
  int width;
  int height;
} Mpv_ScaleVideo;

typedef struct
{
  uint8_t *image;
  int size;
  int width;
  int height;
} Mpv_GrabImage;

#endif
