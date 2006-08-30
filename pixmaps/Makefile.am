gnomebaker_pixmapsdir = $(datadir)/gnomebaker

gnomebaker_pixmaps_DATA = \
	baker-add-dir.png\
	baker-add-files.png\
	baker-audio-copy.png\
	baker-blank-cdrw.png\
	baker-burn-cd.png\
	baker-cd.png\
	baker-data-copy.png\
	splash_2.png\
	baker-import-session.png\
	baker-blank-dvdrw.png\
	baker-cd-iso.png\
	baker-cue-image.png\
	baker-dvd-iso.png\
	baker-remove-files.png\
	gnomebaker-48.png\
	state00.png\
	state01.png\
	state02.png\
	state03.png\
	state04.png\
	state05.png\
	state06.png\
	state07.png\
	state08.png\
	state09.png\
	state10.png\
	state11.png\
	state12.png\
	state13.png

icondir =$(datadir)/icons/hicolor/48x48/apps
icon_DATA = gnomebaker-48.png

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = 				\
	$(gnomebaker_pixmaps_DATA)	\
	$(icon_DATA)
