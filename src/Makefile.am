AM_CPPFLAGS =	                             \
	-I$(top_srcdir)                          \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DPREFIX="\"$(prefix)"\"		\
	-DSYSCONFDIR=\""$(sysconfdir)"\"        \
	-DDATADIR=\""$(datadir)"\"              \
	-DLIBDIR=\""$(libdir)"\"		\
	-DGLADEDIR=\"$(datadir)/gnomebaker\"	\
	-DIMAGEDIR=\"$(datadir)/gnomebaker\"	\
	-DMEDIADIR=\"$(datadir)/gnomebaker\"	\
	@GNOMEBAKER_CFLAGS@
	

bin_PROGRAMS = gnomebaker

gnomebaker_SOURCES =					\
	main.c						\
	gblibnotify.c gblibnotify.h \
	progressdlg.c		progressdlg.h		\
	burn.c			burn.h			\
	exec.c			exec.h			\
	devices.c		devices.h		\
	filebrowser.c		filebrowser.h		\
	preferences.c		preferences.h		\
	gnomebaker.c		gnomebaker.h		\
	startdlg.c		startdlg.h		\
	prefsdlg.c		prefsdlg.h		\
	splashdlg.c		splashdlg.h		\
	audiocd.c		audiocd.h		\
	datacd.c		datacd.h		\
	gbcommon.h		gbcommon.c		\
	selectdevicedlg.c 	selectdevicedlg.h	\
	execfunctions.c		execfunctions.h		\
	media.c			media.h		\
	project.c project.h			\
	dataproject.c dataproject.h \
	audioproject.c audioproject.h
	
gnomebaker_LDADD =  @GNOMEBAKER_LIBS@
