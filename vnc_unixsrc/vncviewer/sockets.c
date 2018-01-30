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
 * sockets.c - functions to deal with sockets.
 */

#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <vncviewer.h>

#ifndef SOL_IPV6
#ifdef  IPPROTO_IPV6
#define SOL_IPV6 IPPROTO_IPV6
#endif
#endif

/* Solaris (sysv?) needs INADDR_NONE */
#ifndef INADDR_NONE
#define INADDR_NONE ((in_addr_t) 0xffffffff)
#endif

void PrintInHex(char *buf, int len);
extern void printChat(char *, Bool);

Bool errorMessageOnReadFailure = True;

#define BUF_SIZE 8192
static char buf[BUF_SIZE];
static char *bufoutptr = buf;
static int buffered = 0;

/*
 * ReadFromRFBServer is called whenever we want to read some data from the RFB
 * server.  It is non-trivial for two reasons:
 *
 * 1. For efficiency it performs some intelligent buffering, avoiding invoking
 *    the read() system call too often.  For small chunks of data, it simply
 *    copies the data out of an internal buffer.  For large amounts of data it
 *    reads directly into the buffer provided by the caller.
 *
 * 2. Whenever read() would block, it invokes the Xt event dispatching
 *    mechanism to process X events.  In fact, this is the only place these
 *    events are processed, as there is no XtAppMainLoop in the program.
 */

static Bool rfbsockReady = False;
static Bool xfrsockReady = False;
static XtInputId rfbsockId = 0;
static XtInputId xfrsockId = 0;
static int do_rfbsockId = 0;
static int do_xfrsockId = 0;

static void
rfbsockReadyCallback(XtPointer clientData, int *fd, XtInputId *id)
{
	rfbsockReady = True;
#if 0
	XtRemoveInput(*id);
#endif
	XtRemoveInput(rfbsockId);
	if (do_xfrsockId) {
		XtRemoveInput(xfrsockId);
	}
	if (clientData || fd || id) {}
}

static void
xfrsockReadyCallback(XtPointer clientData, int *fd, XtInputId *id)
{
	xfrsockReady = True;
	XtRemoveInput(xfrsockId);
	if (do_rfbsockId) {
		XtRemoveInput(rfbsockId);
	}
	if (clientData || fd || id) {}
}


extern int skip_XtUpdate;
extern int skip_XtUpdateAll;
extern int filexfer_sock, filexfer_listen;
extern time_t start_listen;
extern void CheckTextInput(void);
extern time_t last_filexfer;

static char fxfer[65536];
int fxfer_size = 65536;

int rfbsock_is_ready(void) {
	fd_set fds;
	struct timeval tv;

	if (rfbsock < 0)  {
		return 0;
	}
	FD_ZERO(&fds);
	FD_SET(rfbsock,&fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (select(rfbsock+1, &fds, NULL, NULL, &tv) > 0) {
		if (FD_ISSET(rfbsock, &fds)) {
			return 1;
		}
	}
	return 0;
}

time_t filexfer_start = 0;

void CheckFileXfer() {
	fd_set fds;
	struct timeval tv;
	int i, icnt = 0, igot = 0, bytes0 = 0, bytes = 0, grace = 0, n, list = 0;
	int db = 0;

	if (!appData.fileActive || (filexfer_sock < 0 && filexfer_listen < 0)) {
		return;
	}

	if (filexfer_listen >= 0 && time(NULL) > start_listen + 30) {
		fprintf(stderr, "filexfer closing aging listen socket.\n");
		close(filexfer_listen);
		filexfer_listen = -1;
		return;
	}
if (0) fprintf(stderr, "In  CheckFileXfer\n");

	if (filexfer_listen >=0) {
		n = filexfer_listen;
		list = 1;
	} else {
		n = filexfer_sock;
	}

	while (1) {
		icnt++;
		FD_ZERO(&fds);
		FD_SET(n,&fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		if (select(n+1, &fds, NULL, NULL, &tv) > 0) {
		    if (FD_ISSET(n, &fds)) {
			if (list) {
				if (filexfer_sock >= 0) {
					fprintf(stderr, "filexfer close stale(?) filexfer_sock.\n");
					close(filexfer_sock);
					filexfer_sock = -1;
				}
				filexfer_sock = AcceptTcpConnection(filexfer_listen);
				if (filexfer_sock >= 0) {
					fprintf(stderr, "filexfer accept OK.\n");
					close(filexfer_listen);
					filexfer_listen = -1;
					filexfer_start = last_filexfer = time(NULL);
				} else {
					fprintf(stderr, "filexfer accept failed.\n");
				}
				break;
			} else {
				ssize_t rn;
				unsigned char hdr[12];
				unsigned int len;
				if (db) fprintf(stderr, "try read filexfer...\n");
				if (hdr || len || i) {}
#if 1
				rn = read(n, fxfer, 1*8192);
if (db) {
	int i;
	fprintf(stderr, "CFX HDR:");
	for (i=0; i < 12; i++) {
		fprintf(stderr, " %d", (int) fxfer[i]);
	}
	fprintf(stderr, " ?\n");
}
				if (0 || db) fprintf(stderr, "filexfer read[%d] %d.\n", icnt, rn);
				if (rn < 0) {
					fprintf(stderr, "filexfer bad read: %d\n", errno);
					break;
				} else if (rn == 0) {
					fprintf(stderr, "filexfer gone.\n");
					close(n);
					filexfer_sock = -1;
					last_filexfer = time(NULL);
#if 0
					fprintf(stderr, "last_filexfer-2a: %d\n", last_filexfer);
#endif
					appData.fileActive = False;
					SendFramebufferUpdateRequest(0, 0, 1, 1, False);
					return;
				} else if (rn > 0) {
					if (db > 1) write(2, fxfer, rn);
					if (db) fprintf(stderr, "\n");
					bytes += rn;
					last_filexfer = time(NULL);
#if 0
					fprintf(stderr, "last_filexfer-2b: %d\n", last_filexfer);
#endif

					if (0) {
						/* WE TRY TO FIX THIS IN THE JAVA NOW */
						if (appData.ultraDSM) {
							unsigned char msg = rfbFileTransfer;
							unsigned char hdc = (unsigned char) fxfer[0];
							if (msg == hdc) {
								/* cross your fingers... */
								WriteExact(rfbsock, (char *)&msg, 1);
							}
						}
					}
					if (!WriteExact(rfbsock, fxfer, rn)) {
						return;
					}
					igot = 1;
				}
#else
				/* not working, not always 7 msg type.	*/
				rn = read(n, hdr, 12);
				if (db) fprintf(stderr, "filexfer read %d.\n", rn);
				if (rn == 0) {
					fprintf(stderr, "filexfer gone.\n");
					close(n);
					filexfer_sock = -1;
					last_filexfer = time(NULL);
					return;
				}
				if (rn == 12) {
					len = (hdr[8] << 24) | (hdr[9] << 16) | (hdr[10] << 8) | hdr[11];
					if (db) fprintf(stderr, "n=%d len=%d\n", rn, len);
					if (db > 1) write(2, hdr, rn);
					if (db) fprintf(stderr, "\n");
					WriteExact(rfbsock, hdr, rn);
					if (len > 0) {
						rn = read(len, fxfer, len);
						if (!WriteExact(rfbsock, fxfer, len)) {
							last_filexfer = time(NULL);
							return;
						}
						if (db > 1) write(2, fxfer, len);
					}
					if (db) fprintf(stderr, "\n");
				} else {
					if (db) fprintf(stderr, "bad rn: %d\n", rn);
				}
				igot = 1;
#endif
			}
		    }
		} else {
			if (bytes >= 8192) {
				int ok = 0;
				if (bytes0 == 0) {
					ok = 1;
				} else if (bytes >= bytes0 + 12) {
					ok = 1;
				} else if (grace < 20) {
					ok = 1;
				}
				if (ok) {
					grace++;
					bytes0 = bytes;
#if 0
					fprintf(stderr, "grace: %d\n", grace);
					/* forgot that this is about... */
#endif
					usleep(10 * 1000);
					continue;
				}
			}
			break;
		}
	}
	if (igot) {
		last_filexfer = time(NULL);
#if 0
		fprintf(stderr, "last_filexfer-2c: %d\n", last_filexfer);
#endif
	}
#if 0
fprintf(stderr, "Out CheckFileXfer\n");
#endif
	return;
}

static void check_term_chat(void) {
	fd_set fds;
	struct timeval tv;
	int i, igot = -1, n = fileno(stdin);
	char strs[100][512];
	char buf[rfbTextMaxSize];

	for (i=0; i < 100; i++) {
		FD_ZERO(&fds);
		FD_SET(n,&fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		if (select(n+1, &fds, NULL, NULL, &tv) > 0) {
			if (FD_ISSET(n, &fds)) {
				fgets(strs[i], 512, stdin);
				igot = i;
			} else {
				break;
			}
		} else {
			break;
		}
	}
	buf[0] = '\0';
	for (i=0; i <= igot; i++) {
		if (strlen(buf) + strlen(strs[i]) < rfbTextMaxSize) {
			strcat(buf, strs[i]);
		} else {
			SendTextChat(buf);
			buf[0] = '0';
		}
	}
	if (buf[0] != '\0') {
		SendTextChat(buf);
	}
	if (igot >= 0) printChat("Send: ", False);
}

static time_t time_mark;
extern int delay_filexfer;
#include <sys/stat.h>

extern double start_time;

void ProcessXtEvents()
{
	int db = 0;
	static int dyn = -1;
	static int chat_was_active = 0;
	int check_chat = 0;

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

#if 0
	if (0) fprintf(stderr, "ProcessXtEvents: %d  %.4f\n", skip_XtUpdateAll, dnow() - start_time);
#endif

	if (skip_XtUpdateAll) {
		return;
	}

	/* text chat */
	if (appData.chatActive ) {
		check_chat = 1;
	} else if (chat_was_active) {
		static double last_check = 0.0;
		double now = dnow();
		if (now > last_check + 0.75) {
			check_chat = 1;
			last_check = now;
		}
	}
	if (check_chat) {
		if (appData.chatActive) {
			chat_was_active = 1;
		}
		if (!appData.termChat) {
			CheckTextInput();
		} else {
			check_term_chat();
		}
	}

	if (skip_XtUpdate) {
		return;
	}

	rfbsockReady = False;
	xfrsockReady = False;
	do_rfbsockId = 1;
	rfbsockId = XtAppAddInput(appContext, rfbsock, (XtPointer)XtInputReadMask,
	    rfbsockReadyCallback, NULL);

	do_xfrsockId = 0;
	if (filexfer_sock >= 0) {
		do_xfrsockId = 1;
		xfrsockId = XtAppAddInput(appContext, filexfer_sock, (XtPointer)XtInputReadMask,
		    xfrsockReadyCallback, NULL);
	}

	time_mark = time(NULL);

	if (appData.fileActive) {
		static int first = 1;
		if (first) {
			fprintf(stderr, "PXT: dynamic fb updates during filexfer: %d\n", dyn);
			first = 0;
		}
	}

	if (db) fprintf(stderr, "XtAppAddInput: ");
	while (!rfbsockReady && !xfrsockReady) {
		int w = si.framebufferWidth;
		int h = si.framebufferHeight;
		if (db) fprintf(stderr, ".");
		if (dyn && filexfer_sock >= 0 && time(NULL) > time_mark + delay_filexfer) {
			SendFramebufferUpdateRequest(0, 0, w, h, False);
		}
		XtAppProcessEvent(appContext, XtIMAll);
	}
	if (db) fprintf(stderr, " done. r: %d  x: %d\n", rfbsockReady, xfrsockReady);

	if (xfrsockReady) {
		CheckFileXfer();
	}
}

Bool
ReadFromRFBServer(char *out, unsigned int n)
{
#if 0
	double start = dnow(), dn = n;
#endif
  if (n <= buffered) {
    memcpy(out, bufoutptr, n);
    bufoutptr += n;
    buffered -= n;
#if 0
fprintf(stderr, "R0: %06d\n", (int) dn);
#endif
    return True;
  }

  memcpy(out, bufoutptr, buffered);

  out += buffered;
  n -= buffered;

  bufoutptr = buf;
  buffered = 0;

  if (n <= BUF_SIZE) {

    while (buffered < n) {
      int i = read(rfbsock, buf + buffered, BUF_SIZE - buffered);
      if (i <= 0) {
	if (i < 0) {
	  if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    ProcessXtEvents();
	    i = 0;
	  } else {
	    fputs(programName,stderr);
	    perror(": read");
	    return False;
	  }
	} else {
	  if (errorMessageOnReadFailure) {
	    fprintf(stderr,"%s: VNC server closed connection\n",programName);
	  }
	  return False;
	}
      }
      buffered += i;
    }

    memcpy(out, bufoutptr, n);
    bufoutptr += n;
    buffered -= n;
#if 0
fprintf(stderr, "R1: %06d %06d %10.2f KB/sec\n", (int) dn, buffered+n, 1e-3 * (buffered+n)/(dnow() - start));
#endif
    return True;

  } else {

    while (n > 0) {
      int i = read(rfbsock, out, n);
      if (i <= 0) {
	if (i < 0) {
	  if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    ProcessXtEvents();
	    i = 0;
	  } else {
	    fputs(programName,stderr);
	    perror(": read");
	    return False;
	  }
	} else {
	  if (errorMessageOnReadFailure) {
	    fprintf(stderr,"%s: VNC server closed connection\n",programName);
	  }
	  return False;
	}
      }
      out += i;
      n -= i;
    }

#if 0
fprintf(stderr, "R2: %06d %06d %10.2f KB/sec\n", (int) dn, (int) dn, 1e-3 * (dn)/(dnow() - start));
#endif
    return True;
  }
}


int currentMsg = -1;

/*
 * Write an exact number of bytes, and don't return until you've sent them.
 */

Bool
WriteExact(int sock, char *buf, int n)
{
	fd_set fds;
	int i = 0;
	int j;

	if (appData.ultraDSM && currentMsg >= 0) {
		/* this is for goofy UltraVNC DSM send RFB msg char twice: */
		unsigned char msg = (unsigned char) currentMsg;
		currentMsg = -1;
		if (!WriteExact(sock, (char *)&msg, sizeof(msg))) {
			return False;
		}
	}
	currentMsg = -1;

	while (i < n) {
		j = write(sock, buf + i, (n - i));
		if (j <= 0) {
		    if (j < 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				FD_ZERO(&fds);
				FD_SET(rfbsock,&fds);

				if (select(rfbsock+1, NULL, &fds, NULL, NULL) <= 0) {
					fputs(programName,stderr);
					perror(": select");
					return False;
				}
				j = 0;
			} else {
				fputs(programName,stderr);
				perror(": write");
				return False;
			}
		    } else {
			fprintf(stderr,"%s: write failed\n",programName);
			return False;
		    }
		}
		i += j;
	}
	return True;
}

int
ConnectToUnixSocket(char *file) {
	int sock;
	struct sockaddr_un addr;
	int i;

	memset(&addr, 0, sizeof(struct sockaddr_un));

	addr.sun_family = AF_UNIX;

	for (i=0; i < 108; i++) {
		addr.sun_path[i] = file[i];
		if (file[i] == '\0') {
			break;
		}
	}

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		fputs(programName,stderr);
		perror(": ConnectToUnixSocket: socket");
		return -1;
	}

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fputs(programName,stderr);
		perror(": ConnectToUnixSocket: connect");
		close(sock);
		return -1;
	}

	return sock;
}

char *ipv6_getipaddr(struct sockaddr *paddr, int addrlen) {
#if defined(AF_INET6) && defined(NI_NUMERICHOST)
        char name[200];
	if (appData.noipv6) {
                return strdup("unknown");
        }
        if (getnameinfo(paddr, addrlen, name, sizeof(name), NULL, 0, NI_NUMERICHOST) == 0) {
                return strdup(name);
        }
#endif
	if (paddr || addrlen) {}
        return strdup("unknown");
}

char *ipv6_getnameinfo(struct sockaddr *paddr, int addrlen) {
#if defined(AF_INET6)
        char name[200];
	if (appData.noipv6) {
                return strdup("unknown");
        }
        if (getnameinfo(paddr, addrlen, name, sizeof(name), NULL, 0, 0) == 0) {
                return strdup(name);
        }
#endif
	if (paddr || addrlen) {}
        return strdup("unknown");
}

int dotted_ip(char *host, int partial) {
	int len, dots = 0;
	char *p = host;

	if (!host) {
		return 0;
	}

	if (!isdigit((unsigned char) host[0])) {
		return 0;
	}

	len = strlen(host);
	if (!partial && !isdigit((unsigned char) host[len-1])) {
		return 0;
	}

	while (*p != '\0') {
		if (*p == '.') dots++;
		if (*p == '.' || isdigit((unsigned char) (*p))) {
			p++;
			continue;
		}
		return 0;
	}
	if (!partial && dots != 3) {
		return 0;
	}
	return 1;
}

/*
 * ConnectToTcpAddr connects to the given TCP port.
 */

int ConnectToTcpAddr(const char *hostname, int port) {
	int sock = -1, one = 1;
	unsigned int host;
	struct sockaddr_in addr;

	if (appData.noipv4) {
		fprintf(stderr, "ipv4 is disabled via VNCVIEWER_NO_IPV4/-noipv4.\n");
		goto try6;
	}

	if (!StringToIPAddr(hostname, &host)) {
		fprintf(stderr, "Could not convert '%s' to ipv4 host address.\n", hostname);
		goto try6;
	}

	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = host;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("ConnectToTcpAddr[ipv4]: socket");
		sock = -1;
		goto try6;
	}

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("ConnectToTcpAddr[ipv4]: connect");
		close(sock);
		sock = -1;
		goto try6;
	}

	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)) < 0) {
		perror("ConnectToTcpAddr[ipv4]: setsockopt");
		close(sock);
		sock = -1;
		goto try6;
	}

	if (sock >= 0) {
		return sock;
	}

	try6:

#ifdef AF_INET6
	if (!appData.noipv6) {
		int err;
		struct addrinfo *ai;
		struct addrinfo hints;
		char service[32], *host2, *q;

		fprintf(stderr, "Trying ipv6 connection to '%s'\n", hostname);

		memset(&hints, 0, sizeof(hints));
		sprintf(service, "%d", port);

		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
#ifdef AI_ADDRCONFIG
		hints.ai_flags |= AI_ADDRCONFIG;
#endif
#ifdef AI_NUMERICSERV
		hints.ai_flags |= AI_NUMERICSERV;
#endif
		if (!strcmp(hostname, "localhost")) {
			host2 = strdup("::1");
		} else if (!strcmp(hostname, "127.0.0.1")) {
			host2 = strdup("::1");
		} else if (hostname[0] == '[') {
			host2 = strdup(hostname+1);
		} else {
			host2 = strdup(hostname);
		}
		q = strrchr(host2, ']');
		if (q) {
			*q = '\0';
		}

		err = getaddrinfo(host2, service, &hints, &ai);
		if (err != 0) {
			fprintf(stderr, "ConnectToTcpAddr[ipv6]: getaddrinfo[%d]: %s\n", err, gai_strerror(err));
			usleep(100 * 1000);
			err = getaddrinfo(host2, service, &hints, &ai);
		}
		free(host2);

		if (err != 0) {
			fprintf(stderr, "ConnectToTcpAddr[ipv6]: getaddrinfo[%d]: %s (2nd try)\n", err, gai_strerror(err));
		} else {
			struct addrinfo *ap = ai;
			while (ap != NULL) {
				int fd = -1;
				char *s = ipv6_getipaddr(ap->ai_addr, ap->ai_addrlen);
				if (s) {
					fprintf(stderr, "ConnectToTcpAddr[ipv6]: trying ip-addr: '%s'\n", s);
					free(s);
				}
				if (appData.noipv4) {
					struct sockaddr_in6 *s6ptr;
					if (ap->ai_family != AF_INET6) {
						fprintf(stderr, "ConnectToTcpAddr[ipv6]: skipping AF_INET address under VNCVIEWER_NO_IPV4/-noipv4\n");
						ap = ap->ai_next;
						continue;
					}
#ifdef IN6_IS_ADDR_V4MAPPED
					s6ptr = (struct sockaddr_in6 *) ap->ai_addr;
					if (IN6_IS_ADDR_V4MAPPED(&(s6ptr->sin6_addr))) {
						fprintf(stderr, "ConnectToTcpAddr[ipv6]: skipping V4MAPPED address under VNCVIEWER_NO_IPV4/-noipv4\n");
						ap = ap->ai_next;
						continue;
					}
#endif
				}

				fd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
				if (fd == -1) {
					perror("ConnectToTcpAddr[ipv6]: socket");
				} else {
					int dmsg = 0;
					int res = connect(fd, ap->ai_addr, ap->ai_addrlen);
#if defined(SOL_IPV6) && defined(IPV6_V6ONLY)
					if (res != 0) {
						int zero = 0;
						perror("ConnectToTcpAddr[ipv6]: connect");
						dmsg = 1;
						if (setsockopt(fd, SOL_IPV6, IPV6_V6ONLY, (char *)&zero, sizeof(zero)) == 0) {
							fprintf(stderr, "ConnectToTcpAddr[ipv6]: trying again with IPV6_V6ONLY=0\n");
							res = connect(fd, ap->ai_addr, ap->ai_addrlen);
							dmsg = 0;
						}
					}
#endif
					if (res == 0) {
						fprintf(stderr, "ConnectToTcpAddr[ipv6]: connect OK\n");
						sock = fd;
						break;
					} else {
						if (!dmsg) perror("ConnectToTcpAddr[ipv6]: connect");
						close(fd);
					}
				}
				ap = ap->ai_next;
			}
			freeaddrinfo(ai);
		}
		if (sock >= 0 && setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)) < 0) {
			perror("ConnectToTcpAddr: setsockopt");
			close(sock);
			sock = -1;
		}
	}
#endif
	return sock;
}

Bool SocketPair(int fd[2]) {
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, fd) == -1) {
		perror("socketpair");
		return False;
	}
	return True;
}

Bool SetNoDelay(int sock) {
	const int one = 1;
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)) < 0) {
		perror("setsockopt");
		return False;
	}
	return True;
}

/*
 * FindFreeTcpPort tries to find unused TCP port in the range
 * (TUNNEL_PORT_OFFSET, TUNNEL_PORT_OFFSET + 99]. Returns 0 on failure.
 */

int
FindFreeTcpPort(void)
{
	int sock, port;
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		fputs(programName,stderr);
		perror(": FindFreeTcpPort: socket");
		return 0;
	}

	for (port = TUNNEL_PORT_OFFSET + 99; port > TUNNEL_PORT_OFFSET; port--) {
		addr.sin_port = htons((unsigned short)port);
		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
			close(sock);
			return port;
		}
	}

	close(sock);
	return 0;
}


/*
 * ListenAtTcpPort starts listening at the given TCP port.
 */

int use_loopback = 0;

int ListenAtTcpPort(int port) {
	int sock;
	struct sockaddr_in addr;
	int one = 1;

	if (appData.noipv4) {
		fprintf(stderr, "ipv4 is disabled via VNCVIEWER_NO_IPV4/-noipv4.\n");
		return -1;
	}

	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (getenv("VNCVIEWER_LISTEN_LOCALHOST") || use_loopback) {
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("ListenAtTcpPort: socket");
		return -1;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one)) < 0) {
		perror("ListenAtTcpPort: setsockopt");
		close(sock);
		return -1;
	}

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("ListenAtTcpPort: bind");
		close(sock);
		return -1;
	}

	if (listen(sock, 32) < 0) {
		perror("ListenAtTcpPort: listen");
		close(sock);
		return -1;
	}

	return sock;
}

int ListenAtTcpPort6(int port) {
	int sock = -1;
#ifdef AF_INET6
	struct sockaddr_in6 sin;
	int one = 1;

	if (appData.noipv6) {
		fprintf(stderr, "ipv6 is disabled via VNCVIEWER_NO_IPV6/-noipv6.\n");
		return -1;
	}

	sock = socket(AF_INET6, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("ListenAtTcpPort[ipv6]: socket");
		return -1;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) < 0) {
		perror("ListenAtTcpPort[ipv6]: setsockopt1");
		close(sock);
		return -1;
	}

#if defined(SOL_IPV6) && defined(IPV6_V6ONLY)
	if (setsockopt(sock, SOL_IPV6, IPV6_V6ONLY, (char *)&one, sizeof(one)) < 0) {
		perror("ListenAtTcpPort[ipv6]: setsockopt2");
		close(sock);
		return -1;
	}
#endif

	memset((char *)&sin, 0, sizeof(sin));
	sin.sin6_family = AF_INET6;
	sin.sin6_port   = htons(port);
	sin.sin6_addr   = in6addr_any;

	if (getenv("VNCVIEWER_LISTEN_LOCALHOST") || use_loopback) {
		sin.sin6_addr = in6addr_loopback;
	}

	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("ListenAtTcpPort[ipv6]: bind");
		close(sock);
		return -1;
	}

	if (listen(sock, 32) < 0) {
		perror("ListenAtTcpPort[ipv6]: listen");
		close(sock);
		return -1;
	}

#endif
	if (port) {}
	return sock;
}


/*
 * AcceptTcpConnection accepts a TCP connection.
 */

int AcceptTcpConnection(int listenSock) {
	int sock;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	int one = 1;

	sock = accept(listenSock, (struct sockaddr *) &addr, &addrlen);
	if (sock < 0) {
		perror("AcceptTcpConnection: accept");
		return -1;
	}

	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)) < 0) {
		perror("AcceptTcpConnection: setsockopt");
		close(sock);
		return -1;
	}

	return sock;
}

char *accept6_ipaddr = NULL;
char *accept6_hostname = NULL;

int AcceptTcpConnection6(int listenSock) {
	int sock = -1;
#ifdef AF_INET6
	struct sockaddr_in6 addr;
	socklen_t addrlen = sizeof(addr);
	int one = 1;
	char *name;

	if (appData.noipv6) {
		return -1;
	}

	sock = accept(listenSock, (struct sockaddr *) &addr, &addrlen);
	if (sock < 0) {
		perror("AcceptTcpConnection[ipv6]: accept");
		return -1;
	}

	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)) < 0) {
		perror("AcceptTcpConnection[ipv6]: setsockopt");
		close(sock);
		return -1;
	}

	name = ipv6_getipaddr((struct sockaddr *) &addr, addrlen);
	if (!name) name = strdup("unknown");
	accept6_ipaddr = name;
	fprintf(stderr, "AcceptTcpConnection6: ipv6 connection from: '%s'\n", name);

	name = ipv6_getnameinfo((struct sockaddr *) &addr, addrlen);
	if (!name) name = strdup("unknown");
	accept6_hostname = name;
#endif
	if (listenSock) {}
	return sock;
}



/*
 * SetNonBlocking sets a socket into non-blocking mode.
 */

Bool
SetNonBlocking(int sock)
{
  if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
    fputs(programName,stderr);
    perror(": AcceptTcpConnection: fcntl");
    return False;
  }
  return True;
}


/*
 * StringToIPAddr - convert a host string to an IP address.
 */

Bool
StringToIPAddr(const char *str, unsigned int *addr)
{
  struct hostent *hp;

  if (strcmp(str,"") == 0) {
    *addr = 0; /* local */
    return True;
  }

  *addr = inet_addr(str);

  if (*addr != (unsigned int) -1)
    return True;

  hp = gethostbyname(str);

  if (hp) {
    *addr = *(unsigned int *)hp->h_addr;
    return True;
  }

  return False;
}

char *get_peer_ip(int sock) {
	struct sockaddr_in saddr;
	unsigned int saddr_len;
	int saddr_port;
	char *saddr_ip_str = NULL;

	saddr_len = sizeof(saddr);
	memset(&saddr, 0, sizeof(saddr));
	saddr_port = -1;
	if (!getpeername(sock, (struct sockaddr *)&saddr, &saddr_len)) {
		saddr_ip_str = inet_ntoa(saddr.sin_addr);
	}
	if (! saddr_ip_str) {
		saddr_ip_str = "unknown";
	}
	return strdup(saddr_ip_str);
}

char *ip2host(char *ip) {
	char *str;
	struct hostent *hp;
	in_addr_t iaddr;

	iaddr = inet_addr(ip);
	if (iaddr == htonl(INADDR_NONE)) {
		return strdup("unknown");
	}

	hp = gethostbyaddr((char *)&iaddr, sizeof(in_addr_t), AF_INET);
	if (!hp) {
		return strdup("unknown");
	}
	str = strdup(hp->h_name);
	return str;
}


/*
 * Test if the other end of a socket is on the same machine.
 */

Bool
SameMachine(int sock)
{
  struct sockaddr_storage peeraddr, myaddr;
  int addrlen = sizeof(struct sockaddr_storage);

  getpeername(sock, (struct sockaddr *)&peeraddr, &addrlen);
  getsockname(sock, (struct sockaddr *)&myaddr, &addrlen);

  assert(((struct sockaddr*)&peeraddr)->sa_family == ((struct sockaddr*)&myaddr)->sa_family);
  if (((struct sockaddr*)&myaddr)->sa_family == AF_INET6) {
      return 0 == memcmp(&((struct sockaddr_in6*)&peeraddr)->sin6_addr.s6_addr,
			 &((struct sockaddr_in6*)&myaddr)->sin6_addr.s6_addr,
			 sizeof(struct in6_addr));
  } else {
      return ((struct sockaddr_in*)&peeraddr)->sin_addr.s_addr
	  == ((struct sockaddr_in*)&myaddr)->sin_addr.s_addr;
  }
}


/*
 * Print out the contents of a packet for debugging.
 */

void
PrintInHex(char *buf, int len)
{
  int i, j;
  char c, str[17];

  str[16] = 0;

  fprintf(stderr,"ReadExact: ");

  for (i = 0; i < len; i++)
    {
      if ((i % 16 == 0) && (i != 0)) {
	fprintf(stderr,"           ");
      }
      c = buf[i];
      str[i % 16] = (((c > 31) && (c < 127)) ? c : '.');
      fprintf(stderr,"%02x ",(unsigned char)c);
      if ((i % 4) == 3)
	fprintf(stderr," ");
      if ((i % 16) == 15)
	{
	  fprintf(stderr,"%s\n",str);
	}
    }
  if ((i % 16) != 0)
    {
      for (j = i % 16; j < 16; j++)
	{
	  fprintf(stderr,"   ");
	  if ((j % 4) == 3) fprintf(stderr," ");
	}
      str[i % 16] = 0;
      fprintf(stderr,"%s\n",str);
    }

  fflush(stderr);
}
