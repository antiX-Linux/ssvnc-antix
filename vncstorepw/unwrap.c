/*
 * LD_PRELOAD util/hack to disable stunnel using libwrap (tcp wrappers).
 *
 * Default way is to interpose on getpeername() and have it fail with
 * ENOTSOCK.   The second way (rename hosts_access0 -> hosts_access first)
 * is to have hosts_access() always return 1.
 *
 * compile with:

  gcc -shared     -fPIC -o unwrap.so unwrap.c
  gcc -dynamiclib -fPIC -o unwrap.so unwrap.c

 * use via:
 *
 * LD_PRELOAD=/path/to/unwrap.so comannd args
 *
 * we used this for stunnel(8) where a socketpair(2) has been connected
 * to the exec'd stunnel.  It thinks its stdio is a socket and so then
 * applies tcp_wrappers to it.  The getpeername() failure is an easy
 * way to trick stunnel into thinking it is not a socket.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int db = 0;

int getpeername(int s, void *name, void* namelen) {
	if (getenv("UNWRAP_DEBUG")) db = 1;
	if (s || name || namelen) {}
	if (db) {
		fprintf(stderr, "unwrap.so: getpeername() returning -1 & ENOTSOCK\n");
		fflush(stderr);
	}
	errno = ENOTSOCK;
	return -1;
}

int hosts_access0(void *request) {
	if (getenv("UNWRAP_DEBUG")) db = 1;
	if (request) {}
	if (db) {
		fprintf(stderr, "unwrap.so: hosts_access() returning 1\n");
		fflush(stderr);
	}
	return 1;
}

