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
 * misc.c - miscellaneous functions.
 */

#include <vncviewer.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

static void CleanupSignalHandler(int sig);
static int CleanupXErrorHandler(Display *dpy, XErrorEvent *error);
static int CleanupXIOErrorHandler(Display *dpy);
static void CleanupXtErrorHandler(String message);
static Bool IconifyNamedWindow(Window w, char *name, Bool undo);

Dimension dpyWidth, dpyHeight;
Atom wmDeleteWindow, wmState;
int fullscreen_startup = 0;

static Bool xloginIconified = False;
static XErrorHandler defaultXErrorHandler;
static XIOErrorHandler defaultXIOErrorHandler;
static XtErrorHandler defaultXtErrorHandler;

int XError_ign = 0;

void check_tall(void);
int guessCrop(void);
void get_scale_values(double *fx, double *fy);
int scale_round(int n, double factor);
Bool SendTextChatFinished(void);

/*
 * ToplevelInitBeforeRealization sets the title, geometry and other resources
 * on the toplevel window.
 */

void
ToplevelInitBeforeRealization()
{
	char *titleFormat;
	char *title;
	char *geometry;
	int h = si.framebufferHeight;
	int w = si.framebufferWidth;

	check_tall();
	if (appData.yCrop < 0) {
		appData.yCrop = guessCrop();
		fprintf(stderr, "Set -ycrop to: %d\n", appData.yCrop);
		if (appData.yCrop > 0) {
			h = appData.yCrop;
		}
	}
	
	XtVaGetValues(toplevel, XtNtitle, &titleFormat, NULL);
	title = XtMalloc(strlen(titleFormat) + strlen(desktopName) + 1);
	sprintf(title, titleFormat, desktopName);
	XtVaSetValues(toplevel, XtNtitle, title, XtNiconName, title, NULL);

	if (appData.scale != NULL) {
		/* switched to not scaled */
		double frac_x, frac_y;
		get_scale_values(&frac_x, &frac_y);
		if (frac_x > 0.0 && frac_y > 0.0) {
			w = scale_round(w, frac_x);
			h = scale_round(h, frac_y);
		}
	}
	XtVaSetValues(toplevel, XtNmaxWidth, w, XtNmaxHeight, h, NULL);

	dpyWidth  = WidthOfScreen(DefaultScreenOfDisplay(dpy));
	dpyHeight = HeightOfScreen(DefaultScreenOfDisplay(dpy));

	if (appData.fullScreen) {
		/* full screen - set position to 0,0, but defer size calculation until widgets are realized */

		if (!net_wm_supported()) {
			XtVaSetValues(toplevel, XtNoverrideRedirect, True, XtNgeometry, "+0+0", NULL);
		} else {
			fullscreen_startup = 1;
		}

	} else {

		/* not full screen - work out geometry for middle of screen unless specified by user */

		XtVaGetValues(toplevel, XtNgeometry, &geometry, NULL);

		if (geometry == NULL) {
			Dimension toplevelX, toplevelY;
			Dimension toplevelWidth  = w;
			Dimension toplevelHeight = h;

			if ((toplevelWidth + appData.wmDecorationWidth) >= dpyWidth) {
				toplevelWidth = dpyWidth - appData.wmDecorationWidth;
			}

			if ((toplevelHeight + appData.wmDecorationHeight) >= dpyHeight) {
				toplevelHeight = dpyHeight - appData.wmDecorationHeight;
			}

			toplevelX = (dpyWidth  - toplevelWidth  - appData.wmDecorationWidth) / 2;
			toplevelY = (dpyHeight - toplevelHeight - appData.wmDecorationHeight) /2;

			if (appData.appShare) {
				int X = appshare_x_hint;
				int Y = appshare_y_hint;
				if (appData.scale) {
					double fx = 1.0, fy = 1.0;
					get_scale_values(&fx, &fy);
					if (fx > 0.0 && fy > 0.0) {
						X *= fx;
						Y *= fy;
					}
				}
				if (appshare_x_hint != appshare_0_hint) {
					toplevelX = X;
				}
				if (appshare_y_hint != appshare_0_hint) {
					toplevelY = Y;
				}
			}

			/* set position via "geometry" so that window manager thinks it's a
			 user-specified position and therefore honours it */

			geometry = XtMalloc(256);

			sprintf(geometry, "%dx%d+%d+%d", toplevelWidth, toplevelHeight, toplevelX, toplevelY);
			fprintf(stderr, "geometry: %s ycrop: %d\n", geometry, appData.yCrop);
			XtVaSetValues(toplevel, XtNgeometry, geometry, NULL);
		}
	}

  /* Test if the keyboard is grabbed.  If so, it's probably because the
     XDM login window is up, so try iconifying it to release the grab */

	if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), False, GrabModeSync, GrabModeSync, CurrentTime) == GrabSuccess) {
		XUngrabKeyboard(dpy, CurrentTime);
	} else {
		wmState = XInternAtom(dpy, "WM_STATE", False);
		if (IconifyNamedWindow(DefaultRootWindow(dpy), "xlogin", False)) {
			xloginIconified = True;
			XSync(dpy, False);
			sleep(1);
		}
	}

	/* Set handlers for signals and X errors to perform cleanup */
	signal(SIGHUP,  CleanupSignalHandler);
	signal(SIGINT,  CleanupSignalHandler);
	signal(SIGTERM, CleanupSignalHandler);
	defaultXErrorHandler   = XSetErrorHandler(CleanupXErrorHandler);
	defaultXIOErrorHandler = XSetIOErrorHandler(CleanupXIOErrorHandler);
	defaultXtErrorHandler  = XtAppSetErrorHandler(appContext, CleanupXtErrorHandler);
}


/*
 * ToplevelInitAfterRealization initialises things which require the X windows
 * to exist.  It goes into full-screen mode if appropriate, and tells the
 * window manager we accept the "delete window" message.
 */

void
ToplevelInitAfterRealization()
{
	if (appData.fullScreen) {
		FullScreenOn();
		if (net_wm_supported()) {
			/* problem with scroll bars sticking: */
      			XSync(dpy, False);
  			usleep(50 * 1000);
			FullScreenOff();
      			XSync(dpy, False);
  			usleep(50 * 1000);
			FullScreenOn();
		}
	}

	wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, XtWindow(toplevel), &wmDeleteWindow, 1);
	XtOverrideTranslations(toplevel, XtParseTranslationTable ("<Message>WM_PROTOCOLS: Quit()"));
}


/*
 * TimeFromEvent() gets the time field out of the given event.  It returns
 * CurrentTime if the event has no time field.
 */

Time TimeFromEvent(XEvent *ev) {
  switch (ev->type) {
  case KeyPress:
  case KeyRelease:
    return ev->xkey.time;
  case ButtonPress:
  case ButtonRelease:
    return ev->xbutton.time;
  case MotionNotify:
    return ev->xmotion.time;
  case EnterNotify:
  case LeaveNotify:
    return ev->xcrossing.time;
  case PropertyNotify:
    return ev->xproperty.time;
  case SelectionClear:
    return ev->xselectionclear.time;
  case SelectionRequest:
    return ev->xselectionrequest.time;
  case SelectionNotify:
    return ev->xselection.time;
  default:
    return CurrentTime;
  }
}


/*
 * Pause is an action which pauses for a number of milliseconds (100 by
 * default).  It is sometimes useful to space out "fake" pointer events
 * generated by SendRFBEvent.
 */

void Pause(Widget w, XEvent *event, String *params, Cardinal *num_params) {
	int msec;

	if (*num_params == 0) {
		msec = 100;
	} else {
		msec = atoi(params[0]);
	}
	usleep(msec * 1000);
	if (w || event || params || num_params) {}
}


/*
 * Run an arbitrary command via execvp()
 */
void
RunCommand(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  int childstatus;

  if (*num_params == 0)
    return;

  if (fcntl (ConnectionNumber (dpy), F_SETFD, 1L) == -1)
      fprintf(stderr, "warning: file descriptor %d unusable for spawned program", ConnectionNumber(dpy));
  
  if (fcntl (rfbsock, F_SETFD, 1L) == -1)
      fprintf(stderr, "warning: file descriptor %d unusable for spawned program", rfbsock);

  switch (fork()) {
  case -1: 
    perror("fork"); 
    break;
  case 0:
      /* Child 1. Fork again. */
      switch (fork()) {
      case -1:
	  perror("fork");
	  break;

      case 0:
	  /* Child 2. Do some work. */
	  execvp(params[0], params);
	  perror("exec");
	  exit(1);
	  break;  

      default:
	  break;
      }

      /* Child 1. Exit, and let init adopt our child */
      exit(0);

  default:
    break;
  }

  /* Wait for Child 1 to die */
  wait(&childstatus);
  
	if (w || event || params || num_params) {}
  return;
}


/*
 * Quit action - called when we get a "delete window" message.
 */

void Quit(Widget w, XEvent *event, String *params, Cardinal *num_params) {
	Cleanup();
	if (w || event || params || num_params) {}
	exit(0);
}


/*
 * Cleanup - perform any cleanup operations prior to exiting.
 */

extern pid_t exec_pid;

extern char *time_str(void);

void Cleanup() {

	if (appData.chatActive) {
		appData.chatActive = False;
		fprintf(stderr,"Sending SendTextChatClose()\n");
		SendTextChatClose();
		SendTextChatFinished();
	}

	if (xloginIconified) {
		IconifyNamedWindow(DefaultRootWindow(dpy), "xlogin", True);
		XFlush(dpy);
	}
#ifdef MITSHM
	if (appData.useShm) {
		if (UsingShm()) {
			ShmDetach();
		}
		ShmCleanup();
	}
#endif

	releaseAllPressedModifiers();

	if (rfbsock >= 0) {
		close(rfbsock);
		rfbsock = -1;
	}

	if (exec_pid > 0) {
		int status;
		waitpid(exec_pid, &status, WNOHANG);
		if (kill(exec_pid, 0) == 0) {
			if (getenv("SSVNC_NO_KILL_EXEC_CMD")) {
				fprintf(stderr,"Not killing exec=<cmd>  pid: %d\n", (int) exec_pid);
			} else {
				int i;
				fprintf(stderr,"Waiting for exec=<cmd>  pid: %d to finish...\n", (int) exec_pid);
				usleep(50 * 1000);
				for (i=0; i < 15; i++) {
					waitpid(exec_pid, &status, WNOHANG);
					if (kill(exec_pid, 0) != 0) {
						fprintf(stderr, "Process exec=<cmd>  pid: %d finished.  (%d msec)\n",
						    (int) exec_pid, i*100 + 50);
						break;
					}
					usleep(100 * 1000);
				}
				waitpid(exec_pid, &status, WNOHANG);
				if (kill(exec_pid, 0) == 0) {
					fprintf(stderr,"Sending SIGTERM to exec=<cmd> pid: %d\n", (int) exec_pid);
					kill(exec_pid, SIGTERM);
				}
			}
		}
		exec_pid = 0;
	}

	fprintf(stderr,"\n%s VNC Viewer exiting.\n\n", time_str());
	if (listenSpecified) {
		if (listenParent != 0 && getenv("SSVNC_LISTEN_ONCE") && listenParent != getpid()) {
			fprintf(stderr, "SSVNC_LISTEN_ONCE: Trying to kill Listening Parent: %d\n", (int) listenParent);
			fprintf(stderr, "SSVNC_LISTEN_ONCE: Press Ctrl-C if it continues to Listen.\n\n");
			kill(listenParent, SIGTERM);
		} else {
			fprintf(stderr,"(NOTE: You may need to Press Ctrl-C to make the Viewer Stop Listening.)\n\n");
		}
	}
}

static void check_dbg(void) {
	if (getenv("SSVNC_EXIT_DEBUG")) {
		fprintf(stderr, "Press any key to continue: ");
		getc(stdin);
	}
}

static int
CleanupXErrorHandler(Display *dpy, XErrorEvent *error)
{
	if (XError_ign) {
		char str[4096];
		XError_ign++;
		fprintf(stderr,"XError_ign called.\n");
		str[0] = '\0';
		if (XGetErrorText(dpy, error->error_code, str, 4096)) {
			fprintf(stderr, "%s", str);
		}
		return 0;
	}
	fprintf(stderr,"CleanupXErrorHandler called\n");
	check_dbg();
	Cleanup();
	return (*defaultXErrorHandler)(dpy, error);
}

static int
CleanupXIOErrorHandler(Display *dpy)
{
	fprintf(stderr,"CleanupXIOErrorHandler called\n");
	check_dbg();
	Cleanup();
	return (*defaultXIOErrorHandler)(dpy);
}

static void
CleanupXtErrorHandler(String message)
{
	fprintf(stderr,"CleanupXtErrorHandler called\n");
	check_dbg();
	Cleanup();
	(*defaultXtErrorHandler)(message);
}

static void
CleanupSignalHandler(int sig)
{
	fprintf(stderr,"CleanupSignalHandler called\n");
	check_dbg();
	Cleanup();
	if (sig) {}
	exit(1);
}


/*
 * IconifyNamedWindow iconifies another client's window with the given name.
 */

static Bool
IconifyNamedWindow(Window w, char *name, Bool undo)
{
  Window *children, dummy;
  unsigned int nchildren;
  int i;
  char *window_name;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;

  if (XFetchName(dpy, w, &window_name)) {
    if (strcmp(window_name, name) == 0) {
      if (undo) {
	XMapWindow(dpy, w);
      } else {
	XIconifyWindow(dpy, w, DefaultScreen(dpy));
      }
      XFree(window_name);
      return True;
    }
    XFree(window_name);
  }

  XGetWindowProperty(dpy, w, wmState, 0, 0, False,
		     AnyPropertyType, &type, &format, &nitems,
		     &after, &data);
  if (type != None) {
    XFree(data);
    return False;
  }

  if (!XQueryTree(dpy, w, &dummy, &dummy, &children, &nchildren))
    return False;

  for (i = 0; i < (int) nchildren; i++) {
    if (IconifyNamedWindow(children[i], name, undo)) {
      XFree ((char *)children);
      return True;
    }
  }
  if (children) XFree ((char *)children);
  return False;
}
