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
 * vncviewer.c - the Xt-based VNC viewer.
 */

#include "vncviewer.h"
#include <ctype.h>
#include <X11/Xaw/Toggle.h>

char *programName;
XtAppContext appContext;
Display* dpy;

Widget toplevel;

extern void raiseme(int force);
extern void CreateChat(void);

void set_sbwidth(int sbw) {
	char *q, *p, t[5];
	int i, k, N = 4;
	int db = 0;

	if (sbw < 1) {
		sbw = 2;
	} else if (sbw > 100) {
		sbw = 100;
	}
	if (db) fprintf(stderr, "sbw: %d\n", sbw);

	sprintf(t, "%4d", sbw);
	k = 0;
	while (fallback_resources[k] != NULL) {
		q = strstr(fallback_resources[k], "horizontal.height: ");
		if (!q) {
			q = strstr(fallback_resources[k], "vertical.width: ");
		}
		if (q) {
			p = strdup(fallback_resources[k]);
			q = strstr(p, ":   ");
			if (q) {
				q++;
				q++;
				for (i=0; i < N; i++) {
					*(q+i) = t[i];
				}
				fallback_resources[k] = p;
				if (db) fprintf(stderr, "res: %s\n\n", p);
			}
		}
		k++;
	}
}

void min_title(void) {
	char *q;
	int k;

	k = 0;
	while (fallback_resources[k] != NULL) {
		q = strstr(fallback_resources[k], "Ssvnc.title: ");
		if (q) {
			fallback_resources[k] = strdup("Ssvnc.title: %s");
		}
		k++;
	}
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void unixpw(char *instr, int vencrypt_plain) {
	char *str, *q, *infile = NULL;
	FILE *in;
	int i, rmfile = 0;
	struct stat sb;
	int N = 99;
	char username[100], passwd[100];
	static int did = 0;

	if (did) {
		return;
	}
	did = 1;

	for (i=0; i<100; i++) {
		username[i] = '\0';
		passwd[i] = '\0';
	}

	if (instr == NULL) {
		return;
	} else if (!strcmp(instr, "")) {
		return;
	}

	str = strdup(instr);
	
	if (strstr(str, "rm:") == str) {
		rmfile = 1;
		infile = str + strlen("rm:");
	} else if (stat(str, &sb) == 0) {
		infile = str;
	}
	if (!strcmp(str, ".")) {
		char *p;
		if (!use_tty()) {
			char *u;
			fprintf(stderr, "\nEnter Unix Username and Password in the popups.\n");
			u = DoUserDialog();
			if (strlen(u) >= 100) {
				exit(1);
			}
			snprintf(username, sizeof(username), "%s", u);
			p = DoPasswordDialog();
		} else {
			raiseme(1);
			fprintf(stderr, "\nUnix Username: ");
			if (fgets(username, N, stdin) == NULL) {
				exit(1);
			}
			p = getpass("Unix Password: ");
		}
		if (! p) {
			exit(1);
		}
		strncpy(passwd, p, N);
		fprintf(stderr, "\n");
		
	} else if (!strcmp(str, "-")) {
		char *p, *q;
		if (!use_tty()) {
			fprintf(stderr, "\nEnter unixuser@unixpasswd in the popup.\n");
			p = DoPasswordDialog();
		} else {
			raiseme(1);
			p = getpass("unixuser@unixpasswd: ");
		}
		if (! p) {
			exit(1);
		}
		q = strchr(p, '@');
		if (! q) {
			exit(1);
		}
		*q = '\0';
		strncpy(username, p, N);
		strncpy(passwd, q+1, N);
		
	} else if (infile) {
		in = fopen(infile, "r");	
		if (in == NULL) {
			fprintf(stderr, "failed to open -unixpw file.\n");
			exit(1);
		}
		if (fgets(username, N, in) == NULL) {
			exit(1);
		}
		if (fgets(passwd, N, in) == NULL) {
			exit(1);
		}
		fclose(in);
		fprintf(stderr, "read username@passwd from file: %s\n", infile);
		if (rmfile) {
			fprintf(stderr, "deleting username@passwd file:  %s\n", infile);
			unlink(infile);
		}
	} else if (strchr(str, '@'))  {
		char *q = strchr(str, '@');
		*q = '\0';
		strncpy(username, str, N);
		strncpy(passwd, q+1, N);
	} else {
		exit(1);
	}

	free(str);
	
	if (vencrypt_plain) {
		CARD32 ulen, plen;
		char *q;

		q = strrchr(username, '\n');
		if (q) *q = '\0';
		q = strrchr(passwd, '\n');
		if (q) *q = '\0';

		ulen = Swap32IfLE((CARD32)strlen(username));
		plen = Swap32IfLE((CARD32)strlen(passwd));

		if (!WriteExact(rfbsock, (char *)&ulen, 4) ||
		    !WriteExact(rfbsock, (char *)&plen, 4)) {
			return;
		}

		if (!WriteExact(rfbsock, username, strlen(username)) ||
		    !WriteExact(rfbsock, passwd, strlen(passwd))) {
			return;
		}
		return;
	}


	if (! getenv("SSVNC_UNIXPW_NOESC")) {
		SendKeyEvent(XK_Escape, 1);
		SendKeyEvent(XK_Escape, 0);
	}

	q = username;
	while (*q != '\0' && *q != '\n') {
		char c = *q;
		if (c >= 0x20 && c <= 0x07e) {
			KeySym ks = (KeySym) c;
			SendKeyEvent(ks, 1);
			SendKeyEvent(ks, 0);
		}
		q++;
	}

	SendKeyEvent(XK_Return, 1);
	SendKeyEvent(XK_Return, 0);
	
	q = passwd;
	while (*q != '\0' && *q != '\n') {
		char c = *q;
		if (c >= 0x20 && c <= 0x07e) {
			KeySym ks = (KeySym) c;
			SendKeyEvent(ks, 1);
			SendKeyEvent(ks, 0);
		}
		q++;
	}

	SendKeyEvent(XK_Return, 1);
	SendKeyEvent(XK_Return, 0);
}

static void chat_window_only(void) {
	if (appData.chatOnly) {
		static double last_time = 0.0;
		if (dnow() > last_time + 1.5) {
			XSync(dpy, False);
			XUnmapWindow(dpy, XtWindow(toplevel));
		}
	}
}

int saw_appshare = 0;

int
main(int argc, char **argv)
{
	int i, save_sbw, saw_listen = 0;
	char *pw_loc = NULL;
	programName = argv[0];

	if (strrchr(programName, '/') != NULL) {
		programName = strrchr(programName, '/') + 1;
	}

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-env")) {
			if (i+1 < argc) {
				char *estr = argv[i+1];
				if (strchr(estr, '=')) {
					putenv(estr);
				}
			}
		}
		if (!strcmp(argv[i], "-noipv4")) {
			putenv("VNCVIEWER_NO_IPV4=1");
		}
		if (!strcmp(argv[i], "-noipv6")) {
			putenv("VNCVIEWER_NO_IPV6=1");
		}
	}
	if (getenv("VNCVIEWER_NO_IPV4")) {
		appData.noipv4 = True;
	}
	if (getenv("VNCVIEWER_NO_IPV6")) {
		appData.noipv6 = True;
	}

  /* The -listen option is used to make us a daemon process which listens for
     incoming connections from servers, rather than actively connecting to a
     given server. The -tunnel and -via options are useful to create
     connections tunneled via SSH port forwarding. We must test for the
     -listen option before invoking any Xt functions - this is because we use
     forking, and Xt doesn't seem to cope with forking very well. For -listen
     option, when a successful incoming connection has been accepted,
     listenForIncomingConnections() returns, setting the listenSpecified
     flag. */

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-appshare")) {
			putenv("SSVNC_MULTIPLE_LISTEN=1");
			fprintf(stderr, "Enabling -multilisten mode for 'x11vnc -appshare' usage.\n\n");
			saw_appshare = 1;
		}
		if (!strcmp(argv[i], "-multilisten")) {
			putenv("SSVNC_MULTIPLE_LISTEN=1");
			saw_listen = 2;
		}
		if (!strcmp(argv[i], "-listen")) {
			saw_listen = 1;
		}
		if (!strcmp(argv[i], "-acceptpopup")) {
			putenv("SSVNC_ACCEPT_POPUP=1");
		}
		if (!strcmp(argv[i], "-acceptpopupsc")) {
			putenv("SSVNC_ACCEPT_POPUP_SC=1");
		}
		if (strstr(argv[i], " pw=") != NULL) {
			pw_loc = strstr(argv[i], " pw=") + 1;
		}
	}

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-appshare") && !saw_listen) {
			listenForIncomingConnections(&argc, argv, i);
			break;
		}
		if (!strcmp(argv[i], "-multilisten")) {
			listenForIncomingConnections(&argc, argv, i);
			break;
		}
		if (!strcmp(argv[i], "-listen")) {
			listenForIncomingConnections(&argc, argv, i);
			break;
		}
		if (!strcmp(argv[i], "-tunnel") || !strcmp(argv[i], "-via")) {
			if (!createTunnel(&argc, argv, i)) {
				exit(1);
			}
			break;
		}
		if (!strcmp(argv[i], "-printres") || !strcmp(argv[i], "-res")) {
			int j = 0;
			fprintf(stdout, "\n! Ssvnc fallback X resources:\n\n");
			while (1) {
				char *p = fallback_resources[j++];
				int k = 0;
				if (p == NULL) break;
				while (*p != '\0') {
					fprintf(stdout, "%c", *p);
					if (k > 0 && *p == 'n' && *(p-1) == '\\') {
						fprintf(stdout, "\\\n");
					}
					p++; k++;
				}
				fprintf(stdout, "\n\n");
			}
			exit(0);	
		}
	}


	if (argc > 1 && strstr(argv[1], "-h") == argv[1]) {
		usage();
		return 0;
	}

  /* Call the main Xt initialisation function.  It parses command-line options,
     generating appropriate resource specs, and makes a connection to the X
     display. */

	if (saw_appshare || getenv("VNCVIEWER_MIN_TITLE")) {
		min_title();
	}
	appData.sbWidth = 0;
	if (getenv("VNCVIEWER_SBWIDTH")) {
		int sbw = atoi(getenv("VNCVIEWER_SBWIDTH"));
		if (sbw > 0) {
			appData.sbWidth = sbw;
		}
	}
	if (appData.sbWidth == 0) {
		int i, sbw = 0;
		for (i = 1; i < argc - 1; i++) {
			if (!strcmp(argv[i], "-sbwidth")) {
				sbw = atoi(argv[i+1]);
			}
		}
		if (sbw > 0) {
			appData.sbWidth = sbw;
		}
	}
	save_sbw = appData.sbWidth;
	if (save_sbw > 0) {
		set_sbwidth(save_sbw);
	} else {
		set_sbwidth(6);
	}

	toplevel = XtVaAppInitialize(&appContext, "Ssvnc", cmdLineOptions,
	    numCmdLineOptions, &argc, argv, fallback_resources,
	    XtNborderWidth, 0, NULL);

	dpy = XtDisplay(toplevel);

  /* Interpret resource specs and process any remaining command-line arguments
     (i.e. the VNC server name).  If the server name isn't specified on the
     command line, getArgsAndResources() will pop up a dialog box and wait
     for one to be entered. */

	GetArgsAndResources(argc, argv);

	if (saw_appshare) {
		appData.appShare = True;
	}

	if (save_sbw) {
		appData.sbWidth = save_sbw;
	}

	if (appData.chatOnly) {
		appData.encodingsString = "raw hextile";
	}

	if (pw_loc != NULL) {
		char *q = pw_loc;
		while (*q != '\0' && !isspace(*q)) {
			*q = ' ';
			q++;
		}
	}

  /* Unless we accepted an incoming connection, make a TCP connection to the
     given VNC server */

	if (appData.repeaterUltra == NULL) {
		if (getenv("SSVNC_REPEATER") != NULL) {
			appData.repeaterUltra = strdup(getenv("SSVNC_REPEATER"));
		}
	}

	if (!listenSpecified) {
		if (!ConnectToRFBServer(vncServerHost, vncServerPort)) {
			exit(1);
		}
		if (appData.repeaterUltra != NULL) {
			char tmp[256];
			if (strstr(appData.repeaterUltra, "SCIII=") == appData.repeaterUltra) {
				appData.repeaterUltra = strdup(appData.repeaterUltra + strlen("SCIII="));
				fprintf(stderr, "sending 'testB' to ultravnc SC III SSL repeater...\n");
				WriteExact(rfbsock, "testB" , 5); 
			}
			if (ReadFromRFBServer(tmp, 12)) {
				tmp[12] = '\0';
				fprintf(stderr, "repeater 1st proto line: '%s'\n", tmp);
				if (strstr(tmp, "RFB 000.000") == tmp) {
					int i;
					for (i=0; i<256; i++) {
						tmp[i] = '\0';
					}
					for (i=0; i<250; i++) {
						if (i >= (int) strlen(appData.repeaterUltra)) {
							break;
						}
						tmp[i] = appData.repeaterUltra[i];
					}
					fprintf(stderr, "sending '%s' to repeater...\n", tmp);
					WriteExact(rfbsock, tmp, 250); 
				}
			} else {
				fprintf(stderr, "repeater NO proto line!\n");
			}
		}
	}

  /* Initialise the VNC connection, including reading the password */

	if (!InitialiseRFBConnection()) {
		Cleanup();
		exit(1);
	}
	if (appData.unixPW != NULL) {
		unixpw(appData.unixPW, 0);
	} else if (getenv("SSVNC_UNIXPW")) {
		unixpw(getenv("SSVNC_UNIXPW"), 0);
	}

  /* Create the "popup" widget - this won't actually appear on the screen until
     some user-defined event causes the "ShowPopup" action to be invoked */

	CreatePopup();
	CreateScaleN();
	CreateTurboVNC();
	CreateQuality();
	CreateCompress();
	CreateChat();

  /* Find the best pixel format and X visual/colormap to use */

	SetVisualAndCmap();

  /* Create the "desktop" widget, and perform initialisation which needs doing
     before the widgets are realized */

	ToplevelInitBeforeRealization();

	DesktopInitBeforeRealization();

  /* "Realize" all the widgets, i.e. actually create and map their X windows */

	XtRealizeWidget(toplevel);

  /* Perform initialisation that needs doing after realization, now that the X
     windows exist */

	InitialiseSelection();

	ToplevelInitAfterRealization();

	DesktopInitAfterRealization();

  /* Tell the VNC server which pixel format and encodings we want to use */

	SetFormatAndEncodings();

	if (appData.chatOnly) {
		chat_window_only();
		ToggleTextChat(0, NULL, NULL, NULL);
	}

  /* Now enter the main loop, processing VNC messages.  X events will
     automatically be processed whenever the VNC connection is idle. */

	while (1) {
		if (!HandleRFBServerMessage()) {
			break;
		}
		if (appData.chatOnly) {
			chat_window_only();
		}
	}

	Cleanup();

	return 0;
}

/*
 * Toggle8bpp
 */

static int last_ncolors = 0;
static int save_useBGR233 = 0;
static Bool save_useBGR565 = False;

static Widget b8    = NULL;
static Widget b16   = NULL;
static Widget bfull = NULL;

int do_format_change = 0;
int do_cursor_change = 0;
double do_fb_update = 0.0;
static void schedule_format_change(void) {
	do_format_change = 1;
	do_cursor_change = 0;
}
extern double dnow(void);
static void schedule_fb_update(void) {
	do_fb_update = dnow();
}
static void init_format_change(void) {
	appDataNew.useBGR233       = appData.useBGR233;
	appDataNew.useBGR565       = appData.useBGR565;
	appDataNew.useGreyScale    = appData.useGreyScale;
	appDataNew.enableJPEG      = appData.enableJPEG;
	appDataNew.encodingsString = appData.encodingsString;
	appDataNew.useRemoteCursor = appData.useRemoteCursor;
	appDataNew.useX11Cursor    = appData.useX11Cursor;
	appDataNew.useRawLocal     = appData.useRawLocal;
	appDataNew.qualityLevel    = appData.qualityLevel;
	appDataNew.compressLevel   = appData.compressLevel;
}
void cutover_format_change(void) {
	appData.useBGR233       = appDataNew.useBGR233;
	appData.useBGR565       = appDataNew.useBGR565;
	appData.useGreyScale    = appDataNew.useGreyScale;
	appData.enableJPEG      = appDataNew.enableJPEG;
	appData.encodingsString = appDataNew.encodingsString;
	appData.useRemoteCursor = appDataNew.useRemoteCursor;
	appData.useX11Cursor    = appDataNew.useX11Cursor;
	appData.useRawLocal     = appDataNew.useRawLocal;
	appData.qualityLevel    = appDataNew.qualityLevel;
	appData.compressLevel   = appDataNew.compressLevel;
}

void
Toggle8bpp(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	fprintf(stderr, "Toggle8bpp: %d\n", appData.useBGR233);
	b8 = w;
	init_format_change();
	if (appData.useBGR233) {
		last_ncolors = appData.useBGR233;
		appDataNew.useBGR233 = 0;
		appDataNew.useBGR565 = save_useBGR565;
		fprintf(stderr, "8bpp: off\n");
	} else {
		if (!last_ncolors) last_ncolors = 256;
		appDataNew.useBGR233 = last_ncolors;
		save_useBGR565 = appData.useBGR565;
		appDataNew.useBGR565 = False;
		fprintf(stderr, "8bpp: on (%d colors)\n", appDataNew.useBGR233);
	}
	schedule_format_change();
	if (w || ev || params || num_params) {}
}


void
Toggle16bpp(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	fprintf(stderr, "Toggle16bpp: %d\n", appData.useBGR565);
	b16 = w;
	init_format_change();
	if (appData.useBGR565) {
		appDataNew.useBGR565 = False;
		appDataNew.useBGR233 = save_useBGR233;
		fprintf(stderr, "16bpp: off\n");
	} else {
		appDataNew.useBGR565 = True;
		save_useBGR233 = appData.useBGR233;
		appDataNew.useBGR233 = 0;
		fprintf(stderr, "16bpp: on\n");
	}
	schedule_format_change();
	if (w || ev || params || num_params) {}
}

void
ToggleFullColor(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	fprintf(stderr, "ToggleFullColor\n");
	bfull = w;
	init_format_change();
	if (appData.useBGR565 || appData.useBGR233) {
		save_useBGR565 = appData.useBGR565;
		appDataNew.useBGR565 = False;
		save_useBGR233 = appData.useBGR233;
		appDataNew.useBGR233 = 0;
		fprintf(stderr, "FullColor: on\n");
	} else {
		if (save_useBGR565) {
			appDataNew.useBGR565 = True;
			appDataNew.useBGR233 = 0;
			fprintf(stderr, "FullColor off -> 16bpp.\n");
		} else {
			appDataNew.useBGR565 = False;
			if (!save_useBGR233) save_useBGR233 = 256;
			appDataNew.useBGR233 = save_useBGR233;
			fprintf(stderr, "FullColor off -> 8bpp.\n");
		}
	}
	schedule_format_change();
	if (w || ev || params || num_params) {}
}

void
ToggleXGrab(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.grabAll) {
		appData.grabAll = False;
	} else {
		appData.grabAll = True;
	}
	fprintf(stderr, "ToggleXGrab, current=%d\n", appData.grabAll);
	/* always ungrab to be sure, fullscreen will handle the rest */
	XUngrabServer(dpy);
	if (w || ev || params || num_params) {}
}

void
ToggleEscapeActive(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.escapeActive) {
		appData.escapeActive = False;
	} else {
		appData.escapeActive = True;
	}
	if (w || ev || params || num_params) {}
}

/*
 * ToggleNColors
 */

static Widget w256 = NULL;
static Widget w64  = NULL;
static Widget w8   = NULL;

void
Toggle256Colors(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	w256 = w;
	if (appData.useBGR233 != 256) {
		fprintf(stderr, "256 colors: on\n");
		init_format_change();
		last_ncolors = appDataNew.useBGR233 = 256;
		save_useBGR565 = appData.useBGR565;
		appDataNew.useBGR565 = False;
		schedule_format_change();
	}
	if (w || ev || params || num_params) {}
}

void
Toggle64Colors(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	w64 = w;
	if (appData.useBGR233 != 64) {
		fprintf(stderr, "64 colors: on\n");
		init_format_change();
		last_ncolors = appDataNew.useBGR233 = 64;
		save_useBGR565 = appData.useBGR565;
		appDataNew.useBGR565 = False;
		schedule_format_change();
	}
	if (w || ev || params || num_params) {}
}

void
Toggle8Colors(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	w8 = w;
	if (appData.useBGR233 != 8) {
		fprintf(stderr, "8 colors: on\n");
		init_format_change();
		last_ncolors = appDataNew.useBGR233 = 8;
		save_useBGR565 = appData.useBGR565;
		appDataNew.useBGR565 = False;
		schedule_format_change();
	}
	if (w || ev || params || num_params) {}
}

void
ToggleGreyScale(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	fprintf(stderr, "ToggleGreyScale\n");
	init_format_change();
	if (appData.useGreyScale) {
		appDataNew.useGreyScale = False;
		fprintf(stderr, "greyscale: off\n");
	} else {
		appDataNew.useGreyScale = True;
		fprintf(stderr, "greyscale: on\n");
	}
	schedule_format_change();
	if (w || ev || params || num_params) {}
}

/*
 * ToggleJPEG
 */

void
ToggleJPEG(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	init_format_change();
	if (appData.enableJPEG) {
		appDataNew.enableJPEG = False;
		fprintf(stderr, "JPEG: off\n");
	} else {
		appDataNew.enableJPEG = True;
		fprintf(stderr, "JPEG: on\n");
	}
	schedule_format_change();
	if (w || ev || params || num_params) {}
}

/*
 * ToggleTightZRLE
 */

static Bool usingZRLE = False;
static Bool usingZYWRLE = False;
static Bool usingHextile = False;
extern int skip_maybe_sync;

void
ToggleTightZRLE(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char prefTight[] = "copyrect tight zrle zywrle zlib hextile corre rre raw";
	char prefZRLE[]  = "copyrect zrle zywrle tight zlib hextile corre rre raw";
	init_format_change();
	usingHextile = False;
	if (! appData.encodingsString) {
		appDataNew.encodingsString = strdup(prefZRLE);
		usingZRLE = True;
		fprintf(stderr, "prefer: ZRLE\n");
	} else {
		char *t, *z;
		static int first = 1;
		t = strstr(appData.encodingsString, "tight");
		z = strstr(appData.encodingsString, "zrle");
		if (first && usingZRLE) {
			appDataNew.encodingsString = strdup(prefTight);
			usingZRLE = False;
			usingZYWRLE = False;
		} else if (! t) {
			appDataNew.encodingsString = strdup(prefZRLE);
			usingZRLE = True;
			fprintf(stderr, "prefer: ZRLE\n");
		} else if (! z) {
			appDataNew.encodingsString = strdup(prefTight);
			usingZRLE = False;
			usingZYWRLE = False;
			skip_maybe_sync = 0;
			fprintf(stderr, "prefer: Tight\n");
		} else {
			if (t < z) {
				appDataNew.encodingsString = strdup(prefZRLE);
				usingZRLE = True;
				fprintf(stderr, "prefer: ZRLE\n");
			} else {
				appDataNew.encodingsString = strdup(prefTight);
				usingZRLE = False;
				usingZYWRLE = False;
				skip_maybe_sync = 0;
				fprintf(stderr, "prefer: Tight\n");
			}
		}
		first = 0;
	}
	schedule_format_change();
	if (w || ev || params || num_params) {}
}

void
ToggleZRLEZYWRLE(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char prefZYWRLE[] = "copyrect zywrle zrle tight zlib hextile corre rre raw";
	char prefZRLE[]   = "copyrect zrle zywrle tight zlib hextile corre rre raw";
	init_format_change();
	usingZRLE = True;
	usingHextile = False;
	if (! appData.encodingsString) {
		appDataNew.encodingsString = strdup(prefZYWRLE);
		usingZYWRLE = True;
		fprintf(stderr, "prefer: ZYWRLE\n");
	} else {
		char *z, *w;
		w = strstr(appData.encodingsString, "zywrle");
		z = strstr(appData.encodingsString, "zrle");
		if (usingZYWRLE) {
			appDataNew.encodingsString = strdup(prefZRLE);
			fprintf(stderr, "prefer: ZRLE\n");
			usingZYWRLE = False;
			skip_maybe_sync = 0;
		} else {
			appDataNew.encodingsString = strdup(prefZYWRLE);
			fprintf(stderr, "prefer: ZYWRLE\n");
			usingZYWRLE = True;
		}
	}
	schedule_format_change();
	if (w || ev || params || num_params) {}
}

void
ToggleTightHextile(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char prefTight[]   = "copyrect tight zrle zywrle zlib hextile corre rre raw";
	char prefHextile[] = "copyrect hextile tight zrle zywrle zlib corre rre raw";
	init_format_change();
	usingZRLE = False;
	usingZYWRLE = False;
	if (! appData.encodingsString) {
		appDataNew.encodingsString = strdup(prefHextile);
		usingHextile = True;
		fprintf(stderr, "prefer: Hextile\n");
	} else {
		char *t, *z;
		static int first = 1;
		t = strstr(appData.encodingsString, "tight");
		z = strstr(appData.encodingsString, "hextile");
		if (first && usingHextile) {
			appDataNew.encodingsString = strdup(prefTight);
			usingHextile = False;
		} else if (! t) {
			appDataNew.encodingsString = strdup(prefHextile);
			usingHextile = True;
			fprintf(stderr, "prefer: Hextile\n");
		} else if (! z) {
			appDataNew.encodingsString = strdup(prefTight);
			usingHextile = False;
			skip_maybe_sync = 0;
			fprintf(stderr, "prefer: Tight\n");
		} else {
			if (t < z) {
				appDataNew.encodingsString = strdup(prefHextile);
				usingHextile = True;
				fprintf(stderr, "prefer: Hextile\n");
			} else {
				appDataNew.encodingsString = strdup(prefTight);
				usingHextile = False;
				skip_maybe_sync = 0;
				fprintf(stderr, "prefer: Tight\n");
			}
		}
		first = 0;
	}
	schedule_format_change();
	if (w || ev || params || num_params) {}
}

void scale_check_zrle(void) {
	static int didit = 0;
	if (didit) {
		return;
	}
	didit = 1;
	if (getenv("SSVNC_PRESERVE_ENCODING")) {
		return;
	}
	if (!usingZRLE && !usingHextile) {
		Widget w = 0;
		fprintf(stderr, "\nSwitching to faster ZRLE encoding in client-side scaling mode.\n");
		fprintf(stderr, "Switch back to Tight via the Popup menu if you prefer it.\n\n");
		ToggleTightZRLE(w, NULL, NULL, NULL);
	}
}

/*
 * ToggleViewOnly
 */

void
ToggleViewOnly(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.viewOnly) {
		appData.viewOnly = False;
		fprintf(stderr, "viewonly: off\n");
	} else {
		appData.viewOnly = True;
		fprintf(stderr, "viewonly: on\n");
	}
	Xcursors(1);
	if (w || ev || params || num_params) {}
}

void
ToggleCursorShape(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	init_format_change();
	if (appData.useRemoteCursor) {
		appDataNew.useRemoteCursor = False;
		fprintf(stderr, "useRemoteCursor: off\n");
	} else {
		appDataNew.useRemoteCursor = True;
		fprintf(stderr, "useRemoteCursor: on\n");
	}
	schedule_format_change();
	if (!appDataNew.useRemoteCursor) {
		do_cursor_change = 1;
	} else {
		do_cursor_change = -1;
	}
	if (w || ev || params || num_params) {}
}

void
ToggleCursorAlpha(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useCursorAlpha) {
		appData.useCursorAlpha = False;
		fprintf(stderr, "useCursorAlpha: off\n");
	} else {
		appData.useCursorAlpha = True;
		fprintf(stderr, "useCursorAlpha: on\n");
	}
	if (w || ev || params || num_params) {}
}

void
ToggleX11Cursor(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	init_format_change();
	if (appData.useX11Cursor) {
		appDataNew.useX11Cursor = False;
		fprintf(stderr, "useX11Cursor: off\n");
	} else {
		appDataNew.useX11Cursor = True;
		fprintf(stderr, "useX11Cursor: on\n");
	}
	schedule_format_change();
	do_cursor_change = 1;
	if (w || ev || params || num_params) {}
}

void
ToggleBell(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useBell) {
		appData.useBell = False;
		fprintf(stderr, "useBell: off\n");
	} else {
		appData.useBell = True;
		fprintf(stderr, "useBell: on\n");
	}
	if (w || ev || params || num_params) {}
}

void
ToggleRawLocal(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	init_format_change();
	if (appData.useRawLocal) {
		appDataNew.useRawLocal = False;
		fprintf(stderr, "useRawLocal: off\n");
	} else {
		appDataNew.useRawLocal = True;
		fprintf(stderr, "useRawLocal: on\n");
	}
	schedule_format_change();
	if (w || ev || params || num_params) {}
}

void
ToggleServerInput(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.serverInput) {
		appData.serverInput= False;
		fprintf(stderr, "serverInput: off\n");
		SendServerInput(True);
	} else {
		appData.serverInput = True;
		fprintf(stderr, "serverInput: on\n");
		SendServerInput(False);
	}
	if (w || ev || params || num_params) {}
}

void
TogglePipelineUpdates(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.pipelineUpdates) {
		appData.pipelineUpdates= False;
		fprintf(stderr, "pipeline-update: off\n");
	} else {
		appData.pipelineUpdates = True;
		fprintf(stderr, "pipeline-update: on\n");
	}
	/* XXX request one to be sure? */
	if (w || ev || params || num_params) {}
}

void
ToggleSendClipboard(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.sendClipboard) {
		appData.sendClipboard= False;
		fprintf(stderr, "Send CLIPBOARD Selection: off (send PRIMARY instead)\n");
	} else {
		appData.sendClipboard = True;
		fprintf(stderr, "Send CLIPBOARD Selection: on (do not send PRIMARY)\n");
	}
	if (w || ev || params || num_params) {}
}

void
ToggleSendAlways(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.sendAlways) {
		appData.sendAlways= False;
		fprintf(stderr, "Send Selection Always: off\n");
	} else {
		appData.sendAlways = True;
		fprintf(stderr, "Send Selection Always: on\n");
	}
	if (w || ev || params || num_params) {}
}


Bool _sw1_ = False;	/* XXX this is a weird bug... */
Bool _sw2_ = False;
Bool _sw3_ = False;
Bool selectingSingleWindow = False;

extern Cursor bogoCursor;

void
ToggleSingleWindow(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.singleWindow) {
		appData.singleWindow= False;
		fprintf(stderr, "singleWindow: off\n");
		SendSingleWindow(-1, -1);
	} else {
		appData.singleWindow = True;
		selectingSingleWindow = True;
		fprintf(stderr, "singleWindow: on\n");
		if (bogoCursor != None) {
			XDefineCursor(dpy, desktopWin, bogoCursor);
		}
	}
	if (w || ev || params || num_params) {}
}

void raiseme(int force);
void AppendChatInput(char *);

extern void ShowChat(Widget w, XEvent *event, String *params, Cardinal *num_params);
extern void ShowFile(Widget w, XEvent *event, String *params, Cardinal *num_params);
extern Bool SendTextChatFinished(void);


void printChat(char *str, Bool raise) {
	if (appData.termChat) {
		if (raise) {
			raiseme(0);
		}
		fputs(str, stderr);
	} else {
		if (raise) {
			ShowChat(0, 0, 0, 0);
		}
		AppendChatInput(str);
	}
}

void
ToggleTextChat(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.chatActive) {
		printChat("\n*SentClose*\n\n", False);
		SendTextChatClose();
		SendTextChatFinished();
		HideChat(0, NULL, NULL, NULL);
		appData.chatActive= False;
	} else {
		ShowChat(0, 0, 0, 0);
		SendTextChatOpen();
		if (appData.termChat) {
			printChat("\n*SentOpen*\n\nSend: ", True);
		} else {
			printChat("\n*SentOpen*\n", True);
		}
		appData.chatActive = True;
	}
	if (w || ev || params || num_params) {}
}

extern int filexfer_sock;
extern pid_t java_helper;
#define KILLJAVA
#ifdef KILLJAVA
#include <signal.h>
#endif

void
ToggleFileXfer(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	static double last_start = 0.0;
	if (appData.fileActive) {
#if 0
		HideFile(w, ev, params, num_params);
		appData.fileActive = False;
#endif
#ifndef KILLJAVA
		if (filexfer_sock >= 0) {
			close(filexfer_sock);
		}
#else
		if (java_helper != 0) {
			int i;
			if (dnow() < last_start + 6.0) {
				fprintf(stderr, "skipping early kill of java helper (less than 5 secs)\n");
			} else {
				for (i=1; i<=5; i++) {
					pid_t p = java_helper + i;
					fprintf(stderr, "trying to kill java helper: %d\n", p);
					if (kill(p, SIGTERM) == 0) {
						java_helper = 0;
						break;
					}
				}
			}
		}
#endif
	} else {
		ShowFile(w, ev, params, num_params);
		appData.fileActive = True;
		last_start = dnow();
	}
	if (w || ev || params || num_params) {}
}

static int fooHandler(Display *dpy, XErrorEvent *error) {
	if (dpy || error) {}
	return 0;
}

void raiseme(int force) {
	if ((force || appData.termChat) && getenv("WINDOWID")) {
		unsigned long w;
		if (sscanf(getenv("WINDOWID"), "%lu", &w) == 1)  {
			;
		} else if (sscanf(getenv("WINDOWID"), "0x%lx", &w) == 1)  {
			;
		} else {
			w = 0;
		}
		if (w != 0) {
			XErrorHandler old = XSetErrorHandler(fooHandler);
			XMapRaised(dpy, (Window) w);
			XSync(dpy, False);
			XSetErrorHandler(old);
		}
	}
}

void set_server_scale(int n) {
	if (n >= 1 && n < 100) {
		int w = si.framebufferWidth;
		int h = si.framebufferHeight;
		appData.serverScale = n;
		SendServerScale(n);
		if (0) SendFramebufferUpdateRequest(0, 0, w, h, False);
		schedule_fb_update();
	}
}

void
DoServerScale(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char str[100], *s, *q;
	int n;
	if (1) {
		s = DoScaleNDialog();
	} else {
		raiseme(1);
		fprintf(stderr, "\n\n\a\nEnter integer n for 1/n server scaling: ");
		str[0] = '\0';
		fgets(str, 100, stdin); 
		s = str;
		q = strstr(str, "\n");
		if (q) *q = '\0';
	}
	if (s[0] != '\0') {
		n = atoi(s);
		set_server_scale(n);
	}
	if (w || ev || params || num_params) {}
}

void set_server_quality(int n) {
	fprintf(stderr, "set_quality: %d\n", n);
	if (n >= 0 && n <= 9) {
		int w = si.framebufferWidth;
		int h = si.framebufferHeight;
		init_format_change();
		appDataNew.qualityLevel = n;
		SendFramebufferUpdateRequest(0, 0, w, h, False);
		schedule_format_change();
	}
}

void
DoServerQuality(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char str[100], *s, *q;
	int n;
	if (1) {
		s = DoQualityDialog();
	} else {
		raiseme(1);
		fprintf(stderr, "\n\n\a\nEnter integer 1 <= n <= 9 for quality setting: ");
		str[0] = '\0';
		fgets(str, 100, stdin); 
		s = str;
		q = strstr(str, "\n");
		if (q) *q = '\0';
	}
	if (s[0] != '\0') {
		n = atoi(s);
		set_server_quality(n);
	}
	if (w || ev || params || num_params) {}
}

void set_server_compress(int n) {
	fprintf(stderr, "set_compress: %d\n", n);
	if (n >= 0 && n <= 9) {
		int w = si.framebufferWidth;
		int h = si.framebufferHeight;
		init_format_change();
		appDataNew.compressLevel = n;
		SendFramebufferUpdateRequest(0, 0, w, h, False);
		schedule_format_change();
	}
}

void
DoServerCompress(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char str[100], *s, *q;
	int n;
	if (1) {
		s = DoCompressDialog();
	} else {
		raiseme(1);
		fprintf(stderr, "\n\n\a\nEnter integer 1 <= n <= 9 for compress level setting: ");
		str[0] = '\0';
		fgets(str, 100, stdin); 
		s = str;
		q = strstr(str, "\n");
		if (q) *q = '\0';
	}
	if (s[0] != '\0') {
		n = atoi(s);
		set_server_compress(n);
	}
	if (w || ev || params || num_params) {}
}

extern void rescale_image(void);
extern void get_scale_values(double *fx, double *fy);

void
SetScale(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char *s;
	s = DoScaleDialog();
	if (s[0] != '\0') {
#if 0
		int w = si.framebufferWidth;
		int h = si.framebufferHeight;
#endif
		double fx, fy;
		int fs = 0;
		if (appData.scale != NULL && !strcmp(s, appData.scale)) {
			return;
		}
		
		if (!strcasecmp(s, "none")) {
			appData.scale = NULL;
		} else if (!strcmp(s, "1.0")) {
			appData.scale = NULL;
		} else if (!strcmp(s, "1")) {
			appData.scale = NULL;
		} else {
			appData.scale = strdup(s);
		}
		if (appData.scale != NULL) {
			get_scale_values(&fx, &fy);
			if (fx <= 0.0 || fy <= 0.0) {
				appData.scale = NULL;
				return;
			}
		}
		
		if (appData.fullScreen) {
			fs = 1;
			FullScreenOff();
		}
		rescale_image();
		if (fs) {
			FullScreenOn();
		}
	}
	if (w || ev || params || num_params) {}
}

void
SetEscapeKeys(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char *s;
	s = DoEscapeKeysDialog();
	fprintf(stderr, "set escape keys: '%s'\n", s);
	if (s[0] != '\0') {
		appData.escapeKeys = strdup(s);
	}
	if (w || ev || params || num_params) {}
}

void set_ycrop(int n) {
	if (n >= 1) {
		int w = si.framebufferWidth;
		int h = si.framebufferHeight;
		appData.yCrop = n;
		ReDoDesktop();
		SendFramebufferUpdateRequest(0, 0, w, h, False);
		schedule_fb_update();
	}
}

void
SetYCrop(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char str[100], *q, *s;
	int n;
	if (1) {
		s = DoYCropDialog();
	} else {
		raiseme(1);
		fprintf(stderr, "\n\n\a\nEnter pixel size n -ycrop maximum y-height: ");
		str[0] = '\0';
		fgets(str, 100, stdin); 
		s = str;
		q = strstr(str, "\n");
		if (q) *q = '\0';
	}
	if (s[0] != '\0') {
		n = atoi(s);
		set_ycrop(n);
	}
	if (w || ev || params || num_params) {}
}

void set_scbar(int n) {
	if (n >= 1) {
		int w = si.framebufferWidth;
		int h = si.framebufferHeight;
fprintf(stderr, "set_scbat: %d\n", n);
		appData.sbWidth = n;
		ReDoDesktop();
		SendFramebufferUpdateRequest(0, 0, w, h, False);
		schedule_fb_update();
	}
}

void
SetScbar(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	char str[100], *q, *s;
	int n;
	if (1) {
		s = DoScbarDialog();
	} else {
		raiseme(1);
		fprintf(stderr, "\n\n\a\nEnter pixel size n scrollbar width: ");
		str[0] = '\0';
		fgets(str, 100, stdin); 
		s = str;
		q = strstr(str, "\n");
		if (q) *q = '\0';
	}
	if (s[0] != '\0') {
		n = atoi(s);
		set_scbar(n);
	}
	if (w || ev || params || num_params) {}
}

void
SetScaleN(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (*num_params != 0) {
		int n = atoi(params[0]);
		set_server_scale(n);
	}
	if (w || ev || params || num_params) {}
}

void UpdateQual(void) {
	SetFormatAndEncodings();
	UpdateSubsampButtons();
	UpdateQualSlider();
}

extern double latency;

static void LosslessRefresh(void) {
	String encodings = appData.encodingsString;
	int compressLevel = appData.compressLevel;
	int qual = appData.qualityLevel;
	Bool enableJPEG = appData.enableJPEG;
	appData.qualityLevel = -1;
	appData.enableJPEG = False;
	appData.encodingsString = "tight copyrect";
	appData.compressLevel = 1;
	SetFormatAndEncodings();
	SendFramebufferUpdateRequest(0, 0, si.framebufferWidth, si.framebufferHeight, False);
	if (latency > 0.0) {
		if (0) usleep((int) latency * 1000);
	}
	appData.qualityLevel = qual;
	appData.enableJPEG = enableJPEG;
	appData.encodingsString = encodings;
	appData.compressLevel = compressLevel;
	SetFormatAndEncodings();
}

static void QualHigh(void) {
	appData.encodingsString = "tight copyrect";
	if(appData.useBGR233 || appDataNew.useBGR565) {
		fprintf(stderr, "WARNING: Cannot enable JPEG because BGR233/BGR565 is enabled.\n");
	} else {
		appData.enableJPEG = True;
	}
	appData.subsampLevel = TVNC_1X;
	appData.qualityLevel = 95;
	UpdateQual();
}

static void QualMed(void) {
	appData.encodingsString = "tight copyrect";
	if(appData.useBGR233 || appDataNew.useBGR565) {
		fprintf(stderr, "WARNING: Cannot enable JPEG because BGR233/BGR565 is enabled.\n");
	} else {
		appData.enableJPEG = True;
	}
	appData.subsampLevel = TVNC_2X;
	appData.qualityLevel = 80;
	UpdateQual();
}

static void QualLow(void) {
	appData.encodingsString = "tight copyrect";
	if(appData.useBGR233 || appDataNew.useBGR565) {
		fprintf(stderr, "WARNING: Cannot enable JPEG because BGR233/BGR565 is enabled.\n");
	} else {
		appData.enableJPEG = True;
	}
	appData.subsampLevel = TVNC_4X;
	appData.qualityLevel = 30;
	UpdateQual();
}

static void QualLossless(void) {
	appData.encodingsString = "tight copyrect";
	appData.enableJPEG = False;
	appData.compressLevel = 0;
	UpdateQual();
}

static void QualLosslessWAN(void) {
	appData.encodingsString = "tight copyrect";
	appData.enableJPEG = False;
	appData.compressLevel = 1;
	UpdateQual();
}

void
SetTurboVNC(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (*num_params != 0) {
		int n = atoi(params[0]);
		if (0) fprintf(stderr, "SetTurboVNC: %d\n", n);
		if (n == 1) {
			QualHigh();
		} else if (n == 2) {
			QualMed();
		} else if (n == 3) {
			QualLow();
		} else if (n == 4) {
			QualLossless();
		} else if (n == 5) {
			QualLosslessWAN();
		} else if (n == 6) {
			appData.subsampLevel = TVNC_1X;
			UpdateQual();
		} else if (n == 7) {
			appData.subsampLevel = TVNC_2X;
			UpdateQual();
		} else if (n == 8) {
			appData.subsampLevel = TVNC_4X;
			UpdateQual();
		} else if (n == 9) {
			appData.subsampLevel = TVNC_GRAY;
			UpdateQual();
		} else if (n == 10) {
			LosslessRefresh();
		}
	}
	if (w || ev || params || num_params) {}
}

void
SetQuality(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (*num_params != 0) {
		int n = atoi(params[0]);
		set_server_quality(n);
	}
	if (w || ev || params || num_params) {}
}

void
SetCompress(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (*num_params != 0) {
		int n = atoi(params[0]);
		set_server_compress(n);
	}
	if (w || ev || params || num_params) {}
}

void
GotChatText(char *str, int len)
{
	static char *b = NULL;
	static int blen = -1;
	int i, k;
	if (appData.termChat) {
		printChat("\nChat: ", True);
	} else {
		printChat("Chat: ", True);
	}

	if (len < 0) len = 0;

	if (blen < len+1) {
		if (b) free(b);
		blen = 2 * (len + 10);
		b = (char *) malloc(blen);
	}

	k = 0;
	for (i=0; i < len; i++) {
		if (str[i] != '\r') {
			b[k++] = str[i];
		}
	}
	b[k] = '\0';
	b[len] = '\0';
	printChat(b, True);
	
	if (appData.termChat) {
		if (strstr(str, "\n")) {
			printChat("Send: ", True);
		} else {
			printChat("\nSend: ", True);
		}
	}
}

void
SetViewOnlyState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.viewOnly) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetNOJPEGState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.enableJPEG) {
		XtVaSetValues(w, XtNstate, False, NULL);
	} else {
		XtVaSetValues(w, XtNstate, True, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetQualityState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (*num_params != 0) {
		int n = atoi(params[0]);
		if (appData.qualityLevel == n) {
			XtVaSetValues(w, XtNstate, True, NULL);
		} else {
			XtVaSetValues(w, XtNstate, False, NULL);
		}
	}
	if (w || ev || params || num_params) {}
}

void
SetCompressState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (*num_params != 0) {
		int n = atoi(params[0]);
		if (appData.compressLevel == n) {
			XtVaSetValues(w, XtNstate, True, NULL);
		} else {
			XtVaSetValues(w, XtNstate, False, NULL);
		}
	}
	if (w || ev || params || num_params) {}
}

void
SetScaleNState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (*num_params != 0) {
		int n = atoi(params[0]);
		if (appData.serverScale == n || (appData.serverScale >= 6 && n >= 6)) {
			XtVaSetValues(w, XtNstate, True, NULL);
		} else {
			XtVaSetValues(w, XtNstate, False, NULL);
		}
	}
	if (w || ev || params || num_params) {}
}

void
Set8bppState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useBGR233) {
		XtVaSetValues(w, XtNstate, True, NULL);
		if (b16   != NULL) {
			XtVaSetValues(b16,   XtNstate, False, NULL);
		}
		if (bfull != NULL) {
			XtVaSetValues(bfull, XtNstate, False, NULL);
		}
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
Set16bppState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useBGR565) {
		XtVaSetValues(w, XtNstate, True, NULL);
		if (b8    != NULL) {
			XtVaSetValues(b8,    XtNstate, False, NULL);
		}
		if (bfull != NULL) {
			XtVaSetValues(bfull, XtNstate, False, NULL);
		}
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetFullColorState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useBGR565 || appData.useBGR233) {
		XtVaSetValues(w, XtNstate, False, NULL);
	} else {
		XtVaSetValues(w, XtNstate, True, NULL);
		if (b8  != NULL) {
			XtVaSetValues(b8,  XtNstate, False, NULL);
		}
		if (b16 != NULL) {
			XtVaSetValues(b16, XtNstate, False, NULL);
		}
	}
	if (w || ev || params || num_params) {}
}

void
SetXGrabState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.grabAll) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetEscapeKeysState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.escapeActive) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
Set256ColorsState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useBGR233 == 256) {
		XtVaSetValues(w, XtNstate, True, NULL);
		if (w64  != NULL) {
			XtVaSetValues(w64 , XtNstate, False, NULL);
		}
		if (w8   != NULL) {
			XtVaSetValues(w8  , XtNstate, False, NULL);
		}
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
Set64ColorsState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useBGR233 == 64) {
		XtVaSetValues(w, XtNstate, True, NULL);
		if (w256 != NULL) {
			XtVaSetValues(w256, XtNstate, False, NULL);
		}
		if (w8   != NULL) {
			XtVaSetValues(w8  , XtNstate, False, NULL);
		}
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
Set8ColorsState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useBGR233 == 8) {
		XtVaSetValues(w, XtNstate, True, NULL);
		if (w256 != NULL) {
			XtVaSetValues(w256, XtNstate, False, NULL);
		}
		if (w64  != NULL) {
			XtVaSetValues(w64 , XtNstate, False, NULL);
		}
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetGreyScaleState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useGreyScale) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

static void init_state(void) {
	static int first = 1;
	if (first && appData.encodingsString) {
		char *t, *z, *y, *h;
		char *str = appData.encodingsString;
		int len = strlen(str);

		t = strstr(str, "tight");
		z = strstr(str, "zrle");
		y = strstr(str, "zywrle");
		h = strstr(str, "hextile");

		if (!t) t = str + len;
		if (!z) z = str + len;
		if (!y) y = str + len;
		if (!h) h = str + len;

		usingZRLE = False;
		usingZYWRLE = False;
		usingHextile = False;

		if (t < z && t < y && t < h) {
			;
		} else if (z < t && z < y && z < h) {
			usingZRLE = True;
		} else if (y < t && y < z && y < h) {
			usingZYWRLE = True;
			usingZRLE = True;
		} else if (h < t && h < z && h < y) {
			usingHextile = True;
		}
	}
	first = 0;

}

void
SetZRLEState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	init_state();
	if (usingZRLE) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetHextileState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	init_state();
	if (usingHextile) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetZYWRLEState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	init_state();
	if (usingZYWRLE) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetCursorShapeState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useRemoteCursor) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetCursorAlphaState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useCursorAlpha) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetX11CursorState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useX11Cursor) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetBellState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useBell) {
		XtVaSetValues(w, XtNstate, False, NULL);
	} else {
		XtVaSetValues(w, XtNstate, True, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetRawLocalState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.useRawLocal) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetServerInputState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (!appData.serverInput) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetPipelineUpdates(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (!appData.pipelineUpdates) {
		XtVaSetValues(w, XtNstate, False, NULL);
	} else {
		XtVaSetValues(w, XtNstate, True, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetSendClipboard(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (!appData.sendClipboard) {
		XtVaSetValues(w, XtNstate, False, NULL);
	} else {
		XtVaSetValues(w, XtNstate, True, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetSendAlways(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (!appData.sendAlways) {
		XtVaSetValues(w, XtNstate, False, NULL);
	} else {
		XtVaSetValues(w, XtNstate, True, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetSingleWindowState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.singleWindow) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetTextChatState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.chatActive) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}

void
SetFileXferState(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
	if (appData.fileActive) {
		XtVaSetValues(w, XtNstate, True, NULL);
	} else {
		XtVaSetValues(w, XtNstate, False, NULL);
	}
	if (w || ev || params || num_params) {}
}
