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
 * listen.c - listen for incoming connections
 */

#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <vncviewer.h>

#define FLASHWIDTH 50	/* pixels */
#define FLASHDELAY 1	/* seconds */

Bool listenSpecified = False;
pid_t listenParent = 0;
int listenPort = 0, flashPort = 0;

#if 0
static Font flashFont;
static void getFlashFont(Display *d);
static void flashDisplay(Display *d, char *user);
#endif

static Bool AllXEventsPredicate(Display *d, XEvent *ev, char *arg);

void raiseme(int force);

char _tbuf[256];
char *time_str(void) {
	time_t clock;
	time(&clock);
	strftime(_tbuf, 255, "%Y/%m/%d %X", localtime(&clock));
	return _tbuf;
}

static int accept_popup_check(int *argc, char **argv, char *sip, char *sih) {
	char line[16];
	char msg[1000];
	int dopopup = 1;

	if (!getenv("SSVNC_ACCEPT_POPUP")) {
		return 1;
	}
	
	if (!dopopup && use_tty()) {
		raiseme(1);
		fprintf(stderr, "Accept VNC connection? y/[n] ");
		fgets(line, sizeof(line), stdin);
		if (!strchr(line, 'y') && !strchr(line, 'Y')) {
			fprintf(stderr, "Refusing connection.\n");
			return 0;
		} else {
			fprintf(stderr, "Accepting connection.\n");
			return 1;
		}
	} else {
		int pid, pid2, accept_it = 0;

		pid = fork();
		if (pid == -1) {
			perror("fork"); 
			exit(1);
		}
		if (pid == 0) {
			char *geometry = "2x2+0+0";
			String fb[] = { "*message.Scroll: whenNeeded", NULL};
			close(rfbsock);

			toplevel = XtAppInitialize(&appContext, "Ssvnc", cmdLineOptions, numCmdLineOptions,
			    argc, argv, fb, NULL, 0);
			XtVaSetValues(toplevel, XtNmaxWidth, 2, XtNmaxHeight, 2, NULL);
			XtVaSetValues(toplevel, XtNgeometry, geometry, NULL);
			XtRealizeWidget(toplevel);
			dpy = XtDisplay(toplevel);
			sprintf(msg, "\n(LISTEN) Reverse VNC connection from IP: %s  %s\n                               Hostname: %s\n\n", sip, time_str(), sih);
			strcat(msg, "Accept or Reject VNC connection?");
			if (CreateMsg(msg, 2)) {
				XCloseDisplay(dpy);
				exit(0);
			} else {
				XCloseDisplay(dpy);
				exit(1);
			}
		} else {
			int status;
			pid2 = waitpid(pid, &status, 0);
			fprintf(stderr, "waitpid: %d/%d status: %d\n", pid, pid2, status);
			if (pid2 == pid) {
				if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
					accept_it = 1;
				}
			}
		}
		if (accept_it) {
			fprintf(stderr, "Accepting connection.\n");
			return 1;
		} else {
			fprintf(stderr, "Refusing connection.\n");
			return 0;
		}
	}
	return 0;
}

/*
 * listenForIncomingConnections() - listen for incoming connections from
 * servers, and fork a new process to deal with each connection.  We must do
 * all this before invoking any Xt functions - this is because Xt doesn't
 * cope with forking very well.
 */

extern char *accept6_hostname;
extern char *accept6_ipaddr;

void
listenForIncomingConnections(int *argc, char **argv, int listenArgIndex)
{
	Display *d;
	XEvent ev;
	int listenSocket, listenSocket6, flashSocket, sock;
	fd_set fds;
	char flashUser[256];
	int n;
	int i;
	char *displayname = NULL;
	int children = 0;
	int totalconn = 0, maxconn = 0;

	listenSpecified = True;
	listenParent = getpid();

	for (i = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-display") == 0 && i+1 < *argc) {
			displayname = argv[i+1];
		}
	}
	if (sock || flashUser || n) {}

	if (listenArgIndex+1 < *argc && argv[listenArgIndex+1][0] >= '0' &&
					    argv[listenArgIndex+1][0] <= '9') {

		listenPort = LISTEN_PORT_OFFSET + atoi(argv[listenArgIndex+1]);
		flashPort = FLASH_PORT_OFFSET + atoi(argv[listenArgIndex+1]);
		removeArgs(argc, argv, listenArgIndex, 2);

	} else {

		char *display;
		char *colonPos;
		struct utsname hostinfo;

		removeArgs(argc, argv, listenArgIndex, 1);

		display = XDisplayName(displayname);
		colonPos = strchr(display, ':');

		uname(&hostinfo);

		if (colonPos && ((colonPos == display) ||
			     (strncmp(hostinfo.nodename, display,
				      strlen(hostinfo.nodename)) == 0))) {

			listenPort = LISTEN_PORT_OFFSET + atoi(colonPos+1);
			flashPort = FLASH_PORT_OFFSET + atoi(colonPos+1);

		} else {
			fprintf(stderr,"%s: cannot work out which display number to "
			      "listen on.\n", programName);
			fprintf(stderr,"Please specify explicitly with -listen <num>\n");
			exit(1);
		}

	}

	if (!(d = XOpenDisplay(displayname))) {
		fprintf(stderr,"%s: unable to open display %s\n",
		    programName, XDisplayName(displayname));
		exit(1);
	}

#if 0
	getFlashFont(d);
#endif

	listenSocket  = ListenAtTcpPort(listenPort);
	listenSocket6 = ListenAtTcpPort6(listenPort);

#if 0
	flashSocket = ListenAtTcpPort(flashPort);
#endif
	flashSocket = 1234;

	if (listenSocket < 0 && listenSocket6 < 0) {
		fprintf(stderr,"%s -listen: could not obtain a listening socket on port %d\n",
		    programName, listenPort);
		exit(1);
	}

	fprintf(stderr,"%s -listen: Listening on port %d  ipv4_fd: %d ipv6_fd: %d\n",
	  programName, listenPort, listenSocket, listenSocket6);
	fprintf(stderr,"%s -listen: Cmdline errors are not reported until "
	  "a connection comes in.\n", programName);

	/* this will only work if X events drives this loop -- they don't */
	if (getenv("SSVNC_MAX_LISTEN")) {
		maxconn = atoi(getenv("SSVNC_MAX_LISTEN"));
	}

    while (True) {
	int lsock = -1;

	/* reap any zombies */
	int status, pid;
	while ((pid = wait3(&status, WNOHANG, (struct rusage *)0))>0) {
		if (pid > 0 && children > 0) {
			children--;	
			/* this will only work if X events drives this loop -- they don't */
			if (maxconn > 0 && totalconn >= maxconn) {
				fprintf(stderr,"%s -listen: Finished final connection %d\n",
				    programName, maxconn);
				exit(0);
			}
		}
	}

	/* discard any X events */
	while (XCheckIfEvent(d, &ev, AllXEventsPredicate, NULL)) {
		;
	}

	FD_ZERO(&fds); 

#if 0
	FD_SET(flashSocket, &fds);
#endif
	if (listenSocket >= 0) {
    		FD_SET(listenSocket, &fds);
	}
	if (listenSocket6 >= 0) {
    		FD_SET(listenSocket6, &fds);
	}
	FD_SET(ConnectionNumber(d), &fds);

	fprintf(stderr, "\n%s select() start ...\n", time_str()); 
	select(FD_SETSIZE, &fds, NULL, NULL, NULL);
	fprintf(stderr, "%s select() returned.\n", time_str()); 

	while ((pid = wait3(&status, WNOHANG, (struct rusage *)0))>0) {
		if (pid > 0 && children > 0) {
			children--;	
			if (maxconn > 0 && totalconn >= maxconn) {
				fprintf(stderr,"%s -listen: Finished final connection %d\n",
				programName, maxconn);
				exit(0);
			}
		}
	}

#if 0
	if (FD_ISSET(flashSocket, &fds)) {
		sock = AcceptTcpConnection(flashSocket);
		if (sock < 0) exit(1);
		n = read(sock, flashUser, 255);
		if (n > 0) {
			flashUser[n] = 0;
			flashDisplay(d, flashUser);
		} else {
			flashDisplay(d, NULL);
		}
		close(sock);
	}
#endif

	lsock = -1;
	if (listenSocket >= 0 && FD_ISSET(listenSocket, &fds)) {
		lsock = listenSocket;
	} else if (listenSocket6 >= 0 && FD_ISSET(listenSocket6, &fds)) {
		lsock = listenSocket6;
	}

	if (lsock >= 0) {
		int multi_ok = 0;
		char *sml = getenv("SSVNC_MULTIPLE_LISTEN");
		char *sip = NULL;
		char *sih = NULL;

		if (lsock == listenSocket) {
			rfbsock = AcceptTcpConnection(lsock);
		} else {
			rfbsock = AcceptTcpConnection6(lsock);
		}

		if (sml != NULL) {
			if (strstr(sml, "MAX:") == sml || strstr(sml, "max:") == sml) {
				char *q = strchr(sml, ':');
				int maxc = atoi(q+1);
				if (maxc == 0 && strcmp(q+1, "0")) {
					maxc = -99;
				}
				if (maxc < 0) {
					fprintf(stderr, "invalid SSVNC_MULTIPLE_LISTEN=MAX:n, %s, must be 0 or positive, using 1\n", sml); 
				} else if (maxc == 0) {
					multi_ok = 1;
				} else if (children < maxc) {
					multi_ok = 1;
				}
			} else if (strcmp(sml, "") && strcmp(sml, "0")) {
				multi_ok = 1;
			}
		}

		if (rfbsock < 0) exit(1);
		if (!SetNonBlocking(rfbsock)) exit(1);

		if (children > 0 && !multi_ok) {
			fprintf(stderr,"\n");
			fprintf(stderr,"%s: denying extra incoming connection (%d already)\n",
			    programName, children);
			fprintf(stderr,"%s: to override: use '-multilisten' or set SSVNC_MULTIPLE_LISTEN=1\n",
			    programName);
			fprintf(stderr,"\n");
			close(rfbsock);
			rfbsock = -1;
			continue;
		}

		if (lsock == listenSocket) {
			sip = get_peer_ip(rfbsock);
			if (strlen(sip) > 100) sip = "0.0.0.0";
			sih = ip2host(sip);
			if (strlen(sih) > 300) sih = "unknown";
		} else {
			if (accept6_hostname != NULL) {
				sip = accept6_ipaddr;
				accept6_ipaddr = NULL;
				sih = accept6_hostname;
				accept6_hostname = NULL;
			} else {
				sip = "unknown";
				sih = "unknown";
			}
		}

		fprintf(stderr, "\n");
		fprintf(stderr, "(LISTEN) Reverse VNC connection from IP: %s  %s\n", sip, time_str());
		fprintf(stderr, "                               Hostname: %s\n\n", sih);

		if (sml == NULL && !accept_popup_check(argc, argv, sip, sih)) {
			close(rfbsock);
			rfbsock = -1;
			continue;
		}

		totalconn++;

		XCloseDisplay(d);

		/* Now fork off a new process to deal with it... */

		switch (fork()) {

		case -1: 
			perror("fork"); 
			exit(1);

		case 0:
			/* child - return to caller */
			close(listenSocket);
#if 0
			close(flashSocket);
#endif
			if (sml != NULL && !accept_popup_check(argc, argv, sip, sih)) {
				close(rfbsock);
				rfbsock = -1;
				exit(0);
			}
			return;

		default:
			/* parent - go round and listen again */
			children++;
			close(rfbsock);
			if (!(d = XOpenDisplay(displayname))) {
				fprintf(stderr,"%s: unable to open display %s\n",
				    programName, XDisplayName(displayname));
				exit(1);
			}
#if 0
			getFlashFont(d);
#endif
			fprintf(stderr,"\n\n%s -listen: Listening on port %d\n",
			    programName,listenPort);
			fprintf(stderr,"%s -listen: Cmdline errors are not reported until "
			    "a connection comes in.\n\n", programName);
			break;
		}
	}
    }
}


/*
 * getFlashFont
 */

#if 0
static void
getFlashFont(Display *d)
{

#if 1
	/* no longer used */
	if (d) {}
	return;
#else
  char fontName[256];
  char **fontNames;
  int nFontNames;

  sprintf(fontName,"-*-courier-bold-r-*-*-%d-*-*-*-*-*-iso8859-1",
	  FLASHWIDTH);
  fontNames = XListFonts(d, fontName, 1, &nFontNames);
  if (nFontNames == 1) {
    XFreeFontNames(fontNames);
  } else {
    sprintf(fontName,"fixed");
  }
  flashFont = XLoadFont(d, fontName);

#endif

}


/*
 * flashDisplay
 */

static void
flashDisplay(Display *d, char *user)
{
#if 1
	/* no longer used */
	if (d || user) {}
	return;
#else
  Window w1, w2, w3, w4;
  XSetWindowAttributes attr;

  XBell(d, 0);

  XForceScreenSaver(d, ScreenSaverReset);

  attr.background_pixel = BlackPixel(d, DefaultScreen(d));
  attr.override_redirect = 1;
  attr.save_under = True;

  w1 = XCreateWindow(d, DefaultRootWindow(d), 0, 0,
		     WidthOfScreen(DefaultScreenOfDisplay(d)), 
		     FLASHWIDTH, 0, 
		     CopyFromParent, CopyFromParent, CopyFromParent, 
		     CWBackPixel|CWOverrideRedirect|CWSaveUnder,
		     &attr);
  
  w2 = XCreateWindow(d, DefaultRootWindow(d), 0, 0, FLASHWIDTH,
		     HeightOfScreen(DefaultScreenOfDisplay(d)), 0,
		     CopyFromParent, CopyFromParent, CopyFromParent, 
		     CWBackPixel|CWOverrideRedirect|CWSaveUnder,
		     &attr);

  w3 = XCreateWindow(d, DefaultRootWindow(d), 
		     WidthOfScreen(DefaultScreenOfDisplay(d))-FLASHWIDTH, 
		     0, FLASHWIDTH, 
		     HeightOfScreen(DefaultScreenOfDisplay(d)), 0, 
		     CopyFromParent, CopyFromParent, CopyFromParent, 
		     CWBackPixel|CWOverrideRedirect|CWSaveUnder,
		     &attr);

  w4 = XCreateWindow(d, DefaultRootWindow(d), 0,
		     HeightOfScreen(DefaultScreenOfDisplay(d))-FLASHWIDTH, 
		     WidthOfScreen(DefaultScreenOfDisplay(d)), 
		     FLASHWIDTH, 0, 
		     CopyFromParent, CopyFromParent, CopyFromParent, 
		     CWBackPixel|CWOverrideRedirect|CWSaveUnder,
		     &attr);

  XMapWindow(d, w1);
  XMapWindow(d, w2);
  XMapWindow(d, w3);
  XMapWindow(d, w4);

  if (user) {
    GC gc;
    XGCValues gcv;

    gcv.foreground = WhitePixel(d, DefaultScreen(d));
    gcv.font = flashFont;
    gc = XCreateGC(d, w1, GCForeground|GCFont, &gcv);
    XDrawString(d, w1, gc,
		WidthOfScreen(DefaultScreenOfDisplay(d)) / 2 - FLASHWIDTH,
		(FLASHWIDTH * 3 / 4), user, strlen(user));
  }
  XFlush(d);

  sleep(FLASHDELAY);

  XDestroyWindow(d, w1);
  XDestroyWindow(d, w2);
  XDestroyWindow(d, w3);
  XDestroyWindow(d, w4);
  XFlush(d);

#endif

}
#endif

/*
 * AllXEventsPredicate is needed to make XCheckIfEvent return all events.
 */

static Bool
AllXEventsPredicate(Display *d, XEvent *ev, char *arg)
{
	if (d || ev || arg) {}
	return True;
}
