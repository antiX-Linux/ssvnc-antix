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
 * popup.c - functions to deal with popup window.
 */

#include "vncviewer.h"
#include <time.h>
#include <sys/wait.h>

#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Toggle.h>

#include <X11/Xaw/Box.h>
#include <X11/Xaw/Scrollbar.h>

Widget popup, fullScreenToggle;

Bool SendTextChatFinished(void);

void popupFixer(Widget wid) {
	Window rr, cr; 
	unsigned int m;
  	int x0 = 500, y0 = 500;
	int xr, yr, wxr, wyr;
	Dimension ph;
	if (XQueryPointer(dpy, DefaultRootWindow(dpy), &rr, &cr, &xr, &yr, &wxr, &wyr, &m)) {
		x0 = xr;
		y0 = yr;
	}
  	XtPopup(wid, XtGrabNone);
	XtVaGetValues(wid, XtNheight, &ph, NULL);
	if (y0 + (int) ph > dpyHeight) {
		y0 = dpyHeight - (int) ph;
		if (y0 < 0) {
			y0 = 0;
		}
	}
	XtMoveWidget(wid, x0, y0);
}

void Noop(Widget w, XEvent *event, String *params, Cardinal *num_params) {
	if (0) fprintf(stderr, "No-op\n");
	if (w || event || params || num_params) {}
}

void
ShowPopup(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	if (appData.popupFix) {
		popupFixer(popup);
	} else {
		XtMoveWidget(popup, event->xbutton.x_root, event->xbutton.y_root);
		XtPopup(popup, XtGrabNone);
	}
	if (appData.grabAll) {
		XSync(dpy, False);
		XRaiseWindow(dpy, XtWindow(popup));
	}
	XSetWMProtocols(dpy, XtWindow(popup), &wmDeleteWindow, 1);
	XtOverrideTranslations(popup, XtParseTranslationTable ("<Message>WM_PROTOCOLS: HidePopup()"));
	if (w || event || params || num_params) {}
}

void
HidePopup(Widget w, XEvent *event, String *params, Cardinal *num_params) {
	XtPopdown(popup);
	if (w || event || params || num_params) {}
}


static XtResource resources[] = {
  {
    "type", "Type", XtRString, sizeof(String), 0, XtRString,
    (XtPointer) "command",
  },
};

void
CreatePopup() {
	Widget buttonForm1, buttonForm2, twoForm, button = 0, prevButton = NULL;
	int i;
	char buttonName[12];
	String buttonType;

	popup = XtVaCreatePopupShell("popup", transientShellWidgetClass, toplevel, NULL);

	twoForm = XtVaCreateManagedWidget("buttonForm", formWidgetClass, popup, NULL);
	buttonForm1 = XtVaCreateManagedWidget("buttonForm", formWidgetClass, twoForm, NULL);
	buttonForm2 = XtVaCreateManagedWidget("buttonForm", formWidgetClass, twoForm, XtNfromHoriz, (XtArgVal) buttonForm1, NULL);

	if (appData.popupButtonCount > 100) {
		fprintf(stderr,"Too many popup buttons\n");
		exit(1);
	}

	for (i = 1; i <= appData.popupButtonCount; i++) {
		Widget bform;
		sprintf(buttonName, "button%d", i);

		if (i <= appData.popupButtonBreak) {
			bform = buttonForm1;
		} else {
			if (i == appData.popupButtonBreak+1) {
				prevButton = NULL;
			}
			bform = buttonForm2;
		}
		XtVaGetSubresources(bform, (XtPointer)&buttonType, buttonName, "Button", resources, 1, NULL);

		if (strcmp(buttonType, "command") == 0) {
			button = XtVaCreateManagedWidget(buttonName, commandWidgetClass, bform, NULL);
			XtVaSetValues(button, XtNfromVert, prevButton, XtNleft, XawChainLeft, XtNright, XawChainRight, NULL);
		} else if (strcmp(buttonType, "toggle") == 0) {
			button = XtVaCreateManagedWidget(buttonName, toggleWidgetClass, bform, NULL);
			XtVaSetValues(button, XtNfromVert, prevButton, XtNleft, XawChainLeft, XtNright, XawChainRight, NULL);
		} else {
			fprintf(stderr,"unknown button type '%s'\n", buttonType);
		}
		prevButton = button;
	}
}


Widget scaleN;

void
ShowScaleN(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (appData.popupFix) {
	popupFixer(scaleN);
  } else {
	XtMoveWidget(scaleN, event->xbutton.x_root, event->xbutton.y_root);
  	XtPopup(scaleN, XtGrabNone);
  }
  if (appData.grabAll) {
  	XRaiseWindow(dpy, XtWindow(scaleN));
  }
  XSetWMProtocols(dpy, XtWindow(scaleN), &wmDeleteWindow, 1);
  XtOverrideTranslations(scaleN, XtParseTranslationTable ("<Message>WM_PROTOCOLS: HideScaleN()"));
	if (w || event || params || num_params) {}
}

void
HideScaleN(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  XtPopdown(scaleN);
	if (w || event || params || num_params) {}
}


void
CreateScaleN()
{
  Widget buttonForm, button, prevButton = NULL;
  int i;
  char buttonName[32];
  String buttonType;

  scaleN = XtVaCreatePopupShell("scaleN", transientShellWidgetClass, toplevel,
			       NULL);

  buttonForm = XtVaCreateManagedWidget("buttonForm", formWidgetClass, scaleN,
				       NULL);

  for (i = 0; i <= 6; i++) {
    sprintf(buttonName, "button%d", i);
    XtVaGetSubresources(buttonForm, (XtPointer)&buttonType, buttonName,
			"Button", resources, 1, NULL);

    button = XtVaCreateManagedWidget(buttonName, toggleWidgetClass,
				       buttonForm, NULL);
    XtVaSetValues(button, XtNfromVert, prevButton,
		    XtNleft, XawChainLeft, XtNright, XawChainRight, NULL);
    prevButton = button;
  }
}

Widget turbovncW;

static Widget turboButtons[32];

Widget qualtext, qualslider;

void UpdateQualSlider(void) {
#ifdef TURBOVNC
	char text[16];
	XawScrollbarSetThumb(qualslider, (float)appData.qualityLevel/100., 0.);
	sprintf(text, "%3d", appData.qualityLevel);
	XtVaSetValues(qualtext, XtNlabel, text, NULL);
#endif
}

void qualScrollProc(Widget w, XtPointer client, XtPointer p) {
#ifdef TURBOVNC
	float size, val;  int qual, pos=(int)p;
	XtVaGetValues(w, XtNshown, &size, XtNtopOfThumb, &val, 0);
	if(pos<0) val-=.1;  else val+=.1;
	qual=(int)(val*100.);  if(qual<1) qual=1;  if(qual>100) qual=100;
	XawScrollbarSetThumb(w, val, 0.);
	appData.qualityLevel=qual;
	UpdateQual();
#endif
	if (w || client || p) {}
}

void qualJumpProc(Widget w, XtPointer client, XtPointer p) {
#ifdef TURBOVNC
	float val=*(float *)p;  int qual;
	qual=(int)(val*100.);  if(qual<1) qual=1;  if(qual>100) qual=100;
	appData.qualityLevel=qual;
	UpdateQual();
#endif
	if (w || client || p) {}
}

void UpdateSubsampButtons(void) {
#ifdef TURBOVNC
	int i;
	for (i=7; i <= 10; i++) {
		XtVaSetValues(turboButtons[i], XtNstate, 0, NULL);
	}
	if (appData.subsampLevel==TVNC_1X) {
		i = 7;
	} else if (appData.subsampLevel==TVNC_2X) {
		i = 8;
	} else if (appData.subsampLevel==TVNC_4X) {
		i = 9;
	} else if (appData.subsampLevel==TVNC_GRAY) {
		i = 10;
	} else {
		return;
	}
	XtVaSetValues(turboButtons[i], XtNstate, 1, NULL);
#endif
}

void
ShowTurboVNC(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	UpdateSubsampButtons(); 
	UpdateQualSlider();
	if (appData.popupFix) {
		popupFixer(turbovncW);
	} else {
		XtMoveWidget(turbovncW, event->xbutton.x_root, event->xbutton.y_root);
		XtPopup(turbovncW, XtGrabNone);
	}
	if (appData.grabAll) {
		XRaiseWindow(dpy, XtWindow(turbovncW));
	}
	XSetWMProtocols(dpy, XtWindow(turbovncW), &wmDeleteWindow, 1);
	XtOverrideTranslations(turbovncW, XtParseTranslationTable ("<Message>WM_PROTOCOLS: HideTurboVNC()"));
	if (w || event || params || num_params) {}
}

void
HideTurboVNC(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  XtPopdown(turbovncW);
	if (w || event || params || num_params) {}
}

void
CreateTurboVNC() {
	Widget buttonForm, button, prevButton = NULL;
	Widget label;
	int i;
	char buttonName[32];
	String buttonType;

	turbovncW = XtVaCreatePopupShell("turboVNC", transientShellWidgetClass, toplevel, NULL);

	buttonForm = XtVaCreateManagedWidget("buttonForm", formWidgetClass, turbovncW, NULL);

	for (i = 0; i <= 12; i++) {
		sprintf(buttonName, "button%d", i);
#ifndef TURBOVNC
		if (i == 0) {
			sprintf(buttonName, "buttonNone");
		} else if (i > 0) {
			return;
		}
#endif
		XtVaGetSubresources(buttonForm, (XtPointer)&buttonType, buttonName,
				"Button", resources, 1, NULL);

		button = XtVaCreateManagedWidget(buttonName, toggleWidgetClass,
					       buttonForm, NULL);
		turboButtons[i] = button;
		XtVaSetValues(button, XtNfromVert, prevButton,
			    XtNleft, XawChainLeft, XtNright, XawChainRight, NULL);
		prevButton = button;
	}

	label = XtCreateManagedWidget("qualLabel", toggleWidgetClass, buttonForm, NULL, 0);
	XtVaSetValues(label, XtNfromVert, prevButton, XtNleft, XawChainLeft, XtNright, XawChainRight, NULL);

	qualslider = XtCreateManagedWidget("qualBar", scrollbarWidgetClass, buttonForm, NULL, 0);
	XtVaSetValues(qualslider, XtNfromVert, label, XtNleft, XawChainLeft, NULL);
	XtAddCallback(qualslider, XtNscrollProc, qualScrollProc, NULL) ;
	XtAddCallback(qualslider, XtNjumpProc, qualJumpProc, NULL) ;

	qualtext = XtCreateManagedWidget("qualText", labelWidgetClass, buttonForm, NULL, 0);
	XtVaSetValues(qualtext, XtNfromVert, label, XtNfromHoriz, qualslider, XtNright, XawChainRight, NULL);
}

Widget qualityW;

void
ShowQuality(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (appData.popupFix) {
	popupFixer(qualityW);
  } else {
	XtMoveWidget(qualityW, event->xbutton.x_root, event->xbutton.y_root);
  	XtPopup(qualityW, XtGrabNone);
  }
  if (appData.grabAll) {
  	XRaiseWindow(dpy, XtWindow(qualityW));
  }
  XSetWMProtocols(dpy, XtWindow(qualityW), &wmDeleteWindow, 1);
  XtOverrideTranslations(qualityW, XtParseTranslationTable ("<Message>WM_PROTOCOLS: HideQuality()"));
	if (w || event || params || num_params) {}
}

void
HideQuality(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  XtPopdown(qualityW);
	if (w || event || params || num_params) {}
}


void
CreateQuality()
{
  Widget buttonForm, button, prevButton = NULL;
  int i;
  char buttonName[32];
  String buttonType;

  qualityW = XtVaCreatePopupShell("quality", transientShellWidgetClass, toplevel,
			       NULL);

  buttonForm = XtVaCreateManagedWidget("buttonForm", formWidgetClass, qualityW,
				       NULL);

  for (i = -1; i <= 9; i++) {
    if (i < 0) {
	sprintf(buttonName, "buttonD");
    } else {
	sprintf(buttonName, "button%d", i);
    }
    XtVaGetSubresources(buttonForm, (XtPointer)&buttonType, buttonName,
			"Button", resources, 1, NULL);

    button = XtVaCreateManagedWidget(buttonName, toggleWidgetClass,
				       buttonForm, NULL);
    XtVaSetValues(button, XtNfromVert, prevButton,
		    XtNleft, XawChainLeft, XtNright, XawChainRight, NULL);
    prevButton = button;
  }
}

Widget compressW;

void
ShowCompress(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (appData.popupFix) {
	popupFixer(compressW);
  } else {
	XtMoveWidget(compressW, event->xbutton.x_root, event->xbutton.y_root);
  	XtPopup(compressW, XtGrabNone);
  }
  if (appData.grabAll) {
  	XRaiseWindow(dpy, XtWindow(compressW));
  }
  XSetWMProtocols(dpy, XtWindow(compressW), &wmDeleteWindow, 1);
  XtOverrideTranslations(compressW, XtParseTranslationTable ("<Message>WM_PROTOCOLS: HideCompress()"));
	if (w || event || params || num_params) {}
}

void
HideCompress(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  XtPopdown(compressW);
	if (w || event || params || num_params) {}
}


void
CreateCompress()
{
  Widget buttonForm, button, prevButton = NULL;
  int i;
  char buttonName[32];
  String buttonType;

  compressW = XtVaCreatePopupShell("compress", transientShellWidgetClass, toplevel,
			       NULL);

  buttonForm = XtVaCreateManagedWidget("buttonForm", formWidgetClass, compressW,
				       NULL);

  for (i = -1; i <= 9; i++) {
    if (i < 0) {
	sprintf(buttonName, "buttonD");
    } else {
	sprintf(buttonName, "button%d", i);
    }
    XtVaGetSubresources(buttonForm, (XtPointer)&buttonType, buttonName,
			"Button", resources, 1, NULL);

    button = XtVaCreateManagedWidget(buttonName, toggleWidgetClass,
				       buttonForm, NULL);
    XtVaSetValues(button, XtNfromVert, prevButton,
		    XtNleft, XawChainLeft, XtNright, XawChainRight, NULL);
    prevButton = button;
  }
}


int filexfer_sock = -1;
int filexfer_listen = -1;

void HideFile(Widget w, XEvent *event, String *params, Cardinal *num_params) {
	if (filexfer_sock >= 0) {
		close(filexfer_sock);
		filexfer_sock = -1;
	}
	if (filexfer_listen >= 0) {
		close(filexfer_listen);
		filexfer_listen = -1;
	}
	if (w || event || params || num_params) {}
}

extern int use_loopback;
time_t start_listen = 0;
pid_t java_helper = 0;

void ShowFile(Widget w, XEvent *event, String *params, Cardinal *num_params) {
	int i, port0 = 7200, port, sock = -1;
	char *cmd, *jar;
	char fmt[] = "java -cp '%s' VncViewer HOST localhost PORT %d delayAuthPanel yes ignoreMSLogonCheck yes disableSSL yes ftpOnly yes graftFtp yes dsmActive no  &";

	if (getenv("SSVNC_ULTRA_FTP_JAR")) {
		jar = getenv("SSVNC_ULTRA_FTP_JAR");
		cmd = (char *) malloc(strlen(fmt) + strlen(jar) + 100);
	} else {
		fprintf(stderr, "Cannot find UltraVNC FTP jar file.\n");
		return;
	}

	use_loopback = 1;
	for (i = 0; i < 100; i++) {
		port = port0 + i;
		sock = ListenAtTcpPort(port);
		if (sock < 0) {
			sock = ListenAtTcpPort6(port);
		}
		if (sock >= 0) {
			fprintf(stderr, "listening for filexfer on port: %d sock: %d\n", port, sock);
			break;
		}
	}
	use_loopback = 0;

	if (sock >= 0) {
		int st;
		pid_t pid = fork();
		if (pid < 0) {
			free(cmd);
			return;
		} else if (pid == 0) {
			int i;
			sprintf(cmd, fmt, jar, port);
			if (appData.ultraDSM) {
				char *q = strstr(cmd, "dsmActive");
				if (q) {
					q = strstr(q, "no ");
					if (q) {
						q[0] = 'y';
						q[1] = 'e';
						q[2] = 's';
					}
				}
			}
			for (i = 3; i < 100; i++) {
				close(i);
			}
			fprintf(stderr, "\n-- Experimental UltraVNC File Transfer --\n\nRunning cmd:\n\n  %s\n\n", cmd);
			system(cmd);
			exit(0);
		}
		fprintf(stderr, "java helper pid is: %d\n", (int) pid);
		waitpid(pid, &st, 0);
		java_helper = pid;
		start_listen = time(NULL);
	}
	free(cmd);
	filexfer_listen = sock;
	if (w || event || params || num_params) {}
}

Widget chat, entry, text;

static int chat_visible = 0;

void
ShowChat(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	if (appData.termChat) {
		return;
	}
	if (! chat_visible) {
		XtPopup(chat, XtGrabNone);
		chat_visible = 1;
		wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
		XSetWMProtocols(dpy, XtWindow(chat), &wmDeleteWindow, 1);
		if (appData.chatOnly) {
			XtOverrideTranslations(chat, XtParseTranslationTable ("<Message>WM_PROTOCOLS: Quit()"));
		} else {
			XtOverrideTranslations(chat, XtParseTranslationTable ("<Message>WM_PROTOCOLS: HideChat()"));
		}
		XSync(dpy, False);
		usleep(200 * 1000);
	}
	if (w || event || params || num_params) {}
}

void hidechat(void) {
	appData.chatActive = False;
	if (appData.termChat) {
		return;
	}
	if (chat_visible) {
		XtPopdown(chat);
		chat_visible = 0;
		XSync(dpy, False);
		usleep(200 * 1000);
	}
	if (appData.chatOnly) {
		Quit(0, NULL, NULL, NULL);
	}
}

void HideChat(Widget w, XEvent *event, String *params, Cardinal *num_params) {
	SendTextChatClose();
	SendTextChatFinished();
	hidechat();
	if (w || event || params || num_params) {}
}

void dismiss_proc(Widget w, XtPointer client_data, XtPointer call_data) {
	SendTextChatClose();
	SendTextChatFinished();
	hidechat();
	if (w || client_data || call_data) {}
}

extern void printChat(char *, Bool);

static void ChatTextCallback(XtPointer clientData, XtIntervalId *id);
static XtIntervalId timer;
static Bool timerSet = False;

void CheckTextInput(void);
extern double start_time;

static void ChatTextCallback(XtPointer clientData, XtIntervalId *id) {
	static int db = -1;
	if (db < 0) {
		if (getenv("SSVNC_DEBUG_CHAT")) {
			db = 1;
		} else {
			db = 0;
		}
	}
	if (db) fprintf(stderr, "ChatTextCallback: %.4f\n", dnow() - start_time);
	CheckTextInput();
	if (clientData || id) {}
}

void CheckTextInput(void) {
	Arg args[2];
	String str;
	int len;
	static int db = -1;

	if (timerSet) {
		XtRemoveTimeOut(timer);
		timerSet = False;
	}
	if (appData.chatActive) {
		timer = XtAppAddTimeOut(appContext, 333, ChatTextCallback, NULL);
		timerSet = True;
	}
	if (appData.chatOnly && !appData.chatActive) {
		Quit(0, NULL, NULL, NULL);
	}

	if (appData.termChat) {
		return;
	}
#if 0
	if (!appData.chatActive) {
		return;
	}
#endif

	if (db < 0) {
		if (getenv("SSVNC_DEBUG_CHAT")) {
			db = 1;
		} else {
			db = 0;
		}
	}

	XtSetArg(args[0], XtNstring, &str);
	XtGetValues(entry, args, 1);

	if (db) fprintf(stderr, "CheckTextInput\n");

	if (str == NULL || str[0] == '\0') {
		return;
	} else {
		char *q;
		len = strlen(str);
		if (db) fprintf(stderr, "CheckTextInput: len: %d  '%s'\n", len, str);
		if (len <= 0) {
			return;
		}
		q = strrchr(str, '\n'); 
		if (q) {
			char *send, save[2];
			save[0] = *(q+1);
			*(q+1) = '\0';
			send = strdup(str);
			*(q+1) = save[0];
			if (send) {
				SendTextChat(send);
				printChat("Send: ", True);
				printChat(send, True);
				free(send);
				if (save[0] == '\0') {
					XtVaSetValues(entry, XtNtype, XawAsciiString, XtNstring, "", NULL);
				} else {
					char *leak = strdup(q+1);
					XtVaSetValues(entry, XtNtype, XawAsciiString, XtNstring, leak, NULL);
					if (strlen(leak) > 0) {
						XSync(dpy, False);
						XtVaSetValues(entry, XtNinsertPosition, strlen(leak), NULL);
					}
				}
			}
		}
	}
}

void AppendChatInput0(char *in) {
	Arg args[10];
	int n;
	String str;
	int len;
	static char *s = NULL;
	static int slen = -1;
	XawTextPosition pos;

	fprintf(stderr, "AppendChatInput: in= '%s'\n", in);

	XtSetArg(args[0], XtNstring, &str);
	XtGetValues(text, args, 1);
	fprintf(stderr, "AppendChatInput: str='%s'\n", str);
	
	len = strlen(str) + strlen(in);

	if (slen <= len) {
		slen = 2 * (len + 10);
		if (s) free(s);
		s = (char *) malloc(slen+1);
	}
	
	s[0] = '\0';
	strcat(s, str);
	strcat(s, in);
	fprintf(stderr, "AppendChatInput s=  '%s'\n", s);
	pos = (XawTextPosition) (len-1);
	n = 0;
	XtSetArg(args[n], XtNtype, XawAsciiString);	n++;
	XtSetArg(args[n], XtNstring, s);		n++;
	XtSetArg(args[n], XtNdisplayPosition, pos);	n++;
	XtSetArg(args[n], XtNinsertPosition, pos);	n++;
	XtSetValues(text, args, n);
	fprintf(stderr, "AppendChatInput done\n");
}

void AppendChatInput(char *in) {
	XawTextPosition beg, end;
	static XawTextPosition pos = 0;
	XawTextBlock txt;

	if (appData.termChat) {
		return;
	}

	XawTextSetInsertionPoint(text, pos);
	beg = XawTextGetInsertionPoint(text);
	end = beg;
#if 0
	fprintf(stderr, "AppendChatInput: pos=%d in= '%s'\n", beg, in);
#endif

	txt.firstPos = 0;
	txt.length = strlen(in);
	txt.ptr = in;
	txt.format = FMT8BIT;

	XawTextReplace(text, beg, end, &txt);
	XawTextSetInsertionPoint(text, beg + txt.length);

	pos = XawTextGetInsertionPoint(text);
#if 0
	fprintf(stderr, "AppendChatInput done pos=%d\n", pos);
#endif
}

#if 0
static char errorbuf[1] = {0};
#endif

void CreateChat(void) {

	Widget myform, dismiss;
	Dimension w = 400, h = 300;

	chat = XtVaCreatePopupShell("chat", topLevelShellWidgetClass, toplevel, XtNmappedWhenManaged, False, NULL);

	myform = XtVaCreateManagedWidget("myform", formWidgetClass, chat, NULL);

	text = XtVaCreateManagedWidget("text", asciiTextWidgetClass, myform,
	    XtNresize, XawtextResizeBoth, XtNresizable, True, XtNwrap, XawtextWrapWord,
	    XtNscrollHorizontal, XawtextScrollNever, XtNscrollVertical, XawtextScrollAlways,
	    XtNwidth, w, XtNheight, h, XtNdisplayCaret, False,
	    XtNeditType, XawtextAppend, XtNtype, XawAsciiString,
	    XtNuseStringInPlace, False, NULL);

	entry = XtVaCreateManagedWidget("entry", asciiTextWidgetClass, myform,
	    XtNresize, XawtextResizeWidth, XtNresizable, True, XtNwrap, XawtextWrapNever,
	    XtNscrollHorizontal, XawtextScrollNever, XtNscrollVertical, XawtextScrollNever,
	    XtNheight, 20, XtNwidth, 400, XtNfromVert, text, XtNeditType, XawtextEdit,
	    XtNdisplayCaret, True, XtNeditType, XawtextEdit, NULL);

	dismiss = XtVaCreateManagedWidget("dismiss", commandWidgetClass, myform, XtNlabel, "Close Chat", XtNfromVert, entry, NULL);

	AppendChatInput("");

	XtAddCallback(dismiss, XtNcallback, dismiss_proc, NULL);

	XtRealizeWidget(chat);

	XtSetKeyboardFocus(chat, entry); 
}

Widget msgwin, msgtext;

void AppendMsg(char *in) {
	XawTextPosition beg, end;
	static XawTextPosition pos = 0;
	XawTextBlock txt;

	XawTextSetInsertionPoint(msgtext, pos);
	beg = XawTextGetInsertionPoint(msgtext);
	end = beg;

	txt.firstPos = 0;
	txt.length = strlen(in);
	txt.ptr = in;
	txt.format = FMT8BIT;

	XawTextReplace(msgtext, beg, end, &txt);
	XawTextSetInsertionPoint(msgtext, beg + txt.length);

	pos = XawTextGetInsertionPoint(msgtext);
}

static int msg_visible = 0;
static int msg_NO_clicked = 0;

void msg_dismiss_proc(Widget w, XtPointer client_data, XtPointer call_data) {
	XtPopdown(msgwin);
	msg_visible = 0;
	XSync(dpy, False);
	usleep(200 * 1000);
	if (w || client_data || call_data) {}
}

void msg_NO_proc(Widget w, XtPointer client_data, XtPointer call_data) {
	XtPopdown(msgwin);
	msg_visible = 0;
	msg_NO_clicked = 1;
	XSync(dpy, False);
	usleep(200 * 1000);
	if (w || client_data || call_data) {}
}

int CreateMsg(char *msg, int wait) {

	Widget myform, dismiss, reject;
	char *p;
	int n, run, wmax = 0;
	int ret = 1;
	Dimension w, h;


	n = 0;
	run = 0;
	p = msg;
	while (*p != '\0') {
		if (*p == '\n') {
			run = 0;
			n++;
		}
		run++;
		if (run > wmax) wmax = run;
		p++;
	}
	if (wmax > 80) {
		if (wmax > 120) n++;
		if (wmax > 80) n++;
		wmax = 80;
	}
	h = (Dimension) (n+2) * 14;
	w = (Dimension) (wmax+10) * 8;

	msgwin = XtVaCreatePopupShell("Message", topLevelShellWidgetClass, toplevel, XtNmappedWhenManaged, False, NULL);

	myform = XtVaCreateManagedWidget("myform", formWidgetClass, msgwin, NULL);

	msgtext = XtVaCreateManagedWidget("msgtext", asciiTextWidgetClass, myform,
	    XtNresize, XawtextResizeBoth, XtNresizable, True, XtNwrap, XawtextWrapWord,
	    XtNscrollHorizontal, XawtextScrollNever, XtNscrollVertical, XawtextScrollAlways,
	    XtNwidth, w, XtNheight, h, XtNdisplayCaret, False,
	    XtNeditType, XawtextAppend, XtNtype, XawAsciiString,
	    XtNuseStringInPlace, False, NULL);

	if (wait == 2) {
		msg_NO_clicked = 0;

		dismiss = XtVaCreateManagedWidget("dismiss", commandWidgetClass, myform, XtNlabel, "Accept", XtNfromVert, msgtext, NULL);
		XtAddCallback(dismiss, XtNcallback, msg_dismiss_proc, NULL);

		reject = XtVaCreateManagedWidget("reject", commandWidgetClass, myform, XtNlabel, "Reject", XtNfromVert, dismiss, NULL);
		XtAddCallback(reject, XtNcallback, msg_NO_proc, NULL);
	} else {
		dismiss = XtVaCreateManagedWidget("dismiss", commandWidgetClass, myform, XtNlabel, "OK", XtNfromVert, msgtext, NULL);
		XtAddCallback(dismiss, XtNcallback, msg_dismiss_proc, NULL);
		
	}

	AppendMsg("");
	AppendMsg(msg);

	XtRealizeWidget(msgwin);

	XtPopup(msgwin, XtGrabNone);

	XSync(dpy, False);
	msg_visible = 1;
	while (wait && msg_visible) {
		if (0) fprintf(stderr, "mv: %d\n", msg_visible);
		XtAppProcessEvent(appContext, XtIMAll);
	}
	if (wait == 2) {
		if (msg_NO_clicked) {
			ret = 0;
		} else {
			ret = 1;
		}
	}
	return ret;
}
