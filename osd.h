//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#ifndef __MPV_OSD_H
#define __MPV_OSD_H

#include <vdr/osd.h>
#include "player.h"

class cMpvOsdProvider:public cOsdProvider
{
  private:
    cMpvPlayer *Player;

  public:
    cMpvOsdProvider(cMpvPlayer *player);
    virtual cOsd *CreateOsd(int Left, int Top, uint Level);
    virtual bool ProvidesTrueColor();
};

class cMpvOsd:public cOsd
{
  private:
    void WriteToMpv(int sw, int sh, int x, int y, int w, int h, const uint8_t * argb);
    int winWidth;
    int winHeight;
    cMpvPlayer *Player;
    int fdOsd;
    char *pOsd;
    void OsdSizeUpdate();

  protected:
    virtual void SetActive(bool On);

  public:
    cMpvOsd(int Left, int Top, uint Level, cMpvPlayer *player);
    virtual ~cMpvOsd();
    virtual void Flush(void);
};

#endif
