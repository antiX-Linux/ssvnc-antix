#!/bin/sh

EPATH = /usr/ccs/bin:/usr/sfw/bin:/usr/local/bin

CMD = vncstorepw
UNWRAP = unwrap.so
LIMACC = lim_accept.so
ULTDSM = ultravnc_dsm_helper

# gcc must be used:
CC = gcc $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

all: $(CMD) $(UNWRAP) $(LIMACC) $(ULTDSM)

$(CMD): vncauth.o vncstorepw.o d3des.o
	PATH=$$PATH:$(EPATH) $(CC) -o $(CMD) vncstorepw.o vncauth.o d3des.o
	@ls -l $(CMD)

$(UNWRAP): unwrap.c
	if [ `uname` = "Darwin" ]; then \
		PATH=$$PATH:$(EPATH) $(CC) -o $(UNWRAP) -dynamiclib -fPIC unwrap.c; \
	else \
		PATH=$$PATH:$(EPATH) $(CC) -o $(UNWRAP) -shared     -fPIC unwrap.c; \
	fi
	@ls -l $(UNWRAP)

$(LIMACC): lim_accept.c
	if [ `uname` = "Darwin" ]; then \
		PATH=$$PATH:$(EPATH) $(CC) -o $(LIMACC) -dynamiclib -fPIC -nostartfiles lim_accept.c; \
	else \
		PATH=$$PATH:$(EPATH) $(CC) -o $(LIMACC) -shared     -fPIC -nostartfiles lim_accept.c; \
	fi
	@ls -l $(LIMACC)

# use: LD_SSL='-L/usr/foo/lib' CPP_SSL='-I /usr/foo/include':

$(ULTDSM): ultravnc_dsm_helper.c 
	if [ `uname` = "SunOS" ]; then \
		PATH=$$PATH:$(EPATH) $(CC) -o $(ULTDSM) $(CPP_SSL) ultravnc_dsm_helper.c $(LD_SSL) -lssl -lcrypto -lsocket -lnsl; \
	else \
		PATH=$$PATH:$(EPATH) $(CC) -o $(ULTDSM) $(CPP_SSL) ultravnc_dsm_helper.c $(LD_SSL) -lssl -lcrypto; \
	fi
	@ls -l $(ULTDSM)

test: $(CMD)
	./test_it .

clean:
	rm -f *.o $(CMD) $(UNWRAP) $(LIMACC) $(ULTDSM)

realclean: clean
	find . -type d | grep '\./.*\.' | xargs rm -rf
	
tar: realclean
	cd ..; tar cvf $(CMD).tar $(CMD)

sysdir: $(CMD)
	dir=./`uname -sm | sed -e 's/ /./g'`; mkdir -p $$dir
	dir=./`uname -sm | sed -e 's/ /./g'`; cp -p $(CMD) $$dir
	dir=./`uname -sm | sed -e 's/ /./g'`; cp -p $(LIMACC) $$dir
	dir=./`uname -sm | sed -e 's/ /./g'`; cp -p $(ULTDSM) $$dir
