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
#include <gst/gst.h>
#include "gblibnotify.h"

const gchar* glade_file;
gboolean showtrace = FALSE;

static GOptionEntry entries[] = 
{
    { "trace-on", 0, 0, G_OPTION_ARG_NONE, &showtrace, N_("Show tracing information (useful when debugging)"), 0 },
    { NULL }
};


gint 
main(gint argc, gchar *argv[])
{	    	
	GError* error = NULL;
	GOptionContext* context = g_option_context_new(_(" - GNOME CD/DVD burning application"));
	/* add main entries */
	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
	/* recognise gtk/gdk/gstreamer options */
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
    /*g_option_context_add_group(context, gst_init_get_option_group());*/
	/* ignore unknown options */
	g_option_context_set_ignore_unknown_options(context, TRUE);
	g_option_context_parse(context, &argc, &argv, &error);
	g_option_context_free(context);
    if(error != NULL) g_error_free(error);   
    
    gblibnotify_init("GnomeBaker");
    
    g_thread_init(NULL);
	gdk_threads_init();
	
#ifdef ENABLE_NLS
		setlocale(LC_ALL,"");		
		bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
		textdomain (GETTEXT_PACKAGE);
#endif
	
	struct poptOption options[] = 
	{
    	{NULL, '\0', POPT_ARG_INCLUDE_TABLE, NULL, 0, "GStreamer", NULL},
    	POPT_TABLEEND
	};

	/* init GStreamer and GNOME using the GStreamer popt tables */
	options[0].arg = (void *) gst_init_get_popt_table ();	

    GnomeProgram* prog = gnome_program_init ("gnomebaker", PACKAGE_VERSION, LIBGNOMEUI_MODULE, argc, argv, 
        GNOME_PARAM_POPT_TABLE, options, GNOME_PARAM_APP_DATADIR, DATADIR, NULL);
	glade_gnome_init();
	
	glade_file = gnome_program_locate_file(NULL, GNOME_FILE_DOMAIN_APP_DATADIR,
					"gnomebaker/gnomebaker.glade", FALSE, NULL);                

#if !defined(__linux__)
	GtkWidget* dlg = splashdlg_new();		
#endif

	while(g_main_context_pending(NULL))
		g_main_context_iteration(NULL, TRUE);	
	GtkWidget* app = gnomebaker_new();
    
#if !defined(__linux__)    
	splashdlg_delete(dlg);
#endif
	
    gdk_threads_enter();	
	gtk_main();
	gdk_threads_leave();
    gnomebaker_delete(app);
    g_object_unref(prog);
    prog = NULL;
    if(gblibnotify_clear())
    	GB_TRACE("Global notification closed");
	return 0;
}



