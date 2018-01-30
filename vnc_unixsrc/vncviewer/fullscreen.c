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
 * fullscreen.c - functions to deal with full-screen mode.
 */

#include <vncviewer.h>
#include <time.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Toggle.h>

static Bool DoBumpScroll();
static Bool DoJumpScroll();
static void BumpScrollTimerCallback(XtPointer clientData, XtIntervalId *id);
static void JumpScrollTimerCallback(XtPointer clientData, XtIntervalId *id);
static XtIntervalId timer;
static Bool timerSet = False;
static Bool scrollLeft, scrollRight, scrollUp, scrollDown;
Position desktopX, desktopY;
static Dimension viewportWidth, viewportHeight;
static Dimension scrollbarWidth, scrollbarHeight;

int scale_round(int len, double fac);

/*
 * FullScreenOn goes into full-screen mode.  It makes the toplevel window
 * unmanaged by the window manager and sets its geometry appropriately.
 *
 * We have toplevel -> form -> viewport -> desktop.  "form" must always be the
 * same size as "toplevel".  "desktop" should always be fixed at the size of
 * the VNC desktop.  Normally "viewport" is the same size as "toplevel" (<=
 * size of "desktop"), and "viewport" deals with any difference by putting up
 * scrollbars.
 *
 * When we go into full-screen mode, we allow "viewport" and "form" to be
 * different sizes, and we effectively need to work out all the geometries
 * ourselves.  There are two cases to deal with:
 *
 * 1. When the desktop is smaller than the display, "viewport" is simply the
 *    size of the desktop and "toplevel" (and "form") are the size of the
 *    display.  "form" is visible around the edges of the desktop.
 *
 * 2. When the desktop is bigger than the display in either or both dimensions,
 *    we force "viewport" to have scrollbars.
 *
 *    If the desktop width is bigger than the display width, then the width of
 *    "viewport" is the display width plus the scrollbar width, otherwise it's
 *    the desktop width plus the scrollbar width.  The width of "toplevel" (and
 *    "form") is then either the same as "viewport", or just the display width,
 *    respectively.  Similarly for the height of "viewport" and the height of
 *    "toplevel".
 *
 *    So if the desktop is bigger than the display in both dimensions then both
 *    the scrollbars will be just off the screen.  If it's bigger in only one
 *    dimension then that scrollbar _will_ be visible, with the other one just
 *    off the screen.  We treat this as a "feature" rather than a problem - you
 *    can't easily get around it if you want to use the Athena viewport for
 *    doing the scrolling.
 *
 * In either case, we position "viewport" in the middle of "form".
 *
 * We store the calculated size of "viewport" and the scrollbars in global
 * variables so that FullScreenOff can use them.
 */

int net_wm_supported(void) {
	unsigned char *data;
	unsigned long items_read, items_left, i;
	int ret, format; 
	Window wm;
	Atom type;
	Atom _NET_SUPPORTING_WM_CHECK;
	Atom _NET_SUPPORTED;
	Atom _NET_WM_STATE;
	Atom _NET_WM_STATE_FULLSCREEN;

	static time_t last_check = 0;
	static int fs_supported = -1;

	if (fs_supported >= 0 && time(NULL) < last_check + 600) {
		static int first = 1;
		if (first) {
			fprintf(stderr, "fs_supported: %d\n", fs_supported);
		}
		first = 0;
		return fs_supported;
	}
	last_check = time(NULL);

	fs_supported = 0;

	_NET_SUPPORTING_WM_CHECK = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	_NET_SUPPORTED = XInternAtom(dpy, "_NET_SUPPORTED", False);
	_NET_WM_STATE = XInternAtom(dpy, "_NET_WM_STATE", False);
	_NET_WM_STATE_FULLSCREEN = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

	ret = XGetWindowProperty(dpy, DefaultRootWindow(dpy), _NET_SUPPORTING_WM_CHECK,
	    0L, 1L, False, XA_WINDOW, &type, &format, &items_read, &items_left, &data); 

	if (ret != Success || !items_read) {
		if (ret == Success) {
			XFree(data);
		}
		return fs_supported;
	}

	wm = ((Window*) data)[0];
	XFree(data);

	ret = XGetWindowProperty(dpy, wm, _NET_SUPPORTING_WM_CHECK,
	    0L, 1L, False, XA_WINDOW, &type, &format, &items_read, &items_left, &data); 

	if (ret != Success || !items_read) {
		if (ret == Success) {
			XFree(data);
		}
		return fs_supported;
	}

	if (wm != ((Window*) data)[0]) {
		XFree(data);
		return fs_supported;
	}

	ret = XGetWindowProperty(dpy, DefaultRootWindow(dpy), _NET_SUPPORTED,
	    0L, 8192L, False, XA_ATOM, &type, &format, &items_read, &items_left, &data); 

	if (ret != Success || !items_read) {
		if (ret == Success) {
			XFree(data);
		}
		return fs_supported;
	}

	for (i=0; i < items_read; i++) {
		if ( ((Atom*) data)[i] == _NET_WM_STATE_FULLSCREEN) {
			fs_supported = 1;
		}
	}
	XFree(data);

	return fs_supported;
}

static void net_wm_fullscreen(int to_fs) {
	
	int _NET_WM_STATE_REMOVE = 0;
	int _NET_WM_STATE_ADD = 1;
#if 0
	int _NET_WM_STATE_TOGGLE = 2;
#endif
	Atom _NET_WM_STATE = XInternAtom(dpy, "_NET_WM_STATE", False);
	Atom _NET_WM_STATE_FULLSCREEN = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	XEvent xev;	

	if (to_fs == 2) {
		XChangeProperty(dpy, XtWindow(toplevel), _NET_WM_STATE, XA_ATOM, 32, PropModeReplace, (unsigned char*)&_NET_WM_STATE_FULLSCREEN, 1);
	} else {
		xev.xclient.type = ClientMessage;
		xev.xclient.window = XtWindow(toplevel);
		xev.xclient.message_type = _NET_WM_STATE;
		xev.xclient.serial = 0;
		xev.xclient.display = dpy;
		xev.xclient.send_event = True;
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = to_fs ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
		xev.xclient.data.l[1] = _NET_WM_STATE_FULLSCREEN;
		xev.xclient.data.l[2] = 0;
		xev.xclient.data.l[3] = 0;
		xev.xclient.data.l[4] = 0;
		XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
	}

	XSync(dpy, False);
}

time_t main_grab = 0;

#ifndef XTGRABKBD
#define XTGRABKBD 0
#endif
static int xtgrabkbd = XTGRABKBD;

void fs_ungrab(int check) {
	if (check) {
		if (time(NULL) <= main_grab + 2) {
			return;
		}
		if (net_wm_supported()) {
			return;
		}
	}
	fprintf(stderr, "calling fs_ungrab()\n");
	if (appData.grabAll) { /* runge top of FullScreenOff */
		fprintf(stderr, "calling XUngrabServer(dpy)\n");
		XUngrabServer(dpy);     
	}
	if (appData.grabKeyboard) {
		static int first = 1;
		if (first) {
			if (getenv("SSVNC_XTGRAB")) {
				xtgrabkbd = 1;
			} else if (getenv("SSVNC_NOXTGRAB")) {
				xtgrabkbd = 0;
			}
		}
		first = 0;
		if (xtgrabkbd) {
			fprintf(stderr, "calling XtUngrabKeyboard(dpy)\n");
			XtUngrabKeyboard(desktop, CurrentTime);
		} else {
			fprintf(stderr, "calling XUngrabKeyboard(dpy)\n");
			XUngrabKeyboard(dpy, CurrentTime);
		}
	}
}

void fs_grab(int check) {

	if (check) {
		if (time(NULL) <= main_grab + 2) {
			return;
		}
		if (net_wm_supported()) {
			return;
		}
	}

	main_grab = time(NULL);

	fprintf(stderr, "calling fs_grab()\n");
	
#define FORCE_UP \
	XSync(dpy, False); \
	XUnmapWindow(dpy, XtWindow(toplevel)); \
	XSync(dpy, False); \
	XMapWindow(dpy, XtWindow(toplevel)); \
	XRaiseWindow(dpy, XtWindow(toplevel)); \
	XSync(dpy, False);

	if (appData.grabKeyboard) {
		int gres = GrabSuccess;
		static int first = 1;
		if (first) {
			if (getenv("SSVNC_XTGRAB")) {
				xtgrabkbd = 1;
			} else if (getenv("SSVNC_NOXTGRAB")) {
				xtgrabkbd = 0;
			}
		}
		first = 0;
		
		if (xtgrabkbd) {
			fprintf(stderr, "calling XtGrabKeyboard().\n");
			gres = XtGrabKeyboard(desktop, True, GrabModeAsync, GrabModeAsync, CurrentTime);
			if (gres != GrabSuccess) fprintf(stderr, "XtGrabKeyboard() failed.\n");
		} else {
			fprintf(stderr, "calling XGrabKeyboard().\n");
			gres = XGrabKeyboard(dpy, XtWindow(toplevel), True, GrabModeAsync, GrabModeAsync, CurrentTime);
			if (gres != GrabSuccess) fprintf(stderr, "XGrabKeyboard() failed.\n");
		}

		if (gres != GrabSuccess) {
			XSync(dpy, False);
			usleep(100 * 1000);
			FORCE_UP

			if (xtgrabkbd) {
				fprintf(stderr, "calling XtGrabKeyboard() again.\n");
				gres = XtGrabKeyboard(desktop, True, GrabModeAsync, GrabModeAsync, CurrentTime);
				if (gres != GrabSuccess) fprintf(stderr, "XtGrabKeyboard() failed again.\n");
			} else {
				fprintf(stderr, "calling XGrabKeyboard() again.\n");
				gres = XGrabKeyboard(dpy, XtWindow(toplevel), True, GrabModeAsync, GrabModeAsync, CurrentTime);
				if (gres != GrabSuccess) fprintf(stderr, "XGrabKeyboard() failed again.\n");
			}

			if (gres != GrabSuccess) {
				usleep(200 * 1000);
				XSync(dpy, False);
				if (xtgrabkbd) {
					fprintf(stderr, "calling XtGrabKeyboard() 3rd time.\n");
					gres = XtGrabKeyboard(desktop, True, GrabModeAsync, GrabModeAsync, CurrentTime);
					if (gres != GrabSuccess) fprintf(stderr, "XtGrabKeyboard() failed 3rd time.\n");
				} else {
					fprintf(stderr, "calling XGrabKeyboard() 3rd time.\n");
					gres = XGrabKeyboard(dpy, XtWindow(toplevel), True, GrabModeAsync, GrabModeAsync, CurrentTime);
					if (gres != GrabSuccess) fprintf(stderr, "XGrabKeyboard() failed 3rd time.\n");
				}

				if (gres == GrabSuccess) {
					if (xtgrabkbd) {
						fprintf(stderr, "XtGrabKeyboard() OK 3rd try.\n");
					} else {
						fprintf(stderr, "XGrabKeyboard() OK 3rd try.\n");
					}
				}
			} else {
				if (xtgrabkbd) {
					fprintf(stderr, "XtGrabKeyboard() OK 2nd try.\n");
				} else {
					fprintf(stderr, "XGrabKeyboard() OK 2nd try.\n");
				}
			}
			XRaiseWindow(dpy, XtWindow(toplevel));
		} 
	}

	if (appData.grabAll) {
		fprintf(stderr, "calling XGrabServer(dpy)\n");
		if (! XGrabServer(dpy)) {
			XSync(dpy, False);
			usleep(100 * 1000);
			fprintf(stderr, "calling XGrabServer(dpy) 2nd time\n");
			if (!XGrabServer(dpy)) {
				XSync(dpy, False);
				usleep(200 * 1000);
				fprintf(stderr, "calling XGrabServer(dpy) 3rd time\n");
				if (XGrabServer(dpy)) {
					fprintf(stderr, "XGrabServer(dpy) OK 3rd time\n");
				}
			} else {
				fprintf(stderr, "XGrabServer(dpy) OK 2nd time\n");
			}
			XSync(dpy, False);
		}
		if (getenv("VNCVIEWER_FORCE_UP")) {
			fprintf(stderr, "FORCE_UP\n");
			FORCE_UP
		}
	}
}

extern int fullscreen_startup;
extern double last_fullscreen;

#define set_size_hints() \
{ \
	long supplied; \
	XSizeHints *sizehints = XAllocSizeHints(); \
	XGetWMSizeHints(dpy, topwin, sizehints, &supplied, XA_WM_NORMAL_HINTS); \
	if (sizehints->base_width < toplevelWidth) { \
		sizehints->base_width = toplevelWidth; \
	} \
	if (sizehints->base_height < toplevelHeight) { \
		sizehints->base_height = toplevelHeight; \
	} \
	if (sizehints->max_width < toplevelWidth) { \
		sizehints->max_width = toplevelWidth; \
	} \
	if (sizehints->max_height < toplevelHeight) { \
		sizehints->max_height = toplevelHeight; \
	} \
	XSetWMSizeHints(dpy, topwin, sizehints, XA_WM_NORMAL_HINTS); \
	XFree(sizehints); \
}

extern int scale_x, scale_y;
extern double scale_factor_y;

void
FullScreenOn()
{
	Dimension toplevelWidth, toplevelHeight;
	Dimension oldViewportWidth, oldViewportHeight, clipWidth, clipHeight;
	Position viewportX, viewportY;
	int do_net_wm = net_wm_supported();
	int fbW = si.framebufferWidth;
	int fbH = si.framebufferHeight;
	int eff_height;

	Bool fsAlready = appData.fullScreen, toobig = False;
	Window topwin = XtWindow(toplevel);

	appData.fullScreen = True;

	last_fullscreen = dnow();

	if (scale_x > 0) {
		fbW = scale_x;
		fbH = scale_y;
	}

	eff_height = fbH;
	if (appData.yCrop > 0) {
		eff_height = appData.yCrop;
		if (scale_y > 0) {
			eff_height = scale_round(eff_height, scale_factor_y);
		}
	}

	if (fbW > dpyWidth || eff_height > dpyHeight) {

		toobig = True;

		/*
		 * This is a crazy thing to have the scrollbars hang
		 * just a bit offscreen to the right and below.  the user
		 * will not see them and bumpscroll will work.
		 */
	
		XtVaSetValues(viewport, XtNforceBars, True, NULL);
		XtVaGetValues(viewport, XtNwidth, &oldViewportWidth, XtNheight, &oldViewportHeight, NULL);
		XtVaGetValues(XtNameToWidget(viewport, "clip"), XtNwidth, &clipWidth, XtNheight, &clipHeight, NULL);

		scrollbarWidth  = oldViewportWidth  - clipWidth;
		scrollbarHeight = oldViewportHeight - clipHeight;

		if (fbW > dpyWidth) {
			viewportWidth = toplevelWidth = dpyWidth + scrollbarWidth;
		} else {
			viewportWidth = fbW + scrollbarWidth;
			toplevelWidth = dpyWidth;
		}

		if (eff_height > dpyHeight) {
			viewportHeight = toplevelHeight = dpyHeight + scrollbarHeight;
		} else {
			viewportHeight = eff_height + scrollbarHeight;
			toplevelHeight = dpyHeight;
		}
		if (do_net_wm) {
			/* but for _NET_WM we make toplevel be correct dpy size */
			toplevelWidth  = dpyWidth;
			toplevelHeight = dpyHeight;
		}

	} else {
		viewportWidth = fbW;
		viewportHeight = eff_height;
		toplevelWidth  = dpyWidth;
		toplevelHeight = dpyHeight;
	}

	viewportX = (toplevelWidth - viewportWidth) / 2;
	viewportY = (toplevelHeight - viewportHeight) / 2;

	if (viewportX < 0) viewportX = 0;
	if (viewportY < 0) viewportY = 0;


  /* We want to stop the window manager from managing our toplevel window.
     This is not really a nice thing to do, so may not work properly with every
     window manager.  We do this simply by setting overrideRedirect and
     reparenting our window to the root.  The window manager will get a
     ReparentNotify and hopefully clean up its frame window. */

	if (! fsAlready) {
		if (!do_net_wm) {
			/* added to try to raise it on top for some cirumstances */
			XUnmapWindow(dpy, topwin);

			XtVaSetValues(toplevel, XtNoverrideRedirect, True, NULL);
#if 0
			XtVaSetValues(viewport, XtNoverrideRedirect, True, NULL);
			XtVaSetValues(desktop,  XtNoverrideRedirect, True, NULL);
#endif
			XtVaSetValues(popup,    XtNoverrideRedirect, True, NULL);

			XReparentWindow(dpy, topwin, DefaultRootWindow(dpy), 0, 0);

		  /* Some WMs does not obey x,y values of XReparentWindow; the window
		     is not placed in the upper, left corner. The code below fixes
		     this: It manually moves the window, after the Xserver is done
		     with XReparentWindow. The last XSync seems to prevent losing
		     focus, but I don't know why. */

			XSync(dpy, False);

			/* added to try to raise it on top for some cirumstances */
			XMapRaised(dpy, topwin);

			XMoveWindow(dpy, topwin, 0, 0);
			XSync(dpy, False);
		}

		  /* Now we want to fix the size of "viewport".  We shouldn't just change it
		     directly.  Instead we set "toplevel" to the required size (which should
		     propagate through "form" to "viewport").  Then we remove "viewport" from
		     being managed by "form", change its resources to position it and make sure
		     that "form" won't attempt to resize it, then ask "form" to manage it
		     again. */

		XtResizeWidget(toplevel, viewportWidth, viewportHeight, 0);

		XtUnmanageChild(viewport);

		XtVaSetValues(viewport,
			XtNhorizDistance, viewportX,
			XtNvertDistance, viewportY,
			XtNleft, XtChainLeft,
			XtNright, XtChainLeft,
			XtNtop, XtChainTop,
			XtNbottom, XtChainTop,
			NULL);

		XtManageChild(viewport);
		XSync(dpy, False);
	} else {
		XSync(dpy, False);
	}

	/* Now we can set "toplevel" to its proper size. */

#if 0
	XtVaSetValues(toplevel, XtNwidth, toplevelWidth, XtNheight, toplevelHeight, NULL);
	XtResizeWidget(toplevel, toplevelWidth, toplevelHeight, 0);
#endif
	XResizeWindow(dpy, topwin, toplevelWidth, toplevelHeight);

	if (do_net_wm) {
		XWindowAttributes attr;
		int ok = 0, i, delay = 20;

		usleep(delay * 1000);

#define GSIZE() \
	XGetWindowAttributes(dpy, topwin, &attr);

#define PSIZE(s) \
	XSync(dpy, False); \
	XGetWindowAttributes(dpy, topwin, &attr); \
	fprintf(stderr, "%s %dx%d+%d+%d\n", s, attr.width, attr.height, attr.x, attr.y);

		PSIZE("size-A:");

		set_size_hints();

		net_wm_fullscreen(1);

		PSIZE("size-B:");

		for (i=0; i < 30; i++) {
			usleep(delay * 1000);
			GSIZE();
			fprintf(stderr, "size[%d] %dx%d+%d+%d\n", i, attr.width, attr.height, attr.x, attr.y);
			if (attr.width == toplevelWidth && attr.height == toplevelHeight) {
				ok = 1;
				fprintf(stderr, "size ok.\n");
				XSync(dpy, False);
				break;
			}
			set_size_hints();
			XResizeWindow(dpy, topwin, toplevelWidth, toplevelHeight);
			XMoveWindow(dpy, topwin, 0, 0);
			XSync(dpy, False);
		}

		PSIZE("size-C:");
	}

	fprintf(stderr, "\ntoplevel: %dx%d viewport: %dx%d\n", toplevelWidth, toplevelHeight, viewportWidth, viewportHeight);

#if defined (__SVR4) && defined (__sun)
	if (!do_net_wm) {
		/* CDE */
		XSync(dpy, False);
		usleep(200 * 1000);
		XMoveWindow(dpy, topwin, 0, 0);
		XMapRaised(dpy, topwin);
		XSync(dpy, False);
	}
#endif

	if (fsAlready) {
		XtResizeWidget(viewport, viewportWidth, viewportHeight, 0);
		if (! toobig) {
			XtVaSetValues(viewport, XtNforceBars, False, NULL);
		}
		XMoveWindow(dpy, topwin, viewportX, viewportY);
		XSync(dpy, False);
	}

	/* Try to get the input focus. */

	/* original vnc: DefaultRootWindow(dpy) instead of PointerRoot */
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);

	/* Optionally, grab the keyboard. */
	fs_grab(0);

	/* finally done. */
}


/*
 * FullScreenOff leaves full-screen mode.  It makes the toplevel window
 * managed by the window manager and sets its geometry appropriately.
 *
 * We also want to reestablish the link between the geometry of "form" and
 * "viewport".  We do this similarly to the way we broke it in FullScreenOn, by
 * making "viewport" unmanaged, changing certain resources on it and asking
 * "form" to manage it again.
 *
 * There seems to be a slightly strange behaviour with setting forceBars back
 * to false, which results in "desktop" being stretched by the size of the
 * scrollbars under certain circumstances.  Resizing both "toplevel" and
 * "viewport" to the full-screen viewport size minus the scrollbar size seems
 * to fix it, though I'm not entirely sure why. */

void
FullScreenOff()
{
	int toplevelWidth, toplevelHeight;
	int do_net_wm = net_wm_supported();
	int fbW = si.framebufferWidth;
	int fbH = si.framebufferHeight;
	int eff_height;

	appData.fullScreen = False;

	last_fullscreen = dnow();

	if (scale_x > 0) {
		fbW = scale_x;
		fbH = scale_y;
	}

	eff_height = fbH;
	if (appData.yCrop > 0) {
		eff_height = appData.yCrop;
		if (scale_y > 0) {
			eff_height = scale_round(eff_height, scale_factor_y);
		}
	}

	toplevelWidth = fbW;
	toplevelHeight = eff_height;

	fs_ungrab(0);

	if (do_net_wm) {
		net_wm_fullscreen(0);
	} else {
		XtUnmapWidget(toplevel);
	}

	XtResizeWidget(toplevel,
		 viewportWidth - scrollbarWidth,
		 viewportHeight - scrollbarHeight, 0);
	XtResizeWidget(viewport,
		 viewportWidth - scrollbarWidth,
		 viewportHeight - scrollbarHeight, 0);

	XtVaSetValues(viewport, XtNforceBars, False, NULL);

	XtUnmanageChild(viewport);

	XtVaSetValues(viewport,
		XtNhorizDistance, 0,
		XtNvertDistance, 0,
		XtNleft, XtChainLeft,
		XtNright, XtChainRight,
		XtNtop, XtChainTop,
		XtNbottom, XtChainBottom,
		NULL);

	XtManageChild(viewport);

	if (!do_net_wm) {
		XtVaSetValues(toplevel, XtNoverrideRedirect, False, NULL);
#if 0
		XtVaSetValues(viewport, XtNoverrideRedirect, False, NULL);
		XtVaSetValues(desktop,  XtNoverrideRedirect, False, NULL);
#endif
		XtVaSetValues(popup,    XtNoverrideRedirect, False, NULL);
	}

	if ((toplevelWidth + appData.wmDecorationWidth) >= dpyWidth)
		toplevelWidth = dpyWidth - appData.wmDecorationWidth;

	if ((toplevelHeight + appData.wmDecorationHeight) >= dpyHeight)
		toplevelHeight = dpyHeight - appData.wmDecorationHeight;

	XtResizeWidget(toplevel, toplevelWidth, toplevelHeight, 0);

	if (!do_net_wm) {
		XtMapWidget(toplevel);
	}
	XSync(dpy, False);

	/* Set the popup back to non-overrideRedirect */

	XtVaSetValues(popup, XtNoverrideRedirect, False, NULL);

	if (!do_net_wm) {
		int x = (dpyWidth  - toplevelWidth)  / 2;
		int y = (dpyHeight - toplevelHeight) / 2;
		if (x > 0 && y > 0) {
			XSync(dpy, False);
			XMoveWindow(dpy, XtWindow(toplevel), x, y);
		}
	}
}


/*
 * SetFullScreenState is an action which sets the "state" resource of a toggle
 * widget to reflect whether we're in full-screen mode.
 */

void
SetFullScreenState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.fullScreen) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}


/*
 * ToggleFullScreen is an action which toggles in and out of full-screen mode.
 */

void
ToggleFullScreen(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.fullScreen) {
		FullScreenOff();
	} else {
		FullScreenOn();
	}
	if (w || ev || params || num_params) {}
}


/*
 * BumpScroll is called when in full-screen mode and the mouse is against one
 * of the edges of the screen.  It returns true if any scrolling was done.
 */

Bool
BumpScroll(XEvent *ev)
{
	scrollLeft = scrollRight = scrollUp = scrollDown = False;

	if (ev->xmotion.x_root >= dpyWidth - 3)
		scrollRight = True;
	else if (ev->xmotion.x_root <= 2)
		scrollLeft = True;

	if (ev->xmotion.y_root >= dpyHeight - 3)
		scrollDown = True;
	else if (ev->xmotion.y_root <= 2)
		scrollUp = True;

	if (scrollLeft || scrollRight || scrollUp || scrollDown) {
		if (timerSet)
			return True;

		XtVaGetValues(desktop, XtNx, &desktopX, XtNy, &desktopY, NULL);
		desktopX = -desktopX;
		desktopY = -desktopY;

		return DoBumpScroll();
	}

	if (timerSet) {
		XtRemoveTimeOut(timer);
		timerSet = False;
	}

	return False;
}

static Bool
DoBumpScroll()
{
	int oldx = desktopX, oldy = desktopY;
	int fbW = si.framebufferWidth;
	int fbH = si.framebufferHeight;

	if (scale_x > 0) {
		fbW = scale_x;
		fbH = scale_y;
	}

	if (scrollRight) {
		if (desktopX < fbW - dpyWidth) {
			desktopX += appData.bumpScrollPixels;
			if (desktopX > fbW - dpyWidth) {
				desktopX = fbW - dpyWidth;
			}
		}
	} else if (scrollLeft) {
		if (desktopX > 0) {
			desktopX -= appData.bumpScrollPixels;
			if (desktopX < 0) {
				desktopX = 0;
			}
		}
	}

	if (scrollDown) {
		int ycrop = appData.yCrop;
		if (scale_y > 0)  {
			ycrop = scale_round(ycrop, scale_factor_y);
		}
		if (ycrop > 0 && desktopY + dpyHeight >= ycrop) {
			;
		} else if (desktopY < fbH - dpyHeight) {
			desktopY += appData.bumpScrollPixels;
			if (desktopY > fbH - dpyHeight) {
				desktopY = fbH - dpyHeight;
			}
		}
	} else if (scrollUp) {
		if (desktopY > 0) {
			desktopY -= appData.bumpScrollPixels;
			if (desktopY < 0) {
				desktopY = 0;
			}
		}
	}

	if (oldx != desktopX || oldy != desktopY) {
		XawViewportSetCoordinates(viewport, desktopX, desktopY);
		timer = XtAppAddTimeOut(appContext, appData.bumpScrollTime, BumpScrollTimerCallback, NULL);
		timerSet = True;
		return True;
	}

	timerSet = False;
	return False;
}

static void
BumpScrollTimerCallback(XtPointer clientData, XtIntervalId *id)
{
	DoBumpScroll();
	if (clientData || id) {}
}

/* not working: */

Bool
JumpScroll(int up, int vert) {
	scrollLeft = scrollRight = scrollUp = scrollDown = False;

	
	if (appData.fullScreen) {
		return True;
	}
	fprintf(stderr, "JumpScroll(%d, %d)\n", up, vert);

	if (vert) {
		if (up) {
			scrollUp = True;
		} else {
			scrollDown = True;
		}
	} else {
		if (up) {
			scrollRight = True;
		} else {
			scrollLeft = True;
		}
	}

	if (scrollLeft || scrollRight || scrollUp || scrollDown) {
		if (timerSet) {
			return True;
		}

		XtVaGetValues(desktop, XtNx, &desktopX, XtNy, &desktopY, NULL);
		desktopX = -desktopX;
		desktopY = -desktopY;
		return DoJumpScroll();
	}

	if (timerSet) {
		XtRemoveTimeOut(timer);
		timerSet = False;
	}

	return False;
}

static Bool
DoJumpScroll() {
	int oldx = desktopX, oldy = desktopY;
	int jumpH, jumpV; 
	int fbW = si.framebufferWidth;
	int fbH = si.framebufferHeight;

	if (scale_x > 0) {
		fbW = scale_x;
		fbH = scale_y;
	}
	jumpH = fbW / 4;
	jumpV = fbH / 4;

	if (scrollRight) {
		if (desktopX < fbW - dpyWidth) {
			desktopX += jumpH;
			if (desktopX > fbW - dpyWidth)
				desktopX = fbW - dpyWidth;
		}
	} else if (scrollLeft) {
		if (desktopX > 0) {
			desktopX -= jumpH;
			if (desktopX < 0)
				desktopX = 0;
		}
	}

	if (scrollDown) {
		if (appData.yCrop > 0 && desktopY + dpyHeight >= appData.yCrop) {
			;
		} else if (desktopY < fbH - dpyHeight) {
			desktopY += jumpV;
			if (desktopY > fbH - dpyHeight)
				desktopY = fbH - dpyHeight;
		}
	} else if (scrollUp) {
		if (desktopY > 0) {
			desktopY -= jumpV;
			if (desktopY < 0)
				desktopY = 0;
		}
	}

	if (oldx != desktopX || oldy != desktopY) {
		XawViewportSetCoordinates(viewport, desktopX, desktopY);
		timer = XtAppAddTimeOut(appContext, appData.bumpScrollTime,
				    JumpScrollTimerCallback, NULL);
		timerSet = True;
		return True;
	}

	timerSet = False;
	return False;
}

static void
JumpScrollTimerCallback(XtPointer clientData, XtIntervalId *id) {
	DoJumpScroll();
	if (clientData || id) {}
}
void JumpRight(Widget w, XEvent *ev, String *params, Cardinal *num_params) {
	JumpScroll(1, 0); 
	if (w || ev || params || num_params) {}
}
void JumpLeft(Widget w, XEvent *ev, String *params, Cardinal *num_params) {
	JumpScroll(0, 0); 
	if (w || ev || params || num_params) {}
}
void JumpUp(Widget w, XEvent *ev, String *params, Cardinal *num_params) {
	JumpScroll(1, 1); 
	if (w || ev || params || num_params) {}
}
void JumpDown(Widget w, XEvent *ev, String *params, Cardinal *num_params) {
	JumpScroll(0, 1); 
	if (w || ev || params || num_params) {}
}


