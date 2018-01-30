/*
 *  Copyright (C) 2000-2002 Constantin Kaplinsky.  All Rights Reserved.
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
 * rfbproto.c - functions to deal with client side of RFB protocol.
 */

#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <vncviewer.h>
#include <vncauth.h>
#include <zlib.h>
#include <jpeglib.h>

int server_major = 0, server_minor = 0;
int viewer_major = 0, viewer_minor = 0;

pid_t exec_pid = 0;

static void InitCapabilities(void);
static Bool SetupTunneling(void);
static int ReadSecurityType(void);
static int SelectSecurityType(void);
static Bool PerformAuthenticationTight(void);
static Bool AuthenticateVNC(void);
static Bool AuthenticateUltraVNC(void);
static Bool AuthenticateUnixLogin(void);
static Bool ReadInteractionCaps(void);
static Bool ReadCapabilityList(CapsContainer *caps, int count);

static Bool HandleRRE8(int rx, int ry, int rw, int rh);
static Bool HandleRRE16(int rx, int ry, int rw, int rh);
static Bool HandleRRE32(int rx, int ry, int rw, int rh);
static Bool HandleCoRRE8(int rx, int ry, int rw, int rh);
static Bool HandleCoRRE16(int rx, int ry, int rw, int rh);
static Bool HandleCoRRE32(int rx, int ry, int rw, int rh);
static Bool HandleHextile8(int rx, int ry, int rw, int rh);
static Bool HandleHextile16(int rx, int ry, int rw, int rh);
static Bool HandleHextile32(int rx, int ry, int rw, int rh);
static Bool HandleZlib8(int rx, int ry, int rw, int rh);
static Bool HandleZlib16(int rx, int ry, int rw, int rh);
static Bool HandleZlib32(int rx, int ry, int rw, int rh);
static Bool HandleTight8(int rx, int ry, int rw, int rh);
static Bool HandleTight16(int rx, int ry, int rw, int rh);
static Bool HandleTight32(int rx, int ry, int rw, int rh);

/* runge add zrle */
static Bool HandleZRLE8(int rx, int ry, int rw, int rh);
static Bool HandleZRLE15(int rx, int ry, int rw, int rh);
static Bool HandleZRLE16(int rx, int ry, int rw, int rh);
static Bool HandleZRLE24(int rx, int ry, int rw, int rh);
static Bool HandleZRLE24Up(int rx, int ry, int rw, int rh);
static Bool HandleZRLE24Down(int rx, int ry, int rw, int rh);
static Bool HandleZRLE32(int rx, int ry, int rw, int rh);

extern Bool HandleCursorPos(int x, int y);
extern void printChat(char *, Bool);

typedef struct {
    unsigned long length;
} rfbZRLEHeader;

#define sz_rfbZRLEHeader 4

#define rfbZRLETileWidth 64
#define rfbZRLETileHeight 64

#define DO_ZYWRLE 1

#if DO_ZYWRLE

#ifndef ZRLE_ONCE
#define ZRLE_ONCE

static const int bitsPerPackedPixel[] = {
  0, 1, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
};

int zywrle_level;
int zywrleBuf[rfbZRLETileWidth*rfbZRLETileHeight];

#include "zrlepalettehelper.h"
static zrlePaletteHelper paletteHelper;

#endif /* ZRLE_ONCE */
#endif /* DO_ZYWRLE */

static void ReadConnFailedReason(void);
static long ReadCompactLen (void);

static void JpegInitSource(j_decompress_ptr cinfo);
static boolean JpegFillInputBuffer(j_decompress_ptr cinfo);
static void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes);
static void JpegTermSource(j_decompress_ptr cinfo);
static void JpegSetSrcManager(j_decompress_ptr cinfo, CARD8 *compressedData,
                              int compressedLen);

extern void deskey(unsigned char *, int);
extern void des(unsigned char *, unsigned char *);

extern int currentMsg;
extern double scale_factor_x;
extern double scale_factor_y;

extern int skip_maybe_sync;

int sent_FBU = 0;
int skip_XtUpdate = 0;
int skip_XtUpdateAll = 0;

static double dt_out = 0.0;
static double dt_out_sc = 0.0;
double latency = 0.0;
double connect_time = 0.0;

void raiseme(int force);

int rfbsock;
char *desktopName;
rfbPixelFormat myFormat;
rfbServerInitMsg si;
char *serverCutText = NULL;
Bool newServerCutText = False;

/* ultravnc mslogon */
#define rfbUltraVncMsLogon 0xfffffffa
static Bool AuthUltraVncMsLogon(void);
extern void UvncEncryptPasswd_MSLOGON(unsigned char *encryptedPasswd, char *passwd);
extern void UvncEncryptBytes2(unsigned char *where, int length, unsigned char *key);
extern void UvncDecryptBytes2(unsigned char *where, int length, unsigned char *key);
extern unsigned int urandom(void);

/* for ultravnc 1.0.9.5 / RFB 3.8 */
#define rfbUltraVNC_SCPrompt 0x68
#define rfbUltraVNC_SessionSelect 0x69
#define rfbUltraVNC_MsLogonIAuth 0x70
#define rfbUltraVNC_MsLogonIIAuth 0x71
#define rfbUltraVNC_SecureVNCPluginAuth 0x72
#define rfbLegacy_SecureVNCPlugin 17
#define rfbLegacy_MsLogon 0xfffffffa

#define rfbVncAuthContinue 0xffffffff


int endianTest = 1;

static Bool tightVncProtocol = False;
static CapsContainer *tunnelCaps;    /* known tunneling/encryption methods */
static CapsContainer *authCaps;	     /* known authentication schemes       */
static CapsContainer *serverMsgCaps; /* known non-standard server messages */
static CapsContainer *clientMsgCaps; /* known non-standard client messages */
static CapsContainer *encodingCaps;  /* known encodings besides Raw        */


/* Note that the CoRRE encoding uses this buffer and assumes it is big enough
   to hold 255 * 255 * 32 bits -> 260100 bytes.  640*480 = 307200 bytes.
   Hextile also assumes it is big enough to hold 16 * 16 * 32 bits.
   Tight encoding assumes BUFFER_SIZE is at least 16384 bytes. */

#define BUFFER_SIZE (640*480)
static char buffer[BUFFER_SIZE];


/* The zlib encoding requires expansion/decompression/deflation of the
   compressed data in the "buffer" above into another, result buffer.
   However, the size of the result buffer can be determined precisely
   based on the bitsPerPixel, height and width of the rectangle.  We
   allocate this buffer one time to be the full size of the buffer. */

static int raw_buffer_size = -1;
static char *raw_buffer;

static z_stream decompStream;
static Bool decompStreamInited = False;


/*
 * Variables for the ``tight'' encoding implementation.
 */

/* Separate buffer for compressed data. */
#define ZLIB_BUFFER_SIZE 512
static char zlib_buffer[ZLIB_BUFFER_SIZE];

/* Four independent compression streams for zlib library. */
static z_stream zlibStream[4];
static Bool zlibStreamActive[4] = {
  False, False, False, False
};

/* Filter stuff. Should be initialized by filter initialization code. */
static Bool cutZeros;
static int rectWidth, rectColors;
static char tightPalette[256*4];
static CARD8 tightPrevRow[2048*3*sizeof(CARD16)];

/* JPEG decoder state. */
static Bool jpegError;


/*
 * InitCapabilities.
 */

static void
InitCapabilities(void)
{
  tunnelCaps    = CapsNewContainer();
  authCaps      = CapsNewContainer();
  serverMsgCaps = CapsNewContainer();
  clientMsgCaps = CapsNewContainer();
  encodingCaps  = CapsNewContainer();

  /* Supported authentication methods */
  CapsAdd(authCaps, rfbAuthVNC, rfbStandardVendor, sig_rfbAuthVNC,
	  "Standard VNC password authentication");
  CapsAdd(authCaps, rfbAuthUnixLogin, rfbTightVncVendor, sig_rfbAuthUnixLogin,
	  "Login-style Unix authentication");

  /* Supported encoding types */
  CapsAdd(encodingCaps, rfbEncodingCopyRect, rfbStandardVendor,
	  sig_rfbEncodingCopyRect, "Standard CopyRect encoding");
  CapsAdd(encodingCaps, rfbEncodingRRE, rfbStandardVendor,
	  sig_rfbEncodingRRE, "Standard RRE encoding");
  CapsAdd(encodingCaps, rfbEncodingCoRRE, rfbStandardVendor,
	  sig_rfbEncodingCoRRE, "Standard CoRRE encoding");
  CapsAdd(encodingCaps, rfbEncodingHextile, rfbStandardVendor,
	  sig_rfbEncodingHextile, "Standard Hextile encoding");
  CapsAdd(encodingCaps, rfbEncodingZlib, rfbTridiaVncVendor,
	  sig_rfbEncodingZlib, "Zlib encoding from TridiaVNC");
  CapsAdd(encodingCaps, rfbEncodingTight, rfbTightVncVendor,
	  sig_rfbEncodingTight, "Tight encoding by Constantin Kaplinsky");

  /* Supported "fake" encoding types */
  CapsAdd(encodingCaps, rfbEncodingCompressLevel0, rfbTightVncVendor,
	  sig_rfbEncodingCompressLevel0, "Compression level");
  CapsAdd(encodingCaps, rfbEncodingQualityLevel0, rfbTightVncVendor,
	  sig_rfbEncodingQualityLevel0, "JPEG quality level");
  CapsAdd(encodingCaps, rfbEncodingXCursor, rfbTightVncVendor,
	  sig_rfbEncodingXCursor, "X-style cursor shape update");
  CapsAdd(encodingCaps, rfbEncodingRichCursor, rfbTightVncVendor,
	  sig_rfbEncodingRichCursor, "Rich-color cursor shape update");
  CapsAdd(encodingCaps, rfbEncodingPointerPos, rfbTightVncVendor,
	  sig_rfbEncodingPointerPos, "Pointer position update");
  CapsAdd(encodingCaps, rfbEncodingLastRect, rfbTightVncVendor,
	  sig_rfbEncodingLastRect, "LastRect protocol extension");

  CapsAdd(encodingCaps, rfbEncodingNewFBSize, rfbTightVncVendor,
	  sig_rfbEncodingNewFBSize, "New FB size protocol extension");

#ifdef TURBOVNC
  CapsAdd(encodingCaps, rfbJpegQualityLevel1, rfbTurboVncVendor,
	  sig_rfbEncodingNewFBSize, "TurboJPEG quality level");
  CapsAdd(encodingCaps, rfbJpegSubsamp1X, rfbTurboVncVendor,
	  sig_rfbEncodingNewFBSize, "TurboJPEG subsampling level");
#endif
}

static char msgbuf[10000];

static void wmsg(char *msg, int wait) {
	fprintf(stderr, "%s", msg);
	if (!use_tty() && !getenv("SSVNC_NO_MESSAGE_POPUP")) {
		CreateMsg(msg, wait);
	}
}

/*
 * ConnectToRFBServer.
 */

Bool
ConnectToRFBServer(const char *hostname, int port)
{
	char *q, *cmd = NULL;
	Bool setnb;
	struct stat sb;

	if (strstr(hostname, "exec=") == hostname) {
		cmd = strdup(hostname);
		q = strchr(cmd, '=');
		*q = ' ';
		if (getenv("SSVNC_BASEDIR")) {
			char *base = getenv("SSVNC_BASEDIR");
			char *newcmd = (char *)malloc(strlen(base) + strlen(cmd) + 1000);	
			sprintf(newcmd, "%s/unwrap.so", base);
			if (stat(newcmd, &sb) == 0) {
#if (defined(__MACH__) && defined(__APPLE__))
				sprintf(newcmd, "DYLD_FORCE_FLAT_NAMESPACE=1; export DYLD_FORCE_FLAT_NAMESPACE; DYLD_INSERT_LIBRARIES='%s/unwrap.so'; export DYLD_INSERT_LIBRARIES; %s", base, cmd);
#else
				sprintf(newcmd, "LD_PRELOAD='%s/unwrap.so'; export LD_PRELOAD; %s", base, cmd);
#endif
				cmd = newcmd;
			}
		}
	}

	if (cmd != NULL) {
		int sfd[2];
		char *q, *cmd2 = strdup(cmd);
		pid_t pid;

		q = strstr(cmd2, "pw=");
		if (q && !getenv("SSVNC_SHOW_ULTRAVNC_DSM_PASSWORD")) {
			q += strlen("pw=");
			while (*q != '\0' && !isspace(*q)) {
				*q = '*';
				q++;
			}
		}

		fprintf(stderr, "exec-cmd: %s\n\n", cmd2);
		free(cmd2);

		if (! SocketPair(sfd)) {
			return False;
		}
		if (0) {
			fprintf(stderr, "sfd: %d %d\n", sfd[0], sfd[1]);
			fflush(stderr);
		}

		pid = fork();
		if (pid == -1) {
			perror("fork");
			return False;
		}
		if (pid == 0) {
			char *args[4];
			int d;
			args[0] = "/bin/sh";
			args[1] = "-c";
			args[2] = cmd;
			args[3] = NULL;

			close(sfd[1]);
			dup2(sfd[0], 0);
			dup2(sfd[0], 1);
			for (d=3; d < 256; d++) {
				if (d != sfd[0]) {
					close(d);
				}
			}
			execvp(args[0], args);
			perror("exec");
			exit(1);
		} else {
			close(sfd[0]);
			rfbsock = sfd[1];
			exec_pid = pid;
		}
		if (rfbsock < 0) {
			sprintf(msgbuf,"Unable to connect to exec'd command: %s\n", cmd);
			wmsg(msgbuf, 1);
			return False;
		}
	} else if (strstr(hostname, "fd=") == hostname) {
		rfbsock = atoi(hostname + strlen("fd="));
	} else if ((strchr(hostname, '/') && stat(hostname, &sb) == 0) || strstr(hostname, "unix=") == hostname) {
		/* assume unix domain socket */
		char *thost; 
		if (strstr(hostname, "unix=") == hostname) {
			thost = strdup(hostname + strlen("unix=")); 
		} else {
			thost = strdup(hostname); 
		}

		rfbsock = ConnectToUnixSocket(thost);
		free(thost);

		if (rfbsock < 0) {
			sprintf(msgbuf,"Unable to connect to VNC server (unix-domain socket: %s)\n", hostname);
			wmsg(msgbuf, 1);
			return False;
		}
		
	} else {
		rfbsock = ConnectToTcpAddr(hostname, port);

		if (rfbsock < 0 && !appData.noipv4) {
			char *q, *hosttmp;
			if (hostname[0] == '[') {
				hosttmp = strdup(hostname+1);
			} else {
				hosttmp = strdup(hostname);
			}
			q = strrchr(hosttmp, ']');
			if (q) *q = '\0';
			if (strstr(hosttmp, "::ffff:") == hosttmp || strstr(hosttmp, "::FFFF:") == hosttmp) {
				char *host = hosttmp + strlen("::ffff:");
				if (dotted_ip(host, 0))  {
					fprintf(stderr, "ConnectToTcpAddr[ipv4]: re-trying connection using '%s'\n", host);
					rfbsock = ConnectToTcpAddr(host, port);
				}
			}
			free(hosttmp);
		}

		if (rfbsock < 0) {
			sprintf(msgbuf,"Unable to connect to VNC server (%s:%d)\n", hostname, port);
			wmsg(msgbuf, 1);
			return False;
		}
	}

	setnb = SetNonBlocking(rfbsock);
	return setnb;
}

static void printFailureReason(void) {
	CARD32 reasonLen;
	ReadFromRFBServer((char *)&reasonLen, 4);
	reasonLen = Swap32IfLE(reasonLen);
	if (reasonLen < 4096) {
		char *reason = (char *) malloc(reasonLen+1);
		memset(reason, 0, reasonLen+1);
		ReadFromRFBServer(reason, reasonLen);
		sprintf(msgbuf, "Reason: %s\n", reason);
		wmsg(msgbuf, 1);
		free(reason);
	}
}

static char *pr_sec_type(int type) {
	char *str = "unknown";
	if (type == rfbSecTypeInvalid)	str = "rfbSecTypeInvalid";
	if (type == rfbSecTypeNone)	str = "rfbSecTypeNone";
	if (type == rfbSecTypeVncAuth)	str = "rfbSecTypeVncAuth";
	if (type == rfbSecTypeRA2)	str = "rfbSecTypeRA2";
	if (type == rfbSecTypeRA2ne)	str = "rfbSecTypeRA2ne";
	if (type == rfbSecTypeTight)	str = "rfbSecTypeTight";
	if (type == rfbSecTypeUltra)	str = "rfbSecTypeUltra";

	if (type == rfbSecTypeAnonTls)	str = "rfbSecTypeAnonTls";
	if (type == rfbSecTypeVencrypt)	str = "rfbSecTypeVencrypt";

	if (type == (int) rfbUltraVncMsLogon)		str = "rfbUltraVncMsLogon";

	/* careful, not all security types below: */
	if (!strcmp(str, "unknown")) {
		if (type == rfbUltraVNC_SCPrompt)		str = "rfbUltraVNC_SCPrompt";
		if (type == rfbUltraVNC_SessionSelect)		str = "rfbUltraVNC_SessionSelect";
		if (type == rfbUltraVNC_MsLogonIAuth)		str = "rfbUltraVNC_MsLogonIAuth";
		if (type == rfbUltraVNC_MsLogonIIAuth)		str = "rfbUltraVNC_MsLogonIIAuth";
		if (type == rfbUltraVNC_SecureVNCPluginAuth)	str = "rfbUltraVNC_SecureVNCPluginAuth";
		if (type == rfbLegacy_SecureVNCPlugin)		str = "rfbLegacy_SecureVNCPlugin";
		if (type == rfbLegacy_MsLogon)			str = "rfbLegacy_MsLogon";
	}

	return str;
}

static char *pr_sec_subtype(int type) {
	char *str = "unknown";
	if (type == rfbVencryptPlain) str = "rfbVencryptPlain";
	if (type == rfbVencryptTlsNone) str = "rfbVencryptTlsNone";
	if (type == rfbVencryptTlsVnc) str = "rfbVencryptTlsVnc";
	if (type == rfbVencryptTlsPlain) str = "rfbVencryptTlsPlain";
	if (type == rfbVencryptX509None) str = "rfbVencryptX509None";
	if (type == rfbVencryptX509Vnc) str = "rfbVencryptX509Vnc";
	if (type == rfbVencryptX509Plain) str = "rfbVencryptX509Plain";
	return str;
}

extern void ProcessXtEvents(void);
extern char *time_str(void);
/*
 * InitialiseRFBConnection.
 */

Bool
InitialiseRFBConnection(void)
{
	rfbProtocolVersionMsg pv;
	rfbClientInitMsg ci;
	int i, secType, anon_dh = 0, accept_uvnc = 0;
	FILE *pd;
	char *hsfile = NULL;
	char *hsparam[128];
	char *envsetsec = getenv("SSVNC_SET_SECURITY_TYPE");
	char line[128];
	double dt = 0.0;

	/* if the connection is immediately closed, don't report anything, so
	   that pmw's monitor can make test connections */

	if (listenSpecified) {
		errorMessageOnReadFailure = False;
	}

	for (i=0; i < 128; i++) {
		hsparam[i] = NULL;
	}

	skip_XtUpdateAll = 1;
	ProcessXtEvents();
	skip_XtUpdateAll = 0;

	if (getenv("SSVNC_PREDIGESTED_HANDSHAKE")) {
		double start = dnow();
		hsfile = getenv("SSVNC_PREDIGESTED_HANDSHAKE");
		while (dnow() < start + 10.0) {
			int done = 0;
			usleep(100 * 1000);
			if ((pd = fopen(hsfile, "r")) != NULL) {
				while (fgets(line, 128, pd) != NULL) {
					if (strstr(line, "done") == line) {
						done = 1;
						usleep(100 * 1000);
						break;
					}
				}
				fclose(pd);
			}
			if (done) {
				break;
			}
		}
		if ((pd = fopen(hsfile, "r")) != NULL) {
			i = 0;
			while (fgets(line, 128, pd) != NULL) {
				hsparam[i] = strdup(line);
				fprintf(stderr, "%s", line);
				if (i++ > 100) break; 
			}
			fclose(pd);
		}
		unlink(hsfile);
	}

	if (getenv("SSVNC_SKIP_RFB_PROTOCOL_VERSION")) {
		viewer_major = 3;
		viewer_minor = 8;
		goto end_of_proto_msg;
	} else if (hsfile) {
		int k = 0;
		while (hsparam[k] != NULL) {
			char *str = hsparam[k++];
			if (strstr(str, "server=") == str) {
				sprintf(pv, "%s", str + strlen("server="));
				goto readed_pv;
			}
		}
	}

	dt = dnow();
	if (!ReadFromRFBServer(pv, sz_rfbProtocolVersionMsg)) {
		return False;
	}
	if (getenv("PRINT_DELAY1")) fprintf(stderr, "delay1: %.3f ms\n", (dnow() - dt) * 1000);
	dt = 0.0;

	readed_pv:

	errorMessageOnReadFailure = True;

	pv[sz_rfbProtocolVersionMsg] = 0;

	if (strstr(pv, "ID:") == pv) {
		;
	} else if (sscanf(pv, rfbProtocolVersionFormat, &server_major, &server_minor) != 2) {
		if (strstr(pv, "test") == pv) {
			/* now some hacks for ultraVNC SC III (SSL) ... testA, etc */
			int i;
			char *se = NULL;

			fprintf(stderr,"Trying UltraVNC Single Click III workaround: %s\n", pv);
			for (i=0; i < 7 ; i++) {
				pv[i] = pv[i+5];
			}
			if (!ReadFromRFBServer(pv+7, 5)) {
				return False;
			}

			se = getenv("STUNNEL_EXTRA_OPTS");
			if (se == NULL) {
				se = getenv("STUNNEL_EXTRA_OPTS_USER");
			}
			if (se != NULL) {
				if (strstr(se, "options")) {
					if (strstr(se, "ALL") || strstr(se, "DONT_INSERT_EMPTY_FRAGMENTS")) {
						;	/* good */
					} else {
						se = NULL;
					}
				} else {
					se = NULL;
				}
			}
			if (se == NULL) {
				msgbuf[0] = '\0';
				strcat(msgbuf, "\n");
				strcat(msgbuf, "***************************************************************\n");
				strcat(msgbuf, "To work around UltraVNC SC III SSL dropping after a few minutes\n");
				strcat(msgbuf, "you may need to set STUNNEL_EXTRA_OPTS_USER='options = ALL'.\n");
				strcat(msgbuf, "***************************************************************\n");
				strcat(msgbuf, "\n");
				wmsg(msgbuf, 0);
			}
			if (strstr(pv, "ID:") == pv) {
				goto check_ID_string;
			}
			if (sscanf(pv, rfbProtocolVersionFormat, &server_major, &server_minor) == 2) {
				goto ultra_vnc_nonsense;
			}
		}
		sprintf(msgbuf, "Not a valid VNC server: '%s'\n", pv);
		wmsg(msgbuf, 1);
		return False;
	}

	check_ID_string:
	if (strstr(pv, "ID:") == pv) {
		char tmp[256];
		fprintf(stderr, "UltraVNC Repeater string detected: %s\n", pv);
		fprintf(stderr, "Pretending to be UltraVNC repeater: reading 250 bytes...\n\n");
		if (!ReadFromRFBServer(tmp, 250 - 12)) {
			return False;
		}
		if (!ReadFromRFBServer(pv, 12)) {
			return False;
		}
		if (sscanf(pv, rfbProtocolVersionFormat, &server_major, &server_minor) != 2) {
			sprintf(msgbuf,"Not a valid VNC server: '%s'\n", pv);
			wmsg(msgbuf, 1);
			return False;
		}
	}

	ultra_vnc_nonsense:
	fprintf(stderr,"\nProto: %s\n", pv);

	viewer_major = 3;

	if (appData.rfbVersion != NULL && sscanf(appData.rfbVersion, "%d.%d", &viewer_major, &viewer_minor) == 2) {
		fprintf(stderr,"Setting RFB version to %d.%d from -rfbversion.\n\n", viewer_major, viewer_minor);

	} else if (getenv("SSVNC_RFB_VERSION") != NULL && sscanf(getenv("SSVNC_RFB_VERSION"), "%d.%d", &viewer_major, &viewer_minor) == 2) {
		fprintf(stderr,"Setting RFB version to %d.%d from SSVNC_RFB_VERSION.\n\n", viewer_major, viewer_minor);

	} else if (server_major > 3) {
		viewer_minor = 8;
	} else if (server_major == 3 && (server_minor == 14 || server_minor == 16)) {
		/* hack for UltraVNC Single Click. They misuse rfb proto version */
		fprintf(stderr,"Setting RFB version to 3.3 for UltraVNC Single Click.\n\n");
		viewer_minor = 3;

	} else if (server_major == 3 && server_minor >= 8) {
		/* the server supports at least the standard protocol 3.8 */
		viewer_minor = 8;

	} else if (server_major == 3 && server_minor == 7) {
		/* the server supports at least the standard protocol 3.7 */
		viewer_minor = 7;

	} else {
		/* any other server version, request the standard 3.3 */
		viewer_minor = 3;
	}
	/* n.b. Apple Remote Desktop uses 003.889, but we should be OK with 3.8 */

	if (appData.msLogon) {
		if (server_minor == 4) {
			fprintf(stderr,"Setting RFB version to 3.4 for UltraVNC MS Logon.\n\n");
			viewer_minor = 4;
		}
	}
	if (getenv("SSVNC_ACCEPT_POPUP_SC")) {
		if (server_minor == -4 || server_minor == -6 || server_minor == 14 || server_minor == 16) {
			/* 4 and 6 work too? */
			viewer_minor = server_minor;
			accept_uvnc = 1;
			fprintf(stderr,"Reset RFB version to 3.%d for UltraVNC SSVNC_ACCEPT_POPUP_SC.\n\n", viewer_minor);
		}
	}

	fprintf(stderr, "Connected to RFB server, using protocol version %d.%d\n", viewer_major, viewer_minor);

	if (hsfile) {
		int k = 0;
		while (hsparam[k] != NULL) {
			char *str = hsparam[k++];
			if (strstr(str, "latency=") == str) {
				latency = 1000. * atof(str + strlen("latency="));
			}
		}
		k = 0;
		while (hsparam[k] != NULL) {
			char *str = hsparam[k++];
			int v1, v2;
			if (sscanf(str, "viewer=RFB %d.%d\n", &v1, &v2) == 2) {
				viewer_major = v1;
				viewer_minor = v2;
				fprintf(stderr, "\nPre-Handshake set protocol version to: %d.%d  Latency: %.2f ms\n", viewer_major, viewer_minor, latency);
				goto end_of_proto_msg;
			}
		}
	}
	sprintf(pv, rfbProtocolVersionFormat, viewer_major, viewer_minor);

	if (!appData.appShare) {
		usleep(100*1000);
	}
	dt = dnow();
	if (!WriteExact(rfbsock, pv, sz_rfbProtocolVersionMsg)) {
		return False;
	}

	end_of_proto_msg:

	if (envsetsec) {
		secType = atoi(getenv("SSVNC_SET_SECURITY_TYPE"));
		goto sec_type;
	}
	if (hsfile) {
		int k = 0;
		while (hsparam[k] != NULL) {
			char *str = hsparam[k++];
			int st;
			if (sscanf(str, "sectype=%d\n", &st) == 1) {
				secType = st;
				fprintf(stderr, "Pre-Handshake set Security-Type to: %d (%s)\n", st, pr_sec_type(st));
				if (secType == rfbSecTypeVencrypt) {
					goto sec_type;
				} else if (secType == rfbSecTypeAnonTls) {
					break;
				}
			}
		}
	}

	if (accept_uvnc) {
		unsigned int msg_sz = 0;
		unsigned int nimmer = 0;
		char msg[3000];
		char *msg_buf, *sip = NULL, *sih = NULL;

		if (!ReadFromRFBServer((char *) &msg_sz, 4)) {
			return False;
		}
		dt_out_sc = dnow();
		msg_sz = Swap32IfBE(msg_sz);
		if (msg_sz > 1024) {
			fprintf(stderr, "UVNC msg size too big: %d\n", msg_sz);
			exit(1);
		}
		msg_buf = (char *)calloc(msg_sz + 100, 1);
		if (!ReadFromRFBServer(msg_buf, msg_sz)) {
			free(msg_buf);
			return False;
		}

		if (0) {
			fprintf(stderr, "msg_buf: ");
			write(2, msg_buf, msg_sz);
			fprintf(stderr, "\n");
		}

		sip = get_peer_ip(rfbsock);
		if (strlen(sip) > 100) sip = "0.0.0.0";
		sih = ip2host(sip);
		if (strlen(sih) > 300) sih = "unknown";

		sprintf(msg, "\n(LISTEN) Reverse VNC connection from IP: %s  %s\n                               Hostname: %s\n\n", sip, time_str(), sih);
		strcat(msg, "UltraVNC Server Message:\n");
		strcat(msg, msg_buf);
		free(msg_buf);
		strcat(msg, "\n\n");
		strcat(msg, "Accept or Reject VNC connection?");
		if (CreateMsg(msg, 2)) {
			nimmer = 1;
			fprintf(stderr, "Accepting connection.\n\n");
		} else {
			nimmer = 0;
			fprintf(stderr, "Refusing connection.\n\n");
		}
		if (!WriteExact(rfbsock, (char *) &nimmer, 4)) {
			return False;
		}
	}

	/* Read or select the security type. */
	dt_out = 0.0;

	skip_XtUpdateAll = 1;
	if (viewer_minor >= 7 && !accept_uvnc) {
		secType = SelectSecurityType();
	} else {
		secType = ReadSecurityType();
	}
	skip_XtUpdateAll = 0;

	if (accept_uvnc) {
		dt_out = dt_out_sc;
	}

	if (dt > 0.0 && dt_out > dt) {
		latency = (dt_out - dt) * 1000;
	}
	
	fprintf(stderr, "Security-Type: %3d  (%s)  Latency: %.2f ms\n", (int) secType, pr_sec_type(secType), latency);
	if (secType == rfbSecTypeInvalid) {
		return False;
	}

	sec_type:

	if (hsfile) {
		int subsectype = 0;
		int k = 0;
		while (hsparam[k] != NULL) {
			char *str = hsparam[k++];
			int st;
			if (sscanf(str, "subtype=%d\n", &st) == 1) {
				subsectype = st;
				fprintf(stderr, "Pre-Handshake set Sub-Security-Type to: %d (%s)\n\n", st, pr_sec_subtype(st));
				break;
			}
		}

		if (!subsectype) {
			;
		} else if (secType == rfbSecTypeVencrypt) {
			if (subsectype == rfbVencryptTlsNone) {
				anon_dh = 1;
				secType = rfbSecTypeNone;
			} else if (subsectype == rfbVencryptTlsVnc) {
				anon_dh = 1;
				secType = rfbSecTypeVncAuth;
			} else if (subsectype == rfbVencryptTlsPlain) {
				anon_dh = 1;
				secType = rfbSecTypeNone;
			} else if (subsectype == rfbVencryptX509None) {
				secType = rfbSecTypeNone;
			} else if (subsectype == rfbVencryptX509Vnc) {
				secType = rfbSecTypeVncAuth;
			} else if (subsectype == rfbVencryptX509Plain) {
				secType = rfbSecTypeNone;
			}
			if (subsectype == rfbVencryptTlsPlain || subsectype == rfbVencryptX509Plain) {
				usleep(300*1000);
			}
			if (subsectype == rfbVencryptTlsNone || subsectype == rfbVencryptTlsVnc || subsectype == rfbVencryptTlsPlain) {
				char tmp[1000], line[100];
				tmp[0] = '\0';
				strcat(tmp, "\n");
				sprintf(line, "WARNING: Anonymous Diffie-Hellman TLS used (%s),\n", pr_sec_subtype(subsectype));
				strcat(tmp, line);
				strcat(tmp, "WARNING: there will be *NO* Authentication of the VNC Server.\n");
				strcat(tmp, "WARNING: I.e. a Man-In-The-Middle attack is possible.\n");
				strcat(tmp, "WARNING: Configure the server to use X509 certs and verify them.\n\n");
				wmsg(tmp, 1);
			}
			if (subsectype == rfbVencryptTlsPlain || subsectype == rfbVencryptX509Plain) {
				fprintf(stderr, "\nVeNCrypt Plain (username + passwd) selected.\n\n");
				if (appData.unixPW != NULL) {
					unixpw(appData.unixPW, 1);
				} else if (getenv("SSVNC_UNIXPW")) {
					unixpw(getenv("SSVNC_UNIXPW"), 1);
				} else {
					unixpw(".", 1);
				}
			}
		}
	}

	switch (secType) {
		case rfbSecTypeNone:
			fprintf(stderr, "No VNC authentication needed\n");
			if (accept_uvnc) {
				;	/* we pretended 3.16 etc. */
			} else if (viewer_minor >= 8) {
				CARD32 authResult;

				if (!ReadFromRFBServer((char *)&authResult, 4)) {
					return False;
				}

				authResult = Swap32IfLE(authResult);

				if (authResult == rfbVncAuthOK) {
					fprintf(stderr, "VNC authentication succeeded (%d) for rfbSecTypeNone (RFB 3.8)\n", (int) authResult);
				} else {
					sprintf(msgbuf, "VNC authentication failed (%d) for rfbSecTypeNone (RFB 3.8)\n\n", (int) authResult);
					wmsg(msgbuf, 1);
					return False;
				}
			}
			fprintf(stderr, "\n");
			break;
		case rfbSecTypeVncAuth:
			if (!AuthenticateVNC()) {
				return False;
			}
			break;
		case rfbSecTypeTight:
			tightVncProtocol = True;
			InitCapabilities();
			if (!SetupTunneling()) {
				return False;
			}
			if (!PerformAuthenticationTight()) {
				return False;
			}
			break;
		case rfbUltraVncMsLogon:
			if (!AuthUltraVncMsLogon()) {
				return False;
			}
			break;
		case rfbSecTypeUltra:
			if (!AuthenticateUltraVNC()) {
				return False;
			}
			break;
		default:                      /* should never happen */
			sprintf(msgbuf, "Internal error: Invalid security type: %d\n", secType);
			wmsg(msgbuf, 1);
			return False;
	}

	connect_time = dnow();

	ci.shared = (appData.shareDesktop ? 1 : 0);

	if (!WriteExact(rfbsock, (char *)&ci, sz_rfbClientInitMsg)) {
		return False;
	}

	if (!ReadFromRFBServer((char *)&si, sz_rfbServerInitMsg)) {
		return False;
	}

	si.framebufferWidth = Swap16IfLE(si.framebufferWidth);
	si.framebufferHeight = Swap16IfLE(si.framebufferHeight);
	si.format.redMax = Swap16IfLE(si.format.redMax);
	si.format.greenMax = Swap16IfLE(si.format.greenMax);
	si.format.blueMax = Swap16IfLE(si.format.blueMax);
	si.nameLength = Swap32IfLE(si.nameLength);

	if (appData.chatOnly) {
		si.framebufferWidth = 32;
		si.framebufferHeight = 32;
	}

	/* Check arguments to malloc() calls. */
	if (si.nameLength > 10000) {
		fprintf(stderr, "name length too long: %lu bytes\n",
		    (unsigned long)si.nameLength);
		return False;
		
	}
	desktopName = malloc(si.nameLength + 1);
	if (!desktopName) {
		fprintf(stderr, "Error allocating memory for desktop name, %lu bytes\n",
		    (unsigned long)si.nameLength);
		return False;
	}
	memset(desktopName, 0, si.nameLength + 1);

	if (!ReadFromRFBServer(desktopName, si.nameLength)) {
		return False;
	}

	desktopName[si.nameLength] = 0;

	if (appData.appShare) {
		int x_hint, y_hint;
		char *p, *q = NULL;
		p = desktopName;
		while (*p != '\0') {
 			char *t = strstr(p, " XY=");
			if (t) q = t;
			p++;
		}
		if (q) {
			int ok = 1;
			p = q + strlen(" XY=");
			while (*p != '\0') {
				if (!strpbrk(p, "0123456789,+-")) {
					ok = 0;
				}
				p++;
			}
			if (ok && sscanf(q+1, "XY=%d,%d", &x_hint, &y_hint) == 2) {
				fprintf(stderr,"Using x11vnc appshare position: %s\n\n", q);
				*q = '\0';
				appshare_x_hint = x_hint;
				appshare_y_hint = y_hint;
			}
		}
	}

	fprintf(stderr,"Desktop name \"%s\"\n\n", desktopName);

	fprintf(stderr,"VNC server default format:\n");
	PrintPixelFormat(&si.format);

	if (tightVncProtocol) {
		/* Read interaction capabilities (protocol 3.7t) */
		if (!ReadInteractionCaps()) {
			return False;
		}
	}

	return True;
}


/*
 * Read security type from the server (protocol 3.3)
 */

static int
ReadSecurityType(void)
{
	CARD32 secType;

	/* Read the security type */
	if (!ReadFromRFBServer((char *)&secType, sizeof(secType))) {
		return rfbSecTypeInvalid;
	}
	dt_out = dnow();

	secType = Swap32IfLE(secType);

	if (secType == rfbSecTypeInvalid) {
		ReadConnFailedReason();
		return rfbSecTypeInvalid;
	}

	if (secType == rfbSecTypeNone) {
		;	/* OK */
	} else if (secType == rfbSecTypeVncAuth) {
		;	/* OK */
	} else if (secType == rfbUltraVncMsLogon) {
		;	/* OK */
	} else {
		sprintf(msgbuf, "Unknown security type from RFB server: %d\n", (int)secType);
		wmsg(msgbuf, 1);
		return rfbSecTypeInvalid;
	}

	return (int)secType;
}


/*
 * Select security type from the server's list (protocol 3.7)
 */

static int
SelectSecurityType(void)
{
	CARD8 nSecTypes;
	CARD8 knownSecTypes[] = {rfbSecTypeNone, rfbSecTypeVncAuth, rfbSecTypeUltra};
	int nKnownSecTypes = sizeof(knownSecTypes);
	CARD8 *secTypes;
	CARD8 secType = rfbSecTypeInvalid;
	int i, j;
	int have_none = 0;
	int have_vncauth = 0;
	int have_ultra = 0;

	fprintf(stderr, "\nSelectSecurityType:\n");

	/* Read the list of security types. */
	if (!ReadFromRFBServer((char *)&nSecTypes, sizeof(nSecTypes))) {
		return rfbSecTypeInvalid;
	}
	dt_out = dnow();

	if (nSecTypes == 0) {
		ReadConnFailedReason();
		return rfbSecTypeInvalid;
	}

	secTypes = malloc(nSecTypes);
	if (!ReadFromRFBServer((char *)secTypes, nSecTypes)) {
		return rfbSecTypeInvalid;
	}

	for (j = 0; j < (int)nSecTypes; j++) {
		fprintf(stderr, "  sec-type[%d]  %3d  (%s)\n", j, (int) secTypes[j], pr_sec_type(secTypes[j]));
		if (rfbSecTypeNone == secTypes[j]) {
			have_none = 1;
		} else if (rfbSecTypeVncAuth == secTypes[j]) {
			have_vncauth = 1;
		} else if (rfbSecTypeUltra == secTypes[j]) {
			have_ultra = 1;
		}
	}

	/* Find out if the server supports TightVNC protocol extensions */
	for (j = 0; j < (int)nSecTypes; j++) {
		if (getenv("VNCVIEWER_NO_SEC_TYPE_TIGHT")) {
			break;
		} else if (getenv("SSVNC_NO_SEC_TYPE_TIGHT")) {
			break;
		}
#ifdef TURBOVNC
		break;
#endif
		if (secTypes[j] == rfbSecTypeTight) {
			free(secTypes);
			secType = rfbSecTypeTight;
			if (!WriteExact(rfbsock, (char *)&secType, sizeof(secType))) {
				return rfbSecTypeInvalid;
			}
			fprintf(stderr, "Enabling TightVNC protocol extensions\n");
			return rfbSecTypeTight;
		}
	}

	if (have_ultra) {
		if(have_none || have_vncauth) {
			knownSecTypes[0] = rfbSecTypeUltra; 
			knownSecTypes[1] = rfbSecTypeNone; 
			knownSecTypes[2] = rfbSecTypeVncAuth; 
		} else {
			fprintf(stderr, "Info: UltraVNC server not offering security types 'None' or 'VncAuth'.\n");
		}
	}

	/* Find first supported security type in desired order */
	for (i = 0; i < nKnownSecTypes; i++) {
		for (j = 0; j < (int)nSecTypes; j++) {
			if (secTypes[j] == knownSecTypes[i]) {
				secType = secTypes[j];
				if (!WriteExact(rfbsock, (char *)&secType, sizeof(secType))) {
					free(secTypes);
					return rfbSecTypeInvalid;
				}
				break;
			}
		}
		if (secType != rfbSecTypeInvalid) {
			break;
		}
	}

	if (secType == rfbSecTypeInvalid) {
		fprintf(stderr, "Server did not offer supported security type:\n");
		for (j = 0; j < (int)nSecTypes; j++) {
			int st = (int) secTypes[j];
			fprintf(stderr, " sectype[%d] %d  (%s)\n", j, st, pr_sec_type(st));
			if (st == rfbSecTypeAnonTls) {
				fprintf(stderr, "  info: To connect to 'vino SSL/TLS' you must use the SSVNC GUI\n");
				fprintf(stderr, "  info: or the ssvnc_cmd wrapper script with the correct cmdline arguments.\n");
			} else if (st == rfbSecTypeVencrypt) {
				fprintf(stderr, "  info: To connect to 'VeNCrypt SSL/TLS' you must use the SSVNC GUI\n");
				fprintf(stderr, "  info: or the ssvnc_cmd wrapper script with the correct cmdline arguments.\n");
			} else if (st == rfbSecTypeRA2) {
				fprintf(stderr, "  info: RA2 is a proprietary protocol of RealVNC.\n");
			} else if (st == rfbSecTypeRA2ne) {
				fprintf(stderr, "  info: RA2ne is a proprietary protocol of RealVNC.\n");
			}
		}
	}

	free(secTypes);

	return (int)secType;
}


/*
 * Setup tunneling (protocol version 3.7t).
 */

static Bool
SetupTunneling(void)
{
  rfbTunnelingCapsMsg caps;
  CARD32 tunnelType;

  /* In the protocol version 3.7t, the server informs us about
     supported tunneling methods. Here we read this information. */

  if (!ReadFromRFBServer((char *)&caps, sz_rfbTunnelingCapsMsg))
    return False;

  caps.nTunnelTypes = Swap32IfLE(caps.nTunnelTypes);

  if (caps.nTunnelTypes) {
    if (!ReadCapabilityList(tunnelCaps, caps.nTunnelTypes))
      return False;

    /* We cannot do tunneling anyway yet. */
    tunnelType = Swap32IfLE(rfbNoTunneling);
    if (!WriteExact(rfbsock, (char *)&tunnelType, sizeof(tunnelType)))
      return False;
  }

  return True;
}

static char *restart_session_pw = NULL;
static int restart_session_len = 0;


/*
 * Negotiate authentication scheme (protocol version 3.7t)
 */

static Bool
PerformAuthenticationTight(void)
{
	rfbAuthenticationCapsMsg caps;
	CARD32 authScheme;
	int i;

	/* In the protocol version 3.7t, the server informs us about supported
	authentication schemes. Here we read this information. */

	if (!ReadFromRFBServer((char *)&caps, sz_rfbAuthenticationCapsMsg)) {
		return False;
	}

	caps.nAuthTypes = Swap32IfLE(caps.nAuthTypes);

	if (!caps.nAuthTypes) {
		fprintf(stderr, "No VNC authentication needed\n\n");
		if (viewer_minor >= 8) {
			CARD32 authResult;

			if (!ReadFromRFBServer((char *)&authResult, 4)) {
				return False;
			}

			authResult = Swap32IfLE(authResult);

			if (authResult == rfbVncAuthOK) {
				fprintf(stderr, "VNC authentication succeeded (%d) for PerformAuthenticationTight rfbSecTypeNone (RFB 3.8)\n", (int) authResult);
			} else {
				sprintf(msgbuf, "VNC authentication failed (%d) for PerformAuthenticationTight rfbSecTypeNone (RFB 3.8)\n\n", (int) authResult);
				wmsg(msgbuf, 1);
				return False;
			}
		}
		return True;
	}

	if (!ReadCapabilityList(authCaps, caps.nAuthTypes)) {
		return False;
	}

	/* Prefer Unix login authentication if a user name was given. */
	if (appData.userLogin && CapsIsEnabled(authCaps, rfbAuthUnixLogin)) {
		authScheme = Swap32IfLE(rfbAuthUnixLogin);
		if (!WriteExact(rfbsock, (char *)&authScheme, sizeof(authScheme))) {
			return False;
		}
		return AuthenticateUnixLogin();
	}

	/* Otherwise, try server's preferred authentication scheme. */
	for (i = 0; i < CapsNumEnabled(authCaps); i++) {
		authScheme = CapsGetByOrder(authCaps, i);
		if (authScheme != rfbAuthUnixLogin && authScheme != rfbAuthVNC) {
			continue;                 /* unknown scheme - cannot use it */
		}
		authScheme = Swap32IfLE(authScheme);
		if (!WriteExact(rfbsock, (char *)&authScheme, sizeof(authScheme))) {
			return False;
		}
		authScheme = Swap32IfLE(authScheme); /* convert it back */
		if (authScheme == rfbAuthUnixLogin) {
			return AuthenticateUnixLogin();
		} else if (authScheme == rfbAuthVNC) {
			return AuthenticateVNC();
		} else {
			/* Should never happen. */
			fprintf(stderr, "Assertion failed: unknown authentication scheme\n");
			return False;
		}
	}

	sprintf(msgbuf, "No suitable authentication schemes offered by server\n");
	wmsg(msgbuf, 1);
	return False;
}

#if 0
unsigned char encPasswd[8];
unsigned char encPasswd_MSLOGON[32];
char clearPasswd_MSLOGIN[256];
static Bool old_ultravnc_mslogon_code(void) {
	char *passwd = NULL;
	CARD8 challenge_mslogon[CHALLENGESIZE_MSLOGON];

	/* code from the old uvnc way (1.0.2?) that would go into AuthenticateVNC() template */
	
	if (appData.msLogon != NULL) {
		raiseme(1);
		if (!strcmp(appData.msLogon, "1")) {
			char tmp[256];
			fprintf(stderr, "\nUltraVNC MS Logon Username[@Domain]: ");
			if (fgets(tmp, 256, stdin) == NULL) {
				exit(1);
			}
			appData.msLogon = strdup(tmp);
		}
		passwd = getpass("UltraVNC MS Logon Password: ");
		if (! passwd) {
			exit(1);
		}
		fprintf(stderr, "\n");

		UvncEncryptPasswd_MSLOGON(encPasswd_MSLOGON, passwd);
	}
	if (appData.msLogon) {
		if (!ReadFromRFBServer((char *)challenge_mslogon, CHALLENGESIZE_MSLOGON)) {
			return False;
		}
	}
	if (appData.msLogon) {
		int i;
		char tmp[256];
		char *q, *domain = ".";
		for (i=0; i < 32; i++) {
			challenge_mslogon[i] = encPasswd_MSLOGON[i] ^ challenge_mslogon[i];
		}
		q = strchr(appData.msLogon, '@');
		if (q) {
			*q = '\0';
			domain = strdup(q+1);
		}
		memset(tmp, 0, sizeof(tmp));
		strcat(tmp, appData.msLogon);
		if (!WriteExact(rfbsock, tmp, 256)) {
			return False;
		}
		memset(tmp, 0, sizeof(tmp));
		strcat(tmp, domain);
		if (!WriteExact(rfbsock, tmp, 256)) {
			return False;
		}
		memset(tmp, 0, sizeof(tmp));
		strcat(tmp, passwd);
		if (!WriteExact(rfbsock, tmp, CHALLENGESIZE_MSLOGON)) {
			return False;
		}
	}
}
#endif

static void hexprint(char *label, char *data, int len) {
	int i;
	fprintf(stderr, "%s: ", label);
	for (i=0; i < len; i++) {
		unsigned char c = (unsigned char) data[i];
		fprintf(stderr, "%02x ", (int) c);
		if ((i+1) % 20 == 0) {
			fprintf(stderr, "\n%s: ", label);
		}
	}
	fprintf(stderr, "\n");
}

#define DH_MAX_BITS 31
static unsigned long long max_dh = ((unsigned long long) 1) << DH_MAX_BITS;

static unsigned long long bytes_to_uint64(char *bytes) {
	unsigned long long result = 0;
	int i;

	for (i=0; i < 8; i++) {
		result <<= 8;
		result += (unsigned char) bytes[i];
	}
	return result;
}

static void uint64_to_bytes(unsigned long long n, char *bytes) {
	int i;

	for (i=0; i < 8; i++) {
		bytes[i] = (unsigned char) (n >> (8 * (7 - i)));
	}
}

static void try_invert(char *wireuser, char *wirepass, unsigned long long actual_key) {
	if (wireuser || wirepass || actual_key) {}
	return;
}


static unsigned long long XpowYmodN(unsigned long long x, unsigned long long y, unsigned long long N) {
	unsigned long long result = 1;
	unsigned long long oneShift63 = ((unsigned long long) 1) << 63;
	int i;

	for (i = 0; i < 64; y <<= 1, i++) {
		result = result * result % N;
		if (y & oneShift63) {
			result = result * x % N;
		}
	}
	return result;
}

/*
 * UltraVNC MS-Logon authentication (for v1.0.5 and later.)
 */

/*
 * NOTE: The UltraVNC MS-Logon username and password exchange is
 *       VERY insecure.  It can be brute forced in ~2e+9 operations.
 *       It's not clear we should support it...  It is only worth using
 *       in an environment where no one is sniffing the network, in which
 *       case all of this DH exchange secrecy is unnecessary...
 */

static Bool AuthUltraVncMsLogon(void) {
	CARD32 authResult;
	char gen[8], mod[8], pub[8], rsp[8];
	char user[256], passwd[64], *gpw;
	unsigned char key[8];
	unsigned long long ugen, umod, ursp, upub, uprv, ukey;
	double now = dnow();
	int db = 0;

	if (getenv("SSVNC_DEBUG_MSLOGON")) {
		db = atoi(getenv("SSVNC_DEBUG_MSLOGON"));
	}

	fprintf(stderr, "\nAuthUltraVncMsLogon()\n");

	if (!ReadFromRFBServer(gen, sizeof(gen))) {
		return False;
	}
	if (db) hexprint("gen", gen, sizeof(gen));

	if (!ReadFromRFBServer(mod, sizeof(mod))) {
		return False;
	}
	if (db) hexprint("mod", mod, sizeof(mod));
	
	if (!ReadFromRFBServer(rsp, sizeof(rsp))) {
		return False;
	}
	if (db) hexprint("rsp", rsp, sizeof(rsp));

	ugen = bytes_to_uint64(gen);
	umod = bytes_to_uint64(mod);
	ursp = bytes_to_uint64(rsp);

	if (db) {
		fprintf(stderr, "ugen: 0x%016llx %12llu\n", ugen, ugen);
		fprintf(stderr, "umod: 0x%016llx %12llu\n", umod, umod);
		fprintf(stderr, "ursp: 0x%016llx %12llu\n", ursp, ursp);
	}

	if (ugen > max_dh) {
		fprintf(stderr, "ugen: too big: 0x%016llx\n", ugen);
		return False;
	}

	if (umod > max_dh) {
		fprintf(stderr, "umod: too big: 0x%016llx\n", umod);
		return False;
	}

	/* make a random long long: */
	uprv = 0xffffffff * (now - (unsigned int) now);
	uprv = uprv << 32;
	uprv |= (unsigned long long) urandom();
	uprv = uprv % max_dh;

	if (db) fprintf(stderr, "uprv: 0x%016llx %12llu\n", uprv, uprv);

	upub = XpowYmodN(ugen, uprv, umod);

	if (db) fprintf(stderr, "upub: 0x%016llx %12llu\n", upub, upub);

	uint64_to_bytes(upub, pub);

	if (db) hexprint("pub", pub, sizeof(pub));

	if (!WriteExact(rfbsock, (char *)pub, sizeof(pub))) {
		return False;
	}
	if (db) fprintf(stderr, "wrote pub.\n");

	if (ursp > max_dh) {
		fprintf(stderr, "ursp: too big: 0x%016llx\n", ursp);
		return False;
	}

	ukey = XpowYmodN(ursp, uprv, umod);

	if (db) fprintf(stderr, "ukey: 0x%016llx %12llu\n", ukey, ukey);

	if (1)  {
		char tmp[10000];
		tmp[0] = '\0';
		strcat(tmp, "\n");
		strcat(tmp, "WARNING: The UltraVNC Diffie-Hellman Key is weak (key < 2e+9, i.e. 31 bits)\n");
		strcat(tmp, "WARNING: and so an eavesdropper could recover your MS-Logon username and\n");
		strcat(tmp, "WARNING: password via brute force in a few seconds of CPU time. \n");
		strcat(tmp, "WARNING: If this connection is NOT being tunnelled through a separate SSL or\n");
		strcat(tmp, "WARNING: SSH encrypted tunnel, consider things carefully before proceeding...\n");
		strcat(tmp, "WARNING: Do not enter an important username+password when prompted below if\n");
		strcat(tmp, "WARNING: there is a risk of an eavesdropper sniffing this connection.\n");
		strcat(tmp, "WARNING: UltraVNC MSLogon encryption is VERY weak.  You've been warned!\n");
		wmsg(tmp, 1);
	}

	uint64_to_bytes(ukey, (char *) key);

	if (appData.msLogon == NULL || !strcmp(appData.msLogon, "1")) {
		char tmp[256], *q, *s;
		if (!use_tty()) {
			fprintf(stderr, "\nEnter UltraVNC MS-Logon Username[@Domain] in the popup.\n");
			s = DoUserDialog(); 
		} else {
			raiseme(1);
			fprintf(stderr, "\nUltraVNC MS-Logon Username[@Domain]: ");
			if (fgets(tmp, 256, stdin) == NULL) {
				exit(1);
			}
			s = strdup(tmp);
		}
		q = strchr(s, '\n');
		if (q) *q = '\0';
		appData.msLogon = strdup(s);
	}

	if (!use_tty()) {
		gpw = DoPasswordDialog(); 
	} else {
		raiseme(1);
		gpw = getpass("UltraVNC MS-Logon Password: ");
	}
	if (! gpw) {
		return False;
	}
	fprintf(stderr, "\n");

	memset(user, 0, sizeof(user));
	strncpy(user, appData.msLogon, 255);

	memset(passwd, 0, sizeof(passwd));
	strncpy(passwd, gpw, 63);

	if (db > 1) {
		fprintf(stderr, "user='%s'\n", user);
		fprintf(stderr, "pass='%s'\n", passwd);
	}

	UvncEncryptBytes2((unsigned char *) user,   sizeof(user),   key);
	UvncEncryptBytes2((unsigned char *) passwd, sizeof(passwd), key);

	if (getenv("TRY_INVERT")) {
		try_invert(user, passwd, ukey);
		exit(0);
	}

	if (db) {
		hexprint("user", user, sizeof(user));
		hexprint("pass", passwd, sizeof(passwd));
	}

	if (!WriteExact(rfbsock, user, sizeof(user))) {
		return False;
	}
	if (db) fprintf(stderr, "wrote user.\n");

	if (!WriteExact(rfbsock, passwd, sizeof(passwd))) {
		return False;
	}
	if (db) fprintf(stderr, "wrote passwd.\n");

	if (!ReadFromRFBServer((char *) &authResult, 4)) {
		return False;
	}
	authResult = Swap32IfLE(authResult);

	if (db) fprintf(stderr, "authResult: %d\n", (int) authResult);

	switch (authResult) {
	case rfbVncAuthOK:
		fprintf(stderr, "UVNC MS-Logon authentication succeeded.\n\n");
		break;
	case rfbVncAuthFailed:
		fprintf(stderr, "UVNC MS-Logon authentication failed.\n");
		if (viewer_minor >= 8) {
			printFailureReason();
		} else {
			sprintf(msgbuf, "UVNC MS-Logon authentication failed.\n");
			wmsg(msgbuf, 1);
		}
		fprintf(stderr, "\n");
		return False;
	case rfbVncAuthTooMany:
		sprintf(msgbuf, "UVNC MS-Logon authentication failed - too many tries.\n\n");
		wmsg(msgbuf, 1);
		return False;
	default:
		sprintf(msgbuf, "Unknown UVNC MS-Logon authentication result: %d\n\n",
		    (int)authResult);
		wmsg(msgbuf, 1);
		return False;
	}

	return True;
}

/*
 * Standard VNC authentication.
 */

static Bool
AuthenticateVNC(void)
{
	CARD32 authScheme, authResult;
	CARD8 challenge[CHALLENGESIZE];
	char *passwd = NULL;
	char  buffer[64];
	char* cstatus;
	int   len;
	int restart = 0;

	if (authScheme) {}

	fprintf(stderr, "\nPerforming standard VNC authentication\n");

	if (!ReadFromRFBServer((char *)challenge, CHALLENGESIZE)) {
		return False;
	}
		
	if (restart_session_pw != NULL) {
		passwd = restart_session_pw;	
		restart_session_pw = NULL;
		restart = 1;
	} else if (appData.passwordFile) {
		passwd = vncDecryptPasswdFromFile(appData.passwordFile);
		if (!passwd) {
			sprintf(msgbuf, "Cannot read valid password from file \"%s\"\n", appData.passwordFile);
			wmsg(msgbuf, 1);
			return False;
		}
	} else if (appData.autoPass) {
		passwd = buffer;
		raiseme(1);
		cstatus = fgets(buffer, sizeof buffer, stdin);
		if (cstatus == NULL) {
			buffer[0] = '\0';
		} else {
			len = strlen(buffer);
			if (len > 0 && buffer[len - 1] == '\n') {
				buffer[len - 1] = '\0';
			}
		}
	} else if (getenv("VNCVIEWER_PASSWORD")) {
		passwd = strdup(getenv("VNCVIEWER_PASSWORD"));
	} else if (appData.passwordDialog || !use_tty()) {
		passwd = DoPasswordDialog();
	} else {
		raiseme(1);
		passwd = getpass("VNC Password: ");
	}

	if (getenv("VNCVIEWER_PASSWORD")) {
		putenv("VNCVIEWER_PASSWORD=none");
	}

	if (restart) {
#define EN0 0
#define DE1 1
		unsigned char s_fixedkey[8] = {23,82,107,6,35,78,88,7};
		deskey(s_fixedkey, DE1);
		des(passwd, passwd);
	} else {
		if (!passwd || strlen(passwd) == 0) {
			sprintf(msgbuf, "Reading password failed\n\n");
			wmsg(msgbuf, 1);
			return False;
		}
		if (strlen(passwd) > 8) {
			passwd[8] = '\0';
		}
	}

	vncEncryptBytes(challenge, passwd);
	


#if 0
	/* Lose the password from memory */
	memset(passwd, '\0', strlen(passwd));
#endif

	if (!WriteExact(rfbsock, (char *)challenge, CHALLENGESIZE)) {
		return False;
	}

	if (!ReadFromRFBServer((char *)&authResult, 4)) {
		return False;
	}

	authResult = Swap32IfLE(authResult);

	switch (authResult) {
	case rfbVncAuthOK:
		fprintf(stderr, "VNC authentication succeeded\n\n");
		break;
	case rfbVncAuthFailed:
		fprintf(stderr, "VNC authentication failed.\n");
		if (viewer_minor >= 8) {
			printFailureReason();
		} else {
			sprintf(msgbuf, "VNC authentication failed.\n");
			wmsg(msgbuf, 1);
		}
		fprintf(stderr, "\n");
		return False;
	case rfbVncAuthTooMany:
		sprintf(msgbuf, "VNC authentication failed - too many tries\n\n");
		wmsg(msgbuf, 1);
		return False;
	default:
		sprintf(msgbuf, "Unknown VNC authentication result: %d\n\n", (int)authResult);
		wmsg(msgbuf, 1);
		return False;
	}

	return True;
}

static Bool
AuthenticateUltraVNC(void)
{
	CARD32 authCont;
	CARD8 nSecTypes;

	CARD8 knownSecTypes[] = {rfbSecTypeNone, rfbSecTypeVncAuth, rfbUltraVNC_MsLogonIIAuth};
	int nKnownSecTypes = sizeof(knownSecTypes);
	CARD8 *secTypes;
	CARD8 secType = rfbSecTypeInvalid;
	int i, j;

	fprintf(stderr, "\nAuthenticateUltraVNC:\n");
	if (!ReadFromRFBServer((char *)&authCont, 4)) {
		return False;
	}
	authCont = Swap32IfLE(authCont);
	fprintf(stderr, "UltraVNC authCont 0x%x\n", authCont);

	if (authCont != rfbVncAuthContinue) {
		sprintf(msgbuf, "Unknown UltraVNC authentication response: 0x%x\n\n", authCont);
		wmsg(msgbuf, 1);
		return False;
	}

	if (!ReadFromRFBServer((char *)&nSecTypes, sizeof(nSecTypes))) {
		return False;
	}

	if (nSecTypes == 0) {
		ReadConnFailedReason();
		return False;
	}

	secTypes = malloc(nSecTypes);
	if (!ReadFromRFBServer((char *)secTypes, nSecTypes)) {
		return False;
	}

	for (j = 0; j < (int)nSecTypes; j++) {
		fprintf(stderr, "  ultravnc-sub-sec-type[%d] %3d (%s)\n", j, (int) secTypes[j], pr_sec_type(secTypes[j]));
	}

	/* Find first supported security type in desired order */
	for (i = 0; i < nKnownSecTypes; i++) {
		for (j = 0; j < (int)nSecTypes; j++) {
			if (secTypes[j] == knownSecTypes[i]) {
				secType = secTypes[j];
				if (!WriteExact(rfbsock, (char *)&secType, sizeof(secType))) {
					free(secTypes);
					return False;
				}
				break;
			}
		}
		if (secType != rfbSecTypeInvalid) {
			break;
		}
	}

	free(secTypes);

	if (secType == rfbSecTypeInvalid) {
		return False;
	} else if (secType == rfbSecTypeNone) {
		CARD32 authResult;
		fprintf(stderr, "No VNC authentication needed for UltraVNC server.\n");
		/* must be RFB 3.8 or later here */

		if (!ReadFromRFBServer((char *)&authResult, 4)) {
			return False;
		}

		authResult = Swap32IfLE(authResult);

		if (authResult == rfbVncAuthOK) {
			fprintf(stderr, "VNC authentication succeeded (%d) for rfbSecTypeNone (RFB 3.8)\n", (int) authResult);
		} else {
			sprintf(msgbuf, "VNC authentication failed (%d) for rfbSecTypeNone (RFB 3.8)\n\n", (int) authResult);
			wmsg(msgbuf, 1);
			return False;
		}
		return True;
	} else if (secType == rfbSecTypeVncAuth) {
		return AuthenticateVNC();
	} else if (secType == rfbUltraVNC_MsLogonIIAuth) {
		return AuthUltraVncMsLogon();
	}

	return False;
}

/*
 * Unix login-style authentication.
 */

static Bool
AuthenticateUnixLogin(void)
{
	CARD32 loginLen, passwdLen, authResult;
	char *login;
	char *passwd;
	struct passwd *ps;

	fprintf(stderr, "\nPerforming Unix login-style authentication\n");

	if (appData.userLogin) {
		login = appData.userLogin;
	} else {
		ps = getpwuid(getuid());
		login = ps->pw_name;
	}

	fprintf(stderr, "Using user name \"%s\"\n", login);

	if (appData.passwordDialog || !use_tty()) {
		passwd = DoPasswordDialog();
	} else {
		raiseme(1);
		passwd = getpass("VNC Password: ");
	}
	if (!passwd || strlen(passwd) == 0) {
		fprintf(stderr, "Reading password failed\n");
		return False;
	}

	loginLen = Swap32IfLE((CARD32)strlen(login));
	passwdLen = Swap32IfLE((CARD32)strlen(passwd));

	if (!WriteExact(rfbsock, (char *)&loginLen, sizeof(loginLen)) ||
	    !WriteExact(rfbsock, (char *)&passwdLen, sizeof(passwdLen))) {
		return False;
	}

	if (!WriteExact(rfbsock, login, strlen(login)) ||
	    !WriteExact(rfbsock, passwd, strlen(passwd))) {
		return False;
	}

#if 0
	/* Lose the password from memory */
	memset(passwd, '\0', strlen(passwd));
#endif

	if (!ReadFromRFBServer((char *)&authResult, sizeof(authResult))) {
		return False;
	}

	authResult = Swap32IfLE(authResult);

	switch (authResult) {
	case rfbVncAuthOK:
		fprintf(stderr, "Authentication succeeded\n\n");
		break;
	case rfbVncAuthFailed:
		sprintf(msgbuf, "Authentication failed\n\n");
		wmsg(msgbuf, 1);
		return False;
	case rfbVncAuthTooMany:
		sprintf(msgbuf, "Authentication failed - too many tries\n\n");
		wmsg(msgbuf, 1);
		return False;
	default:
		sprintf(msgbuf, "Unknown authentication result: %d\n\n",
		    (int)authResult);
		wmsg(msgbuf, 1);
		return False;
	}

	return True;
}


/*
 * In the protocol version 3.7t, the server informs us about supported
 * protocol messages and encodings. Here we read this information.
 */

static Bool
ReadInteractionCaps(void)
{
	rfbInteractionCapsMsg intr_caps;

	/* Read the counts of list items following */
	if (!ReadFromRFBServer((char *)&intr_caps, sz_rfbInteractionCapsMsg)) {
		return False;
	}
	intr_caps.nServerMessageTypes = Swap16IfLE(intr_caps.nServerMessageTypes);
	intr_caps.nClientMessageTypes = Swap16IfLE(intr_caps.nClientMessageTypes);
	intr_caps.nEncodingTypes = Swap16IfLE(intr_caps.nEncodingTypes);

	/* Read the lists of server- and client-initiated messages */
	return (ReadCapabilityList(serverMsgCaps, intr_caps.nServerMessageTypes) &&
		ReadCapabilityList(clientMsgCaps, intr_caps.nClientMessageTypes) &&
		ReadCapabilityList(encodingCaps, intr_caps.nEncodingTypes));
}


/*
 * Read the list of rfbCapabilityInfo structures and enable corresponding
 * capabilities in the specified container. The count argument specifies how
 * many records to read from the socket.
 */

static Bool
ReadCapabilityList(CapsContainer *caps, int count)
{
	rfbCapabilityInfo msginfo;
	int i;

	for (i = 0; i < count; i++) {
		if (!ReadFromRFBServer((char *)&msginfo, sz_rfbCapabilityInfo)) {
			return False;
		}
		msginfo.code = Swap32IfLE(msginfo.code);
		CapsEnable(caps, &msginfo);
	}

	return True;
}


/* used to have !tunnelSpecified */

static int guess_compresslevel(void) {
	int n;
	if (latency > 200.0) {
		n = 8;
	} else if (latency > 100.0) {
		n = 7;
	} else if (latency > 60.0) {
		n = 6;
	} else if (latency > 15.0) {
		n = 4;
	} else if (latency > 8.0) {
		n = 2;
	} else if (latency > 0.0) {
		n = 1;
	} else {
		/* no latency measurement */
		n = 3;
	}
	return n;
}

static int guess_qualitylevel(void) {
	int n;
	if (latency > 200.0) {
		n = 4;
	} else if (latency > 100.0) {
		n = 5;
	} else if (latency > 60.0) {
		n = 6;
	} else if (latency > 15.0) {
		n = 7;
	} else if (latency > 8.0) {
		n = 8;
	} else if (latency > 0.0) {
		n = 9;
	} else {
		/* no latency measurement */
		n = 6;
	}
#ifdef TURBOVNC
	n *= 10;
#endif
	return n;
}

/*
 * SetFormatAndEncodings.
 */

Bool
SetFormatAndEncodings()
{
  rfbSetPixelFormatMsg spf;
  char buf[sz_rfbSetEncodingsMsg + MAX_ENCODINGS * 4];
  rfbSetEncodingsMsg *se = (rfbSetEncodingsMsg *)buf;
  CARD32 *encs = (CARD32 *)(&buf[sz_rfbSetEncodingsMsg]);
  int len = 0;
  Bool requestCompressLevel = False;
  Bool requestQualityLevel = False;
  Bool requestLastRectEncoding = False;
  Bool requestNewFBSizeEncoding = True;
  Bool requestTextChatEncoding = True;
  Bool requestSubsampLevel = False;
  int dsm = 0;
  int tQL, tQLmax = 9;
  static int qlmsg = 0, clmsg = 0;
#ifdef TURBOVNC
	tQLmax = 100;
#endif

	if (requestTextChatEncoding || requestSubsampLevel || tQL) {}

#if 0
  fprintf(stderr, "SetFormatAndEncodings: sent_FBU state: %2d\n", sent_FBU);
#endif

  spf.type = rfbSetPixelFormat;
  spf.format = myFormat;
  spf.format.redMax = Swap16IfLE(spf.format.redMax);
  spf.format.greenMax = Swap16IfLE(spf.format.greenMax);
  spf.format.blueMax = Swap16IfLE(spf.format.blueMax);


  currentMsg = rfbSetPixelFormat;
  if (!WriteExact(rfbsock, (char *)&spf, sz_rfbSetPixelFormatMsg))
    return False;

  se->type = rfbSetEncodings;
  se->nEncodings = 0;

  if (appData.ultraDSM) {
  	dsm = 1;
  }

  if (appData.encodingsString) {
    char *encStr = appData.encodingsString;
    int encStrLen;
	if (strchr(encStr, ','))  {
		char *p;
		encStr = strdup(encStr);
		p = encStr;
		while (*p != '\0') {
			if (*p == ',') {
				*p = ' ';
			}
			p++;
		}
	}
    do {
      char *nextEncStr = strchr(encStr, ' ');
      if (nextEncStr) {
	encStrLen = nextEncStr - encStr;
	nextEncStr++;
      } else {
	encStrLen = strlen(encStr);
      }

if (getenv("DEBUG_SETFORMAT")) {
	fprintf(stderr, "encs: ");
	write(2, encStr, encStrLen);
	fprintf(stderr, "\n");
}

      if (strncasecmp(encStr,"raw",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRaw);
      } else if (strncasecmp(encStr,"copyrect",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
      } else if (strncasecmp(encStr,"tight",encStrLen) == 0 && !dsm) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingTight);
	requestLastRectEncoding = True;
	if (appData.compressLevel >= 0 && appData.compressLevel <= 9) {
		requestCompressLevel = True;
        }
	if (appData.enableJPEG) {
		requestQualityLevel = True;
        }
#ifdef TURBOVNC
	requestSubsampLevel = True;
#endif
      } else if (strncasecmp(encStr,"hextile",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingHextile);
      } else if (strncasecmp(encStr,"zlib",encStrLen) == 0 && !dsm) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZlib);
	if (appData.compressLevel >= 0 && appData.compressLevel <= 9) {
		requestCompressLevel = True;
	}
      } else if (strncasecmp(encStr,"corre",encStrLen) == 0 && !dsm) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCoRRE);
      } else if (strncasecmp(encStr,"rre",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRRE);
      } else if (strncasecmp(encStr,"zrle",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZRLE);
#if DO_ZYWRLE
      } else if (strncasecmp(encStr,"zywrle",encStrLen) == 0) {
		int qlevel = appData.qualityLevel;
		if (qlevel < 0 || qlevel > tQLmax) qlevel = guess_qualitylevel();
		encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZYWRLE);
		requestQualityLevel = True;
		if (qlevel < 3) {
			zywrle_level = 3;
		} else if (qlevel < 6) {
			zywrle_level = 2;
		} else {
			zywrle_level = 1;
		}
#endif
      } else {
	fprintf(stderr,"Unknown encoding '%.*s'\n",encStrLen,encStr);
        if (dsm && strstr(encStr, "tight") == encStr) fprintf(stderr, "tight encoding does not yet work with ultraDSM, skipping it.\n");
        if (dsm && strstr(encStr, "corre") == encStr) fprintf(stderr, "corre encoding does not yet work with ultraDSM, skipping it.\n");
        if (dsm && strstr(encStr, "zlib" ) == encStr) fprintf(stderr, "zlib  encoding does not yet work with ultraDSM, skipping it.\n");
      }

      encStr = nextEncStr;
    } while (encStr && se->nEncodings < MAX_ENCODINGS);

    if (se->nEncodings < MAX_ENCODINGS && requestCompressLevel) {
	;
    } else if (se->nEncodings < MAX_ENCODINGS) {
	appData.compressLevel = guess_compresslevel();
	if (clmsg++ == 0) fprintf(stderr, "guessed: -compresslevel %d\n", appData.compressLevel);
    }
    encs[se->nEncodings++] = Swap32IfLE(appData.compressLevel + rfbEncodingCompressLevel0);

    if (se->nEncodings < MAX_ENCODINGS && requestQualityLevel) {
	if (appData.qualityLevel < 0 || appData.qualityLevel > tQLmax) {
		appData.qualityLevel = guess_qualitylevel();
		if (qlmsg++ == 0) fprintf(stderr, "guessed: -qualitylevel  %d\n", appData.qualityLevel);
	}
    } else if (se->nEncodings < MAX_ENCODINGS) {
	appData.qualityLevel = guess_qualitylevel();
	if (qlmsg++ == 0) fprintf(stderr, "guessed: -qualitylevel  %d\n", appData.qualityLevel);
    }
#ifdef TURBOVNC
    tQL = appData.qualityLevel / 10;
    if (tQL < 0) tQL = 1;
    if (tQL > 9) tQL = 9;
    encs[se->nEncodings++] = Swap32IfLE(tQL + rfbEncodingQualityLevel0);
    encs[se->nEncodings++] = Swap32IfLE(appData.qualityLevel + rfbJpegQualityLevel1 - 1);
    if (se->nEncodings < MAX_ENCODINGS && requestSubsampLevel) {
	if (appData.subsampLevel < 0 || appData.subsampLevel > TVNC_SAMPOPT - 1) {
		appData.subsampLevel = TVNC_1X;
	}
	encs[se->nEncodings++] = Swap32IfLE(appData.subsampLevel + rfbJpegSubsamp1X);
    }
#else
    encs[se->nEncodings++] = Swap32IfLE(appData.qualityLevel + rfbEncodingQualityLevel0);
#endif

    if (appData.useRemoteCursor) {
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingXCursor);
      if (se->nEncodings < MAX_ENCODINGS && !appData.useX11Cursor)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRichCursor);
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingPointerPos);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestLastRectEncoding) {
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestNewFBSizeEncoding) {
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingNewFBSize);
    }

  } else {
	/* DIFFERENT CASE */

    if (appData.useRawLocal && SameMachine(rfbsock)) {
      if (!tunnelSpecified) {
	fprintf(stderr,"Same machine: preferring raw encoding\n");
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRaw);
      } else {
	fprintf(stderr,"Tunneling active: preferring tight encoding\n");
      }
    }

    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
    if (!dsm) encs[se->nEncodings++] = Swap32IfLE(rfbEncodingTight);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZRLE);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZYWRLE);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingHextile);
    if (!dsm) encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZlib);
    if (!dsm) encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCoRRE);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRRE);

    if (!dsm && appData.compressLevel >= 0 && appData.compressLevel <= 9) {
	encs[se->nEncodings++] = Swap32IfLE(appData.compressLevel + rfbEncodingCompressLevel0);
    } else {
	/*
         * OUT OF DATE: If -tunnel option was provided, we assume that server machine is
	 * not in the local network so we use default compression level for
	 * tight encoding instead of fast compression. Thus we are
	 * requesting level 1 compression only if tunneling is not used.
         */
	appData.compressLevel = guess_compresslevel();
	if (clmsg++ == 0) fprintf(stderr, "guessed: -compresslevel %d\n", appData.compressLevel);
	encs[se->nEncodings++] = Swap32IfLE(appData.compressLevel + rfbEncodingCompressLevel0);
    }

    if (!dsm && appData.enableJPEG) {
	if (appData.qualityLevel < 0 || appData.qualityLevel > tQLmax) {
		appData.qualityLevel = guess_qualitylevel();
		if (qlmsg++ == 0) fprintf(stderr, "guessed: -qualitylevel  %d\n", appData.qualityLevel);
	}

#ifdef TURBOVNC
    requestSubsampLevel = True;
    tQL = appData.qualityLevel / 10;
    if (tQL < 0) tQL = 1;
    if (tQL > 9) tQL = 9;
    encs[se->nEncodings++] = Swap32IfLE(tQL + rfbEncodingQualityLevel0);
    encs[se->nEncodings++] = Swap32IfLE(appData.qualityLevel + rfbJpegQualityLevel1 - 1);
    if (se->nEncodings < MAX_ENCODINGS && requestSubsampLevel) {
	if (appData.subsampLevel < 0 || appData.subsampLevel > TVNC_SAMPOPT - 1) {
		appData.subsampLevel = TVNC_1X;
	}
	encs[se->nEncodings++] = Swap32IfLE(appData.subsampLevel + rfbJpegSubsamp1X);
    }
#else
    encs[se->nEncodings++] = Swap32IfLE(appData.qualityLevel + rfbEncodingQualityLevel0);
#endif

    }

    if (appData.useRemoteCursor) {
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingXCursor);
      if (!appData.useX11Cursor) {
      	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRichCursor);
      }
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingPointerPos);
    }

    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingNewFBSize);
  }

  len = sz_rfbSetEncodingsMsg + se->nEncodings * 4;

	if (!appData.ultraDSM) {
		se->nEncodings = Swap16IfLE(se->nEncodings);

		if (!WriteExact(rfbsock, buf, len)) return False;
	} else {
		/* for UltraVNC encryption DSM we have to send each encoding separately (why?) */
		int i, errs = 0, nenc = se->nEncodings;
		
		se->nEncodings = Swap16IfLE(se->nEncodings);

		currentMsg = rfbSetEncodings;
		if (!WriteExact(rfbsock, buf, sz_rfbSetEncodingsMsg)) errs++;
		for (i=0; i < nenc; i++) {
			if (!WriteExact(rfbsock, (char *)&encs[i], sizeof(CARD32))) errs++;
		}
		if (errs) return False;
	}

  return True;
}


/*
 * SendIncrementalFramebufferUpdateRequest.
 */

Bool
SendIncrementalFramebufferUpdateRequest()
{
	return SendFramebufferUpdateRequest(0, 0, si.framebufferWidth,
	    si.framebufferHeight, True);
}

time_t last_filexfer = 0;
int delay_filexfer = 3;
extern void CheckFileXfer(void);
extern int rfbsock_is_ready(void);


static int dyn = -1;
extern int filexfer_sock;
extern int filexfer_listen;

/*
 * SendFramebufferUpdateRequest.
 */
Bool
SendFramebufferUpdateRequest(int x, int y, int w, int h, Bool incremental)
{
	rfbFramebufferUpdateRequestMsg fur;
	static int db = -1;

	if (db < 0) {
		if (getenv("SSVNC_DEBUG_RECTS")) {
			db = atoi(getenv("SSVNC_DEBUG_RECTS"));
		} else {
			db = 0;
		}
	}

	if (db) fprintf(stderr, "SendFramebufferUpdateRequest(%d, %d, %d, %d, incremental=%d)\n", x, y, w, h, (int) incremental);

	if (dyn < 0) {
		struct stat sb;
		if (getenv("USER") && !strcmp(getenv("USER"), "runge")) {
			if (stat("/tmp/nodyn", &sb) == 0) {
				putenv("NOFTFBUPDATES=1");
				unlink("/tmp/nodyn");
			}
		}
		if (getenv("NOFTFBUPDATES")) {
			dyn = 0;
		} else {
			dyn = 1;
		}
	}

	if (appData.fileActive && filexfer_sock >= 0) {
		static int first = 1;
		if (first) {
			fprintf(stderr, "SFU: dynamic fb updates during filexfer: %d\n", dyn);
			first = 0;
		}
if (db > 2 || 0) fprintf(stderr, "A sfur: %d %d %d %d d_last: %d\n", x, y, w, h, (int) (time(NULL) - last_filexfer));
		if (!dyn || time(NULL) < last_filexfer + delay_filexfer) {
			return True;
		}
	}
if (db > 1) fprintf(stderr, "B sfur: %d %d %d %d\n", x, y, w, h);

	fur.type = rfbFramebufferUpdateRequest;
	fur.incremental = incremental ? 1 : 0;
	fur.x = Swap16IfLE(x);
	fur.y = Swap16IfLE(y);
	fur.w = Swap16IfLE(w);
	fur.h = Swap16IfLE(h);

	if (incremental) {
		sent_FBU = 1;
	} else {
		sent_FBU = 2;
	}

	currentMsg = rfbFramebufferUpdateRequest;
	if (!WriteExact(rfbsock, (char *)&fur, sz_rfbFramebufferUpdateRequestMsg)) {
		return False;
	}

	return True;
}


/*
 * SendPointerEvent.
 */

Bool
SendPointerEvent(int x, int y, int buttonMask)
{
	rfbPointerEventMsg pe;

	if (appData.fileActive) {
		if (!dyn || time(NULL) < last_filexfer + delay_filexfer) {
#if 0
			fprintf(stderr, "skip SendPointerEvent: %d - %d\n", last_filexfer, time(NULL));
#endif
			return True;
		}
	}

	pe.type = rfbPointerEvent;
	pe.buttonMask = buttonMask;

	if (scale_factor_x > 0.0 && scale_factor_x != 1.0) {
		x /= scale_factor_x;
	}
	if (scale_factor_y > 0.0 && scale_factor_y != 1.0) {
		y /= scale_factor_y;
	}

	if (x < 0) x = 0;
	if (y < 0) y = 0;

	if (!appData.useX11Cursor) {
		SoftCursorMove(x, y);
	}

	pe.x = Swap16IfLE(x);
	pe.y = Swap16IfLE(y);
	currentMsg = rfbPointerEvent;
	return WriteExact(rfbsock, (char *)&pe, sz_rfbPointerEventMsg);
}


/*
 * SendKeyEvent.
 */

Bool
SendKeyEvent(CARD32 key, Bool down)
{
	rfbKeyEventMsg ke;

	if (appData.fileActive) {
		if (!dyn || time(NULL) < last_filexfer + delay_filexfer) {
#if 0
			fprintf(stderr, "skip SendPointerEvent: %d - %d\n", last_filexfer, time(NULL));
#endif
			return True;
		}
	}

	ke.type = rfbKeyEvent;
	ke.down = down ? 1 : 0;
	ke.key = Swap32IfLE(key);
	currentMsg = rfbKeyEvent;
	return WriteExact(rfbsock, (char *)&ke, sz_rfbKeyEventMsg);
}


/*
 * SendClientCutText.
 */

Bool
SendClientCutText(char *str, int len)
{
	rfbClientCutTextMsg cct;

	if (serverCutText) {
		free(serverCutText);
	}
	serverCutText = NULL;

	if (appData.fileActive) {
		if (!dyn || time(NULL) < last_filexfer + delay_filexfer) {
			/* ultravnc java viewer lets this one through. */
			return True;
		}
	}

	if (appData.viewOnly) {
		return True;
	}

	cct.type = rfbClientCutText;
	cct.length = Swap32IfLE((unsigned int) len);
	currentMsg = rfbClientCutText;
	return  (WriteExact(rfbsock, (char *)&cct, sz_rfbClientCutTextMsg) &&
	    WriteExact(rfbsock, str, len));
}

static int ultra_scale = 0;

Bool
SendServerScale(int nfac)
{
	rfbSetScaleMsg ssc;
	if (nfac < 0 || nfac > 100) {
		return True;
	}

	ultra_scale = nfac;
	ssc.type = rfbSetScale;
	ssc.scale = nfac;
	currentMsg = rfbSetScale;
	return WriteExact(rfbsock, (char *)&ssc, sz_rfbSetScaleMsg);
}

Bool
SendServerInput(Bool enabled)
{
	rfbSetServerInputMsg sim;

	sim.type = rfbSetServerInput;
	sim.status = enabled;
	currentMsg = rfbSetServerInput;
	return WriteExact(rfbsock, (char *)&sim, sz_rfbSetServerInputMsg);
}

Bool
SendSingleWindow(int x, int y)
{
	static int w_old = -1, h_old = -1;
	rfbSetSWMsg sw;

	fprintf(stderr, "SendSingleWindow: %d %d\n", x, y);

	if (x == -1 && y == -1)  {
		sw.type = rfbSetSW;
		sw.x = Swap16IfLE(1);
		sw.y = Swap16IfLE(1);
		if (w_old > 0) {
			si.framebufferWidth  = w_old;
			si.framebufferHeight = h_old;
			ReDoDesktop();
		}
		w_old = h_old = -1;
	} else {
		sw.type = rfbSetSW;
		sw.x = Swap16IfLE(x);
		sw.y = Swap16IfLE(y);
		w_old = si.framebufferWidth;
		h_old = si.framebufferHeight;
		
	}
	sw.status = True;
	currentMsg = rfbSetSW;
	return WriteExact(rfbsock, (char *)&sw, sz_rfbSetSWMsg);
}

Bool
SendTextChat(char *str)
{
	static int db = -1;
	rfbTextChatMsg chat;

	if (db < 0) {
		if (getenv("SSVNC_DEBUG_CHAT")) {
			db = 1;
		} else {
			db = 0;
		}
	}
	if (!appData.chatActive) {
		SendTextChatOpen();
		appData.chatActive = True;
	}

	chat.type = rfbTextChat;
	chat.pad1 = 0;
	chat.pad2 = 0;
	chat.length = (unsigned int) strlen(str);
	if (db) fprintf(stderr, "SendTextChat: %d '%s'\n", (int) chat.length, str);
	chat.length = Swap32IfLE(chat.length);
	if (!WriteExact(rfbsock, (char *)&chat, sz_rfbTextChatMsg)) {
		return False;
	}
	currentMsg = rfbTextChat;
	return WriteExact(rfbsock, str, strlen(str));
}

extern void raiseme(int force);

Bool
SendTextChatOpen(void)
{
	rfbTextChatMsg chat;

	raiseme(0);
	chat.type = rfbTextChat;
	chat.pad1 = 0;
	chat.pad2 = 0;
	chat.length = Swap32IfLE(rfbTextChatOpen);
	return WriteExact(rfbsock, (char *)&chat, sz_rfbTextChatMsg);
}

Bool
SendTextChatClose(void)
{
	rfbTextChatMsg chat;
	chat.type = rfbTextChat;
	chat.pad1 = 0;
	chat.pad2 = 0;
	chat.length = Swap32IfLE(rfbTextChatClose);
	appData.chatActive = False;
	return WriteExact(rfbsock, (char *)&chat, sz_rfbTextChatMsg);
}

Bool
SendTextChatFinished(void)
{
	rfbTextChatMsg chat;
	chat.type = rfbTextChat;
	chat.pad1 = 0;
	chat.pad2 = 0;
	chat.length = Swap32IfLE(rfbTextChatFinished);
	appData.chatActive = False;
	return WriteExact(rfbsock, (char *)&chat, sz_rfbTextChatMsg);
}

extern int do_format_change;
extern int do_cursor_change;
extern double do_fb_update;
extern void cutover_format_change(void);

double dtime(double *t_old) {
        /* 
         * usage: call with 0.0 to initialize, subsequent calls give
         * the time difference since last call.
         */
        double t_now, dt;
        struct timeval now;

        gettimeofday(&now, NULL);
        t_now = now.tv_sec + ( (double) now.tv_usec/1000000. );
        if (*t_old == 0.0) {
                *t_old = t_now;
                return t_now;
        }
        dt = t_now - *t_old;
        *t_old = t_now;
        return(dt);
}

/* common dtime() activities: */
double dtime0(double *t_old) {
        *t_old = 0.0;
        return dtime(t_old);
}

double dnow(void) {
        double t;
        return dtime0(&t);
}

static char fxfer[65536];

Bool HandleFileXfer(void) {
	unsigned char hdr[12];
	unsigned int len;

        int rfbDirContentRequest = 1;
        int rfbDirPacket = 2; /* Full directory name or full file name. */
        int rfbFileTransferRequest = 3;
        int rfbFileHeader = 4;
        int rfbFilePacket = 5; /* One slice of the file */
        int rfbEndOfFile = 6;
        int rfbAbortFileTransfer = 7;
        int rfbFileTransferOffer = 8;
        int rfbFileAcceptHeader = 9; /* The server accepts or rejects the file */
        int rfbCommand = 10;
        int rfbCommandReturn = 11;
        int rfbFileChecksums = 12;

        int rfbRDirContent = 1; /* Request a Server Directory contents */
        int rfbRDrivesList = 2; /* Request the server's drives list */

        int rfbADirectory = 1; /* Reception of a directory name */
        int rfbAFile = 2; /* Reception of a file name  */
        int rfbADrivesList = 3; /* Reception of a list of drives */
        int rfbADirCreate = 4; /* Response to a create dir command  */
        int rfbADirDelete = 5; /* Response to a delete dir command  */
        int rfbAFileCreate = 6; /* Response to a create file command  */
        int rfbAFileDelete = 7; /* Response to a delete file command */

        int rfbCDirCreate = 1; /* Request the server to create the given directory */
        int rfbCDirDelete = 2; /* Request the server to delete the given directory */
        int rfbCFileCreate = 3; /* Request the server to create the given file */
        int rfbCFileDelete = 4; /* Request the server to delete the given file */

        int rfbRErrorUnknownCmd = 1; /* Unknown FileTransfer command. */
#define rfbRErrorCmd 0xFFFFFFFF

	static int db = -1;
	static int guess_x11vnc = 0;

#if 0
	if (filexfer_sock < 0) {
		return True;
	}
	/* instead, we read and discard the ft msg data. */
#endif

/*fprintf(stderr, "In  HandleFileXfer\n"); */

	if (db < 0) {
		if (getenv("DEBUG_HandleFileXfer")) {
			db = 1;
		} else {
			db = 0;
		}
	}

	last_filexfer = time(NULL);
	/*fprintf(stderr, "last_filexfer-1: %d\n", last_filexfer); */

	/* load first byte to send to Java be the FT msg number: */
	hdr[0] = rfbFileTransfer;

	/* this is to avoid XtAppProcessEvent() calls induce by our ReadFromRFBServer calls below: */
	skip_XtUpdateAll = 1;
	if (!ReadFromRFBServer(&hdr[1], 11)) {
		skip_XtUpdateAll = 0;
		return False;
	}
	if (filexfer_sock >= 0) {
		write(filexfer_sock, hdr, 12);
	} else {
		fprintf(stderr, "filexfer_sock closed, discarding 12 bytes\n");
	}
	if (db) fprintf(stderr, "\n");
	if (db) fprintf(stderr, "Got rfbFileTransfer hdr\n");
	if (db > 1) write(2, hdr, 12);

	if (db) {
		int i;
		fprintf(stderr, "HFX HDR:");
		for (i=0; i < 12; i++) {
			fprintf(stderr, " %d", (int) hdr[i]);
		}
		fprintf(stderr, "\n");
	}

	if (hdr[1] == rfbEndOfFile) {
		goto read_no_more;
	} else if (hdr[1] == rfbAbortFileTransfer) {
		goto read_no_more;
	}

	if (hdr[1] == rfbDirPacket && hdr[3] == rfbADirectory) {
		
	}

	len = (hdr[8] << 24) | (hdr[9] << 16) | (hdr[10] << 8) | hdr[11];
	if (db) fprintf(stderr, "Got rfbFileTransfer: len1 %u\n", len);
	if (len > 0) {
		if (!ReadFromRFBServer(fxfer, len)) {
			skip_XtUpdateAll = 0;
			return False;
		}
		if (db > 1) write(2, fxfer, len);
		if (len >= 12 && hdr[1] == rfbDirPacket) {
			/* try to guess if x11vnc or not... */
			if (db) {
				int i;
				fprintf(stderr, "HFX DIR PKT (attr, timeL, timeH):");
				for (i=0; i < 12; i++) {
					fprintf(stderr, " %d", (unsigned char) fxfer[i]);
				}
				fprintf(stderr, "\n");
			}
			if (hdr[2] == 1) {
				int dattr  = (unsigned char) fxfer[0];
				int timeL1 = (unsigned char) fxfer[4];
				int timeL2 = (unsigned char) fxfer[5];
				int timeL3 = (unsigned char) fxfer[6];
				int timeL4 = (unsigned char) fxfer[7];
				int timeH1 = (unsigned char) fxfer[8];
				int timeH2 = (unsigned char) fxfer[9];
				int timeH3 = (unsigned char) fxfer[10];
				int timeH4 = (unsigned char) fxfer[11];
				if (dattr != 0) {
					if (timeH1 == 0 && timeH2 == 0 && timeH3 == 0 && timeH4 == 0) {
						if ((timeL1 != 0 || timeL2 != 0) && timeL3 != 0 && timeL4 != 0) {
							if (!guess_x11vnc) fprintf(stderr, "guessed x11vnc server\n");
							guess_x11vnc = 1;
						}
					}
				}
			}
		}
		if (db && 0) fprintf(stderr, "\n");
		if (filexfer_sock >= 0) {
			write(filexfer_sock, fxfer, len);
		} else {
			fprintf(stderr, "filexfer_sock closed, discarding %d bytes\n", len);
		}
	}

	len = (hdr[4] << 24) | (hdr[5] << 16) | (hdr[6] << 8) | hdr[7];
	if (db) fprintf(stderr, "Got rfbFileTransfer: len2 %u\n", len);

#if 0
	if (hdr[1] == rfbFileHeader && len != rfbRErrorCmd)
#else
	/* the extra 4 bytes get send on rfbRErrorCmd as well. */
	if (hdr[1] == rfbFileHeader) {
#endif
		int is_err = 0;
		if (len == rfbRErrorCmd) {
			is_err = 1;
		}
		if (db) fprintf(stderr, "Got rfbFileTransfer: rfbFileHeader\n");
		if (is_err && guess_x11vnc) {
			fprintf(stderr, "rfbRErrorCmd x11vnc skip read 4 bytes.\n");
			goto read_no_more;
		}
		len = 4;
		if (!ReadFromRFBServer(fxfer, len)) {
			skip_XtUpdateAll = 0;
			return False;
		}
		if (db > 1) write(2, fxfer, len);
		if (db && 0) fprintf(stderr, "\n");
		if (is_err) {
			fprintf(stderr, "rfbRErrorCmd skip write 4 bytes.\n");
			goto read_no_more;
		}
		if (filexfer_sock >= 0) {
			write(filexfer_sock, fxfer, len);
		} else {
			fprintf(stderr, "filexfer_sock closed, discarding %d bytes\n", len);
		}
	}

	read_no_more:

	if (filexfer_sock < 0) {
		int stop = 0;
		static time_t last_stop = 0;
#if 0
		/* this isn't working */
		if (hdr[1] == rfbFilePacket || hdr[1] == rfbFileHeader) {
			fprintf(stderr, "filexfer_sock closed, trying to abort receive\n");
			stop = 1;
		}
#endif
		if (stop && time(NULL) > last_stop+1) {
			unsigned char rpl[12];
			int k;
			rpl[0] = rfbFileTransfer;
			rpl[1] = rfbAbortFileTransfer;
			for (k=2; k < 12; k++) {
				rpl[k] = 0;
			}
			WriteExact(rfbsock, rpl, 12);
			last_stop = time(NULL);
		}
	}

	if (db) fprintf(stderr, "Got rfbFileTransfer done.\n");
	skip_XtUpdateAll = 0;

	if (db) fprintf(stderr, "CFX: B\n");
	CheckFileXfer();
/*fprintf(stderr, "Out HandleFileXfer\n"); */
	return True;
}

/*
 * HandleRFBServerMessage.
 */


Bool
HandleRFBServerMessage()
{
	static int db = -1;
	rfbServerToClientMsg msg;

	if (db < 0) {
		if (getenv("DEBUG_RFB_SMSG")) {
			db = 1;
		} else {
			db = 0;
		}
	}

	if (!ReadFromRFBServer((char *)&msg, 1)) {
		return False;
	}
	if (appData.ultraDSM) {
		if (!ReadFromRFBServer((char *)&msg, 1)) {
			return False;
		}
	}

/*fprintf(stderr, "msg.type: %d\n", msg.type); */

	if (msg.type == rfbFileTransfer) {
		return HandleFileXfer();
	}

    switch (msg.type) {

    case rfbSetColourMapEntries:
    {
	int i;
	CARD16 rgb[3];
	XColor xc;

	if (!ReadFromRFBServer(((char *)&msg) + 1, sz_rfbSetColourMapEntriesMsg - 1)) {
		return False;
	}

	msg.scme.firstColour = Swap16IfLE(msg.scme.firstColour);
	msg.scme.nColours = Swap16IfLE(msg.scme.nColours);

	for (i = 0; i < msg.scme.nColours; i++) {
		if (!ReadFromRFBServer((char *)rgb, 6)) {
			return False;
		}
		xc.pixel = msg.scme.firstColour + i;
		xc.red = Swap16IfLE(rgb[0]);
		xc.green = Swap16IfLE(rgb[1]);
		xc.blue = Swap16IfLE(rgb[2]);
		if (appData.useGreyScale) {
			int ave = (xc.red + xc.green + xc.blue)/3;
			xc.red   = ave;
			xc.green = ave;
			xc.blue  = ave;
		}
		xc.flags = DoRed|DoGreen|DoBlue;
		XStoreColor(dpy, cmap, &xc);
	}

	break;
    }

    case rfbFramebufferUpdate:
    {
	rfbFramebufferUpdateRectHeader rect;
	int linesToRead;
	int bytesPerLine;
	int i;

	int area_copyrect = 0;
	int area_tight = 0;
	int area_zrle = 0;
	int area_raw = 0;
	static int rdb = -1;
	static int delay_sync = -1;
	static int delay_sync_env = -1;
	int try_delay_sync = 0;
	int cnt_pseudo = 0;
	int cnt_image  = 0;

	int skip_incFBU = 0;

	if (db) fprintf(stderr, "FBU-0:    %.6f\n", dnow());
	if (rdb < 0) {
		if (getenv("SSVNC_DEBUG_RECTS")) {
			rdb = atoi(getenv("SSVNC_DEBUG_RECTS"));
		} else {
			rdb = 0;
		}
	}
	if (delay_sync < 0) {
		if (getenv("SSVNC_DELAY_SYNC")) {
			delay_sync = atoi(getenv("SSVNC_DELAY_SYNC"));
			delay_sync_env = delay_sync;
		} else {
			delay_sync = 0;
		}
	}

	sent_FBU = -1;

	if (appData.pipelineUpdates) {
		/* turbovnc speed idea */
		XEvent ev;
		memset(&ev, 0, sizeof(ev));
		ev.xclient.type = ClientMessage;
		ev.xclient.window = XtWindow(desktop);
		ev.xclient.message_type = XA_INTEGER;
		ev.xclient.format = 8;
		strcpy(ev.xclient.data.b, "SendRFBUpdate");
		XSendEvent(dpy, XtWindow(desktop), False, 0, &ev);
	}

	if (!ReadFromRFBServer(((char *)&msg.fu) + 1, sz_rfbFramebufferUpdateMsg - 1)) {
		return False;
	}

	msg.fu.nRects = Swap16IfLE(msg.fu.nRects);

	if (rdb) fprintf(stderr, "Begin rect loop %d\n", msg.fu.nRects);

	if (delay_sync) {
		try_delay_sync = 1;
	} else {
		if (delay_sync_env != -1 && delay_sync_env == 0) {
			;
		} else if (appData.yCrop > 0) {
			;
		} else if (scale_factor_x > 0.0 && scale_factor_x != 1.0) {
			;
		} else if (scale_factor_y > 0.0 && scale_factor_y != 1.0) {
			;
		} else {
			static int msg = 0;
			/* fullScreen? */
			/* useXserverBackingStore? */
			/* useX11Cursor & etc? */
			/* scrollbars? */
			if (!msg) {
				fprintf(stderr, "enabling 'delay_sync' mode for faster local drawing,\ndisable via env SSVNC_DELAY_SYNC=0 if there are painting errors.\n"); 
				msg = 1;
			}
			try_delay_sync = 1;
		}
	}
	if (try_delay_sync) {
		skip_maybe_sync = 1;
	}
#define STOP_DELAY_SYNC \
	if (try_delay_sync) { \
		if (cnt_image && skip_maybe_sync) { \
			XSync(dpy, False); \
		} \
		try_delay_sync = 0; \
		skip_maybe_sync = 0; \
	}

	for (i = 0; i < msg.fu.nRects; i++) {
		if (!ReadFromRFBServer((char *)&rect, sz_rfbFramebufferUpdateRectHeader)) {
			return False;
		}

		rect.encoding = Swap32IfLE(rect.encoding);
		if (rect.encoding == rfbEncodingLastRect) {
			break;
		}

		rect.r.x = Swap16IfLE(rect.r.x);
		rect.r.y = Swap16IfLE(rect.r.y);
		rect.r.w = Swap16IfLE(rect.r.w);
		rect.r.h = Swap16IfLE(rect.r.h);

		if (rdb > 1) fprintf(stderr, "nRects: %d  i=%d enc: %d   %dx%d+%d+%d\n", msg.fu.nRects, i, (int) rect.encoding, rect.r.w, rect.r.h, rect.r.x, rect.r.y); 
			
		if (rect.encoding == rfbEncodingXCursor || rect.encoding == rfbEncodingRichCursor) {
			cnt_pseudo++;
			STOP_DELAY_SYNC	
			
			if (db) fprintf(stderr, "FBU-Cur1  %.6f\n", dnow());
			if (!HandleCursorShape(rect.r.x, rect.r.y, rect.r.w, rect.r.h, rect.encoding)) {
				return False;
			}
			if (db) fprintf(stderr, "FBU-Cur2  %.6f\n", dnow());
			continue;
		}

		if (rect.encoding == rfbEncodingPointerPos) {
			cnt_pseudo++;
			STOP_DELAY_SYNC	
			if (db) fprintf(stderr, "FBU-Pos1  %.6f\n", dnow());
			if (0) fprintf(stderr, "CursorPos: %d %d / %d %d\n", rect.r.x, rect.r.y, rect.r.w, rect.r.h);
			if (ultra_scale > 0) {
				int f = ultra_scale;
				if (!HandleCursorPos(rect.r.x/f, rect.r.y/f)) {
					return False;
				}
			} else {
				if (!HandleCursorPos(rect.r.x, rect.r.y)) {
					return False;
				}
			}
			if (db) fprintf(stderr, "FBU-Pos2  %.6f\n", dnow());
			continue;
		}
		if (rect.encoding == rfbEncodingNewFBSize) {
			cnt_pseudo++;
			STOP_DELAY_SYNC	
			if (appData.chatOnly) {
				continue;
			}
			fprintf(stderr,"New Size: %dx%d at (%d, %d)\n", rect.r.w, rect.r.h, rect.r.x, rect.r.y);
			si.framebufferWidth = rect.r.w;
			si.framebufferHeight = rect.r.h;
			/*fprintf(stderr, "si: %d %d\n", si.framebufferWidth, si.framebufferHeight); */
			ReDoDesktop();
			continue;
		}
		if (rdb) fprintf(stderr,"Rect: %dx%d at (%d, %d)\n", rect.r.w, rect.r.h, rect.r.x, rect.r.y);
		cnt_image++;

		if (appData.ultraDSM) {
			/*
			 * What a huge mess the UltraVNC DSM plugin is!!!
			 * We read and ignore their little "this much data" hint...
			 */
			switch (rect.encoding)
			{
			case rfbEncodingRaw:
			case rfbEncodingRRE:
			case rfbEncodingCoRRE:
			case rfbEncodingHextile:
			/*case rfbEncodingUltra: */
/*			case rfbEncodingZlib: */
			/*case rfbEncodingXOR_Zlib: */
			/*case rfbEncodingXORMultiColor_Zlib: */
			/*case rfbEncodingXORMonoColor_Zlib: */
			/*case rfbEncodingSolidColor: */
			case rfbEncodingTight:
			case rfbEncodingZlibHex:
			case rfbEncodingZRLE:
			case rfbEncodingZYWRLE:
			    {
				CARD32 discard;
				ReadFromRFBServer((char *)&discard, sizeof(CARD32));
			    }
			    break;
			}
		}

		if ((rect.r.x + rect.r.w > si.framebufferWidth) ||
		    (rect.r.y + rect.r.h > si.framebufferHeight)) {
			if (!appData.chatOnly) {
				fprintf(stderr,"Rect too large: %dx%d at (%d, %d) encoding=%d\n",
				rect.r.w, rect.r.h, rect.r.x, rect.r.y, (int) rect.encoding);
				return False;
			}
		}

		if (rect.r.h * rect.r.w == 0) {
			fprintf(stderr,"*** Warning *** Zero size rect: %dx%d+%d+%d  encoding=%d\n",
			    rect.r.w, rect.r.h, rect.r.x, rect.r.y, (int) rect.encoding);
			if (0) continue;
		}

		/* If RichCursor encoding is used, we should prevent collisions
		   between framebuffer updates and cursor drawing operations. */
		if (db) fprintf(stderr, "FBU-SCL1  %.6f\n", dnow());

		SoftCursorLockArea(rect.r.x, rect.r.y, rect.r.w, rect.r.h);

		if (db) fprintf(stderr, "FBU-SCL2  %.6f\n", dnow());


		switch (rect.encoding) {

		case rfbEncodingRaw:

			bytesPerLine = rect.r.w * myFormat.bitsPerPixel / 8;
			linesToRead = BUFFER_SIZE / bytesPerLine;

			if (db) fprintf(stderr, "Raw:     %dx%d+%d+%d\n", rect.r.w, rect.r.h, rect.r.x, rect.r.y);
			area_raw += rect.r.w * rect.r.h;

			while (rect.r.h > 0) {
				if (linesToRead > rect.r.h) {
					linesToRead = rect.r.h;
				}

				if (!ReadFromRFBServer(buffer,bytesPerLine * linesToRead)) {
					return False;
				}

				CopyDataToScreen(buffer, rect.r.x, rect.r.y, rect.r.w, linesToRead);

				rect.r.h -= linesToRead;
				rect.r.y += linesToRead;
			}
			break;

		case rfbEncodingCopyRect:
		{
			rfbCopyRect cr;

			STOP_DELAY_SYNC	
			XSync(dpy, False);

			if (!ReadFromRFBServer((char *)&cr, sz_rfbCopyRect)) {
				return False;
			}
			if (appData.chatOnly) {
				break;
			}

			cr.srcX = Swap16IfLE(cr.srcX);
			cr.srcY = Swap16IfLE(cr.srcY);

			if (db) fprintf(stderr, "Copy:    %dx%d+%d+%d\n", rect.r.w, rect.r.h, rect.r.x, rect.r.y);
			area_copyrect += rect.r.w * rect.r.h;

			/* If RichCursor encoding is used, we should extend our
			   "cursor lock area" (previously set to destination
			   rectangle) to the source rectangle as well. */

			if (db) fprintf(stderr, "FBU-SCL3  %.6f\n", dnow());

			SoftCursorLockArea(cr.srcX, cr.srcY, rect.r.w, rect.r.h);

			if (db) fprintf(stderr, "FBU-SCL4  %.6f\n", dnow());

			if (appData.copyRectDelay != 0) {
				XFillRectangle(dpy, desktopWin, srcGC, cr.srcX, cr.srcY, rect.r.w, rect.r.h);
				XFillRectangle(dpy, desktopWin, dstGC, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
				XSync(dpy,False);
				usleep(appData.copyRectDelay * 1000);
				XFillRectangle(dpy, desktopWin, dstGC, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
				XFillRectangle(dpy, desktopWin, srcGC, cr.srcX, cr.srcY, rect.r.w, rect.r.h);
			}

			if (db) fprintf(stderr, "FBU-CPA1  %.6f\n", dnow());
			if (!appData.useXserverBackingStore) {
				copy_rect(rect.r.x, rect.r.y, rect.r.w, rect.r.h, cr.srcX, cr.srcY);
				put_image(rect.r.x, rect.r.y, rect.r.x, rect.r.y, rect.r.w, rect.r.h, 0);
				XSync(dpy, False);
			} else {
				XCopyArea(dpy, desktopWin, desktopWin, gc, cr.srcX, cr.srcY,
				    rect.r.w, rect.r.h, rect.r.x, rect.r.y);
			}
			if (db) fprintf(stderr, "FBU-CPA2  %.6f\n", dnow());

			break;
		}

		case rfbEncodingRRE:
		{
			switch (myFormat.bitsPerPixel) {
			case 8:
				if (!HandleRRE8(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 16:
				if (!HandleRRE16(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 32:
				if (!HandleRRE32(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			}
			break;
		}

		case rfbEncodingCoRRE:
		{
			switch (myFormat.bitsPerPixel) {
			case 8:
				if (!HandleCoRRE8(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 16:
				if (!HandleCoRRE16(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 32:
				if (!HandleCoRRE32(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			}
			break;
		}

		case rfbEncodingHextile:
		{
			switch (myFormat.bitsPerPixel) {
			case 8:
				if (!HandleHextile8(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 16:
				if (!HandleHextile16(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 32:
				if (!HandleHextile32(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			}
			break;
		}

		case rfbEncodingZlib:
		{
			switch (myFormat.bitsPerPixel) {
			case 8:
				if (!HandleZlib8(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 16:
				if (!HandleZlib16(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 32:
				if (!HandleZlib32(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			}
			break;
		}

		case rfbEncodingTight:
		{
			if (db) fprintf(stderr, "Tight:   %dx%d+%d+%d\n", rect.r.w, rect.r.h, rect.r.x, rect.r.y);
			area_tight += rect.r.w * rect.r.h;
			if (db) fprintf(stderr, "FBU-TGH1  %.6f\n", dnow());

			switch (myFormat.bitsPerPixel) {
			case 8:
				if (!HandleTight8(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 16:
				if (!HandleTight16(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 32:
				if (!HandleTight32(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			}
			if (db) fprintf(stderr, "FBU-TGH2  %.6f\n", dnow());
			break;
		}

		/* runge adds zrle and zywrle: */
		case rfbEncodingZRLE:
#if DO_ZYWRLE
		zywrle_level = 0;
		case rfbEncodingZYWRLE:
#endif
		{
			if (db) fprintf(stderr, "ZRLE:    %dx%d+%d+%d\n", rect.r.w, rect.r.h, rect.r.x, rect.r.y);
			area_zrle += rect.r.w * rect.r.h;
			switch (myFormat.bitsPerPixel) {
			case 8:
				if (!HandleZRLE8(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			case 16:
				if (myFormat.greenMax > 0x1f) {
					if (!HandleZRLE16(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
						return False;
					}
				} else {
					if (!HandleZRLE15(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
						return False;
					}
				}
				break;
			case 32:
			    {
				unsigned int maxColor=(myFormat.redMax<<myFormat.redShift)|
				    (myFormat.greenMax<<myFormat.greenShift)|
				    (myFormat.blueMax<<myFormat.blueShift);
				static int ZRLE32 = -1;

				if (ZRLE32 < 0) {
					/* for debugging or workaround e.g. BE display to LE */
					if (getenv("ZRLE32")) {
						if (strstr(getenv("ZRLE32"), "24Up")) {
							ZRLE32 = 3;
						} else if (strstr(getenv("ZRLE32"), "24Down")) {
							ZRLE32 = 2;
						} else {
							ZRLE32 = 1;
						}
					} else {
						ZRLE32 = 0;
					}
				}

if (db) fprintf(stderr, "maxColor: 0x%x  mfbigEnding: %d\n", maxColor, myFormat.bigEndian);

				if (ZRLE32 == 1) {
if (db) fprintf(stderr, "HandleZRLE32\n");
					if (!HandleZRLE32(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
						return False;
					}
				} else if (ZRLE32 == 2) {
if (db) fprintf(stderr, "HandleZRLE24Down\n");
					if (!HandleZRLE24Down(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
						return False;
					}
				} else if (ZRLE32 == 3) {
if (db) fprintf(stderr, "HandleZRLE24Up\n");
					if (!HandleZRLE24Up(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
						return False;
					}
				} else if ((myFormat.bigEndian && (maxColor&0xff)==0) || (!myFormat.bigEndian && (maxColor&0xff000000)==0)) {
if (db) fprintf(stderr, "HandleZRLE24\n");
					if (!HandleZRLE24(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
						return False;
					}
				} else if (!myFormat.bigEndian && (maxColor&0xff)==0) {
if (db) fprintf(stderr, "HandleZRLE24Up\n");
					if (!HandleZRLE24Up(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
						return False;
					}
				} else if (myFormat.bigEndian && (maxColor&0xff000000)==0) {
if (db) fprintf(stderr, "HandleZRLE24Down\n");
					if (!HandleZRLE24Down(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
						return False;
					}
				} else if (!HandleZRLE32(rect.r.x,rect.r.y,rect.r.w,rect.r.h)) {
					return False;
				}
				break;
			    }
			}
			break;
		}

		default:
		fprintf(stderr,"Unknown rect encoding %d\n", (int)rect.encoding);
		return False;
		}

		/* Now we may discard "soft cursor locks". */
		if (db) fprintf(stderr, "FBU-SUL1  %.6f\n", dnow());

		SoftCursorUnlockScreen();

		if (db) fprintf(stderr, "FBU-SUL2  %.6f\n", dnow());
	}

	if (try_delay_sync) {
		skip_maybe_sync = 0;
	}

	if (1 || area_copyrect) {
		/* we always do this now for some reason... */
		if (db) fprintf(stderr, "FBU-XSN1  %.6f\n", dnow());
		XSync(dpy, False);
		if (db) fprintf(stderr, "FBU-XSN2  %.6f\n", dnow());
	}
	sent_FBU = 0;
	/*
	 * we need to be careful since Xt events are processed
	 * usually in the middle of FBU.  So we do any scheduled ones now
	 * which is pretty safe but not absolutely safe.
         */
	if (do_format_change) {
		cutover_format_change();
		do_format_change = 0;
		SetVisualAndCmap();
		SetFormatAndEncodings();
		if (do_cursor_change) {
			if (do_cursor_change == 1) {
				DesktopCursorOff();
			}
			do_cursor_change = 0;
		} else {
			SendFramebufferUpdateRequest(0, 0, si.framebufferWidth,
			    si.framebufferHeight, False);
			skip_incFBU = 1;
		}
	}
	if (do_fb_update != 0.0) {
		if (dnow() > do_fb_update + 1.1) {
			do_fb_update = 0.0;
			SendFramebufferUpdateRequest(0, 0, si.framebufferWidth,
			    si.framebufferHeight, False);
		}
	}

#ifdef MITSHM
    /* if using shared memory PutImage, make sure that the X server has
       updated its framebuffer before we reuse the shared memory.  This is
       mainly to avoid copyrect using invalid screen contents - not sure
       if we'd need it otherwise. */

	if (appData.useShm) {
		XSync(dpy, False);
	} else
#endif
	{
		/* we do it always now. */
		XSync(dpy, False);
	}
	
	if (skip_XtUpdate || skip_incFBU) {
		;
	} else if (appData.pipelineUpdates) {
		;
	} else if (!SendIncrementalFramebufferUpdateRequest()) {
		return False;
	}

	break;
  }

  case rfbBell:
  {
	Window toplevelWin;

	if (appData.useBell) {
		XBell(dpy, 0);
	}

	if (appData.raiseOnBeep) {
		toplevelWin = XtWindow(toplevel);
		XMapRaised(dpy, toplevelWin);
	}

	break;
    }

    case rfbServerCutText:
    {
	if (!ReadFromRFBServer(((char *)&msg) + 1, sz_rfbServerCutTextMsg - 1)) {
		return False;
	}

	msg.sct.length = Swap32IfLE(msg.sct.length);

	if (serverCutText) {
		free(serverCutText);
	}

	serverCutText = malloc(msg.sct.length+1);

	if (!ReadFromRFBServer(serverCutText, msg.sct.length)) {
		return False;
	}

	serverCutText[msg.sct.length] = 0;

	newServerCutText = True;

	break;
    }

    case rfbTextChat:
    {
	char *buffer = NULL;
	if (!ReadFromRFBServer(((char *)&msg) + 1, sz_rfbTextChatMsg - 1)) {
		return False;
	}
	msg.tc.length = Swap32IfLE(msg.tc.length);
	switch(msg.tc.length) {
	case rfbTextChatOpen:
		if (appData.termChat) {
			printChat("\n*ChatOpen*\n\nSend: ", True);
		} else {
			printChat("\n*ChatOpen*\n", True);
		}
		appData.chatActive = True;
		break;
	case rfbTextChatClose:
		printChat("\n*ChatClose*\n", False);
		appData.chatActive = False;
		break;
	case rfbTextChatFinished:
		printChat("\n*ChatFinished*\n", False);
		appData.chatActive = False;
		break;
	default:
		buffer = (char *)malloc(msg.tc.length+1);
		if (!ReadFromRFBServer(buffer, msg.tc.length)) {
			free(buffer);
			return False;
		}
		buffer[msg.tc.length] = '\0';
		appData.chatActive = True;
		GotChatText(buffer, msg.tc.length);
		free(buffer);
	}
	break;
    }

    case rfbResizeFrameBuffer:
    {
	rfbResizeFrameBufferMsg rsmsg;
	if (!ReadFromRFBServer(((char *)&rsmsg) + 1, sz_rfbResizeFrameBufferMsg - 1)) {
		return False;
	}
	si.framebufferWidth  = Swap16IfLE(rsmsg.framebufferWidth);
	si.framebufferHeight = Swap16IfLE(rsmsg.framebufferHeight);
	fprintf(stderr,"UltraVNC ReSize: %dx%d\n", si.framebufferWidth, si.framebufferHeight);
	ReDoDesktop();
	break;
    }

    case rfbRestartConnection:
    {
	rfbRestartConnectionMsg rc;
	int len;
	char *rs_str;
	char buf[5] = "\xff\xff\xff\xff";
	fprintf(stderr, "rfbRestartConnection. type=%d\n", (int) rc.type);
	if (!ReadFromRFBServer((char *)&rc + 1, sz_rfbRestartConnectionMsg - 1)) {
		return False;
	}
	len = Swap32IfLE(rc.length);
	fprintf(stderr, "rfbRestartConnection. pad1=%d\n", (int) rc.pad1);
	fprintf(stderr, "rfbRestartConnection. pad2=%d\n", (int) rc.pad2);
	fprintf(stderr, "rfbRestartConnection. len=%d\n", len);
	if (len) {
		rs_str = (char *)malloc(2*len);	
		if (!ReadFromRFBServer(rs_str, len)) {
			return False;
		}
		restart_session_pw = rs_str;
		restart_session_len = len;
	}
	if (!WriteExact(rfbsock, buf, 4)) {
		return False;
	}
	InitialiseRFBConnection();
	SetVisualAndCmap();
	SetFormatAndEncodings();
	DesktopCursorOff();
	SendFramebufferUpdateRequest(0, 0, si.framebufferWidth, si.framebufferHeight, False);

	break;
    }

    default:
	fprintf(stderr,"Unknown message type %d from VNC server\n",msg.type);
	return False;
    }

	if (appData.fileActive) {
		if (filexfer_sock < 0 && filexfer_listen < 0) {
			appData.fileActive = False;
			SendFramebufferUpdateRequest(0, 0, 1, 1, False);
		} else {
/*fprintf(stderr, "CFX: A\n"); */
			CheckFileXfer();
		}
	}

    return True;
}


#define GET_PIXEL8(pix, ptr) ((pix) = *(ptr)++)

#define GET_PIXEL16(pix, ptr) (((CARD8*)&(pix))[0] = *(ptr)++, \
			       ((CARD8*)&(pix))[1] = *(ptr)++)

#define GET_PIXEL32(pix, ptr) (((CARD8*)&(pix))[0] = *(ptr)++, \
			       ((CARD8*)&(pix))[1] = *(ptr)++, \
			       ((CARD8*)&(pix))[2] = *(ptr)++, \
			       ((CARD8*)&(pix))[3] = *(ptr)++)

/* CONCAT2 concatenates its two arguments.  CONCAT2E does the same but also
   expands its arguments if they are macros */

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)

#define CONCAT3(a,b,c) a##b##c
#define CONCAT3E(a,b,c) CONCAT3(a,b,c)

static unsigned char* frameBuffer = NULL;
static int frameBufferLen = 0;

#ifdef TURBOVNC
#include "turbovnc/turbojpeg.h"
tjhandle tjhnd=NULL;
static char *compressedData = NULL;
static char *uncompressedData = NULL;
#define CopyDataToImage CopyDataToScreen
static void turbovnc_FillRectangle(XGCValues *gcv, int rx, int ry, int rw, int rh) {
	if (!appData.useXserverBackingStore) {
		FillScreen(rx, ry, rw, rh, gcv->foreground);
	} else {
		XChangeGC(dpy, gc, GCForeground, gcv);
		XFillRectangle(dpy, desktopWin, gc, rx, ry, rw, rh);
	}
}
static void CopyImageToScreen(int x, int y, int w, int h) {
	put_image(x, y, x, y, w, h, 0);
}
#endif

#define BPP 8
#include "rre.c"
#include "corre.c"
#include "hextile.c"
#include "zlib.c"

#ifdef TURBOVNC
#undef FillRectangle
#define FillRectangle turbovnc_FillRectangle
#include "turbovnc/tight.c"
#undef FillRectangle
#else
#include "tight.c"
#endif

#include "zrle.c"
#undef BPP

#define BPP 16
#include "rre.c"
#include "corre.c"
#include "hextile.c"
#include "zlib.c"

#ifdef TURBOVNC
#undef FillRectangle
#define FillRectangle turbovnc_FillRectangle
#include "turbovnc/tight.c"
#undef FillRectangle
#else
#include "tight.c"
#endif

#include "zrle.c"
#define REALBPP 15
#include "zrle.c"
#undef BPP

#define BPP 32
#include "rre.c"
#include "corre.c"
#include "hextile.c"
#include "zlib.c"

#ifdef TURBOVNC
#undef FillRectangle
#define FillRectangle turbovnc_FillRectangle
#include "turbovnc/tight.c"
#undef FillRectangle
#else
#include "tight.c"
#endif

#include "zrle.c"
#define REALBPP 24
#include "zrle.c"
#define REALBPP 24
#define UNCOMP 8
#include "zrle.c"
#define REALBPP 24
#define UNCOMP -8
#include "zrle.c"
#undef BPP

/*
 * Read the string describing the reason for a connection failure.
 */

static void
ReadConnFailedReason(void)
{
	CARD32 reasonLen;
	char *reason = NULL;

	if (ReadFromRFBServer((char *)&reasonLen, sizeof(reasonLen))) {
		reasonLen = Swap32IfLE(reasonLen);
		if ((reason = malloc(reasonLen)) != NULL &&
		    ReadFromRFBServer(reason, reasonLen)) {
			int len = (int) reasonLen < sizeof(msgbuf) - 10 ? (int) reasonLen : sizeof(msgbuf) - 10;
			sprintf(msgbuf,"VNC connection failed: %.*s\n", len, reason);
			wmsg(msgbuf, 1);
			free(reason);
			return;
		}
	}

	sprintf(msgbuf, "VNC connection failed\n");
	wmsg(msgbuf, 1);

	if (reason != NULL) {
		free(reason);
	}
}

/*
 * PrintPixelFormat.
 */

void
PrintPixelFormat(format)
    rfbPixelFormat *format;
{
  if (format->bitsPerPixel == 1) {
    fprintf(stderr,"  Single bit per pixel.\n");
    fprintf(stderr,
	    "  %s significant bit in each byte is leftmost on the screen.\n",
	    (format->bigEndian ? "Most" : "Least"));
  } else {
    fprintf(stderr,"  %d bits per pixel.  ",format->bitsPerPixel);
    if (format->bitsPerPixel != 8) {
      fprintf(stderr,"%s significant byte first in each pixel.\n",
	      (format->bigEndian ? "Most" : "Least"));
    }
    if (format->trueColour) {
      fprintf(stderr,"  True colour: max red %d green %d blue %d",
	      format->redMax, format->greenMax, format->blueMax);
      fprintf(stderr,", shift red %d green %d blue %d\n",
	      format->redShift, format->greenShift, format->blueShift);
    } else {
      fprintf(stderr,"  Colour map (not true colour).\n");
    }
  }
}

/*
 * Read an integer value encoded in 1..3 bytes. This function is used
 * by the Tight decoder.
 */

static long
ReadCompactLen (void)
{
  long len;
  CARD8 b;

  if (!ReadFromRFBServer((char *)&b, 1))
    return -1;
  len = (int)b & 0x7F;
  if (b & 0x80) {
    if (!ReadFromRFBServer((char *)&b, 1))
      return -1;
    len |= ((int)b & 0x7F) << 7;
    if (b & 0x80) {
      if (!ReadFromRFBServer((char *)&b, 1))
	return -1;
      len |= ((int)b & 0xFF) << 14;
    }
  }
  return len;
}


/*
 * JPEG source manager functions for JPEG decompression in Tight decoder.
 */

static struct jpeg_source_mgr jpegSrcManager;
static JOCTET *jpegBufferPtr;
static size_t jpegBufferLen;

static void
JpegInitSource(j_decompress_ptr cinfo)
{
  jpegError = False;
}

static boolean
JpegFillInputBuffer(j_decompress_ptr cinfo)
{
  jpegError = True;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;
  jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;

  return TRUE;
}

static void
JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes)
{
  if (num_bytes < 0 || num_bytes > jpegSrcManager.bytes_in_buffer) {
    jpegError = True;
    jpegSrcManager.bytes_in_buffer = jpegBufferLen;
    jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;
  } else {
    jpegSrcManager.next_input_byte += (size_t) num_bytes;
    jpegSrcManager.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
JpegTermSource(j_decompress_ptr cinfo)
{
  /* No work necessary here. */
}

static void
JpegSetSrcManager(j_decompress_ptr cinfo, CARD8 *compressedData,
		  int compressedLen)
{
  jpegBufferPtr = (JOCTET *)compressedData;
  jpegBufferLen = (size_t)compressedLen;

  jpegSrcManager.init_source = JpegInitSource;
  jpegSrcManager.fill_input_buffer = JpegFillInputBuffer;
  jpegSrcManager.skip_input_data = JpegSkipInputData;
  jpegSrcManager.resync_to_restart = jpeg_resync_to_restart;
  jpegSrcManager.term_source = JpegTermSource;
  jpegSrcManager.next_input_byte = jpegBufferPtr;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;

  cinfo->src = &jpegSrcManager;
}
