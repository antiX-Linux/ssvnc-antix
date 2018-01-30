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
 * shm.c - code to set up shared memory extension.
 */

#include <vncviewer.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
static XShmSegmentInfo shminfo;

static Bool caughtShmError = False;
static Bool needShmCleanup = False;

static int ShmCreationXErrorHandler(Display *dpy, XErrorEvent *error) {
	caughtShmError = True;
	if (dpy || error) {}
	return 0;
}

void ShmDetach() {
	if (needShmCleanup) {
		XErrorHandler oldXErrorHandler = XSetErrorHandler(ShmCreationXErrorHandler);
		fprintf(stderr,"ShmDetach called.\n");
		XShmDetach(dpy, &shminfo);
		XSync(dpy, False);
		XSetErrorHandler(oldXErrorHandler);
	}
}

void ShmCleanup() {
	if (needShmCleanup) {
		fprintf(stderr,"ShmCleanup called.\n");
		XSync(dpy, False);
		shmdt(shminfo.shmaddr);
		shmctl(shminfo.shmid, IPC_RMID, 0);

		needShmCleanup = False;
	}
}

Bool UsingShm() {
	return needShmCleanup;
}

int scale_round(int len, double fac);
extern int scale_x, scale_y;
extern double scale_factor_x, scale_factor_y;

XImage *
CreateShmImage(int do_ycrop)
{
	XImage *image;
	XErrorHandler oldXErrorHandler;
	int ymax = si.framebufferHeight;
	int xmax = si.framebufferWidth;

	if (!XShmQueryExtension(dpy)) {
		return NULL;
	}
	if (!appData.useShm) {
		return NULL;
	}
	if (do_ycrop == -1) {
		/* kludge to test for shm prescence */
		return (XImage *) 0x1;
	}

	if (do_ycrop) {
		ymax = appData.yCrop;
	}

	if (scale_x > 0) {
		xmax = scale_round(xmax, scale_factor_x);
		ymax = scale_round(ymax, scale_factor_y);
	}

	image = XShmCreateImage(dpy, vis, visdepth, ZPixmap, NULL, &shminfo, xmax, ymax);
	if (!image) {
		return NULL;
	}

	shminfo.shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT|0777);

	if (shminfo.shmid == -1) {
		XDestroyImage(image);
		if (0) fprintf(stderr, "CreateShmImage: destroyed 'image' (1)\n");
		return NULL;
	}

	shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);

	if (shminfo.shmaddr == (char *)-1) {
		XDestroyImage(image);
#if 0
		fprintf(stderr, "CreateShmImage: destroyed 'image' (2)\n");
#endif
		shmctl(shminfo.shmid, IPC_RMID, 0);
		return NULL;
	}

	shminfo.readOnly = True;

	oldXErrorHandler = XSetErrorHandler(ShmCreationXErrorHandler);
	XShmAttach(dpy, &shminfo);
	XSync(dpy, False);
	XSetErrorHandler(oldXErrorHandler);

	if (caughtShmError) {
		XDestroyImage(image);
#if 0
		fprintf(stderr, "CreateShmImage: destroyed 'image' (3)\n");
#endif
		shmdt(shminfo.shmaddr);
		shmctl(shminfo.shmid, IPC_RMID, 0);
		return NULL;
	}

	needShmCleanup = True;

	fprintf(stderr,"Using shared memory (PutImage ycrop=%d, Size %dx%d)\n", do_ycrop, xmax, ymax);

	return image;
}
