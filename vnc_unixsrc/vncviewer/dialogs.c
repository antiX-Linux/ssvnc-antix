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
 * dialog.c - code to deal with dialog boxes.
 */

#include "vncviewer.h"
#include <X11/Xaw/Dialog.h>

static Bool serverDialogDone = False;
static Bool userDialogDone = False;
static Bool passwordDialogDone = False;
static Bool ycropDialogDone = False;
static Bool scaleDialogDone = False;
static Bool escapeDialogDone = False;
static Bool scbarDialogDone = False;
static Bool scaleNDialogDone = False;
static Bool qualityDialogDone = False;
static Bool compressDialogDone = False;

extern void popupFixer(Widget wid);

int use_tty(void) {
	if (appData.notty) {
		return 0;
	} else if (!isatty(0)) {
		return 0;
	}
	return 1;
}

void
ScaleDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	scaleDialogDone = True;
	if (w || event || params || num_params) {}
}

void
EscapeDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	escapeDialogDone = True;
	if (w || event || params || num_params) {}
}

void dialog_over(Widget wid) {
	if (appData.fullScreen) {
		if (!net_wm_supported()) {
			XtVaSetValues(wid, XtNoverrideRedirect, True, NULL);
			XSync(dpy, True);
		}
	}	
}

extern int XError_ign;

void dialog_input(Widget wid) {
	XError_ign = 1;
	XSetInputFocus(dpy, XtWindow(wid), RevertToParent, CurrentTime);
	XSync(dpy, False);
	usleep(30 * 1000);
	XSync(dpy, False);
	usleep(20 * 1000);
	XSync(dpy, False);
	XError_ign = 0;
}

static void rmNL(char *s) {
	int len;
	if (s == NULL) {
		return;
	}
	len = strlen(s);
	if (len > 0 && s[len-1] == '\n') {
		s[len-1] = '\0';
	}
}

static void wm_delete(Widget w, char *func) {
	char str[1024];
	Atom wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, XtWindow(w), &wmDeleteWindow, 1);
	if (func) {
		sprintf(str, "<Message>WM_PROTOCOLS: %s", func);
		XtOverrideTranslations(w, XtParseTranslationTable (str));
	}
}

static void xtmove(Widget w) {
	XtMoveWidget(w, WidthOfScreen(XtScreen(w))*2/5, HeightOfScreen(XtScreen(w))*2/5);
}

char *
DoScaleDialog()
{
	Widget pshell, dialog;
	char *scaleValue;
	char *valueString;

	pshell = XtVaCreatePopupShell("scaleDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);

	if (0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (appData.scale != NULL) {
		String label;
		char tmp[410];
		XtVaGetValues(dialog, XtNlabel, &label, NULL);
		if (strlen(label) + strlen(appData.scale) < 400) {
			sprintf(tmp, "%s %s", label, appData.scale);
			XtVaSetValues(dialog, XtNlabel, tmp, NULL);
		}
	}


	if (1 && appData.popupFix) {
		popupFixer(pshell);
	} else {
		xtmove(pshell);
	}
	dialog_input(pshell);
	wm_delete(pshell, "ScaleDialogDone()");

	scaleDialogDone = False;

	while (!scaleDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	scaleValue = XtNewString(valueString);

	XtPopdown(pshell);
	return scaleValue;
}

char *
DoEscapeKeysDialog()
{
	Widget pshell, dialog;
	char *escapeValue;
	char *valueString;
	char *curr = appData.escapeKeys ? appData.escapeKeys : "default";

	pshell = XtVaCreatePopupShell("escapeDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);

	if(0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (curr != NULL) {
		String label;
		char tmp[3010];
		XtVaGetValues(dialog, XtNlabel, &label, NULL);
		if (strlen(label) + strlen(curr) < 3000) {
			sprintf(tmp, "%s %s", label, curr);
			XtVaSetValues(dialog, XtNlabel, tmp, NULL);
		}
	}

	if (appData.popupFix) {
		popupFixer(pshell);
	} else {
		/* too big */
		if (0) xtmove(pshell);
	}
	dialog_input(pshell);
	wm_delete(pshell, "EscapeDialogDone()");

	escapeDialogDone = False;

	while (!escapeDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	escapeValue = XtNewString(valueString);

	XtPopdown(pshell);
	return escapeValue;
}

void
YCropDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	ycropDialogDone = True;
	if (w || event || params || num_params) {}
}

char *
DoYCropDialog()
{
	Widget pshell, dialog;
	char *ycropValue;
	char *valueString;

	pshell = XtVaCreatePopupShell("ycropDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);

	if(0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (1 && appData.popupFix) {
		popupFixer(pshell);
	} else {
		xtmove(pshell);
	}
	dialog_input(pshell);
	wm_delete(pshell, "YCropDialogDone()");

	ycropDialogDone = False;

	while (!ycropDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	ycropValue = XtNewString(valueString);

	XtPopdown(pshell);
	return ycropValue;
}

void
ScbarDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	scbarDialogDone = True;
	if (w || event || params || num_params) {}
}

char *
DoScbarDialog()
{
	Widget pshell, dialog;
	char *scbarValue;
	char *valueString;

	pshell = XtVaCreatePopupShell("scbarDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);

	if(0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (1 && appData.popupFix) {
		popupFixer(pshell);
	} else {
		xtmove(pshell);
	}
	dialog_input(pshell);
	wm_delete(pshell, "ScbarDialogDone()");

	scbarDialogDone = False;

	while (!scbarDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	scbarValue = XtNewString(valueString);

	XtPopdown(pshell);
	return scbarValue;
}

void
ScaleNDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	scaleNDialogDone = True;
	if (w || event || params || num_params) {}
}

char *
DoScaleNDialog()
{
	Widget pshell, dialog;
	char *scaleNValue;
	char *valueString;

	pshell = XtVaCreatePopupShell("scaleNDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);
	wm_delete(pshell, "ScaleNDialogDone()");

	if(0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (appData.popupFix) {
		popupFixer(pshell);
	} else {
		xtmove(pshell);
	}
	dialog_input(pshell);
	wm_delete(pshell, "ScaleNDialogDone()");

	scaleNDialogDone = False;

	while (!scaleNDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	scaleNValue = XtNewString(valueString);

	XtPopdown(pshell);
	return scaleNValue;
}

void
QualityDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	qualityDialogDone = True;
	if (w || event || params || num_params) {}
}

char *
DoQualityDialog()
{
	Widget pshell, dialog;
	char *qualityValue;
	char *valueString;

	pshell = XtVaCreatePopupShell("qualityDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);

	if(0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (1 && appData.popupFix) {
		popupFixer(pshell);
	} else {
		xtmove(pshell);
	}
	dialog_input(pshell);
	wm_delete(pshell, "QualityDialogDone() HideQuality()");

	qualityDialogDone = False;

	while (!qualityDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	qualityValue = XtNewString(valueString);

	XtPopdown(pshell);
	return qualityValue;
}

void
CompressDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	compressDialogDone = True;
	if (w || event || params || num_params) {}
}

char *
DoCompressDialog()
{
	Widget pshell, dialog;
	char *compressValue;
	char *valueString;

	fprintf(stderr, "compress start:\n");

	pshell = XtVaCreatePopupShell("compressDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);

	if(0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (1 && appData.popupFix) {
		popupFixer(pshell);
	} else {
		xtmove(pshell);
	}
	dialog_input(pshell);
	wm_delete(pshell, "CompressDialogDone() HideCompress()");

	compressDialogDone = False;

	while (!compressDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	compressValue = XtNewString(valueString);

	fprintf(stderr, "compress done: %s\n", compressValue);

	XtPopdown(pshell);
	return compressValue;
}

void
ServerDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	serverDialogDone = True;
	if (w || event || params || num_params) {}
}

char *
DoServerDialog()
{
	Widget pshell, dialog;
	char *vncServerName;
	char *valueString;

	pshell = XtVaCreatePopupShell("serverDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);

	if (0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (0 && appData.popupFix) {
		popupFixer(pshell);
	} else {
		xtmove(pshell);
	}
#if 0
	dialog_input(pshell);
#endif
	wm_delete(pshell, "ServerDialogDone()");

	serverDialogDone = False;

	while (!serverDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	vncServerName = XtNewString(valueString);

	XtPopdown(pshell);
	return vncServerName;
}

void
UserDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	userDialogDone = True;
	if (w || event || params || num_params) {}
}

char *
DoUserDialog()
{
	Widget pshell, dialog;
	char *userName;
	char *valueString;

	pshell = XtVaCreatePopupShell("userDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);

	if(0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (0 && appData.popupFix) {
		popupFixer(pshell);
	} else {
		xtmove(pshell);
	}
#if 0
	dialog_input(pshell);
#endif
	wm_delete(pshell, "UserDialogDone()");

	userDialogDone = False;

	while (!userDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	userName = XtNewString(valueString);

	XtPopdown(pshell);
	return userName;
}

void
PasswordDialogDone(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	passwordDialogDone = True;
	if (w || event || params || num_params) {}
}

char *
DoPasswordDialog()
{
	Widget pshell, dialog;
	char *password;
	char *valueString;

	pshell = XtVaCreatePopupShell("passwordDialog", transientShellWidgetClass,
				toplevel, NULL);
	dialog = XtVaCreateManagedWidget("dialog", dialogWidgetClass, pshell, NULL);

	dialog_over(pshell);

	if(0) XtMoveWidget(pshell, WidthOfScreen(XtScreen(pshell))*2/5, HeightOfScreen(XtScreen(pshell))*2/5);
	XtPopup(pshell, XtGrabNonexclusive);
	XtRealizeWidget(pshell);

	if (0 && appData.popupFix) {
		popupFixer(pshell);
	} else {
		xtmove(pshell);
	}
#if 0
	dialog_input(pshell);
#endif
	wm_delete(pshell, "PasswordDialogDone()");

	passwordDialogDone = False;

	while (!passwordDialogDone) {
		XtAppProcessEvent(appContext, XtIMAll);
	}

	valueString = XawDialogGetValueString(dialog);
	rmNL(valueString);
	password = XtNewString(valueString);

	XtPopdown(pshell);
	return password;
}
