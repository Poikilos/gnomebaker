## Process this file with automake to produce Makefile.in

## Created by Anjuta - will be overwritten
## If you don't want it to overwrite it,
## 	Please disable it in the Anjuta project configuration

INCLUDES = \
	$(GNOME_CFLAGS)

AM_CFLAGS =\
	 -Wall\
	 -g

AM_CPPFLAGS = $(DEPS_CFLAGS) \
	      -DLOCALE_DIR=\""$(datadir)/locale"\" \
	      -DPIXMAPS_DIR=\""$(datadir)/pixmaps"\"


bin_PROGRAMS = gnomebaker

gnomebaker_SOURCES = \
	main.c\
	progressdlg.c\
	progressdlg.h\
	burn.c\
	burn.h\
	exec.c\
	exec.h\
	cdda2wav.c\
	cdda2wav.h\
	cdrecord.c\
	cdrecord.h\
	readcd.c\
	readcd.h\
	devices.c\
	devices.h\
	filebrowser.c\
	filebrowser.h\
	mkisofs.c\
	mkisofs.h\
	preferences.c\
	preferences.h\
	gnomebaker.c\
	gnomebaker.h\
	startdlg.c\
	startdlg.h\
	prefsdlg.c\
	prefsdlg.h\
	splashdlg.c\
	splashdlg.h\
	audiocd.c\
	audiocd.h\
	datacd.c\
	datacd.h\
	gbcommon.h\
	gbcommon.c\
	audioinfo.c\
	audioinfo.h\
	sox.c\
	sox.h\
	mpg123.c\
	mpg123.h\
	oggdec.c\
	oggdec.h\
	dvdfunctions.c\
	dvdfunctions.h\
	startdlgdvd.c\
	startdlgdvd.h

gnomebaker_LDFLAGS = 

gnomebaker_LDADD =  \
	$(GNOME_LIBS)\
	 -lvorbisfile

