//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#include <sys/mman.h>

#include "osd.h"

cMpvOsdProvider::cMpvOsdProvider(cMpvPlayer *player)
{
  Player = player;
}

cOsd *cMpvOsdProvider::CreateOsd(int Left, int Top, uint Level)
{
  return new cMpvOsd(Left, Top, Level, Player);
}

bool cMpvOsdProvider::ProvidesTrueColor()
{
  return true;
}

cMpvOsd::cMpvOsd(int Left, int Top, uint Level, cMpvPlayer *player)
:cOsd(Left, Top, Level)
{
  Player = player;

  winWidth = Player->WindowWidth();
  winHeight = Player->WindowHeight();

  fdOsd = open ("/tmp/vdr_mpv_osd", O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
  if (fdOsd < 0)
  {
    esyslog("[mpv] ERROR! no access to /tmp, no OSD!\n");
    pOsd = NULL;
  }
  lseek(fdOsd, winWidth*winHeight*4, SEEK_SET);
  int ret=write(fdOsd, "", 1);
  if (ret > 0)
      pOsd = (char*) mmap (NULL, winWidth*winHeight*4, PROT_WRITE, MAP_SHARED, fdOsd, 0);
#ifdef DEBUG
  dsyslog("[mpv] Osd %d %d \n",fdOsd,ret);
#endif
  SetActive(true);
}

cMpvOsd::~cMpvOsd()
{
#ifdef DEBUG
  dsyslog("[mpv] %s\n", __FUNCTION__);
#endif
  SetActive(false);
  munmap(pOsd, sizeof(pOsd));
  close(fdOsd);
  remove("/tmp/vdr_mpv_osd");
}

void cMpvOsd::SetActive(bool On)
{
  if (On == Active())
  {
    return;
  }
  if (On) cOsd::SetActive(true);

  if (!On)
  {
    if (cMpvPlayer::PlayerIsRunning())
      Player->OsdClose();
    cOsd::SetActive(false);
  }
}

void cMpvOsd::OsdSizeUpdate()
{
    winWidth = Player->WindowWidth();
    winHeight = Player->WindowHeight();
    munmap(pOsd, sizeof(pOsd));
    lseek(fdOsd, winWidth*winHeight*4, SEEK_SET);
    int ret=write(fdOsd, "", 1);
    if (ret > 0)
      pOsd = (char*) mmap (NULL, winWidth*winHeight*4, PROT_WRITE, MAP_SHARED, fdOsd, 0);
    else return;
}

void cMpvOsd::WriteToMpv(int sw, int sh, int x, int y, int w, int h, const uint8_t * argb)
{
  if (!cMpvPlayer::PlayerIsRunning() || !pOsd)
    return;
  int sx;
  int sy;
  int pos;
  char cmd[64];
  double scalew, scaleh;

  int osdWidth = 0;
  int osdHeight = 0;
  double Aspect;
  int a;

  cDevice::PrimaryDevice()->GetOsdSize(osdWidth, osdHeight, Aspect);
  if (!winWidth) winWidth = osdWidth;
  if (!winHeight) winHeight = osdHeight;

  scalew = (double) winWidth / osdWidth;
  scaleh = (double) winHeight / osdHeight;

  for (sy = 0; sy < h; ++sy) {
    for (sx = 0; sx < w; ++sx) {
      pos=0;
      pos = pos + 4 * winWidth * (int)(scaleh *(sy + y));
      pos = pos + 4 * (int)(scalew *(sx + x));

      if ((pos + 3) > (winWidth * winHeight * 4)) break; //memory overflow prevention
      for (a = 0; a < 4; ++a)
        pOsd[pos + a] = argb[(w * sy + sx) * 4 + a];

      //upscale
      if (scalew > 1.0) {
        if ((pos + 7 + 4 + winWidth) > (winWidth * winHeight * 4)) break; //memory overflow prevention
        for (a = 0; a < 4; ++a) {
          pOsd[pos + a + 4] = argb[(w * sy + sx) * 4 + a];
          pOsd[pos + a + 4 * winWidth] = argb[(w * sy + sx) * 4 + a];
          pOsd[pos + a + 4 + 4 * winWidth] = argb[(w * sy + sx) * 4 + a];
        }
      }
    }
  }
  snprintf (cmd, sizeof(cmd), "overlay-add 1 0 0 @%d  0 \"bgra\" %d %d %d\n", fdOsd, winWidth, winHeight, winWidth * 4);
  Player->SendCommand (cmd);
}

void cMpvOsd::Flush()
{
  cPixmapMemory *pm;
  if (!cMpvPlayer::PlayerIsRunning())
    cOsd::SetActive(false);
  if (!Active())
    return;

  if (winWidth && winHeight && (winWidth != Player->WindowWidth() || winHeight != Player->WindowHeight()))
  {
    SetActive(false);
    OsdSizeUpdate();
    cOsdProvider::UpdateOsdSize(true);
    return;
  }

  int OsdAreaWidth = OsdWidth() + cOsd::Left();
  int OsdAreaHeight = OsdHeight()+ cOsd::Top();

  if (IsTrueColor())
  {
    LOCK_PIXMAPS;
    while ((pm = dynamic_cast<cPixmapMemory *>(RenderPixmaps())))
    {
      int x;
      int y;
      int w;
      int h;

      x = Left() + pm->ViewPort().X();
      y = Top() + pm->ViewPort().Y();
      w = pm->ViewPort().Width();
      h = pm->ViewPort().Height();

      WriteToMpv(OsdAreaWidth, OsdAreaHeight, x, y, w, h, pm->Data());

      DestroyPixmap(pm);
    }
  }
  else
  {
    cBitmap *bitmap;
    int i;

    // draw all bitmaps
    for (i = 0; (bitmap = GetBitmap(i)); ++i)
    {
      uint8_t *argb;
      int x;
      int y;
      int w;
      int h;
      int x1;
      int y1;
      int x2;
      int y2;

      if (!bitmap->Dirty(x1, y1, x2, y2))
      {
        continue;  // nothing dirty continue
      }
      // convert and upload only dirty areas
      w = x2 - x1 + 1;
      h = y2 - y1 + 1;
      if (1)  // just for the case it makes trouble
      {
        if (w > OsdAreaWidth)
        {
          w = OsdAreaWidth;
          x2 = x1 + OsdAreaWidth - 1;
        }
        if (h > OsdAreaHeight)
        {
          h = OsdAreaHeight;
          y2 = y1 + OsdAreaHeight - 1;
        }
      }
      argb = (uint8_t *) malloc(w * h * sizeof(uint32_t));
      for (y = y1; y <= y2; ++y)
      {
        for (x = x1; x <= x2; ++x)
        {
          ((uint32_t *) argb)[x - x1 + (y - y1) * w] =
          bitmap->GetColor(x, y);
        }
      }
      WriteToMpv(OsdAreaWidth, OsdAreaHeight, Left() + bitmap->X0() + x1, Top() + bitmap->Y0() + y1, w, h, argb);

      bitmap->Clean();
      // FIXME: reuse argb
      free(argb);
    }
  }
}

