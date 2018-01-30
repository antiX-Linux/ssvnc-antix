/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
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
 * vncviewer.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xmd.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xmu/StdSel.h>
#include "rfbproto.h"
#include "caps.h"

extern int endianTest;

#define Swap16IfLE(s) \
    (*(char *)&endianTest ? ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)) : (s))

#define Swap32IfLE(l) \
    (*(char *)&endianTest ? ((((l) & 0xff000000) >> 24) | \
			     (((l) & 0x00ff0000) >> 8)  | \
			     (((l) & 0x0000ff00) << 8)  | \
			     (((l) & 0x000000ff) << 24))  : (l))

#define Swap32IfBE(l) \
    (*(char *)&endianTest ? (l) : ((((l) & 0xff000000) >> 24) | \
			     (((l) & 0x00ff0000) >> 8)  | \
			     (((l) & 0x0000ff00) << 8)  | \
			     (((l) & 0x000000ff) << 24)) )

#define MAX_ENCODINGS 24

#define FLASH_PORT_OFFSET 5400
#define LISTEN_PORT_OFFSET 5500
#define TUNNEL_PORT_OFFSET 5500
#define SERVER_PORT_OFFSET 5900

#define DEFAULT_SSH_CMD "/usr/bin/ssh"
#define DEFAULT_TUNNEL_CMD  \
  (DEFAULT_SSH_CMD " -f -L %L:localhost:%R %H sleep 20")
#define DEFAULT_VIA_CMD     \
  (DEFAULT_SSH_CMD " -f -L %L:%H:%R %G sleep 20")

#define TVNC_SAMPOPT 4
enum {TVNC_1X=0, TVNC_4X, TVNC_2X, TVNC_GRAY};

#if 0
static const char *subsampLevel2str[TVNC_SAMPOPT] = {
  "1X", "4X", "2X", "Gray"
};
#endif
#ifdef TURBOVNC
#define rfbTightNoZlib 0x0A
#define rfbTurboVncVendor "TRBO"
#define rfbJpegQualityLevel1       0xFFFFFE01
#define rfbJpegQualityLevel100     0xFFFFFE64
#define rfbJpegSubsamp1X           0xFFFFFD00
#define rfbJpegSubsamp4X           0xFFFFFD01
#define rfbJpegSubsamp2X           0xFFFFFD02
#define rfbJpegSubsampGray         0xFFFFFD03
#endif

/* for debugging width, height, etc */
#if 0
#define XtVaSetValues printf("%s:%d\n", __FILE__, __LINE__); XtVaSetValues
#endif


/* argsresources.c */

typedef struct {
	Bool shareDesktop;
	Bool viewOnly;
	Bool fullScreen;
	Bool grabKeyboard;
	Bool raiseOnBeep;

	String encodingsString;

	int useBGR233;
	int nColours;
	Bool useSharedColours;
	Bool forceOwnCmap;
	Bool forceTrueColour;
	int requestedDepth;
	Bool useBGR565;
	Bool useGreyScale;

	Bool grabAll;
	Bool useXserverBackingStore;
	Bool overrideRedir;
	Bool popupFix;

	Bool useShm;
	Bool termChat;

	int wmDecorationWidth;
	int wmDecorationHeight;

	char *userLogin;
	char *unixPW;
	char *msLogon;
	char *repeaterUltra;
	Bool ultraDSM;
	Bool acceptPopup;
	char *rfbVersion;

	char *passwordFile;
	Bool passwordDialog;
	Bool notty;

	int rawDelay;
	int copyRectDelay;

	int yCrop;
	int sbWidth;
	Bool useCursorAlpha;
	Bool useRawLocal;

	Bool debug;

	int popupButtonCount;
	int popupButtonBreak;

	int bumpScrollTime;
	int bumpScrollPixels;

	int compressLevel;
	int qualityLevel;
	Bool enableJPEG;
	Bool useRemoteCursor;
	Bool useX11Cursor;
	Bool useBell;
	Bool autoPass;

	Bool serverInput;
	Bool singleWindow;
	int serverScale;
	Bool chatActive;
	Bool chatOnly;
	Bool fileActive;

	char *scale;
	char *escapeKeys;
	Bool appShare;
	Bool escapeActive;
	Bool pipelineUpdates;

	Bool sendClipboard;
	Bool sendAlways;
	char *recvText;

	/* only for turbovnc mode */
	String subsampString;
	int subsampLevel;
	Bool doubleBuffer;

	Bool noipv4;
	Bool noipv6;

} AppData;

extern AppData appData;
extern AppData appDataNew;

extern char *fallback_resources[];
extern char vncServerHost[];
extern int vncServerPort;
extern Bool listenSpecified;
extern pid_t listenParent;
extern int listenPort, flashPort;

extern XrmOptionDescRec cmdLineOptions[];
extern int numCmdLineOptions;

extern void removeArgs(int *argc, char** argv, int idx, int nargs);
extern void usage(void);
extern void GetArgsAndResources(int argc, char **argv);

/* colour.c */

extern unsigned long BGR233ToPixel[];
extern unsigned long BGR565ToPixel[];

extern Colormap cmap;
extern Visual *vis;
extern unsigned int visdepth, visbpp, isLSB;

extern void SetVisualAndCmap();

/* cursor.c */

extern Bool HandleCursorShape(int xhot, int yhot, int width, int height,
                              CARD32 enc);
extern void SoftCursorLockArea(int x, int y, int w, int h);
extern void SoftCursorUnlockScreen(void);
extern void SoftCursorMove(int x, int y);

/* desktop.c */

extern Atom wmDeleteWindow;
extern Widget form, viewport, desktop;
extern Window desktopWin;
extern Cursor dotCursor;
extern GC gc;
extern GC srcGC, dstGC;
extern Dimension dpyWidth, dpyHeight;

extern int appshare_0_hint;
extern int appshare_x_hint;
extern int appshare_y_hint;

extern void DesktopInitBeforeRealization();
extern void DesktopInitAfterRealization();
extern void Xcursors(int set);
extern void SendRFBEvent(Widget w, XEvent *event, String *params,
			 Cardinal *num_params);
extern void CopyDataToScreen(char *buf, int x, int y, int width, int height);
extern void FillScreen(int x, int y, int width, int height, unsigned long fill);
extern void SynchroniseScreen();

extern void ReDoDesktop();
extern void DesktopCursorOff();
extern void put_image(int x1, int y1, int x2, int y2, int width, int height, int solid);
extern void copy_rect(int x, int y, int width, int height, int src_x, int src_y);

extern void releaseAllPressedModifiers(void);
extern void fs_grab(int check);
extern void fs_ungrab(int check);

/* dialogs.c */

extern int use_tty(void);

extern void ScaleDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoScaleDialog();

extern void EscapeDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoEscapeKeysDialog();

extern void YCropDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoYCropDialog();

extern void ScbarDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoScbarDialog();

extern void ScaleNDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoScaleNDialog();

extern void QualityDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoQualityDialog();

extern void CompressDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoCompressDialog();

extern void ServerDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoServerDialog();
extern void PasswordDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoPasswordDialog();

extern void UserDialogDone(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern char *DoUserDialog();

/* fullscreen.c */

extern void ToggleFullScreen(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);
extern void SetFullScreenState(Widget w, XEvent *event, String *params,
			       Cardinal *num_params);
extern Bool BumpScroll(XEvent *ev);
extern void FullScreenOn();
extern void FullScreenOff();

extern int net_wm_supported(void);

extern void JumpLeft(Widget w, XEvent *event, String *params, Cardinal *num_params);
extern void JumpRight(Widget w, XEvent *event, String *params, Cardinal *num_params);
extern void JumpUp(Widget w, XEvent *event, String *params, Cardinal *num_params);
extern void JumpDown(Widget w, XEvent *event, String *params, Cardinal *num_params);

/* listen.c */

extern void listenForIncomingConnections();

/* misc.c */

extern void ToplevelInitBeforeRealization();
extern void ToplevelInitAfterRealization();
extern Time TimeFromEvent(XEvent *ev);
extern void Pause(Widget w, XEvent *event, String *params,
		  Cardinal *num_params);
extern void RunCommand(Widget w, XEvent *event, String *params,
		       Cardinal *num_params);
extern void Quit(Widget w, XEvent *event, String *params,
		 Cardinal *num_params);
extern void HideChat(Widget w, XEvent *event, String *params,
		 Cardinal *num_params);
extern void Cleanup();

/* popup.c */

extern Widget popup;
extern void ShowPopup(Widget w, XEvent *event, String *params,
		      Cardinal *num_params);
extern void HidePopup(Widget w, XEvent *event, String *params,
		      Cardinal *num_params);
extern void CreatePopup();

extern void HideScaleN(Widget w, XEvent *event, String *params,
		      Cardinal *num_params);
extern void CreateScaleN();

extern void HideTurboVNC(Widget w, XEvent *event, String *params,
		      Cardinal *num_params);
extern void CreateTurboVNC();
extern void UpdateSubsampButtons();
extern void UpdateQualSlider();
extern void UpdateQual();

extern void HideQuality(Widget w, XEvent *event, String *params,
		      Cardinal *num_params);
extern void CreateQuality();

extern void HideCompress(Widget w, XEvent *event, String *params,
		      Cardinal *num_params);
extern void CreateCompress();

extern void Noop(Widget w, XEvent *event, String *params,
		      Cardinal *num_params);

extern int CreateMsg(char *msg, int wait);
/* rfbproto.c */

extern int rfbsock;
extern Bool canUseCoRRE;
extern Bool canUseHextile;
extern char *desktopName;
extern rfbPixelFormat myFormat;
extern rfbServerInitMsg si;
extern char *serverCutText;
extern Bool newServerCutText;

extern Bool ConnectToRFBServer(const char *hostname, int port);
extern Bool InitialiseRFBConnection();
extern Bool SetFormatAndEncodings();
extern Bool SendIncrementalFramebufferUpdateRequest();
extern Bool SendFramebufferUpdateRequest(int x, int y, int w, int h,
					 Bool incremental);
extern Bool SendPointerEvent(int x, int y, int buttonMask);
extern Bool SendKeyEvent(CARD32 key, Bool down);
extern Bool SendClientCutText(char *str, int len);
extern Bool HandleRFBServerMessage();

extern Bool SendServerInput(Bool enabled);
extern Bool SendSingleWindow(int x, int y);
extern Bool SendServerScale(int n);

extern Bool SendTextChat(char *str);
extern Bool SendTextChatOpen(void);
extern Bool SendTextChatClose(void);
extern Bool SendTextChatFinish(void);

extern void PrintPixelFormat(rfbPixelFormat *format);

extern double dnow(void);

/* selection.c */

extern void InitialiseSelection();
extern void SelectionToVNC(Widget w, XEvent *event, String *params,
			   Cardinal *num_params);
extern void SelectionFromVNC(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);

/* shm.c */

extern XImage *CreateShmImage(int do_ycrop);
extern void ShmCleanup();
extern void ShmDetach();
extern Bool UsingShm();

/* sockets.c */

extern Bool errorMessageOnReadFailure;

extern Bool ReadFromRFBServer(char *out, unsigned int n);
extern Bool WriteExact(int sock, char *buf, int n);
extern int FindFreeTcpPort(void);
extern int ListenAtTcpPort(int port);
extern int ListenAtTcpPort6(int port);
extern int dotted_ip(char *host, int partial);
extern int ConnectToTcpAddr(const char *hostname, int port);
extern int ConnectToUnixSocket(char *file);
extern int AcceptTcpConnection(int listenSock);
extern int AcceptTcpConnection6(int listenSock);
extern Bool SetNonBlocking(int sock);
extern Bool SetNoDelay(int sock);
extern Bool SocketPair(int fd[2]);

extern int StringToIPAddr(const char *str, unsigned int *addr);
extern char *get_peer_ip(int sock);
extern char *ip2host(char *ip);
extern Bool SameMachine(int sock);

/* tunnel.c */

extern Bool tunnelSpecified;

extern Bool createTunnel(int *argc, char **argv, int tunnelArgIndex);

/* vncviewer.c */

extern char *programName;
extern XtAppContext appContext;
extern Display* dpy;
extern Widget toplevel;

extern void GotChatText(char *str, int len);
extern void unixpw(char *instr, int vencrypt_plain);

extern void Toggle8bpp(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void Toggle16bpp(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleFullColor(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void Toggle256Colors(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void Toggle64Colors(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void Toggle8Colors(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleGreyScale(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleTightZRLE(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleTightHextile(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleZRLEZYWRLE(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleViewOnly(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleJPEG(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleCursorShape(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleCursorAlpha(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleX11Cursor(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleBell(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleRawLocal(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleServerInput(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void TogglePipelineUpdates(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleSendClipboard(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleSendAlways(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleSingleWindow(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleXGrab(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleEscapeActive(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetEscapeKeys(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void DoServerScale(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void DoServerQuality(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void DoServerCompress(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetScale(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetYCrop(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetScbar(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ShowScaleN(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ShowTurboVNC(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ShowQuality(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ShowCompress(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetScaleN(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetTurboVNC(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetQuality(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetCompress(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleTextChat(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleFileXfer(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void ToggleTermTextChat(Widget w, XEvent *ev, String *params, Cardinal *num_params);

extern void scale_check_zrle(void);

extern void SetViewOnlyState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetNOJPEGState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetScaleNState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetQualityState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetCompressState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void Set8bppState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void Set16bppState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetFullColorState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void Set256ColorsState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void Set64ColorsState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void Set8ColorsState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetGreyScaleState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetZRLEState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetHextileState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetZYWRLEState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetCursorShapeState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetCursorAlphaState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetX11CursorState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetBellState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetRawLocalState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetServerInputState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetPipelineUpdates(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetSendClipboard(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetSendAlways(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetSingleWindowState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetTextChatState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetTermTextChatState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetFileXferState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetXGrabState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
extern void SetEscapeKeysState(Widget w, XEvent *ev, String *params, Cardinal *num_params);
