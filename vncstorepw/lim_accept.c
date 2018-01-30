/*
  gcc -shared     -nostartfiles -fPIC -o lim_accept.so lim_accept.c
  gcc -dynamiclib -nostartfiles -fPIC -o lim_accept.so lim_accept.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/* rename accept something else while we do the includes: */
#define accept __accept_5_Moos
#include <sys/types.h>
#include <sys/socket.h>
#undef accept

#define __USE_GNU
#include <dlfcn.h>

static int n_accept = -1;
static time_t t_start = 0;
static int maxa = -1, maxt = -1;
static int db = 0;

#if (defined(__MACH__) && defined(__APPLE__))
__attribute__((constructor))
#endif
void _init(void) {
	if (getenv("LIM_ACCEPT_DEBUG")) db = 1;
	if (db) fprintf(stderr, "lim_accept _init()\n");
	t_start = time(NULL);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
	static int (*real_accept)(int s, struct sockaddr *addr, socklen_t *addrlen) = NULL;
	int reject = 0, ret;

	if (n_accept < 1e+8) n_accept++;
	if (! real_accept) {
		real_accept = (int (*)(int s, struct sockaddr *addr, socklen_t *addrlen)) dlsym(RTLD_NEXT, "accept");
	}

	if (maxa == -1) {
		if (getenv("LIM_ACCEPT_DEBUG")) db = 1;
		maxa = 0;
		if (getenv("LIM_ACCEPT")) {
			maxa = atoi(getenv("LIM_ACCEPT"));
			if (maxa < 0) maxa = 0;
		}
		maxt = 0;
		if (getenv("LIM_ACCEPT_TIME")) {
			maxt = atoi(getenv("LIM_ACCEPT_TIME"));
			if (maxt < 0) maxt = 0;
		}
	}

	ret = real_accept(s, addr, addrlen);
	if (db) fprintf(stderr, "accept called %d times: ret=%d  maxa=%d maxt=%d\r\n", n_accept, ret, maxa, maxt);

	if (maxa > 0 && n_accept >= maxa) {
		if (db) fprintf(stderr, "rejecting extra accept: too many: %d >= %d\r\n", n_accept, maxa);
		reject = 1;
	}
	if (maxt > 0 && time(NULL) > t_start + maxt) {
		if (db) fprintf(stderr, "rejecting extra accept: too late: %d > %d\r\n", (int) (time(NULL) - t_start), maxt);
		reject = 1;
	}
	if (reject) {
		if (ret >= 0) {
			close(ret); 
		}
		errno = ECONNABORTED;
		return -1;
	}
	return ret;
}

#if (defined(__MACH__) && defined(__APPLE__))
int accept$UNIX2003(int s, struct sockaddr *addr, socklen_t *addrlen) {
	return accept(s, addr, addrlen);
}
#endif
