/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

/*
 * hextile.c - handle hextile encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles a hextile
 * encoded rectangle with BPP bits per pixel.
 */

#define HandleHextileBPP CONCAT2E(HandleHextile,BPP)
#define CARDBPP CONCAT2E(CARD,BPP)
#define GET_PIXEL CONCAT2E(GET_PIXEL,BPP)

#define FillRectangle(x, y, w, h, color)  \
	{ \
		XGCValues _gcv; \
		_gcv.foreground = color; \
		if (!appData.useXserverBackingStore) { \
			FillScreen(x, y, w, h, _gcv.foreground); \
		} else { \
			XChangeGC(dpy, gc, GCForeground, &_gcv); \
			XFillRectangle(dpy, desktopWin, gc, x, y, w, h); \
		} \
	}

extern int skip_maybe_sync;
extern void maybe_sync(int w, int h);

static Bool
HandleHextileBPP (int rx, int ry, int rw, int rh)
{
  CARDBPP bg, fg;
  XGCValues gcv;
  int i;
  CARD8 *ptr;
  int x, y, w, h;
  int sx, sy, sw, sh;
  CARD8 subencoding;
  CARD8 nSubrects;
  int irect = 0, nrects = (rw * rh) / (16 * 16);
  static int nosync_ycrop = -1;

	if (nosync_ycrop < 0) {
		nosync_ycrop = 0;
		if (getenv("HEXTILE_YCROP_TOO")) {
			nosync_ycrop = 1;
		}
	}

  for (y = ry; y < ry+rh; y += 16) {
    for (x = rx; x < rx+rw; x += 16) {
      w = h = 16;
      if (rx+rw - x < 16) {
	w = rx+rw - x;
      }
      if (ry+rh - y < 16) {
	h = ry+rh - y;
      }

	if (nrects > 400 && (appData.yCrop == 0 || nosync_ycrop)) {
		skip_maybe_sync = 0;	
		if (irect++ % 2000 != 0) {
			if (x < rx+rw-16 || y < ry+rh-16) {
				skip_maybe_sync	= 1;
			}
		}
	}

      if (!ReadFromRFBServer((char *)&subencoding, 1)) {
	return False;
      }

      if (subencoding & rfbHextileRaw) {
	if (!ReadFromRFBServer(buffer, w * h * (BPP / 8))) {
	  return False;
        }

	CopyDataToScreen(buffer, x, y, w, h);
	continue;
      }

      if (subencoding & rfbHextileBackgroundSpecified)
	if (!ReadFromRFBServer((char *)&bg, sizeof(bg)))
	  return False;

#if (BPP == 8)
      if (appData.useBGR233) {
	gcv.foreground = BGR233ToPixel[bg];
      } else
#endif
#if (BPP == 16)
      if (appData.useBGR565) {
	gcv.foreground = BGR565ToPixel[bg];
      } else
#endif
      {
	gcv.foreground = bg;
      }

#if 0
	XChangeGC(dpy, gc, GCForeground, &gcv);
	XFillRectangle(dpy, desktopWin, gc, x, y, w, h);
#else
	FillRectangle(x, y, w, h, gcv.foreground);
#endif

      if (subencoding & rfbHextileForegroundSpecified)
	if (!ReadFromRFBServer((char *)&fg, sizeof(fg)))
	  return False;

      if (!(subencoding & rfbHextileAnySubrects)) {
	continue;
      }

      if (!ReadFromRFBServer((char *)&nSubrects, 1))
	return False;

      ptr = (CARD8 *)buffer;

      if (subencoding & rfbHextileSubrectsColoured) {
	if (!ReadFromRFBServer(buffer, nSubrects * (2 + (BPP / 8))))
	  return False;

	for (i = 0; i < nSubrects; i++) {
	  GET_PIXEL(fg, ptr);
	  sx = rfbHextileExtractX(*ptr);
	  sy = rfbHextileExtractY(*ptr);
	  ptr++;
	  sw = rfbHextileExtractW(*ptr);
	  sh = rfbHextileExtractH(*ptr);
	  ptr++;
#if (BPP == 8)
	  if (appData.useBGR233) {
	    gcv.foreground = BGR233ToPixel[fg];
	  } else
#endif
#if (BPP == 16)
	  if (appData.useBGR565) {
	    gcv.foreground = BGR565ToPixel[fg];
	  } else
#endif
	  {
	    gcv.foreground = fg;
	  }

#if 0
          XChangeGC(dpy, gc, GCForeground, &gcv);
          XFillRectangle(dpy, desktopWin, gc, x+sx, y+sy, sw, sh);
#else
	  FillRectangle(x+sx, y+sy, sw, sh, gcv.foreground);
#endif
	}

      } else {
	if (!ReadFromRFBServer(buffer, nSubrects * 2))
	  return False;

#if (BPP == 8)
	if (appData.useBGR233) {
	  gcv.foreground = BGR233ToPixel[fg];
	} else
#endif
#if (BPP == 16)
        if (appData.useBGR565) {
          gcv.foreground = BGR565ToPixel[fg];
	} else
#endif
	{
	  gcv.foreground = fg;
	}

#if 0
	XChangeGC(dpy, gc, GCForeground, &gcv);
#endif

	for (i = 0; i < nSubrects; i++) {
	  sx = rfbHextileExtractX(*ptr);
	  sy = rfbHextileExtractY(*ptr);
	  ptr++;
	  sw = rfbHextileExtractW(*ptr);
	  sh = rfbHextileExtractH(*ptr);
	  ptr++;
#if 0
	  XFillRectangle(dpy, desktopWin, gc, x+sx, y+sy, sw, sh);
#else
	  FillRectangle(x+sx, y+sy, sw, sh, gcv.foreground);
#endif
	}
      }
    }
  }

  return True;
}

#undef FillRectangle
