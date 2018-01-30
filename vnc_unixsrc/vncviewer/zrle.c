/*
 *  Copyright (C) 2005 Johannes E. Schindelin.  All Rights Reserved.
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
 * zrle.c - handle zrle encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles an zrle
 * encoded rectangle with BPP bits per pixel.
 */

#ifndef REALBPP
#define REALBPP BPP
#endif

#if !defined(UNCOMP) || UNCOMP==0
#define HandleZRLE CONCAT2E(HandleZRLE,REALBPP)
#define HandleZRLETile CONCAT2E(HandleZRLETile,REALBPP)
#elif UNCOMP>0
#define HandleZRLE CONCAT3E(HandleZRLE,REALBPP,Down)
#define HandleZRLETile CONCAT3E(HandleZRLETile,REALBPP,Down)
#else
#define HandleZRLE CONCAT3E(HandleZRLE,REALBPP,Up)
#define HandleZRLETile CONCAT3E(HandleZRLETile,REALBPP,Up)
#endif
#undef CARDBPP
#undef CARDREALBPP
#define CARDBPP CONCAT2E(CARD, BPP)
#define CARDREALBPP CONCAT2E(CARD,REALBPP)

#define FillRectangle(x, y, w, h, color)  \
	{ \
		XGCValues _gcv; \
		_gcv.foreground = color; \
		if (!appData.useXserverBackingStore) { \
			FillScreen(x, y, w, h, _gcv.foreground); \
		} else { \
			XChangeGC(dpy, gc, GCForeground, &_gcv); \
			XFillRectangle(dpy, desktopWin, gc, x, y, w, h); \
		} \
	}

#if defined(__sparc) || defined(__sparc__) || defined(__ppc__) || defined(__POWERPC__) || defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

#if DO_ZYWRLE

#define ENDIAN_LITTLE 0
#define ENDIAN_BIG 1
#define ENDIAN_NO 2
#if IS_BIG_ENDIAN
#define ZYWRLE_ENDIAN ENDIAN_BIG
#else
#define ZYWRLE_ENDIAN ENDIAN_LITTLE
#endif
#undef END_FIX
#if ZYWRLE_ENDIAN == ENDIAN_LITTLE
#  define END_FIX LE
#elif ZYWRLE_ENDIAN == ENDIAN_BIG
#  define END_FIX BE
#else
#  define END_FIX NE
#endif
#define __RFB_CONCAT3E(a,b,c) CONCAT3E(a,b,c)
#define __RFB_CONCAT2E(a,b) CONCAT2E(a,b)
#undef CPIXEL
#if REALBPP != BPP
#if UNCOMP == 0
#define CPIXEL REALBPP
#elif UNCOMP>0
#define CPIXEL CONCAT2E(REALBPP,Down)
#else
#define CPIXEL CONCAT2E(REALBPP,Up)
#endif
#endif
#define PIXEL_T CARDBPP
#if BPP!=8
#define ZYWRLE_DECODE 1
#include "zywrletemplate.c"
#endif
#undef CPIXEL

#endif	/* DO_ZYWRLE */

static int HandleZRLETile(
	unsigned char* buffer,size_t buffer_length,
	int x,int y,int w,int h);

static Bool
HandleZRLE (int rx, int ry, int rw, int rh)
{
	rfbZRLEHeader header;
	int remaining;
	int inflateResult;
	int toRead;
	int min_buffer_size = rw * rh * (REALBPP / 8) * 2;

	/* First make sure we have a large enough raw buffer to hold the
	 * decompressed data.  In practice, with a fixed REALBPP, fixed frame
	 * buffer size and the first update containing the entire frame
	 * buffer, this buffer allocation should only happen once, on the
	 * first update.
	 */
	if ( raw_buffer_size < min_buffer_size) {

		if ( raw_buffer != NULL ) {

			free( raw_buffer );

		}

		raw_buffer_size = min_buffer_size;
		raw_buffer = (char*) malloc( raw_buffer_size );

	}

	if (!ReadFromRFBServer((char *)&header, sz_rfbZRLEHeader))
		return False;

	remaining = Swap32IfLE(header.length);

	/* Need to initialize the decompressor state. */
	decompStream.next_in   = ( Bytef * )buffer;
	decompStream.avail_in  = 0;
	decompStream.next_out  = ( Bytef * )raw_buffer;
	decompStream.avail_out = raw_buffer_size;
	decompStream.data_type = Z_BINARY;

	/* Initialize the decompression stream structures on the first invocation. */
	if ( decompStreamInited == False ) {

		inflateResult = inflateInit( &decompStream );

		if ( inflateResult != Z_OK ) {
			fprintf(stderr, 
					"inflateInit returned error: %d, msg: %s\n",
					inflateResult,
					decompStream.msg);
			return False;
		}

		decompStreamInited = True;

	}

	inflateResult = Z_OK;

	/* Process buffer full of data until no more to process, or
	 * some type of inflater error, or Z_STREAM_END.
	 */
	while (( remaining > 0 ) &&
			( inflateResult == Z_OK )) {

		if ( remaining > BUFFER_SIZE ) {
			toRead = BUFFER_SIZE;
		}
		else {
			toRead = remaining;
		}

		/* Fill the buffer, obtaining data from the server. */
		if (!ReadFromRFBServer(buffer,toRead))
			return False;

		decompStream.next_in  = ( Bytef * )buffer;
		decompStream.avail_in = toRead;

		/* Need to uncompress buffer full. */
		inflateResult = inflate( &decompStream, Z_SYNC_FLUSH );

		/* We never supply a dictionary for compression. */
		if ( inflateResult == Z_NEED_DICT ) {
			fprintf(stderr, "zlib inflate needs a dictionary!\n");
			return False;
		}
		if ( inflateResult < 0 ) {
			fprintf(stderr, 
					"zlib inflate returned error: %d, msg: %s\n",
					inflateResult,
					decompStream.msg);
			return False;
		}

		/* Result buffer allocated to be at least large enough.  We should
		 * never run out of space!
		 */
		if (( decompStream.avail_in > 0 ) &&
				( decompStream.avail_out <= 0 )) {
			fprintf(stderr, "zlib inflate ran out of space!\n");
			return False;
		}

		remaining -= toRead;

	} /* while ( remaining > 0 ) */

	if ( inflateResult == Z_OK ) {
		void* buf=raw_buffer;
		int i,j;

		remaining = raw_buffer_size-decompStream.avail_out;

		for(j=0; j<rh; j+=rfbZRLETileHeight)
			for(i=0; i<rw; i+=rfbZRLETileWidth) {
				int subWidth=(i+rfbZRLETileWidth>rw)?rw-i:rfbZRLETileWidth;
				int subHeight=(j+rfbZRLETileHeight>rh)?rh-j:rfbZRLETileHeight;
				int result=HandleZRLETile(buf,remaining,rx+i,ry+j,subWidth,subHeight);

				if(result<0) {
					fprintf(stderr, "ZRLE decoding failed (%d)\n",result);
return True;
					return False;
				}

				buf+=result;
				remaining-=result;
			}
	}
	else {

		fprintf(stderr, 
				"zlib inflate returned error: %d, msg: %s\n",
				inflateResult,
				decompStream.msg);
		return False;

	}

	return True;
}

#if REALBPP!=BPP && defined(UNCOMP) && UNCOMP!=0
# if BPP == 32 && IS_BIG_ENDIAN
#  define UncompressCPixel(p) ( (*p << myFormat.redShift) | (*(p+1) << myFormat.greenShift) | (*(p+2) << myFormat.blueShift) )
# else
#  if UNCOMP>0
#   define UncompressCPixel(pointer) ((*(CARDBPP*)pointer)>>UNCOMP)
#  else
#   define UncompressCPixel(pointer) ((*(CARDBPP*)pointer)<<(-(UNCOMP)))
#  endif
# endif
#else
# define UncompressCPixel(pointer) (*(CARDBPP*)pointer)
#endif

extern XImage *image;
extern XImage *image_scale;
extern int skip_maybe_sync;

static int HandleZRLETile(
		unsigned char* buffer,size_t buffer_length,
		int x,int y,int w,int h) {
	unsigned char* buffer_copy = buffer;
	unsigned char* buffer_end = buffer+buffer_length;
	unsigned char type;

	if(buffer_length<1)
		return -2;

	if (frameBufferLen < w * h * BPP/8) {
		if(frameBuffer) {
			free(frameBuffer);
		}
		frameBufferLen = w * h * BPP/8 * 2;
		frameBuffer = (unsigned char *) malloc(frameBufferLen);
	}

zywrle_top:
	type = *buffer;
	buffer++;
	switch(type) {
		case 0: /* raw */
		{
#if DO_ZYWRLE && BPP != 8
		    if (zywrle_level > 0 && !(zywrle_level & 0x80) ) {
			zywrle_level |= 0x80;
			goto zywrle_top;
		    } else
#endif
		    {
#if REALBPP!=BPP
			int m0 = 0, i,j;


			if(1+w*h*REALBPP/8>buffer_length) {
				fprintf(stderr, "expected %d bytes, got only %d (%dx%d)\n",1+w*h*REALBPP/8,buffer_length,w,h);
				return -3;
			}

			for(j=y*si.framebufferWidth; j<(y+h)*si.framebufferWidth; j+=si.framebufferWidth) {
				for(i=x; i<x+w; i++,buffer+=REALBPP/8) {
#  if 0
					((CARDBPP*)frameBuffer)[j+i] = UncompressCPixel(buffer);
					/* alt */
					CARDBPP color = UncompressCPixel(buffer);
					CopyDataToScreen((char *)&color, i, j/si.framebufferWidth, 1, 1);
#  else
					((CARDBPP*)frameBuffer)[m0++] = UncompressCPixel(buffer);
#  endif
				}
			}
			CopyDataToScreen((char *)frameBuffer, x, y, w, h);
if (0) fprintf(stderr, "cha1: %dx%d+%d+%d\n", w, h, x, y);

#else
#  if 0
			CopyRectangle(buffer, x, y, w, h);
#  else
			CopyDataToScreen((char *)buffer, x, y, w, h);
#  endif
			buffer+=w*h*REALBPP/8;
#endif
		    }
			break;
		}
		case 1: /* solid */
		{
			CARDBPP color = UncompressCPixel(buffer);

			if(1+REALBPP/8>buffer_length)
				return -4;
				
			if ((BPP == 8 && appData.useBGR233) || (BPP == 16 && appData.useBGR565)) {
				int m0;
				for (m0=0; m0 < w*h; m0++) {
					((CARDBPP*)frameBuffer)[m0] = color;
				}
				CopyDataToScreen((char *)frameBuffer, x, y, w, h);
			} else {
				FillRectangle(x, y, w, h, color);
			}
if (0) fprintf(stderr, "cha2: %dx%d+%d+%d\n", w, h, x, y);

			buffer+=REALBPP/8;

			break;
		}
		case 2 ... 127: /* packed Palette */
		{
			CARDBPP palette[16];
			int m0, i,j,shift,
				bpp=(type>4?(type>16?8:4):(type>2?2:1)),
				mask=(1<<bpp)-1,
				divider=(8/bpp);

			if(1+type*REALBPP/8+((w+divider-1)/divider)*h>buffer_length)
				return -5;

			/* read palette */
			for(i=0; i<type; i++,buffer+=REALBPP/8)
				palette[i] = UncompressCPixel(buffer);

			m0 = 0;
			/* read palettized pixels */
			for(j=y*si.framebufferWidth; j<(y+h)*si.framebufferWidth; j+=si.framebufferWidth) {
				for(i=x,shift=8-bpp; i<x+w; i++) {
#  if 0
					((CARDBPP*)frameBuffer)[j+i] = palette[((*buffer)>>shift)&mask];
					/* alt */
					CARDBPP color = palette[((*buffer)>>shift)&mask];
					CopyDataToScreen((char *)&color, i, j/si.framebufferWidth, 1, 1);
#  else
					((CARDBPP*)frameBuffer)[m0++] = palette[((*buffer)>>shift)&mask];
#  endif
					shift-=bpp;
					if(shift<0) {
						shift=8-bpp;
						buffer++;
					}
				}
				if(shift<8-bpp)
					buffer++;
			}
			CopyDataToScreen((char *)frameBuffer, x, y, w, h);
if (0) fprintf(stderr, "cha3: %dx%d+%d+%d\n", w, h, x, y);

			break;
		}
		/* case 17 ... 127: not used, but valid */
		case 128: /* plain RLE */
		{
			int m0=0, i=0,j=0;
			while(j<h) {
				int color,length;
				/* read color */
				if(buffer+REALBPP/8+1>buffer_end)
					return -7;
				color = UncompressCPixel(buffer);
				buffer+=REALBPP/8;
				/* read run length */
				length=1;
				while(*buffer==0xff) {
					if(buffer+1>=buffer_end)
						return -8;
					length+=*buffer;
					buffer++;
				}
				length+=*buffer;
				buffer++;
				while(j<h && length>0) {
#  if 0
					((CARDBPP*)frameBuffer)[(y+j)*si.framebufferWidth+x+i] = color;
					/* alt */
					CopyDataToScreen((char *)&color, x+i, y+j, 1, 1);
#  else
					((CARDBPP*)frameBuffer)[m0++] = color;
#  endif
					length--;
					i++;
					if(i>=w) {
						i=0;
						j++;
					}
				}
				if(length>0)
					fprintf(stderr, "Warning: possible ZRLE corruption\n");
			}
			CopyDataToScreen((char *)frameBuffer, x, y, w, h);
if (0) fprintf(stderr, "cha4: %dx%d+%d+%d\n", w, h, x, y);

			break;
		}
		case 129: /* unused */
		{
			return -8;
		}
		case 130 ... 255: /* palette RLE */
		{
			CARDBPP palette[128];
			int m0 = 0, i,j;

			if(2+(type-128)*REALBPP/8>buffer_length)
				return -9;

			/* read palette */
			for(i=0; i<type-128; i++,buffer+=REALBPP/8)
				palette[i] = UncompressCPixel(buffer);
			/* read palettized pixels */
			i=j=0;
			while(j<h) {
				int color,length;
				/* read color */
				if(buffer>=buffer_end)
					return -10;
				color = palette[(*buffer)&0x7f];
				length=1;
				if(*buffer&0x80) {
					if(buffer+1>=buffer_end)
						return -11;
					buffer++;
					/* read run length */
					while(*buffer==0xff) {
						if(buffer+1>=buffer_end)
							return -8;
						length+=*buffer;
						buffer++;
					}
					length+=*buffer;
				}
				buffer++;
				while(j<h && length>0) {
#  if 0
					((CARDBPP*)frameBuffer)[(y+j)*si.framebufferWidth+x+i] = color;
					/* alt */
					CopyDataToScreen((char *)&color, x+i, y+j, 1, 1);
#  else
					((CARDBPP*)frameBuffer)[m0++] = color;
#  endif
					length--;
					i++;
					if(i>=w) {
						i=0;
						j++;
					}
				}
				if(length>0)
					fprintf(stderr, "Warning: possible ZRLE corruption\n");
			}
			CopyDataToScreen((char *)frameBuffer, x, y, w, h);
if (0) fprintf(stderr, "cha5: %dx%d+%d+%d\n", w, h, x, y);

			break;
		}
	}

#if DO_ZYWRLE && BPP != 8
	if (zywrle_level & 0x80) {
		int th, tx;
		int widthInBytes = w * BPP / 8;
		int scrWidthInBytes;
		char *scr, *buf;
		static CARDBPP *ptmp = NULL;
		static int ptmp_len = 0;
		XImage *im = image_scale ? image_scale : image; 

		if (w * h > ptmp_len) {
			ptmp_len = w * h;
			if (ptmp_len < rfbZRLETileWidth*rfbZRLETileHeight) {
				ptmp_len = rfbZRLETileWidth*rfbZRLETileHeight;
			}
			if (ptmp) {
				free(ptmp);
			}
			ptmp = (CARDBPP *) malloc(ptmp_len * sizeof(CARDBPP));
		}

		zywrle_level &= 0x7F;
		/* Reverse copy: screen to buf/ptmp: */
		/* make this CopyDataFromScreen() or something. */
		if (!appData.useBGR565) {
			scrWidthInBytes = si.framebufferWidth * myFormat.bitsPerPixel / 8;
			if (scrWidthInBytes != im->bytes_per_line) scrWidthInBytes = im->bytes_per_line;
			scr = im->data + y * scrWidthInBytes + x * myFormat.bitsPerPixel / 8;
			buf = (char *) ptmp;

			for (th = 0; th < h; th++) {
				memcpy(buf, scr, widthInBytes);
				buf += widthInBytes;
				scr += scrWidthInBytes;
			}
		} else {
			scrWidthInBytes = si.framebufferWidth * 4;
			if (scrWidthInBytes != im->bytes_per_line) scrWidthInBytes = im->bytes_per_line;
			scr = im->data + y * scrWidthInBytes + x * 4;
			buf = (char *) ptmp;

			for (th = 0; th < h; th++) {
				for (tx = 0; tx < w; tx++) {
					unsigned long pix = *((unsigned int *)scr + tx);
					unsigned int r1 = (pix & 0xff0000) >> 16; 
					unsigned int g1 = (pix & 0x00ff00) >> 8; 
					unsigned int b1 = (pix & 0x0000ff) >> 0; 
					int r2, g2, b2, idx;
					int rok = 0, gok = 0, bok = 0, is0, sh = 10;
					r2 = (31 * r1)/255;
					g2 = (63 * g1)/255;
					b2 = (31 * b1)/255;
					for (is0 = 0; is0 < sh; is0++) {
						int is, i, t;
						for (i = 0; i < 2; i++) {
							if (i == 0) {
								is = -is0;
							} else {
								is = +is0;
							}
							if (!rok) {
								t = r2 + is;
								if (r1 == (255 * t)/31) {
									r2 = t; rok = 1;
								}
							}
							if (!gok) {
								t = g2 + is;
								if (g1 == (255 * t)/63) {
									g2 = t; gok = 1;
								}
							}
							if (!bok) {
								t = b2 + is;
								if (b1 == (255 * t)/31) {
									b2 = t; bok = 1;
								}
							}
						}
						if (rok && gok && bok) {
							break;
						}
					}
					idx = (r2 << 11) | (g2 << 5) | (b2 << 0);
					*((CARDBPP *)buf + tx) = (CARDBPP) idx;
				}
				buf += widthInBytes;
				scr += scrWidthInBytes;
			}
		}
		ZYWRLE_SYNTHESIZE((PIXEL_T *)ptmp, (PIXEL_T *)ptmp, w, h, w, zywrle_level, zywrleBuf );
		skip_maybe_sync = 1;

		if (appData.yCrop > 0) {
			skip_maybe_sync = 0;
		}
		CopyDataToScreen((char *)ptmp, x, y, w, h);

	}
#endif

	return buffer-buffer_copy;	
}

#undef CARDBPP
#undef CARDREALBPP
#undef HandleZRLE
#undef HandleZRLETile
#undef UncompressCPixel
#undef REALBPP

#undef UNCOMP

#undef FillRectangle
#undef IS_BIG_ENDIAN
