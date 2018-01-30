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
 * desktop.c - functions to deal with "desktop" window.
 */

#include <vncviewer.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xmu/Converters.h>
#ifdef MITSHM
#include <X11/extensions/XShm.h>
#endif

#include <X11/cursorfont.h>

GC gc;
GC srcGC, dstGC; /* used for debugging copyrect */
Window desktopWin;
Cursor dotCursor3 = None;
Cursor dotCursor4 = None;
Cursor bogoCursor = None;
Cursor waitCursor = None;
Widget form, viewport, desktop;

int appshare_0_hint = -10000;
int appshare_x_hint = -10000;
int appshare_y_hint = -10000;

static Bool modifierPressed[256];

XImage *image = NULL;
XImage *image_ycrop = NULL;
XImage *image_scale = NULL;

int image_is_shm = 0;

static Cursor CreateDotCursor();
static void CopyBGR233ToScreen(CARD8 *buf, int x, int y, int width,int height);
static void HandleBasicDesktopEvent(Widget w, XtPointer ptr, XEvent *ev,
				    Boolean *cont);
static void HandleResizeEvent(Widget w, XtPointer ptr, XEvent *ev, Boolean *cont);

static void CopyBGR565ToScreen(CARD16 *buf, int x, int y, int width,int height);

static XtResource desktopBackingStoreResources[] = {
  {
	XtNbackingStore, XtCBackingStore, XtRBackingStore, sizeof(int), 0,
	XtRImmediate, (XtPointer) Always,
  },
};

double scale_factor_x = 0.0;
double scale_factor_y = 0.0;
int scale_x = 0, scale_y = 0;
int scale_round(int len, double fac);

double last_rescale = 0.0;
double last_fullscreen = 0.0;
double start_time = 0.0;

int prev_fb_width = -1;
int prev_fb_height = -1;

void get_scale_values(double *fx, double *fy) {
	char *s = appData.scale;
	double f, frac_x = -1.0, frac_y = -1.0;
	int n, m;
	int xmax = si.framebufferWidth;
	int ymax = si.framebufferHeight;

	if (appData.yCrop > 0) {
		ymax = appData.yCrop;
	}

	if (sscanf(s, "%d/%d", &n, &m) == 2) {
		if (m == 0) {
			frac_x = 1.0;
		} else {
			frac_x = ((double) n) / ((double) m);
		}
	}
	if (sscanf(s, "%dx%d", &n, &m) == 2) {
		frac_x = ((double) n) / ((double) xmax);
		frac_y = ((double) m) / ((double) ymax);
	}
	if (!strcasecmp(s, "fit")) {
		frac_x = ((double) dpyWidth)  / ((double) xmax);
		frac_y = ((double) dpyHeight) / ((double) ymax);
	}
	if (!strcasecmp(s, "auto")) {
		Dimension w, h;
		XtVaGetValues(toplevel, XtNheight, &h, XtNwidth, &w, NULL);
		fprintf(stderr, "auto: %dx%d\n", w, h);
		if (w > 32 && h > 32) {
			frac_x = ((double) w) / ((double) xmax);
			frac_y = ((double) h) / ((double) ymax);
		} else {
		    frac_x = frac_y = 1.0;
		}
	}
	if (frac_x < 0.0 && sscanf(s, "%lf", &f) == 1) {
		if (f > 0.0) {
			frac_x = f;
		}
	}

	if (frac_y < 0.0) {
		frac_y = frac_x;
	}

	if (frac_y > 0.0 && frac_x > 0.0) {
		if (fx != NULL) {
			*fx = frac_x;
		}
		if (fy != NULL) {
			*fy = frac_y;
		}
	} else {
		if (appData.scale) {
			fprintf(stderr, "Invalid scale string: '%s'\n", appData.scale); 
		} else {
			fprintf(stderr, "Invalid scale string.\n"); 
		}
		appData.scale = NULL;
	}
}

void try_create_image(void);
void put_image(int src_x, int src_y, int dst_x, int dst_y, int width, int height, int solid);
void create_image();

/* toplevel -> form -> viewport -> desktop */

void adjust_Xt_win(int w, int h) {
	int x, y, dw, dh, h0 = h;
	int mw = w, mh = h;
	int autoscale = 0;

	if (!appData.fullScreen && appData.scale != NULL && !strcmp(appData.scale, "auto")) {
		autoscale = 1;
		mw = dpyWidth;
		mh = dpyHeight;
	}

	if (appData.yCrop > 0) {
		int ycrop = appData.yCrop; 
		if (image_scale && scale_factor_y > 0.0) {
			ycrop = scale_round(ycrop, scale_factor_y);
			if (!autoscale) {
				mh = ycrop;
			}
		}
		XtVaSetValues(toplevel, XtNmaxWidth, mw, XtNmaxHeight, mh, XtNwidth, w, XtNheight, ycrop, NULL);
		XtVaSetValues(form,     XtNmaxWidth, mw, XtNmaxHeight, mh, XtNwidth, w, XtNheight, ycrop, NULL);
		h0 = ycrop;
	} else {
		XtVaSetValues(toplevel, XtNmaxWidth, mw, XtNmaxHeight, mh, XtNwidth, w, XtNheight, h, NULL);
	}

	fprintf(stderr, "adjust_Xt_win: %dx%d & %dx%d\n", w, h, w, h0);

	XtVaSetValues(desktop,  XtNwidth, w, XtNheight, h, NULL);

	XtResizeWidget(desktop, w, h, 0);

	if (!autoscale) {
		dw = appData.wmDecorationWidth;
		dh = appData.wmDecorationHeight;

		x = (dpyWidth  - w  - dw)/2;
		y = (dpyHeight - h0 - dh)/2;

		XtConfigureWidget(toplevel, x + dw, y + dh, w, h0, 0);
	}
}

void rescale_image(void) {
	double frac_x, frac_y; 
	int w, h;

	if (image == NULL) {
		create_image();
		return;
	}

	if (appData.useXserverBackingStore) {
		create_image();
		return;
	}

	if (image == NULL && image_scale == NULL) {
		create_image();
		return;
	}

	if (appData.scale == NULL) {
		/* switching to not scaled */
		frac_x = frac_y = 1.0;
	} else {
		get_scale_values(&frac_x, &frac_y);
		if (frac_x < 0.0 || frac_y < 0.0) {
			create_image();
			return;
		}
	}

	last_rescale = dnow();

	SoftCursorLockArea(0, 0, si.framebufferWidth, si.framebufferHeight);

	if (image_scale == NULL) {
		/* switching from not scaled */
		int i;
		int Bpl = image->bytes_per_line;
		char *dst, *src = image->data;

		image_scale = XCreateImage(dpy, vis, visdepth, ZPixmap, 0, NULL,
		    si.framebufferWidth, si.framebufferHeight, BitmapPad(dpy), 0);

		image_scale->data = (char *) malloc(image_scale->bytes_per_line * image_scale->height);

		fprintf(stderr, "rescale_image: switching from not scaled. created image_scale %dx%d\n", image_scale->width, image_scale->height);
		fprintf(stderr, "rescale_image: copying image -> image_scale %dx%d -> %dx%d\n", image->width, image->height, image_scale->width, image_scale->height);

		dst = image_scale->data;

		/* copy from image->data */
		for (i=0; i < image->height; i++) {
			memcpy(dst, src, Bpl);
			dst += Bpl;
			src += Bpl;
		}
	}

	/* now destroy image */
	if (image && image->data) {
		if (UsingShm()) {
			ShmDetach();
		}
		XDestroyImage(image);
		fprintf(stderr, "rescale_image: destroyed 'image'\n");
		if (UsingShm()) {
			ShmCleanup();
		}
		image = NULL;
	}
	if (image_ycrop && image_ycrop->data) {
		XDestroyImage(image_ycrop);
		fprintf(stderr, "rescale_image: destroyed 'image_ycrop'\n");
		image_ycrop = NULL;
	}

	if (frac_x == 1.0 && frac_y == 1.0) {
		/* switching to not scaled */
		fprintf(stderr, "rescale_image: switching to not scaled.\n");
		w = si.framebufferWidth;
		h = si.framebufferHeight;

		scale_factor_x = 0.0;
		scale_factor_y = 0.0;
		scale_x = 0;
		scale_y = 0;
	} else {
		w = scale_round(si.framebufferWidth,  frac_x);
		h = scale_round(si.framebufferHeight, frac_y);

		scale_factor_x = frac_x;
		scale_factor_y = frac_y;
		scale_x = w;
		scale_y = h;
	}

	adjust_Xt_win(w, h);

	fprintf(stderr, "rescale: %dx%d  %.4f %.4f\n", w, h, scale_factor_x, scale_factor_y);

	try_create_image();

	if (image && image->data && image_scale && frac_x == 1.0 && frac_y == 1.0) {
		/* switched to not scaled */
		int i;
		int Bpl = image->bytes_per_line;
		char *dst = image->data;
		char *src = image_scale->data;

		fprintf(stderr, "rescale_image: switching to not scaled.\n");

		for (i=0; i < image->height; i++) {
			memcpy(dst, src, Bpl);
			dst += Bpl;
			src += Bpl;
		}
		XDestroyImage(image_scale);
		fprintf(stderr, "rescale_image: destroyed 'image_scale'\n");
		image_scale = NULL;
	}

	if (appData.yCrop > 0) {
		int ycrop = appData.yCrop;
		/* do the top part first so they can see it earlier */
		put_image(0, 0, 0, 0, si.framebufferWidth, ycrop, 0);
		if (si.framebufferHeight > ycrop) {
			/* this is a big fb and so will take a long time */
			if (waitCursor != None) {
				XDefineCursor(dpy, desktopWin, waitCursor);
				XSync(dpy, False);
			}
			put_image(0, 0, 0, 0, si.framebufferWidth, si.framebufferHeight - ycrop, 0);
			if (waitCursor != None) {
				Xcursors(1);
				if (appData.useX11Cursor) {
					XSetWindowAttributes attr;
					unsigned long valuemask = 0;
					if (appData.viewOnly) {
						attr.cursor = dotCursor4;    
					} else {
						attr.cursor = dotCursor3;    
					}
					valuemask |= CWCursor;
					XChangeWindowAttributes(dpy, desktopWin, valuemask, &attr);
				}
			}
		}
	} else {
		put_image(0, 0, 0, 0, si.framebufferWidth, si.framebufferHeight, 0);
	}

	SoftCursorUnlockScreen();

	fprintf(stderr, "rescale: image_scale=%p image=%p image_ycrop=%p\n", (void *) image_scale, (void *) image, (void *) image_ycrop);
	last_rescale = dnow();

}

void try_create_image(void) {
	
	image_is_shm = 0;
	if (appData.useShm) {
#ifdef MITSHM
		image = CreateShmImage(0);
		if (!image) {
			if (appData.yCrop > 0) {
				if (appData.scale != NULL && scale_x > 0) {
					;
				} else {
					image_ycrop = CreateShmImage(1);
					if (!image_ycrop) {
						appData.useShm = False;
					} else {
						fprintf(stderr, "created smaller image_ycrop shm image: %dx%d\n",
						    image_ycrop->width, image_ycrop->height);
					}
				}
			} else {
				appData.useShm = False;
			}
		} else {
			image_is_shm = 1;
			fprintf(stderr, "created shm image: %dx%d\n", image->width, image->height);
		}
#endif
	}

	if (!image) {
		fprintf(stderr, "try_create_image: shm image create fail: image == NULL\n");
		if (scale_x > 0) {
			image = XCreateImage(dpy, vis, visdepth, ZPixmap, 0, NULL,
			    scale_x, scale_y, BitmapPad(dpy), 0);
		} else {
			image = XCreateImage(dpy, vis, visdepth, ZPixmap, 0, NULL,
			    si.framebufferWidth, si.framebufferHeight, BitmapPad(dpy), 0);
		}

		image->data = malloc(image->bytes_per_line * image->height);

		if (!image->data) {
			fprintf(stderr, "try_create_image: malloc failed\n");
			exit(1);
		} else {
			fprintf(stderr, "try_create_image: created *non-shm* image: %dx%d\n", image->width, image->height);
		}
	}
	fprintf(stderr, "try_create_image: image->bytes_per_line: %d\n", image->bytes_per_line);
}

void create_image() {
	image = NULL;
	image_ycrop = NULL;
	image_scale = NULL;

	fprintf(stderr, "create_image()\n");

	if (CreateShmImage(-1) == NULL) {
		appData.useShm = False;
	}
	if (appData.scale != NULL) {
		if (appData.useXserverBackingStore) {
			fprintf(stderr, "Cannot scale when using X11 backingstore.\n");
		} else {
			double frac_x = -1.0, frac_y = -1.0;

			get_scale_values(&frac_x, &frac_y);

			if (frac_x < 0.0 || frac_y < 0.0) {
				fprintf(stderr, "Cannot figure out scale factor!\n");
				goto bork;
			}

			scale_factor_x = 0.0;
			scale_factor_y = 0.0;
			scale_x = 0;
			scale_y = 0;


			if (1) {
				int w, h, hyc;

				w = scale_round(si.framebufferWidth,  frac_x);
				h = scale_round(si.framebufferHeight, frac_y);
				hyc = h;
				if (appData.yCrop > 0) {
					hyc = scale_round(appData.yCrop, frac_y);
				}

				/* image scale is full framebuffer */
				image_scale = XCreateImage(dpy, vis, visdepth, ZPixmap, 0, NULL,
				    si.framebufferWidth, si.framebufferHeight, BitmapPad(dpy), 0);

				image_scale->data = (char *) malloc(image_scale->bytes_per_line * image_scale->height);

				fprintf(stderr, "create_image: created image_scale %dx%d\n", image_scale->width, image_scale->height);

				if (!image_scale->data) {
					fprintf(stderr, "create_image: malloc failed\n");
					XDestroyImage(image_scale);
					fprintf(stderr, "create_image: destroyed 'image_scale'\n");
					image_scale = NULL;
				} else {
					int h2;
					scale_factor_x = frac_x;
					scale_factor_y = frac_y;
					scale_x = w;
					scale_y = h;

					if (!strcmp(appData.scale, "auto")) {
						XtVaSetValues(toplevel, XtNmaxWidth, dpyWidth, XtNmaxHeight, dpyHeight, NULL);
					} else {
						XtVaSetValues(toplevel, XtNmaxWidth, w, XtNmaxHeight, hyc, NULL);
					}

					h2 = scale_round(si.framebufferHeight, frac_y);
					XtVaSetValues(desktop,  XtNwidth, w, XtNheight, h2, NULL);

				}
				fprintf(stderr, "create_image: scale: %dx%d  %.4f %.4f\n", w, h,
				    scale_factor_x, scale_factor_y);
			}
		}
	}
	bork:
	try_create_image();
}

int old_width = 0;
int old_height = 0;

int guessCrop(void) {
	int w = si.framebufferWidth;

	if (w == 320) {
		return 240;
	} else if (w == 400) {
		return 300;
	} else if (w == 640) {
		return 480;
	} else if (w == 800) {
		return 600;
	} else if (w == 1024) {
		return 768;
	} else if (w == 1152) {
		return 864;
	} else if (w == 1280) {
		return 1024;
	} else if (w == 1440) {
		return 900;
	} else if (w == 1600) {
		return 1200;
	} else if (w == 1680) {
		return 1050;
	} else if (w == 1920) {
		return 1200;
	} else {
		int h = (3 * w) / 4;
		return h;
	}
}

void check_tall(void) {
	if (appData.appShare) {
		return;
	}
	if (! appData.yCrop) {
		int w = si.framebufferWidth;
		int h = si.framebufferHeight;
		if (h > 2 * w) {
			fprintf(stderr, "Tall display (%dx%d) suspect 'x11vnc -ncache' mode,\n", w, h);
			fprintf(stderr, "  setting auto -ycrop detection.\n");
			appData.yCrop = -1;
		}
	}
}

/*
 * DesktopInitBeforeRealization creates the "desktop" widget and the viewport
 * which controls it.
 */

void
DesktopInitBeforeRealization()
{
	int i;
	int h = si.framebufferHeight;
	int w = si.framebufferWidth;
	double frac_x = 1.0, frac_y = 1.0;

	start_time = dnow();

	prev_fb_width = si.framebufferWidth;
	prev_fb_height = si.framebufferHeight;

	if (appData.scale != NULL) {
		get_scale_values(&frac_x, &frac_y);
		if (frac_x > 0.0 && frac_y > 0.0) {
			w = scale_round(w,  frac_x);
			h = scale_round(h,  frac_y);
		} else {
			appData.scale = NULL;
		}
	}

	form = XtVaCreateManagedWidget("form", formWidgetClass, toplevel,
	    XtNborderWidth, 0, XtNdefaultDistance, 0, NULL);

	viewport = XtVaCreateManagedWidget("viewport", viewportWidgetClass, form,
	    XtNborderWidth, 0, NULL);

	desktop = XtVaCreateManagedWidget("desktop", coreWidgetClass, viewport,
	    XtNborderWidth, 0, NULL);

	XtVaSetValues(desktop, XtNwidth, w, XtNheight, h, NULL);

	XtAddEventHandler(desktop, LeaveWindowMask|EnterWindowMask|ExposureMask,
	    True, HandleBasicDesktopEvent, NULL);
	XtAddEventHandler(form, StructureNotifyMask,
	    False, HandleResizeEvent, NULL);

	if (appData.yCrop) {
		int hm;
		if (appData.yCrop < 0) {
			appData.yCrop = guessCrop();
			fprintf(stderr, "Set -ycrop to: %d\n", appData.yCrop);
		}
		hm = appData.yCrop;

		fprintf(stderr, "ycrop h: %d -> %d\n", hm, (int) (hm*frac_y));

		hm *= frac_y;

		XtVaSetValues(toplevel, XtNmaxHeight, hm, XtNheight, hm, NULL);
		XtVaSetValues(form,     XtNmaxHeight, hm, XtNheight, hm, NULL);
		XtVaSetValues(viewport, XtNforceBars, False, NULL);
		XSync(dpy, False);
	}

	old_width  = si.framebufferWidth;
	old_height = si.framebufferHeight;

	for (i = 0; i < 256; i++) {
		modifierPressed[i] = False;
	}

	create_image();
}

#if 0
static Widget scrollbar_y = NULL;
static int xsst = 2;
#endif

#include <X11/Xaw/Scrollbar.h>

#if 0
static XtCallbackProc Scrolled(Widget w, XtPointer closure, XtPointer call_data) {
	Position x, y;
	XtVaGetValues(desktop, XtNx, &x, XtNy, &y, NULL);
	if (0) fprintf(stderr, "scrolled by %d pixels x=%d y=%d\n", (int) call_data, x, y);
	if (xsst == 2) {
		x = 0;
		y = 0;
		XtVaSetValues(desktop, XtNx, x, XtNy, y, NULL);
	} else if (xsst) {
		XawScrollbarSetThumb(w, 0.0, 0.0);
	} else {
		float t = 0.0;
		XtVaSetValues(w, XtNtopOfThumb, &t, NULL);
	}
	if (closure) {}
}

static XtCallbackProc Jumped(Widget w, XtPointer closure, XtPointer call_data) {
	float top = *((float *) call_data);
	Position x, y;
	XtVaGetValues(desktop, XtNx, &x, XtNy, &y, NULL);
	if (0) fprintf(stderr, "thumb value: %.4f x=%d y=%d\n", top, x, y);
	if (top > 0.01) {
		if (xsst == 2) {
			x = 0;
			y = 0;
			XtVaSetValues(desktop, XtNx, x, XtNy, y, NULL);
		} else if (xsst) {
			XawScrollbarSetThumb(w, 0.0, 0.0);
		} else {
			float t = 0.0, s = 1.0;
			XtVaSetValues(w, XtNtopOfThumb, *(XtArgVal*)&t, XtNshown, *(XtArgVal*)&s, NULL);
		}
	}
	if (closure) {}
}
#endif

extern double dnow(void);

void check_things() {
	static int first = 1;
	static double last_scrollbar = 0.0;
	int w = si.framebufferWidth;
	int h = si.framebufferHeight;
	double now = dnow();
	static double last = 0;
	double fac = image_scale ? scale_factor_y : 1.0;

	if (first) {
		first = 0;
		SendFramebufferUpdateRequest(0, 0, si.framebufferWidth, si.framebufferHeight, False);
	}
	if (appData.yCrop > 0 && appData.yCrop * fac < dpyHeight && h > 2*w && now > last_scrollbar + 0.25) {
		Widget wv, wh, wc;
		Position x0, y0;
		Position x1, y1;
		Dimension w0, h0, b0;
		Dimension w1, h1, b1;
		Dimension w2, h2, b2;
		
		wc = XtNameToWidget(viewport, "clip");
		wv = XtNameToWidget(viewport, "vertical");
		wh = XtNameToWidget(viewport, "horizontal");
		if (wc && wv && wh) {
			int sb = appData.sbWidth;
			XtVaGetValues(wv, XtNwidth, &w0, XtNheight, &h0, XtNborderWidth, &b0, XtNx, &x0, XtNy, &y0, NULL);
			XtVaGetValues(wh, XtNwidth, &w1, XtNheight, &h1, XtNborderWidth, &b1, XtNx, &x1, XtNy, &y1, NULL);
			XtVaGetValues(wc, XtNwidth, &w2, XtNheight, &h2, XtNborderWidth, &b2, NULL);
			if (!sb) {
				sb = 2;
			}
			if (w0 != sb || h1 != sb) {
				fprintf(stderr, "Very tall (-ncache) fb, setting scrollbar thickness to: %d pixels (%d/%d)\n\n", sb, w0, h1);
				
				XtUnmanageChild(wv);
				XtUnmanageChild(wh);
				XtUnmanageChild(wc);

				XtVaSetValues(wv, XtNwidth,  sb, XtNx, x0 + (w0 - sb), NULL);
				XtVaSetValues(wh, XtNheight, sb, XtNy, y1 + (h1 - sb), NULL);
				w2 = w2 + (w0 - sb);
				h2 = h2 + (h1 - sb);
				if (w2 > 10 && h2 > 10) {
					XtVaSetValues(wc, XtNwidth, w2, XtNheight, h2, NULL);
				}

				XtManageChild(wv);
				XtManageChild(wh);
				XtManageChild(wc);

				appData.sbWidth = sb;
			}
		}
		last_scrollbar = dnow();
	}
	
	if (now <= last + 0.25) {
		return;
	}

	if (image_scale) {
		scale_check_zrle();
	}

	/* e.g. xrandr resize */
	dpyWidth  = WidthOfScreen(DefaultScreenOfDisplay(dpy));
	dpyHeight = HeightOfScreen(DefaultScreenOfDisplay(dpy));

	last = dnow();
}

/*
 * DesktopInitAfterRealization does things which require the X windows to
 * exist.  It creates some GCs and sets the dot cursor.
 */

void Xcursors(int set) {
	if (dotCursor3 == None) {
		dotCursor3 = CreateDotCursor(3);
	}
	if (dotCursor4 == None) {
		dotCursor4 = CreateDotCursor(4);
	}
	if (set) {
		XSetWindowAttributes attr;
		unsigned long valuemask = 0;

		if (!appData.useX11Cursor) {
			if (appData.viewOnly) {
				attr.cursor = dotCursor4;    
			} else {
				attr.cursor = dotCursor3;    
			}
			valuemask |= CWCursor;
			XChangeWindowAttributes(dpy, desktopWin, valuemask, &attr);
		}
	}
}

void
DesktopInitAfterRealization()
{
	XGCValues gcv;
	XSetWindowAttributes attr;
	XWindowAttributes gattr;
	unsigned long valuemask = 0;

	desktopWin = XtWindow(desktop);

	gc = XCreateGC(dpy,desktopWin,0,NULL);

	gcv.function = GXxor;
	gcv.foreground = 0x0f0f0f0f;
	srcGC = XCreateGC(dpy,desktopWin,GCFunction|GCForeground,&gcv);
	gcv.foreground = 0xf0f0f0f0;
	dstGC = XCreateGC(dpy,desktopWin,GCFunction|GCForeground,&gcv);

	XtAddConverter(XtRString, XtRBackingStore, XmuCvtStringToBackingStore,
	    NULL, 0);

	if (appData.useXserverBackingStore) {
		Screen *s = DefaultScreenOfDisplay(dpy);
		if (DoesBackingStore(s) != Always) {
			fprintf(stderr, "X server does not do backingstore, disabling it.\n");
			appData.useXserverBackingStore = False;
		}
	}

	if (appData.useXserverBackingStore) {
		XtVaGetApplicationResources(desktop, (XtPointer)&attr.backing_store,
		    desktopBackingStoreResources, 1, NULL);
		valuemask |= CWBackingStore;
	} else {
		attr.background_pixel = BlackPixel(dpy, DefaultScreen(dpy));
		valuemask |= CWBackPixel;
	}

	Xcursors(0);
	if (!appData.useX11Cursor) {
		if (appData.viewOnly) {
			attr.cursor = dotCursor4;    
		} else {
			attr.cursor = dotCursor3;    
		}
		valuemask |= CWCursor;
	}
	bogoCursor = XCreateFontCursor(dpy, XC_bogosity);
	waitCursor = XCreateFontCursor(dpy, XC_watch);

	XChangeWindowAttributes(dpy, desktopWin, valuemask, &attr);

	if (XGetWindowAttributes(dpy, desktopWin, &gattr)) {
#if 0
		fprintf(stderr, "desktopWin backingstore: %d save_under: %d\n", gattr.backing_store, gattr.save_under);
#endif
	}
	fprintf(stderr, "\n");
}

extern void FreeX11Cursor(void);
extern void FreeSoftCursor(void);

void
DesktopCursorOff()
{
	if (dotCursor3 == None) {
		dotCursor3 = CreateDotCursor(3);
		dotCursor4 = CreateDotCursor(4);
	}
	if (appData.viewOnly) {
		XDefineCursor(dpy, desktopWin, dotCursor4);
	} else {
		XDefineCursor(dpy, desktopWin, dotCursor3);
	}
	FreeX11Cursor();
	FreeSoftCursor();
}


#define CEIL(x)  ( (double) ((int) (x)) == (x) ? \
	(double) ((int) (x)) : (double) ((int) (x) + 1) )
#define FLOOR(x) ( (double) ((int) (x)) )

#if 0
static int nfix(int i, int n) {
	if (i < 0) {
		i = 0;
	} else if (i >= n) {
		i = n - 1;
	}
	return i;
}
#else
#define nfix(i, n) ( i < 0 ? 0 : ( (i >= n) ? (n - 1) : i  ) )
#endif

int scale_round(int len, double fac) {
        double eps = 0.000001;

        len = (int) (len * fac + eps);
        if (len < 1) {
                len = 1;
        }
        return len;
}

static void scale_rect(double factor_x, double factor_y, int blend, int interpolate,
    int *px, int *py, int *pw, int *ph, int solid) {

	int i, j, i1, i2, j1, j2;       /* indices for scaled fb (dest) */
	int I, J, I1, I2, J1, J2;       /* indices for main fb   (source) */

	double w, wx, wy, wtot; /* pixel weights */

	double x1 = 0, y1, x2 = 0, y2;  /* x-y coords for destination pixels edges */
	double dx, dy;          /* size of destination pixel */
	double ddx=0, ddy=0;    /* for interpolation expansion */

	char *src, *dest;       /* pointers to the two framebuffers */

	unsigned short us = 0;
	unsigned char  uc = 0;
	unsigned int   ui = 0;

	int use_noblend_shortcut = 1;
	int shrink;             /* whether shrinking or expanding */
	static int constant_weights = -1, mag_int = -1;
	static int last_Nx = -1, last_Ny = -1, cnt = 0;
	static double last_factor = -1.0;
	int b, k;
	double pixave[4];       /* for averaging pixel values */

	/* internal */

	int X1, X2, Y1, Y2;

	int Nx = si.framebufferWidth;
	int Ny = si.framebufferHeight;

	int nx = scale_round(Nx, factor_x);
	int ny = scale_round(Ny, factor_y);

	int Bpp = image->bits_per_pixel / 8;
	int dst_bytes_per_line = image->bytes_per_line;
	int src_bytes_per_line = image_scale->bytes_per_line;

	unsigned long main_red_mask = image->red_mask;
	unsigned long main_green_mask = image->green_mask;
	unsigned long main_blue_mask = image->blue_mask;
	int mark = 1;

	char *src_fb = image_scale->data;
	char *dst_fb = image->data;

	static int nosolid = -1;
	int sbdy = 3;
	double fmax = factor_x > factor_y ? factor_x : factor_y;
#if 0
	double fmin = factor_x < factor_y ? factor_x : factor_y;
#endif

	X1 = *px;
	X2 = *px + *pw;
	Y1 = *py;
	Y2 = *py + *ph;

	if (fmax > 1.0) {
		/* try to avoid problems with bleeding... */
		sbdy = (int) (2.0 * fmax * sbdy);
	}

	/* fprintf(stderr, "scale_rect: %dx%d+%d+%d\n", *pw, *ph, *px, *py); */

	*px = (int) (*px * factor_x);
	*py = (int) (*py * factor_y);
	*pw = scale_round(*pw, factor_x);
	*ph = scale_round(*ph, factor_y);

	if (nosolid < 0) {
		if (getenv("SSVNC_NOSOLID")) {
			nosolid = 1;
		} else {
			nosolid = 0;
		}
	}
	if (nosolid) solid = 0;

#define rfbLog printf
/* Begin taken from x11vnc scale: */

	if (factor_x <= 1.0 || factor_y <= 1.0) {
		shrink = 1;
	} else {
		shrink = 0;
		interpolate = 1;
	}

	/*
	 * N.B. width and height (real numbers) of a scaled pixel.
	 * both are > 1   (e.g. 1.333 for -scale 3/4)
	 * they should also be equal but we don't assume it.
	 *
	 * This new way is probably the best we can do, take the inverse
	 * of the scaling factor to double precision.
	 */
	dx = 1.0/factor_x;
	dy = 1.0/factor_y;

	/*
	 * There is some speedup if the pixel weights are constant, so
	 * let's special case these.
	 *
	 * If scale = 1/n and n divides Nx and Ny, the pixel weights
	 * are constant (e.g. 1/2 => equal on 2x2 square).
	 */
	if (factor_x != last_factor || Nx != last_Nx || Ny != last_Ny) {
		constant_weights = -1;
		mag_int = -1;
		last_Nx = Nx;
		last_Ny = Ny;
		last_factor = factor_x;
	}

	if (constant_weights < 0 && factor_x != factor_y) {
		constant_weights = 0;
		mag_int = 0;
	} else if (constant_weights < 0) {
		int n = 0;
		double factor = factor_x;

		constant_weights = 0;
		mag_int = 0;

		for (i = 2; i<=128; i++) {
			double test = ((double) 1)/ i;
			double diff, eps = 1.0e-7;
			diff = factor - test;
			if (-eps < diff && diff < eps) {
				n = i;
				break;
			}
		}
		if (! blend || ! shrink || interpolate) {
			;
		} else if (n != 0) {
			if (Nx % n == 0 && Ny % n == 0) {
				static int didmsg = 0;
				if (mark && ! didmsg) {
					didmsg = 1;
					rfbLog("scale_and_mark_rect: using "
					    "constant pixel weight speedup "
					    "for 1/%d\n", n);
				}
				constant_weights = 1;
			}
		}

		n = 0;
		for (i = 2; i<=32; i++) {
			double test = (double) i;
			double diff, eps = 1.0e-7;
			diff = factor - test;
			if (-eps < diff && diff < eps) {
				n = i;
				break;
			}
		}
		if (! blend && factor > 1.0 && n) {
			mag_int = n;
		}
	}
if (0) fprintf(stderr, "X1: %d Y1: %d X2: %d Y2: %d\n", X1, Y1, X2, Y2);

	if (mark && !shrink && blend) {
		/*
		 * kludge: correct for interpolating blurring leaking
		 * up or left 1 destination pixel.
		 */
		if (X1 > 0) X1--;
		if (Y1 > 0) Y1--;
	}

	/*
	 * find the extent of the change the input rectangle induces in
	 * the scaled framebuffer.
	 */

	/* Left edges: find largest i such that i * dx <= X1  */
	i1 = FLOOR(X1/dx);

	/* Right edges: find smallest i such that (i+1) * dx >= X2+1  */
	i2 = CEIL( (X2+1)/dx ) - 1;

	/* To be safe, correct any overflows: */
	i1 = nfix(i1, nx);
	i2 = nfix(i2, nx) + 1;	/* add 1 to make a rectangle upper boundary */

	/* Repeat above for y direction: */
	j1 = FLOOR(Y1/dy);
	j2 = CEIL( (Y2+1)/dy ) - 1;

	j1 = nfix(j1, ny);
	j2 = nfix(j2, ny) + 1;

	/*
	 * special case integer magnification with no blending.
	 * vision impaired magnification usage is interested in this case.
	 */
	if (mark && ! blend && mag_int && Bpp != 3) {
		int jmin, jmax, imin, imax;

		/* outer loop over *source* pixels */
		for (J=Y1; J < Y2; J++) {
		    jmin = J * mag_int;
		    jmax = jmin + mag_int;
		    for (I=X1; I < X2; I++) {
			/* extract value */
			src = src_fb + J*src_bytes_per_line + I*Bpp;
			if (Bpp == 4) {
				ui = *((unsigned int *)src);
			} else if (Bpp == 2) {
				us = *((unsigned short *)src);
			} else if (Bpp == 1) {
				uc = *((unsigned char *)src);
			}
			imin = I * mag_int;
			imax = imin + mag_int;
			/* inner loop over *dest* pixels */
			for (j=jmin; j<jmax; j++) {
			    dest = dst_fb + j*dst_bytes_per_line + imin*Bpp;
			    for (i=imin; i<imax; i++) {
				if (Bpp == 4) {
					*((unsigned int *)dest) = ui;
				} else if (Bpp == 2) {
					*((unsigned short *)dest) = us;
				} else if (Bpp == 1) {
					*((unsigned char *)dest) = uc;
				}
				dest += Bpp;
			    }
			}
		    }
		}
		goto markit;
	}

	/* set these all to 1.0 to begin with */
	wx = 1.0;
	wy = 1.0;
	w  = 1.0;

	/*
	 * Loop over destination pixels in scaled fb:
	 */
	for (j=j1; j<j2; j++) {
		int jbdy = 1, I1_solid = 0;

		y1 =  j * dy;	/* top edge */
		if (y1 > Ny - 1) {
			/* can go over with dy = 1/scale_fac */
			y1 = Ny - 1;
		}
		y2 = y1 + dy;	/* bottom edge */

		/* Find main fb indices covered by this dest pixel: */
		J1 = (int) FLOOR(y1);
		J1 = nfix(J1, Ny);

		if (shrink && ! interpolate) {
			J2 = (int) CEIL(y2) - 1;
			J2 = nfix(J2, Ny);
		} else {
			J2 = J1 + 1;	/* simple interpolation */
			ddy = y1 - J1;
		}

		/* destination char* pointer: */
		dest = dst_fb + j*dst_bytes_per_line + i1*Bpp;

		if (solid) {
			if (j1+sbdy <= j && j < j2-sbdy) {
				jbdy = 0;
				x1 = (i1+sbdy) * dx;
				if (x1 > Nx - 1) {
					x1 = Nx - 1;
				}
				I1_solid = (int) FLOOR(x1);
				if (I1_solid >= Nx) I1_solid = Nx - 1;
			}
		}
		
		for (i=i1; i<i2; i++) {
			int solid_skip = 0;

			if (solid) {
				/* if the region is solid, we can use the noblend speedup */
				if (!jbdy && i1+sbdy <= i && i < i2-sbdy) {
					solid_skip = 1;
					/* pixels all the same so use X1: */
					I1 = I1_solid;
					goto jsolid;
				}
			}

			x1 =  i * dx;	/* left edge */
			if (x1 > Nx - 1) {
				/* can go over with dx = 1/scale_fac */
				x1 = Nx - 1;
			}
			x2 = x1 + dx;	/* right edge */

			/* Find main fb indices covered by this dest pixel: */
			I1 = (int) FLOOR(x1);
			if (I1 >= Nx) I1 = Nx - 1;

			jsolid:
			cnt++;

			if ((!blend && use_noblend_shortcut) || solid_skip) {
				/*
				 * The noblend case involves no weights,
				 * and 1 pixel, so just copy the value
				 * directly.
				 */
				src = src_fb + J1*src_bytes_per_line + I1*Bpp;
				if (Bpp == 4) {
					*((unsigned int *)dest)
					    = *((unsigned int *)src);
				} else if (Bpp == 2) {
					*((unsigned short *)dest)
					    = *((unsigned short *)src);
				} else if (Bpp == 1) {
					*(dest) = *(src);
				} else if (Bpp == 3) {
					/* rare case */
					for (k=0; k<=2; k++) {
						*(dest+k) = *(src+k);
					}
				}
				dest += Bpp;
				continue;
			}
			
			if (shrink && ! interpolate) {
				I2 = (int) CEIL(x2) - 1;
				if (I2 >= Nx) I2 = Nx - 1;
			} else {
				I2 = I1 + 1;	/* simple interpolation */
				ddx = x1 - I1;
			}
#if 0
if (first) fprintf(stderr, "  I1=%d I2=%d J1=%d J2=%d\n", I1, I2, J1, J2);
#endif

			/* Zero out accumulators for next pixel average: */
			for (b=0; b<4; b++) {
				pixave[b] = 0.0; /* for RGB weighted sums */
			}

			/*
			 * wtot is for accumulating the total weight.
			 * It should always sum to 1/(scale_fac * scale_fac).
			 */
			wtot = 0.0;

			/*
			 * Loop over source pixels covered by this dest pixel.
			 * 
			 * These "extra" loops over "J" and "I" make
			 * the cache/cacheline performance unclear.
			 * For example, will the data brought in from
			 * src for j, i, and J=0 still be in the cache
			 * after the J > 0 data have been accessed and
			 * we are at j, i+1, J=0?  The stride in J is
			 * main_bytes_per_line, and so ~4 KB.
			 *
			 * Typical case when shrinking are 2x2 loop, so
			 * just two lines to worry about.
			 */
			for (J=J1; J<=J2; J++) {
			    /* see comments for I, x1, x2, etc. below */
			    if (constant_weights) {
				;
			    } else if (! blend) {
				if (J != J1) {
					continue;
				}
				wy = 1.0;

				/* interpolation scheme: */
			    } else if (! shrink || interpolate) {
				if (J >= Ny) {
					continue;
				} else if (J == J1) {
					wy = 1.0 - ddy;
				} else if (J != J1) {
					wy = ddy;
				}

				/* integration scheme: */
			    } else if (J < y1) {
				wy = J+1 - y1;
			    } else if (J+1 > y2) {
				wy = y2 - J;
			    } else {
				wy = 1.0;
			    }

			    src = src_fb + J*src_bytes_per_line + I1*Bpp;

			    for (I=I1; I<=I2; I++) {

				/* Work out the weight: */

				if (constant_weights) {
					;
				} else if (! blend) {
					/*
					 * Ugh, PseudoColor colormap is
					 * bad news, to avoid random
					 * colors just take the first
					 * pixel.  Or user may have
					 * specified :nb to fraction.
					 * The :fb will force blending
					 * for this case.
					 */
					if (I != I1) {
						continue;
					}
					wx = 1.0;

					/* interpolation scheme: */
				} else if (! shrink || interpolate) {
					if (I >= Nx) {
						continue;	/* off edge */
					} else if (I == I1) {
						wx = 1.0 - ddx;
					} else if (I != I1) {
						wx = ddx;
					}

					/* integration scheme: */
				} else if (I < x1) {
					/* 
					 * source left edge (I) to the
					 * left of dest left edge (x1):
					 * fractional weight
					 */
					wx = I+1 - x1;
				} else if (I+1 > x2) {
					/* 
					 * source right edge (I+1) to the
					 * right of dest right edge (x2):
					 * fractional weight
					 */
					wx = x2 - I;
				} else {
					/* 
					 * source edges (I and I+1) completely
					 * inside dest edges (x1 and x2):
					 * full weight
					 */
					wx = 1.0;
				}

				w = wx * wy;
				wtot += w;

				/* 
				 * We average the unsigned char value
				 * instead of char value: otherwise
				 * the minimum (char 0) is right next
				 * to the maximum (char -1)!  This way
				 * they are spread between 0 and 255.
				 */
				if (Bpp == 4) {
					/* unroll the loops, can give 20% */
					pixave[0] += w * ((unsigned char) *(src  ));
					pixave[1] += w * ((unsigned char) *(src+1));
					pixave[2] += w * ((unsigned char) *(src+2));
					pixave[3] += w * ((unsigned char) *(src+3));
				} else if (Bpp == 2) {
					/*
					 * 16bpp: trickier with green
					 * split over two bytes, so we
					 * use the masks:
					 */
					us = *((unsigned short *) src);
					pixave[0] += w*(us & main_red_mask);
					pixave[1] += w*(us & main_green_mask);
					pixave[2] += w*(us & main_blue_mask);
				} else if (Bpp == 1) {
					pixave[0] += w *
					    ((unsigned char) *(src));
				} else {
					for (b=0; b<Bpp; b++) {
						pixave[b] += w *
						    ((unsigned char) *(src+b));
					}
				}
				src += Bpp;
			    }
			}

			if (wtot <= 0.0) {
				wtot = 1.0;
			}
			wtot = 1.0/wtot;	/* normalization factor */

			/* place weighted average pixel in the scaled fb: */
			if (Bpp == 4) {
				*(dest  ) = (char) (wtot * pixave[0]);
				*(dest+1) = (char) (wtot * pixave[1]);
				*(dest+2) = (char) (wtot * pixave[2]);
				*(dest+3) = (char) (wtot * pixave[3]);
			} else if (Bpp == 2) {
				/* 16bpp / 565 case: */
				pixave[0] *= wtot;
				pixave[1] *= wtot;
				pixave[2] *= wtot;
				us =  (main_red_mask   & (int) pixave[0])
				    | (main_green_mask & (int) pixave[1])
				    | (main_blue_mask  & (int) pixave[2]);
				*( (unsigned short *) dest ) = us;
			} else if (Bpp == 1) {
				*(dest) = (char) (wtot * pixave[0]);
			} else {
				for (b=0; b<Bpp; b++) {
					*(dest+b) = (char) (wtot * pixave[b]);
				}
			}
			dest += Bpp;
		}
	}
	markit:
/* End taken from x11vnc scale: */
	if (0) {}
}

void do_scale_stats(int width, int height) {
	static double calls = 0.0, sum = 0.0, var = 0.0, last = 0.0;
	double A = width * height;

	if (last == 0.0) {
		last = dnow();
	}

	calls += 1.0;
	sum += A;
	var += A*A;

	if (dnow() > last + 4.0) {
		double cnt = calls;
		if (cnt <= 0.0) cnt = 1.0;
		var /= cnt;
		sum /= cnt;
		var = var - sum * sum;
		if (sum > 0.0) {
			var = var / (sum*sum);
		}
		fprintf(stderr, "scale_rect stats: %10d %10.1f ave: %10.3f var-rat: %10.3f\n", (int) calls, sum * cnt, sum, var); 

		calls = 0.0;
		sum = 0.0;
		var = 0.0;
		last = dnow();
	}
}

void put_image(int src_x, int src_y, int dst_x, int dst_y, int width,
    int height, int solid) {
	int db = 0;
	int xmax = si.framebufferWidth;
	int ymax = si.framebufferHeight;

if (db || 0) fprintf(stderr, "put_image(%d %d %d %d %d %d)\n", src_x, src_y, dst_x, dst_y, width, height);

	if (image_scale) {
		int i;
		static int scale_stats = -1;

		for (i=0; i < 2; i++) {
			if (src_x > 0) src_x--;
			if (src_y > 0) src_y--;
		}
		for (i=0; i < 4; i++) {
			if (src_x + width  < xmax) width++;
			if (src_y + height < ymax) height++;
		}

		if (db) fprintf(stderr, "put_image(%d %d %d %d %d %d)\n", src_x, src_y, dst_x, dst_y, width, height);
		if (db) fprintf(stderr, "scale_rect(%d %d %d %d)\n", src_x, src_y, width, height);

		if (scale_stats < 0) {
			if (getenv("SSVNC_SCALE_STATS")) {
				scale_stats = 1;
			} else {
				scale_stats = 0;
			}
		}
		if (scale_stats) {
			do_scale_stats(width, height);
		}

		scale_rect(scale_factor_x, scale_factor_y, 1, 0, &src_x, &src_y, &width, &height, solid);
		dst_x = src_x;
		dst_y = src_y;
	}
	
#ifdef MITSHM
	if (appData.useShm) {
		double fac = image_scale ? scale_factor_y : 1.0;
		if (image_ycrop == NULL) {
			if (image_is_shm) {
				XShmPutImage(dpy, desktopWin, gc, image, src_x, src_y,
				    dst_x, dst_y, width, height, False);
			} else {
				XPutImage(dpy, desktopWin, gc, image, src_x, src_y,
				    dst_x, dst_y, width, height);
			}
		} else if ((width < 32 && height < 32) || height > appData.yCrop * fac) {
			XPutImage(dpy, desktopWin, gc, image, src_x, src_y,
			    dst_x, dst_y, width, height);
		} else {
			char *src, *dst;
			int Bpp = image->bits_per_pixel / 8;
			int Bpl  = image->bytes_per_line, h;
			int Bpl2 = image_ycrop->bytes_per_line;
			src = image->data + src_y * Bpl + src_x * Bpp;
			dst = image_ycrop->data;
			for (h = 0; h < height; h++) {
				memcpy(dst, src, width * Bpp);
				src += Bpl;
				dst += Bpl2;
			}
			XShmPutImage(dpy, desktopWin, gc, image_ycrop, 0, 0,
			    dst_x, dst_y, width, height, False);
		}
	} else
#endif
	{
		XPutImage(dpy, desktopWin, gc, image, src_x, src_y,
		   dst_x, dst_y, width, height);
	}
}

#if 0
fprintf(stderr, "non-shmB image %d %d %d %d %d %d\n", src_x, src_y, dst_x, dst_y, width, height);
fprintf(stderr, "shm image_ycrop %d %d %d %d %d %d\n", 0, 0, dst_x, dst_y, width, height);
fprintf(stderr, "non-shmA image %d %d %d %d %d %d\n", src_x, src_y, dst_x, dst_y, width, height);
#endif

void releaseAllPressedModifiers(void) {
	int i;
	static int debug_release = -1;
	if (debug_release < 0) {
		if (getenv("SSVNC_DEBUG_RELEASE")) {
			debug_release = 1;
		} else {
			debug_release = 0;
		}
	}
	if (debug_release) fprintf(stderr, "into releaseAllPressedModifiers()\n");
	for (i = 0; i < 256; i++) {
		if (modifierPressed[i]) {
			SendKeyEvent(XKeycodeToKeysym(dpy, i, 0), False);
			modifierPressed[i] = False;
			if (debug_release) fprintf(stderr, "releasing[%d] %s\n", i, XKeysymToString(XKeycodeToKeysym(dpy, i, 0)));
		}
	}
}

#define PR_EXPOSE fprintf(stderr, "Expose: %04dx%04d+%04d+%04d %04d/%04d/%04d now: %8.4f rescale: %8.4f fullscreen: %8.4f\n", width, height, x, y, si.framebufferWidth, appData.yCrop, si.framebufferHeight, now - start_time, now - last_rescale, now - last_fullscreen);

static void
HandleResizeEvent(Widget wdg, XtPointer ptr, XEvent *ev, Boolean *cont)
{
    static Dimension last_w = 0, last_h = 0;
    Dimension w, h;
    
    if (ev->type == ConfigureNotify && !appData.fullScreen
	&& appData.scale != NULL && !strcasecmp(appData.scale, "auto")) {
	XtVaGetValues(form, XtNwidth, &w, XtNheight, &h, NULL);
	if (w != last_w || h != last_h) {
	    rescale_image();
	    last_w = w;
	    last_h = h;
	}
    }
}
/*
 * HandleBasicDesktopEvent - deal with expose and leave events.
 */

static void
HandleBasicDesktopEvent(Widget w, XtPointer ptr, XEvent *ev, Boolean *cont)
{
	int x, y, width, height;
	double now = dnow();

	if (w || ptr || cont) {}

	if (0) {
		PR_EXPOSE;
	}


  switch (ev->type) {
  case Expose:
  case GraphicsExpose:
    /* sometimes due to scrollbars being added/removed we get an expose outside
       the actual desktop area.  Make sure we don't pass it on to the RFB
       server. */
	x = ev->xexpose.x;
	y = ev->xexpose.y;
	width  = ev->xexpose.width;
	height = ev->xexpose.height;

	if (image_scale) {
		int i;
		x /= scale_factor_x;
		y /= scale_factor_y;
		width  /= scale_factor_x;
		height /= scale_factor_y;
		/* make them a little wider to avoid painting errors */
		for (i=0; i < 3; i++) {
			if (x > 0) x--;
			if (y > 0) y--;
		}
		for (i=0; i < 6; i++) {
			if (x + width  < si.framebufferWidth)   width++;
			if (y + height < si.framebufferHeight)  height++;
		}
	}

	if (x + width > si.framebufferWidth) {
		width = si.framebufferWidth - x;
		if (width <= 0) {
			break;
		}
	}

	if (y + height > si.framebufferHeight) {
		height = si.framebufferHeight - y;
		if (height <= 0) {
			break;
		}
	}

	if (appData.useXserverBackingStore) {
		SendFramebufferUpdateRequest(x, y, width, height, False);
	} else {
		int ok = 1;
		double delay = 2.5;
		if (appData.fullScreen && now < last_fullscreen + delay) {
			int xmax = si.framebufferWidth;
			int ymax = si.framebufferHeight;
			if (appData.yCrop > 0) {
				ymax = appData.yCrop;
			}
			xmax = scale_round(xmax, scale_factor_x);
			ymax = scale_round(ymax, scale_factor_y);
			if (dpyWidth < xmax) {
				xmax = dpyWidth;
			}
			if (dpyHeight < ymax) {
				ymax = dpyHeight;
			}
			if (x != 0 && y != 0) {
				ok = 0;
			}
			if (width < 0.9 * xmax) {
				ok = 0;
			}
			if (height < 0.9 * ymax) {
				ok = 0;
			}
		}
		if (appData.yCrop > 0) {
			if (now < last_fullscreen + delay || now < last_rescale + delay) {
				if (y + height > appData.yCrop) {
					height = appData.yCrop - y;
				}
			}
		}
		if (ok) {
			put_image(x, y, x, y, width, height, 0);
			XSync(dpy, False);
		} else {
			fprintf(stderr, "Skip ");
			PR_EXPOSE;
		}
	}
	break;

  case LeaveNotify:
	releaseAllPressedModifiers();
	if (appData.fullScreen) {
		fs_ungrab(1);
	}
	break;
  case EnterNotify:
	if (appData.fullScreen) {
		fs_grab(1);
	}
	break;
  case ClientMessage:
	if (ev->xclient.window == XtWindow(desktop) && ev->xclient.message_type == XA_INTEGER &&
	    ev->xclient.format == 8 && !strcmp(ev->xclient.data.b, "SendRFBUpdate")) {
		SendIncrementalFramebufferUpdateRequest();
	}
	break;
  }
	check_things();
}

extern Position desktopX, desktopY;

void x11vnc_appshare(char *cmd) {
	char send[200], str[100];
	char *id = "cmd=id_cmd";
	int m_big = 80, m_fine = 15;
	int resize = 100, db = 0;

	if (getenv("X11VNC_APPSHARE_DEBUG")) {
		db = atoi(getenv("X11VNC_APPSHARE_DEBUG"));
	}

	if (db) fprintf(stderr, "x11vnc_appshare: cmd=%s\n", cmd);

	str[0] = '\0';

	if (!strcmp(cmd, "left")) {
		sprintf(str, "%s:move:-%d+0", id, m_big);
	} else if (!strcmp(cmd, "right")) {
		sprintf(str, "%s:move:+%d+0", id, m_big);
	} else if (!strcmp(cmd, "up")) {
		sprintf(str, "%s:move:+0-%d", id, m_big);
	} else if (!strcmp(cmd, "down")) {
		sprintf(str, "%s:move:+0+%d", id, m_big);
	} else if (!strcmp(cmd, "left-fine")) {
		sprintf(str, "%s:move:-%d+0", id, m_fine);
	} else if (!strcmp(cmd, "right-fine")) {
		sprintf(str, "%s:move:+%d+0", id, m_fine);
	} else if (!strcmp(cmd, "up-fine")) {
		sprintf(str, "%s:move:+0-%d", id, m_fine);
	} else if (!strcmp(cmd, "down-fine")) {
		sprintf(str, "%s:move:+0+%d", id, m_fine);
	} else if (!strcmp(cmd, "taller")) {
		sprintf(str, "%s:resize:+0+%d", id, resize);
	} else if (!strcmp(cmd, "shorter")) {
		sprintf(str, "%s:resize:+0-%d", id, resize);
	} else if (!strcmp(cmd, "wider")) {
		sprintf(str, "%s:resize:+%d+0", id, resize);
	} else if (!strcmp(cmd, "narrower")) {
		sprintf(str, "%s:resize:-%d+0", id, resize);
	} else if (!strcmp(cmd, "lower")) {
		sprintf(str, "%s:lower", id);
	} else if (!strcmp(cmd, "raise")) {
		sprintf(str, "%s:raise", id);
	} else if (!strcmp(cmd, "delete")) {
		sprintf(str, "%s:wm_delete", id);
	} else if (!strcmp(cmd, "position")) {
		Position x, y;
		int xi, yi;

		XtVaGetValues(toplevel, XtNx, &x, XtNy, &y, NULL);
		xi = (int) x;
		yi = (int) y;
		if (appData.scale) {
			double fx = 1.0, fy = 1.0;
			get_scale_values(&fx, &fy);
			if (fx > 0.0 && fy > 0.0) {
				xi /= fx;
				yi /= fx;
			}
		}
		sprintf(str, "%s:geom:0x0+%d+%d", id, xi, yi);
		fprintf(stderr, "str=%s\n", str);
	}
	if (strcmp(str, "")) {
		Bool vo = appData.viewOnly;
		strcpy(send, "X11VNC_APPSHARE_CMD:");
		strcat(send, str);
		if (db) fprintf(stderr, "x11vnc_appshare: send=%s\n", send);
		if (vo) appData.viewOnly = False;
		SendClientCutText(send, strlen(send));
		if (vo) appData.viewOnly = True;
	}
}

void scroll_desktop(int horiz, int vert, double amount) {
	Dimension h, w;
	Position x, y;
	Position x2, y2;
	static int db = -1;

	if (db < 0) {
		if (getenv("SSVNC_DEBUG_ESCAPE_KEYS")) {
			db = 1;
		} else {
			db = 0;
		}
	}

	XtVaGetValues(form, XtNheight, &h, XtNwidth, &w, NULL);
	XtVaGetValues(desktop, XtNx, &x, XtNy, &y, NULL);

	x2 = -x;
	y2 = -y;

	if (amount == -1.0) {
		int dx = horiz;
		int dy = vert;
		if (dx == 0 && dy == 0) {
			return;
		}
		x2 -= dx;
		y2 -= dy;
	} else {
		if (horiz) {
			int dx = (int) (amount * w);
			if (dx < 0) dx = -dx;
			if (amount == 0.0) dx = 1;
			if (horiz > 0) {
				x2 += dx;
			} else {
				x2 -= dx;
			}
			if (x2 < 0) x2 = 0;
		}
		if (vert) {
			int dy = (int) (amount * h);
			if (amount == 0.0) dy = 1;
			if (dy < 0) dy = -dy;
			if (vert < 0) {
				y2 += dy;
			} else {
				y2 -= dy;
			}
			if (y2 < 0) y2 = 0;
		}
	}

	if (db) fprintf(stderr, "%d %d %f viewport(%dx%d): %d %d -> %d %d\n", horiz, vert, amount, w, h, -x, -y, x2, y2); 
	XawViewportSetCoordinates(viewport, x2, y2);

	if (appData.fullScreen) {
		XSync(dpy, False);
		XtVaGetValues(desktop, XtNx, &x, XtNy, &y, NULL);
		desktopX = -x;
		desktopY = -y;
	} else if (amount == -1.0) {
		XSync(dpy, False);
	}
}

void scale_desktop(int bigger, double frac) {
	double current, new;
	char tmp[100];
	char *s;
	int fs;

	if (appData.scale == NULL) {
		s = "1.0";
	} else {
		s = appData.scale;
	}
	if (!strcmp(s, "auto")) {
		fprintf(stderr, "scale_desktop: skipping scale mode '%s'\n", s);
		return;
	} else if (!strcmp(s, "fit")) {
		fprintf(stderr, "scale_desktop: skipping scale mode '%s'\n", s);
		return;
	} else if (strstr(s, "x")) {
		fprintf(stderr, "scale_desktop: skipping scale mode '%s'\n", s);
		return;
	} else if (!strcmp(s, "none")) {
		s = "1.0";
	}

	if (sscanf(s, "%lf", &current) != 1) {
		fprintf(stderr, "scale_desktop: skipping scale mode '%s'\n", s);
		return;
	}
	if (bigger) {
		new = current * (1.0 + frac);
	} else {
		new = current / (1.0 + frac);
	}
	if (0.99 < new && new < 1.01) {
		new = 1.0;
	}

	if (new > 5.0) {
		fprintf(stderr, "scale_desktop: not scaling > 5.0: %f\n", new);
		return;
	} else if (new < 0.05) {
		fprintf(stderr, "scale_desktop: not scaling < 0.05: %f\n", new);
		return;
	}
	sprintf(tmp, "%.16f", new);
	appData.scale = strdup(tmp);

	fs = 0;
	if (appData.fullScreen) {
		fs = 1;
		FullScreenOff();
	}
	if (1) {
		double fx, fy;
		get_scale_values(&fx, &fy);
		if (fx > 0.0 && fy > 0.0) {
			rescale_image();
		}
	}
	if (fs) {
		FullScreenOn();
	}
}

static int escape_mods[8];
static int escape_drag_in_progress = 0, last_x = 0, last_y = 0;
static double last_drag = 0.0;
static double last_key  = 0.0;

static int escape_sequence_pressed(void) {
	static char *prev = NULL;
	char *str = "default";
	int sum, i, init = 0, pressed;
	static int db = -1;

	if (db < 0) {
		if (getenv("SSVNC_DEBUG_ESCAPE_KEYS")) {
			db = 1;
		} else {
			db = 0;
		}
	}

	if (appData.escapeKeys != NULL) {
		str = appData.escapeKeys;
	}
	if (prev == NULL) {
		init = 1;
		prev = strdup(str);
	} else {
		if (strcmp(prev, str)) 	{
			init = 1;
			free(prev);
			prev = strdup(str);
		}
	}
	if (db) fprintf(stderr, "str: %s\n", str);

	if (init) {
		char *p, *s;
		KeySym ks;
		int k = 0, failed = 0;

		for (i = 0; i < 8; i++) {
			escape_mods[i] = -1;
		}

		if (!strcasecmp(str, "default")) {
#if (defined(__MACH__) && defined(__APPLE__))
			s = strdup("Control_L,Meta_L");
#else
			s = strdup("Alt_L,Super_L");
#endif
		} else {
			s = strdup(str);
		}
		
		p = strtok(s, ",+ ");
		while (p) {
			ks = XStringToKeysym(p);
			if (ks == XK_Shift_L || ks == XK_Shift_R) {
				putenv("NO_X11VNC_APPSHARE=1");
			}
			if (k >= 8) {
				fprintf(stderr, "EscapeKeys: more than 8 modifier keys.\n");
				failed = 1;
				break;
			}
			if (ks == NoSymbol) {
				fprintf(stderr, "EscapeKeys: failed lookup for '%s'\n", p);
				failed = 1;
				break;
			} else if (!IsModifierKey(ks)) {
				fprintf(stderr, "EscapeKeys: not a modifier key '%s'\n", p);
				failed = 1;
				break;
			} else {
				KeyCode kc = XKeysymToKeycode(dpy, ks);
				if (kc == NoSymbol) {
					fprintf(stderr, "EscapeKeys: no keycode for modifier key '%s'\n", p);
					failed = 1;
					break;
				}
				if (db) fprintf(stderr, "set: %d %d\n", k, kc);
				escape_mods[k++] = kc; 
			}
			
			p = strtok(NULL, ",+ ");
		}
		free(s);

		if (failed) {
			for (i = 0; i < 8; i++) {
				escape_mods[i] = -1;
			}
		}
	}

	pressed = 1;
	sum = 0;
	for (i = 0; i < 8; i++) {
		int kc = escape_mods[i];
		if (kc != -1 && kc < 256) {
			if (db) fprintf(stderr, "try1: %d %d = %d\n", i, kc, modifierPressed[kc]);
			if (!modifierPressed[kc]) {
				pressed = 0;
				break;
			} else {
				sum++;
			}
		}
	}
	if (sum == 0) pressed = 0;

	if (!pressed) {
		/* user may have dragged mouse outside of toplevel window */
		int i, k;
		int keystate[256];
		char keys[32];

		/* so query server instead of modifierPressed[] */
		XQueryKeymap(dpy, keys);
		for (i=0; i<32; i++) {
			char c = keys[i];

			for (k=0; k < 8; k++) {
				if (c & 0x1) {
					keystate[8*i + k] = 1;
				} else {
					keystate[8*i + k] = 0;
				}
				c = c >> 1;
			}
		}

		/* check again using keystate[] */
		pressed = 2;
		sum = 0;
		for (i = 0; i < 8; i++) {
			int kc = escape_mods[i];
			if (kc != -1 && kc < 256) {
				if (db) fprintf(stderr, "try2: %d %d = %d\n", i, kc, keystate[kc]);
				if (!keystate[kc]) {
					pressed = 0;
					break;
				} else {
					sum++;
				}
			}
		}
		if (sum == 0) pressed = 0;
	}

	return pressed;
}

static int shift_is_down(void) {
	int shift_down = 0;
	KeyCode kc;

	if (appData.viewOnly) {
		int i, k;
		char keys[32];
		int keystate[256];

		XQueryKeymap(dpy, keys);
		for (i=0; i<32; i++) {
			char c = keys[i];

			for (k=0; k < 8; k++) {
				if (c & 0x1) {
					keystate[8*i + k] = 1;
				} else {
					keystate[8*i + k] = 0;
				}
				c = c >> 1;
			}
		}

		kc = XKeysymToKeycode(dpy, XK_Shift_L);
		if (kc != NoSymbol && keystate[kc]) {
			shift_down = 1;
		} else {
			kc = XKeysymToKeycode(dpy, XK_Shift_R);
			if (kc != NoSymbol && keystate[kc]) {
				shift_down = 1;
			}
		}
		return shift_down;
	} else {
		kc = XKeysymToKeycode(dpy, XK_Shift_L);
		if (kc != NoSymbol && modifierPressed[kc]) {
			shift_down = 1;
		} else {
			kc = XKeysymToKeycode(dpy, XK_Shift_R);
			if (kc != NoSymbol && modifierPressed[kc]) {
				shift_down = 1;
			}
		}
		return shift_down;
	}
}
			
/*
 * SendRFBEvent is an action which sends an RFB event.  It can be used in two
 * ways.  Without any parameters it simply sends an RFB event corresponding to
 * the X event which caused it to be called.  With parameters, it generates a
 * "fake" RFB event based on those parameters.  The first parameter is the
 * event type, either "fbupdate", "ptr", "keydown", "keyup" or "key"
 * (down&up).  The "fbupdate" event requests full framebuffer update. For a
 * "key" event the second parameter is simply a keysym string as understood by
 * XStringToKeysym().  For a "ptr" event, the following three parameters are
 * just X, Y and the button mask (0 for all up, 1 for button1 down, 2 for
 * button2 down, 3 for both, etc).
 */

extern Bool selectingSingleWindow;

extern Cursor dotCursor3;
extern Cursor dotCursor4;

extern void set_server_scale(int);

void
SendRFBEvent(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	KeySym ks;
	char keyname[256];
	int buttonMask, x, y;
	int do_escape;
	static int db = -1;
	char *ek = appData.escapeKeys;

	if (db < 0) {
		if (getenv("SSVNC_DEBUG_ESCAPE_KEYS")) {
			db = 1;
		} else {
			db = 0;
		}
	}

	if (ev->type == MotionNotify || ev->type == KeyRelease) {
		static double last = 0.0;
		double now = dnow();
		if (now > last + 0.25) {
			check_things();
			last = now;
		}
	}

	if (selectingSingleWindow && ev->type == ButtonPress) {
		selectingSingleWindow = False;
		SendSingleWindow(ev->xbutton.x, ev->xbutton.y);
		if (appData.viewOnly) {
			XDefineCursor(dpy, desktopWin, dotCursor4);
		} else {
			XDefineCursor(dpy, desktopWin, dotCursor3);
		}
		return;
	}

	if (appData.fullScreen && ev->type == MotionNotify && !escape_drag_in_progress) {
		if (BumpScroll(ev)) {
			return;
		}
	}

	do_escape = 0;
	if (ek != NULL && (ek[0] == 'n' || ek[0] == 'N') && !strcasecmp(ek, "never")) {
		;
	} else if (appData.viewOnly) {
		do_escape = 1;
	} else if (appData.escapeActive) {
		int skip = 0, is_key = 0;

		if (ev->type == KeyPress || ev->type == KeyRelease) {
			is_key = 1;
			XLookupString(&ev->xkey, keyname, 256, &ks, NULL);
			if (IsModifierKey(ks)) {
				skip = 1;
			}
		}
		if (!skip) {
			int es = escape_sequence_pressed();
			if (es == 1) {
				do_escape = 1;
			} else if (es == 2) {
				if (is_key) {
					if (dnow() < last_key + 5.0) {
						do_escape = 1;
					}
				} else {
					if (dnow() < last_drag + 5.0) {
						do_escape = 1;
					}
				}
			}
		}
	}
	if (!do_escape) {
		escape_drag_in_progress = 0;
	}
	if (db) fprintf(stderr, "do_escape: %d\n", do_escape);

	if (do_escape) {
		int W = si.framebufferWidth;
		int H = si.framebufferHeight;
		int shift_down = 0;

		if (!getenv("NO_X11VNC_APPSHARE")) {
			shift_down = shift_is_down();
		}
		if (db) fprintf(stderr, "shift_down: %d\n", shift_down);

		if (*num_params != 0) {
			if (strcasecmp(params[0],"fbupdate") == 0) {
				SendFramebufferUpdateRequest(0, 0, W, H, False);
			}
		}
		if (ev->type == ButtonRelease) {
			XButtonEvent *b = (XButtonEvent *) ev;
			if (db) fprintf(stderr, "ButtonRelease: %d %d %d\n", b->x_root, b->y_root, b->state); 
			if (b->button == 3) {
				if (shift_down) {
					x11vnc_appshare("delete");
				} else {
					ShowPopup(w, ev, params, num_params);
				}
			} else if (escape_drag_in_progress && b->button == 1) {
				escape_drag_in_progress = 0;
			}
		} else if (ev->type == ButtonPress) {
			XButtonEvent *b = (XButtonEvent *) ev;
			if (db) fprintf(stderr, "ButtonPress:   %d %d %d\n", b->x_root, b->y_root, b->state); 
			if (b->button == 1) {
				if (shift_down) {
					x11vnc_appshare("position");
				} else {
					escape_drag_in_progress = 1;
					last_x = b->x_root;
					last_y = b->y_root;
				}
			} else {
				escape_drag_in_progress = 0;
			}
		} else if (ev->type == MotionNotify) {
			XMotionEvent *m = (XMotionEvent *) ev;
			if (escape_drag_in_progress) {
				if (db) fprintf(stderr, "MotionNotify:   %d %d %d\n", m->x_root, m->y_root, m->state); 
				scroll_desktop(m->x_root - last_x, m->y_root - last_y, -1.0);
				last_x = m->x_root;
				last_y = m->y_root;
			}
		} else if (ev->type == KeyRelease) {
			int did = 1;

			XLookupString(&ev->xkey, keyname, 256, &ks, NULL);
			if (ks == XK_1 || ks == XK_KP_1) {
				set_server_scale(1);
			} else if (ks == XK_2 || ks == XK_KP_2) {
				set_server_scale(2);
			} else if (ks == XK_3 || ks == XK_KP_3) {
				set_server_scale(3);
			} else if (ks == XK_4 || ks == XK_KP_4) {
				set_server_scale(4);
			} else if (ks == XK_5 || ks == XK_KP_5) {
				set_server_scale(5);
			} else if (ks == XK_6 || ks == XK_KP_6) {
				set_server_scale(6);
			} else if (ks == XK_r || ks == XK_R) {
				SendFramebufferUpdateRequest(0, 0, W, H, False);
			} else if (ks == XK_b || ks == XK_B) {
				ToggleBell(w, ev, params, num_params);
			} else if (ks == XK_c || ks == XK_C) {
				Toggle8bpp(w, ev, params, num_params);
			} else if (ks == XK_x || ks == XK_X) {
				ToggleX11Cursor(w, ev, params, num_params);
			} else if (ks == XK_z || ks == XK_Z) {
				ToggleTightZRLE(w, ev, params, num_params);
			} else if (ks == XK_h || ks == XK_H) {
				ToggleTightHextile(w, ev, params, num_params);
			} else if (ks == XK_f || ks == XK_F) {
				ToggleFileXfer(w, ev, params, num_params);
			} else if (ks == XK_V) {
				ToggleViewOnly(w, ev, params, num_params);
			} else if (ks == XK_Q) {
				Quit(w, ev, params, num_params);
			} else if (ks == XK_l || ks == XK_L) {
				ToggleFullScreen(w, ev, params, num_params);
			} else if (ks == XK_a || ks == XK_A) {
				ToggleCursorAlpha(w, ev, params, num_params);
			} else if (ks == XK_s || ks == XK_S) {
				SetScale(w, ev, params, num_params);
			} else if (ks == XK_t || ks == XK_T) {
				ToggleTextChat(w, ev, params, num_params);
			} else if (ks == XK_e || ks == XK_E) {
				SetEscapeKeys(w, ev, params, num_params);
			} else if (ks == XK_g || ks == XK_G) {
				ToggleXGrab(w, ev, params, num_params);
			} else if (ks == XK_D) {
				if (shift_down || appData.appShare) {
					x11vnc_appshare("delete");
				}
			} else if (ks == XK_M) {
				if (shift_down || appData.appShare) {
					x11vnc_appshare("position");
				}
			} else if (ks == XK_Left) {
				if (shift_down) {
					x11vnc_appshare("left");
				} else {
					scroll_desktop(-1, 0, 0.1);
				}
			} else if (ks == XK_Right) {
				if (shift_down) {
					x11vnc_appshare("right");
				} else {
					scroll_desktop(+1, 0, 0.1);
				}
			} else if (ks == XK_Up) {
				if (shift_down) {
					x11vnc_appshare("up");
				} else {
					scroll_desktop(0, +1, 0.1);
				}
			} else if (ks == XK_Down) {
				if (shift_down) {
					x11vnc_appshare("down");
				} else {
					scroll_desktop(0, -1, 0.1);
				}
			} else if (ks == XK_KP_Left) {
				if (shift_down) {
					x11vnc_appshare("left-fine");
				} else {
					scroll_desktop(-1, 0, 0.0);
				}
			} else if (ks == XK_KP_Right) {
				if (shift_down) {
					x11vnc_appshare("right-fine");
				} else {
					scroll_desktop(+1, 0, 0.0);
				}
			} else if (ks == XK_KP_Up) {
				if (shift_down) {
					x11vnc_appshare("up-fine");
				} else {
					scroll_desktop(0, +1, 0.0);
				}
			} else if (ks == XK_KP_Down) {
				if (shift_down) {
					x11vnc_appshare("down-fine");
				} else {
					scroll_desktop(0, -1, 0.0);
				}
			} else if (ks == XK_Next || ks == XK_KP_Next) {
				if (shift_down && ks == XK_Next) {
					x11vnc_appshare("shorter");
				} else {
					scroll_desktop(0, -1, 1.0);
				}
			} else if (ks == XK_Prior || ks == XK_KP_Prior) {
				if (shift_down && ks == XK_Prior) {
					x11vnc_appshare("taller");
				} else {
					scroll_desktop(0, +1, 1.0);
				}
			} else if (ks == XK_End || ks == XK_KP_End) {
				if (shift_down && ks == XK_End) {
					x11vnc_appshare("narrower");
				} else {
					scroll_desktop(+1, 0, 1.0);
				}
			} else if (ks == XK_Home || ks == XK_KP_Home) {
				if (shift_down && ks == XK_Home) {
					x11vnc_appshare("wider");
				} else {
					scroll_desktop(-1, 0, 1.0);
				}
			} else if (ks == XK_equal || ks == XK_plus) {
				if (shift_down) {
					x11vnc_appshare("raise");
				} else {
					scale_desktop(1, 0.1);
				}
			} else if (ks == XK_underscore || ks == XK_minus) {
				if (shift_down) {
					x11vnc_appshare("lower");
				} else {
					scale_desktop(0, 0.1);
				}
			} else {
				did = 0;
			}
			if (did) {
				last_key = dnow();
			}
		}
		if (escape_drag_in_progress) {
			last_drag = dnow();
		}
		return;
	}
	if (appData.viewOnly) {
		return;
	}

	if (*num_params != 0) {
		if (strncasecmp(params[0],"key",3) == 0) {
			if (*num_params != 2) {
				fprintf(stderr, "Invalid params: "
				    "SendRFBEvent(key|keydown|keyup,<keysym>)\n");
				return;
			}
			ks = XStringToKeysym(params[1]);
			if (ks == NoSymbol) {
				fprintf(stderr,"Invalid keysym '%s' passed to "
				    "SendRFBEvent\n", params[1]);
				return;
			}
			if (strcasecmp(params[0],"keydown") == 0) {
				SendKeyEvent(ks, 1);
			} else if (strcasecmp(params[0],"keyup") == 0) {
				SendKeyEvent(ks, 0);
			} else if (strcasecmp(params[0],"key") == 0) {
				SendKeyEvent(ks, 1);
				SendKeyEvent(ks, 0);
			} else {
				fprintf(stderr,"Invalid event '%s' passed to "
				    "SendRFBEvent\n", params[0]);
				return;
			}
		} else if (strcasecmp(params[0],"fbupdate") == 0) {
			if (*num_params != 1) {
				fprintf(stderr, "Invalid params: "
				    "SendRFBEvent(fbupdate)\n");
				return;
			}
			SendFramebufferUpdateRequest(0, 0, si.framebufferWidth,
			    si.framebufferHeight, False);

		} else if (strcasecmp(params[0],"ptr") == 0) {
			if (*num_params == 4) {
				x = atoi(params[1]);
				y = atoi(params[2]);
				buttonMask = atoi(params[3]);
				SendPointerEvent(x, y, buttonMask);
			} else if (*num_params == 2) {
				switch (ev->type) {
				case ButtonPress:
				case ButtonRelease:
					x = ev->xbutton.x;
					y = ev->xbutton.y;
					break;
				case KeyPress:
				case KeyRelease:
					x = ev->xkey.x;
					y = ev->xkey.y;
					break;
				default:
					fprintf(stderr, "Invalid event caused "
					    "SendRFBEvent(ptr,<buttonMask>)\n");
					return;
				}
				buttonMask = atoi(params[1]);
				SendPointerEvent(x, y, buttonMask);
			} else {
				fprintf(stderr, "Invalid params: "
				    "SendRFBEvent(ptr,<x>,<y>,<buttonMask>)\n"
				    "             or SendRFBEvent(ptr,<buttonMask>)\n");
				return;
			}
		} else {
			fprintf(stderr,"Invalid event '%s' passed to "
			    "SendRFBEvent\n", params[0]);
		}
		return;
	}

	switch (ev->type) {
	case MotionNotify:
		while (XCheckTypedWindowEvent(dpy, desktopWin, MotionNotify, ev)) {
			;	/* discard all queued motion notify events */
		}

		SendPointerEvent(ev->xmotion.x, ev->xmotion.y,
			     (ev->xmotion.state & 0x1f00) >> 8);
		return;

	case ButtonPress:
		SendPointerEvent(ev->xbutton.x, ev->xbutton.y,
			     (((ev->xbutton.state & 0x1f00) >> 8) |
			      (1 << (ev->xbutton.button - 1))));
		return;

	case ButtonRelease:
		SendPointerEvent(ev->xbutton.x, ev->xbutton.y,
			     (((ev->xbutton.state & 0x1f00) >> 8) &
			      ~(1 << (ev->xbutton.button - 1))));
		return;

	case KeyPress:
	case KeyRelease:
		XLookupString(&ev->xkey, keyname, 256, &ks, NULL);

		if (IsModifierKey(ks)) {
			ks = XKeycodeToKeysym(dpy, ev->xkey.keycode, 0);
			modifierPressed[ev->xkey.keycode] = (ev->type == KeyPress);
		}

		SendKeyEvent(ks, (ev->type == KeyPress));
		return;

	default:
		fprintf(stderr,"Invalid event passed to SendRFBEvent\n");
	}
}


/*
 * CreateDotCursor.
 */

#ifndef very_small_dot_cursor
static Cursor
CreateDotCursor(int which)
{
	Cursor cursor;
	Pixmap src, msk;
	static char srcBits3[] = { 0x00, 0x02, 0x00 };
	static char mskBits3[] = { 0x02, 0x07, 0x02 };
	static char srcBits4[] = { 0x00, 0x06, 0x06, 0x00 };
	static char mskBits4[] = { 0x06, 0x0f, 0x0f, 0x06 };
	XColor fg, bg;

	if (which == 3) {
		src = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), srcBits3, 3, 3);
		msk = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), mskBits3, 3, 3);
	} else {
		src = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), srcBits4, 4, 4);
		msk = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), mskBits4, 4, 4);
	}
	XAllocNamedColor(dpy, DefaultColormap(dpy,DefaultScreen(dpy)), "black",
	    &fg, &fg);
	XAllocNamedColor(dpy, DefaultColormap(dpy,DefaultScreen(dpy)), "white",
	    &bg, &bg);
	cursor = XCreatePixmapCursor(dpy, src, msk, &fg, &bg, 1, 1);
	XFreePixmap(dpy, src);
	XFreePixmap(dpy, msk);

	return cursor;
}
#else
static Cursor
CreateDotCursor()
{
	Cursor cursor;
	Pixmap src, msk;
	static char srcBits[] = { 0, 14, 0 };
	static char mskBits[] = { 14,31,14 };
	XColor fg, bg;

	src = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), srcBits, 3, 3);
	msk = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), mskBits, 3, 3);
	XAllocNamedColor(dpy, DefaultColormap(dpy,DefaultScreen(dpy)), "black",
	    &fg, &fg);
	XAllocNamedColor(dpy, DefaultColormap(dpy,DefaultScreen(dpy)), "white",
	    &bg, &bg);
	cursor = XCreatePixmapCursor(dpy, src, msk, &fg, &bg, 1, 1);
	XFreePixmap(dpy, src);
	XFreePixmap(dpy, msk);

	return cursor;
}
#endif

int skip_maybe_sync = 0;
void maybe_sync(int width, int height) {
	static int singles = 0, always_skip = -1;
	int singles_max = 64;

	if (always_skip < 0) {
		if (getenv("SSVNC_NO_MAYBE_SYNC")) {
			always_skip = 1;
		} else {
			always_skip = 0;
		}
	}
	if (skip_maybe_sync || always_skip) {
		return;
	}
#if 0
	if (width > 1 || height > 1) {
		XSync(dpy, False);
		singles = 0;
	} else {
		if (++singles >= singles_max) {
			singles = 0;
			XSync(dpy, False);
		}
	}
#else
	if (width * height >= singles_max) {
		XSync(dpy, False);
		singles = 0;
	} else {
		singles += width * height;
		if (singles >= singles_max) {
			XSync(dpy, False);
			singles = 0;
		}
	}
#endif
}
/*
 * FillImage.
 */

void
FillScreen(int x, int y, int width, int height, unsigned long fill)
{
	XImage *im = image_scale ? image_scale : image;
	int bpp = im->bits_per_pixel;
	int Bpp = im->bits_per_pixel / 8;
	int Bpl = im->bytes_per_line;
	int h, widthInBytes = width * Bpp;
	static char *buf = NULL;
	static int buflen = 0;
	unsigned char  *ucp;
	unsigned short *usp;
	unsigned int   *uip;
	char *scr;
	int b0, b1, b2;

#if 0
fprintf(stderr, "FillImage bpp=%d %04dx%04d+%04d+%04d -- 0x%x\n", bpp, width, height, x, y, fill);
#endif
	if (appData.chatOnly) {
		return;
	}

	if (widthInBytes > buflen || !buf)  {
		if (buf) {
			free(buf);
		}
		buflen = widthInBytes * 2;
		buf = (char *)malloc(buflen); 
	}
	ucp = (unsigned char*) buf;
	usp = (unsigned short*) buf;
	uip = (unsigned int*) buf;

	if (isLSB) {
		b0 = 0; b1 = 1; b2 = 2;
	} else {
		b0 = 2; b1 = 1; b2 = 0;
	}

	for (h = 0; h < width; h++) {
		if (bpp == 8) {
			*(ucp+h) = (unsigned char)  fill;
		} else if (bpp == 16) {
			*(usp+h) = (unsigned short) fill;
		} else if (bpp == 24) {
			*(ucp + 3*h + b0) = (unsigned char) ((fill & 0x0000ff) >> 0);
			*(ucp + 3*h + b1) = (unsigned char) ((fill & 0x00ff00) >> 8);
			*(ucp + 3*h + b2) = (unsigned char) ((fill & 0xff0000) >> 16);
		} else if (bpp == 32) {
			*(uip+h) = (unsigned int)   fill;
		}
	}

	scr = im->data + y * Bpl + x * Bpp;

	for (h = 0; h < height; h++) {
		memcpy(scr, buf, widthInBytes);
		scr += Bpl;
	}
	put_image(x, y, x, y, width, height, 1);
	maybe_sync(width, height);
}

void copy_rect(int x, int y, int width, int height, int src_x, int src_y) {
	char *src, *dst;
	int i;
	XImage *im = image_scale ? image_scale : image;
	int Bpp = im->bits_per_pixel / 8;
	int Bpl = im->bytes_per_line;
	int did2 = 0;

#if 0
fprintf(stderr, "copy_rect: %04dx%04d+%04d+%04d -- %04d %04d Bpp=%d Bpl=%d\n", width, height, x, y, src_x, src_y, Bpp, Bpl);
#endif
	copyrect2:

	if (y < src_y) {
		src = im->data + src_y * Bpl + src_x * Bpp;
		dst = im->data +     y * Bpl +     x * Bpp;
		for (i = 0; i < height; i++)  {
			memmove(dst, src, Bpp * width); 
			src += Bpl;
			dst += Bpl;
		}
	} else {
		src = im->data + (src_y + height - 1) * Bpl + src_x * Bpp;
		dst = im->data + (y     + height - 1) * Bpl +     x * Bpp;
		for (i = 0; i < height; i++)  {
			memmove(dst, src, Bpp * width); 
			src -= Bpl;
			dst -= Bpl;
		}
	}

	if (image_scale && !did2) {
		im = image;
		Bpp = im->bits_per_pixel / 8;
		Bpl = im->bytes_per_line;

		x *= scale_factor_x;
		y *= scale_factor_y;
		src_x *= scale_factor_x;
		src_y *= scale_factor_y;
		width  = scale_round(width,  scale_factor_x);
		height = scale_round(height, scale_factor_y);

		did2 = 1;
		goto copyrect2;
	}
}


/*
 * CopyDataToScreen.
 */

void
CopyDataToScreen(char *buf, int x, int y, int width, int height)
{
	if (appData.chatOnly) {
		return;
	}
	if (appData.rawDelay != 0) {
		XFillRectangle(dpy, desktopWin, gc, x, y, width, height);
		XSync(dpy,False);
		usleep(appData.rawDelay * 1000);
	}

	if (appData.useBGR233) {
		CopyBGR233ToScreen((CARD8 *)buf, x, y, width, height);
	} else if (appData.useBGR565) {
		CopyBGR565ToScreen((CARD16 *)buf, x, y, width, height);
	} else {
		int h;
		int widthInBytes = width * myFormat.bitsPerPixel / 8;
		int scrWidthInBytes = si.framebufferWidth * myFormat.bitsPerPixel / 8;
		char *scr;
		XImage *im = image_scale ? image_scale : image;

		if (scrWidthInBytes != im->bytes_per_line) scrWidthInBytes = im->bytes_per_line;

		scr = (im->data + y * scrWidthInBytes
		    + x * myFormat.bitsPerPixel / 8);

		for (h = 0; h < height; h++) {
			memcpy(scr, buf, widthInBytes);
			buf += widthInBytes;
			scr += scrWidthInBytes;
		}
	}

	put_image(x, y, x, y, width, height, 0);
	maybe_sync(width, height);
}


/*
 * CopyBGR233ToScreen.
 */

static void
CopyBGR233ToScreen(CARD8 *buf, int x, int y, int width, int height)
{
	XImage *im = image_scale ? image_scale : image;
	int p, q;
	int xoff = 7 - (x & 7);
	int xcur;
	int fbwb = si.framebufferWidth / 8;
	int src_width8  = im->bytes_per_line/1;
	int src_width16 = im->bytes_per_line/2;
	int src_width32 = im->bytes_per_line/4;
	CARD8 *src1 = ((CARD8 *)im->data) + y * fbwb + x / 8;
	CARD8 *srct;
	CARD8  *src8  = ( (CARD8 *)im->data) + y * src_width8  + x;
	CARD16 *src16 = ((CARD16 *)im->data) + y * src_width16 + x;
	CARD32 *src32 = ((CARD32 *)im->data) + y * src_width32 + x;
	int b0, b1, b2;

	switch (visbpp) {

    /* thanks to Chris Hooper for single bpp support */

	case 1:
		for (q = 0; q < height; q++) {
			xcur = xoff;
			srct = src1;
			for (p = 0; p < width; p++) {
				*srct = ((*srct & ~(1 << xcur))
					 | (BGR233ToPixel[*(buf++)] << xcur));

				if (xcur-- == 0) {
					xcur = 7;
					srct++;
				}
			}
			src1 += fbwb;
		}
		break;

	case 8:
		for (q = 0; q < height; q++) {
			for (p = 0; p < width; p++) {
				*(src8++) = BGR233ToPixel[*(buf++)];
			}
			src8 += src_width8 - width;
		}
		break;

	case 16:
		for (q = 0; q < height; q++) {
			for (p = 0; p < width; p++) {
				*(src16++) = BGR233ToPixel[*(buf++)];
			}
			src16 += src_width16 - width;
		}
		break;

	case 24:
		if (isLSB) {
			b0 = 0; b1 = 1; b2 = 2;
		} else {
			b0 = 2; b1 = 1; b2 = 0;
		}
		src8  = ((CARD8 *)im->data) + (y * si.framebufferWidth + x) * 3;
		for (q = 0; q < height; q++) {
			for (p = 0; p < width; p++) {
				CARD32 v = BGR233ToPixel[*(buf++)];
				*(src8 + b0) = (unsigned char) ((v & 0x0000ff) >> 0);
				*(src8 + b1) = (unsigned char) ((v & 0x00ff00) >> 8);
				*(src8 + b2) = (unsigned char) ((v & 0xff0000) >> 16);
				src8 += 3;
			}
			src8 += (si.framebufferWidth - width) * 3;
		}
		break;

	case 32:
		for (q = 0; q < height; q++) {
			for (p = 0; p < width; p++) {
				*(src32++) = BGR233ToPixel[*(buf++)];
			}
			src32 += src_width32 - width;
		}
		break;
	}
}

static void
BGR565_24bpp(CARD16 *buf, int x, int y, int width, int height)
{
	int p, q;
	int b0, b1, b2;
	XImage *im = image_scale ? image_scale : image;
	unsigned char *src= (unsigned char *)im->data + (y * si.framebufferWidth + x) * 3;

	if (isLSB) {
		b0 = 0; b1 = 1; b2 = 2;
	} else {
		b0 = 2; b1 = 1; b2 = 0;
	}

	/*  case 24: */
	for (q = 0; q < height; q++) {
		for (p = 0; p < width; p++) {
			CARD32 v = BGR565ToPixel[*(buf++)];
			*(src + b0) = (unsigned char) ((v & 0x0000ff) >> 0);
			*(src + b1) = (unsigned char) ((v & 0x00ff00) >> 8);
			*(src + b2) = (unsigned char) ((v & 0xff0000) >> 16);
			src += 3;
		}
		src += (si.framebufferWidth - width) * 3;
	}
}

static void
CopyBGR565ToScreen(CARD16 *buf, int x, int y, int width, int height)
{
	int p, q;
	XImage *im = image_scale ? image_scale : image;
	int src_width32 = im->bytes_per_line/4;
	CARD32 *src32 = ((CARD32 *)im->data) + y * src_width32 + x;

	if (visbpp == 24) {
		BGR565_24bpp(buf, x, y, width, height);
		return;
	}

	/*  case 32: */
	for (q = 0; q < height; q++) {
		for (p = 0; p < width; p++) {
			*(src32++) = BGR565ToPixel[*(buf++)];
		}
		src32 += src_width32 - width;
	}
}

static void reset_image(void) {
	if (UsingShm()) {
		ShmDetach();
	}
	if (image && image->data) {
		XDestroyImage(image);
		fprintf(stderr, "reset_image: destroyed 'image'\n");
	}
	image = NULL;
	if (image_ycrop && image_ycrop->data) {
		XDestroyImage(image_ycrop);
		fprintf(stderr, "reset_image: destroyed 'image_ycrop'\n");
	}
	image_ycrop = NULL;
	if (image_scale && image_scale->data) {
		XDestroyImage(image_scale);
		fprintf(stderr, "reset_image: destroyed 'image_scale'\n");
	}
	image_scale = NULL;

	if (UsingShm()) {
		ShmCleanup();
	}
	create_image();
	XFlush(dpy);
}

void ReDoDesktop(void) {
	int w, w0, h, h0, x, y, dw, dh;
	int fs = 0;
	int autoscale = 0;
	Position x_orig, y_orig;
	Dimension w_orig, h_orig;

	if (!appData.fullScreen && appData.scale != NULL && !strcmp(appData.scale, "auto")) {
		autoscale = 1;
	}

	fprintf(stderr, "ReDoDesktop: ycrop: %d\n", appData.yCrop);

	XtVaGetValues(toplevel, XtNx, &x_orig, XtNy, &y_orig, NULL);
	XtVaGetValues(toplevel, XtNheight, &h_orig, XtNwidth, &w_orig, NULL);

	check_tall();

	if (appData.yCrop) {
		if (appData.yCrop < 0 || old_width <= 0) {
			appData.yCrop = guessCrop();
			fprintf(stderr, "Set -ycrop to: %d\n", appData.yCrop);
		} else {
			int w1 = si.framebufferWidth;
			appData.yCrop = (w1 * appData.yCrop) / old_width;
			if (appData.yCrop <= 100)  {
				appData.yCrop = guessCrop();
				fprintf(stderr, "Set small -ycrop to: %d\n", appData.yCrop);
			}
		}
		fprintf(stderr, "Using -ycrop: %d\n", appData.yCrop);
	}

	old_width  = si.framebufferWidth;
	old_height = si.framebufferHeight;

	if (appData.fullScreen) {
		if (prev_fb_width != si.framebufferWidth || prev_fb_height != si.framebufferHeight) {
			int xmax = si.framebufferWidth;
			int ymax = si.framebufferHeight;
			if (appData.yCrop > 0) {
				ymax = appData.yCrop;
			}
			if (scale_x > 0) {
				xmax = scale_round(xmax, scale_factor_x);
				ymax = scale_round(ymax, scale_factor_y);
			}
			if (xmax < dpyWidth || ymax < dpyHeight) {
				FullScreenOff();
				fs = 1;
			}
		}
	}

	prev_fb_width  = si.framebufferWidth;
	prev_fb_height = si.framebufferHeight;

	if (appData.fullScreen) {

		int xmax = si.framebufferWidth;
		int ymax = si.framebufferHeight;
		if (scale_x > 0) {
			xmax = scale_round(xmax, scale_factor_x);
			ymax = scale_round(ymax, scale_factor_y);
		}

		if (image && image->data) {
			int len;
			int h = image->height;
			int w = image->width;
			len = image->bytes_per_line * image->height;
			/* black out window first: */
			memset(image->data, 0, len);
  			XPutImage(dpy, XtWindow(desktop), gc, image, 0, 0, 0, 0, w, h);
			XFlush(dpy);
		}

		/* XXX scaling?? */
		XtResizeWidget(desktop, xmax, ymax, 0);

		XSync(dpy, False);
		usleep(100*1000);
		FullScreenOn();
		XSync(dpy, False);
		usleep(100*1000);
		reset_image();
		return;
	}

	dw = appData.wmDecorationWidth;
	dh = appData.wmDecorationHeight;

	w = si.framebufferWidth;
	h = si.framebufferHeight;
	w0 = w;
	h0 = h;
	if (appData.yCrop > 0) {
		h = appData.yCrop;
	}
	if (image_scale) {
		w = scale_round(w, scale_factor_x);
		h = scale_round(h, scale_factor_y);
		w0 = scale_round(w0, scale_factor_x);
		h0 = scale_round(h0, scale_factor_y);
	}

	if (w + dw >= dpyWidth) {
		w = dpyWidth - dw;
	}
	if (h + dh >= dpyHeight) {
		h = dpyHeight - dh;
	}

	if (!autoscale) {
		XtVaSetValues(toplevel, XtNmaxWidth, w, XtNmaxHeight, h, NULL);
	} else {
		XtVaSetValues(toplevel, XtNmaxWidth, dpyWidth, XtNmaxHeight, dpyHeight, NULL);
	}

	XtVaSetValues(desktop, XtNwidth, w0, XtNheight, h0, NULL);

	XtResizeWidget(desktop, w0, h0, 0);

	if (appData.yCrop > 0) {
		int ycrop = appData.yCrop;
		if (image_scale) {
			ycrop *= scale_factor_y;
		}
		XtVaSetValues(toplevel, XtNmaxHeight, ycrop, NULL);
		XtVaSetValues(form,     XtNmaxHeight, ycrop, NULL);
	}

	x = (dpyWidth  - w - dw)/2;
	y = (dpyHeight - h - dh)/2;

	if (!autoscale) {

		if (!getenv("VNCVIEWER_ALWAYS_RECENTER")) {
			int x_cm_old, y_cm_old;
			int x_cm_new, y_cm_new;
			int x_try, y_try;

			x_cm_old = (int) x_orig + ((int) w_orig)/2;
			y_cm_old = (int) y_orig + ((int) h_orig)/2;

			x_cm_new = dpyWidth/2;
			y_cm_new = dpyHeight/2;

			x_try = x + (x_cm_old - x_cm_new);
			y_try = y + (y_cm_old - y_cm_new);
			if (x_try < 0) {
				x_try = 0;
			}
			if (y_try < 0) {
				y_try = 0;
			}
			if (x_try + w + dw > dpyWidth) {
				x_try = dpyWidth - w - dw;
			}
			if (y_try + h + dh > dpyHeight) {
				y_try = dpyHeight - h - dh;
			}
			x = x_try;
			y = y_try;
		}

		XtConfigureWidget(toplevel, x + dw, y + dh, w, h, 0);
	}

	reset_image();

	if (fs) {
		FullScreenOn();
	}
}
