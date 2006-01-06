SUBDIRS = po src pixmaps media help

desktop_in_files=gnomebaker.desktop.in
desktop_files=$(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

Applicationsdir = $(datadir)/applications
Applications_DATA = $(desktop_files)

gladedir = $(datadir)/gnomebaker
glade_DATA = gnomebaker.glade

INTLTOOL = 			\
	intltool-extract.in 	\
	intltool-merge.in 	\
	intltool-update.in

EXTRA_DIST =				\
	AUTHORS				\
	README				\
	ChangeLog			\
	TODO				\
	autogen.sh			\
	xmldocs.make			\
	omf.make			\
	$(desktop_in_files)		\
	$(desktop_files)		\
	$(glade_DATA)			\
	$(INTLTOOL)

DISTCLEANFILES = 		\
	intltool-extract	\
	intltool-merge		\
	intltool-update		\
	$(desktop_files)
