## Process this file with automake to produce Makefile.in
## Created by Anjuta - will be overwritten
## If you don't want it to overwrite it,
## 	Please disable it in the Anjuta project configuration

SUBDIRS = intl po macros src pixmaps help

gnomebakerdocdir = ${prefix}/doc/gnomebaker
gnomebakerdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS\
	TODO\
	xmldocs.make\
	omf.make


#gnomemenudir = $(prefix)/@NO_PREFIX_PACKAGE_MENU_DIR@/Applications
gnomemenudir = /usr/share/applications
gnomemenu_DATA = gnomebaker.desktop

##gnomebaker_glade_filedir = $(prefix)/@NO_PREFIX_PACKAGE_DATA_DIR@/@PACKAGE@/glade
gnomebaker_glade_filedir = $(prefix)/@NO_PREFIX_PACKAGE_DATA_DIR@/@PACKAGE@
gnomebaker_glade_file_DATA = gnomebaker.glade

EXTRA_DIST = $(gnomebakerdoc_DATA) $(gnomebaker_glade_file_DATA) $(gnomemenu_DATA)


# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done
