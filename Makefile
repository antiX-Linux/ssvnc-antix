#
# One can modify the install location via, e.g.:
#
#   make PREFIX=/usr/local install
#
# Files are installed into $(PREFIX)/bin and $(PREFIX)/lib/ssvnc
#
# Set ROOT to a staging area directory for an install that thinks
# it is installed in $(PREFIX) but is really in $(ROOT)$(PREFIX).
#
# Set MANDIR to, say, share/man to install the manpages in a
# different place.
#
# BINNAME will be the name of the viewer binary installed in bin,
# default is 'ssvncviewer'.  The copy in lib/ssvnc is used by
# the GUI to be sure it gets the right one.  Set BINNAME to the
# empty string to skip installing ssvncviewer in bin.
#
# APPS is set to the sub location of the ssvnc.desktop file.
# Set APPS to the empty string to skip installing ssvnc.desktop. 

# N.B. ?= is gnu make specific.  Some of the subdir Makefiles are too. 
#
PREFIX  ?= /usr/local
ROOT    ?=
BIN      = bin
LIB      = lib/ssvnc
MAN      = man
MANDIR  ?= $(MAN)
APPS    ?= share/applications
BINNAME ?= ssvncviewer


VSRC = vnc_unixsrc
JSRC = ultraftp
PSRC = vncstorepw

VIEWER  = $(VSRC)/vncviewer/vncviewer
VNCSPW  = $(PSRC)/vncstorepw
UNWRAP  = $(PSRC)/unwrap.so
LIMACC  = $(PSRC)/lim_accept.so
ULTDSM  = $(PSRC)/ultravnc_dsm_helper
ARCHIVE = $(JSRC)/ultraftp.jar

config:
	sh -c 'type xmkmf'
	if [ "X$(JSRC)" != "X" ]; then sh -c 'type javac'; fi
	if [ "X$(JSRC)" != "X" ]; then sh -c 'type jar'; fi
	@echo
	cd $(VSRC)/libvncauth; pwd; xmkmf
	cd $(VSRC)/vncviewer;  pwd; xmkmf
	@echo
	@echo Now run: "'make all'"

all:
	cd $(VSRC)/libvncauth; $(MAKE) EXTRA_DEFINES="$(CPPFLAGS)" CDEBUGFLAGS="$(CFLAGS)" LOCAL_LDFLAGS="$(LDFLAGS)"
	cd $(VSRC)/vncviewer;  $(MAKE) EXTRA_DEFINES="$(CPPFLAGS)" CDEBUGFLAGS="$(CFLAGS)" LOCAL_LDFLAGS="$(LDFLAGS)"
	if [ "X$(JSRC)" != "X" ]; then cd $(JSRC); $(MAKE); fi
	cd $(PSRC); $(MAKE) CPPFLAGS="$(CPPFLAGS)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

clean:
	cd $(VSRC)/libvncauth; $(MAKE) clean
	cd $(VSRC)/vncviewer;  $(MAKE) clean
	if [ "X$(JSRC)" != "X" ]; then cd $(JSRC); $(MAKE) clean; fi
	cd $(PSRC); $(MAKE) clean

install: all
	mkdir -p $(ROOT)$(PREFIX)/$(BIN) $(ROOT)$(PREFIX)/$(LIB) $(ROOT)$(PREFIX)/$(MANDIR)/man1
	cp -p $(VIEWER) $(ROOT)$(PREFIX)/$(LIB)
	cp -p $(VNCSPW) $(ROOT)$(PREFIX)/$(LIB)
	cp -p $(UNWRAP) $(ROOT)$(PREFIX)/$(LIB)
	cp -p $(LIMACC) $(ROOT)$(PREFIX)/$(LIB)
	cp -p $(ULTDSM) $(ROOT)$(PREFIX)/$(LIB)
	cp -pR scripts/* $(ROOT)$(PREFIX)/$(LIB)
	if [ "X$(JSRC)" != "X" ]; then cp -p $(ARCHIVE) $(ROOT)$(PREFIX)/$(LIB)/util; fi
	cp -p $(MAN)/man1/ssvnc.1 $(ROOT)$(PREFIX)/$(MANDIR)/man1
	./wr_tool $(ROOT)$(PREFIX)/$(BIN)/ssvnc  $(PREFIX)/$(LIB)/ssvnc
	./wr_tool $(ROOT)$(PREFIX)/$(BIN)/tsvnc  $(PREFIX)/$(LIB)/tsvnc
	./wr_tool $(ROOT)$(PREFIX)/$(BIN)/sshvnc $(PREFIX)/$(LIB)/sshvnc
	if [ "X$(APPS)" != X ]; then mkdir -p $(ROOT)$(PREFIX)/$(APPS); fi
	if [ "X$(APPS)" != X ]; then cp -p ssvnc.desktop $(ROOT)$(PREFIX)/$(APPS); fi
	if [ "X$(BINNAME)" != X ]; then cp -p $(VIEWER) $(ROOT)$(PREFIX)/$(BIN)/$(BINNAME); fi
	if [ "X$(BINNAME)" != X ]; then cp -p $(MAN)/man1/ssvncviewer.1 $(ROOT)$(PREFIX)/$(MANDIR)/man1/$(BINNAME).1; fi


#internal use only, a test install:
TINDIR = /var/tmp/TIN

tin:
	rm -rf $(TINDIR)
	$(MAKE) PREFIX=$(TINDIR) config all install
	find $(TINDIR) -ls
