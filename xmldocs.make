# ************* Begin of section some packagers may need to modify  **************
# This variable (docdir) specifies where the documents should be installed.
# This default value should work for most packages.
docdir = $(datadir)/@PACKAGE@/doc/$(docname)/$(lang)
#docdir = $(datadir)/gnome/help/$(docname)/$(lang)

# **************  You should not have to edit below this line  *******************
# ** GSt did ... 
#   more C docs was added

xml_files = $(entities) $(docname).xml
# $(more_C_docs)
EXTRA_DIST = $(xml_files) $(omffile) 
CLEANFILES = omf_timestamp

# If the following file is in a subdir (like help/) you need to add that to the path
include $(top_srcdir)/omf.make

all: omf

$(docname).xml: $(entities)
	-ourdir=`pwd`;  \
	cd $(srcdir);   \
	cp $(entities) $$ourdir

app-dist-hook:
	if test "$(figdir)"; then \
	  $(mkinstalldirs) $(distdir)/$(figdir); \
	  for file in $(srcdir)/$(figdir)/*.png; do \
	    basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	    $(INSTALL_DATA) $$file $(distdir)/$(figdir)/$$basefile; \
	  done \
	fi

install-data-local: omf
	$(mkinstalldirs) $(DESTDIR)$(docdir)
	for file in $(xml_files); do \
	  cp $(srcdir)/$$file $(DESTDIR)$(docdir); \
	done
	if test "$(figdir)"; then \
	  $(mkinstalldirs) $(DESTDIR)$(docdir)/$(figdir); \
	  for file in $(srcdir)/$(figdir)/*.png; do \
	    basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	    $(INSTALL_DATA) $$file $(DESTDIR)$(docdir)/$(figdir)/$$basefile; \
	  done \
	fi

install-data-hook: install-data-hook-omf

uninstall-local: uninstall-local-doc uninstall-local-omf

uninstall-local-doc:
	-if test "$(figdir)"; then \
	  for file in $(srcdir)/$(figdir)/*.png; do \
	    basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	    rm -f $(docdir)/$(figdir)/$$basefile; \
	  done; \
	  rmdir $(DESTDIR)$(docdir)/$(figdir); \
	fi
	-for file in $(xml_files); do \
	  rm -f $(DESTDIR)$(docdir)/$$file; \
	done
	-rmdir $(DESTDIR)$(docdir)
