/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 /*
 * File: burn.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Tue Jan 28 22:12:09 2003
 */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gnomebaker.h"
#include <gnome.h>
#include <glade/glade.h>
#include "splashdlg.h"
#include "gbcommon.h"
#include <libintl.h>
#include <locale.h>

const gchar* glade_file;
gboolean showtrace = FALSE;

gint 
main(gint argc, gchar *argv[])
{	
	gint i = 0;	
	for(; i < argc; ++i)
	{
		if(g_ascii_strcasecmp("--trace-on", argv[i]) == 0)
		{
			showtrace = TRUE;
			break;
		}
	}
	
	g_thread_init(NULL);
	gdk_threads_init();

	#ifdef ENABLE_NLS
		setlocale(LC_ALL,"");		
		bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
		textdomain (GETTEXT_PACKAGE);
	#endif
	
	/*gnome_init(PACKAGE, VERSION, argc, argv);*/
	gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                      argc, argv,
                      GNOME_PARAM_APP_DATADIR, PACKAGE_DATA_DIR,
                      NULL);


	glade_gnome_init();
	
	glade_file = gnome_program_locate_file(
		NULL,
		GNOME_FILE_DOMAIN_APP_DATADIR,
		"gnomebaker/gnomebaker.glade",
		FALSE,
		NULL);
		
	/*gtk_window_set_auto_startup_notification(TRUE);*/

	GtkWidget* dlg = splashdlg_new();
		
	while(g_main_context_pending(NULL))
		g_main_context_iteration(NULL, TRUE);
	
	GtkWidget* app = gnomebaker_new();

	splashdlg_delete(dlg);
	
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	
	gnomebaker_delete(app);
	
	return 0;
}
