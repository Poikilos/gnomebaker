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
 * File: gnomebaker.h
 * Created by: luke_biddell@yahoo.com
 * Created on: Tue Apr  6 23:28:51 2004
 */

#include <gnome.h>
#include "gnomebaker.h"
#include "filebrowser.h"
#include "audiocd.h"
#include "datacd.h"
#include "datadvd.h"
#include "burn.h"
#include "preferences.h"
#include "devices.h"
#include "prefsdlg.h"
#include "splashdlg.h"
#include "gbcommon.h"
#include <libgnomevfs/gnome-vfs-mime-utils.h>


GladeXML *xml = NULL;
gint datacdsize = 0;
gint audiocdsize = 0;
gint datadvdsize = 0;


void 
gnomebaker_on_toolbar_style_changed(GConfClient *client,
                                   guint cnxn_id,
                                   GConfEntry *entry,
                                   gpointer user_data)
{
	GB_LOG_FUNC
	GtkWidget* toptoolbar = glade_xml_get_widget(xml, widget_top_toolbar);
	gtk_toolbar_set_style(GTK_TOOLBAR(toptoolbar), preferences_get_toolbar_style());
	
	GtkWidget* middletoolbar = glade_xml_get_widget(xml, widget_middle_toolbar);	
	gtk_toolbar_set_style(GTK_TOOLBAR(middletoolbar), preferences_get_toolbar_style());	
}


GtkWidget* 
gnomebaker_new()
{
	GB_LOG_FUNC	
	
	splashdlg_set_text("Loading preferences...");
	preferences_init();
	
	splashdlg_set_text("Detecting devices...");
	devices_init();
		
	splashdlg_set_text("Loading GUI...");
	xml = glade_xml_new(glade_file, widget_gnomebaker, NULL);

	/* This is important */
	glade_xml_signal_autoconnect(xml);			

	/* set up the tree and lists */	
	GtkWidget *tree1 = glade_xml_get_widget(xml, widget_browser_dirtree);
	GtkWidget *tree2 = glade_xml_get_widget(xml, widget_browser_filelist);
	filebrowser_setup_tree_and_list(GTK_TREE_VIEW(tree1), GTK_TREE_VIEW(tree2));
	
	GtkWidget *tree4 = glade_xml_get_widget(xml, widget_datacd_tree);
	datacd_setup_list(GTK_TREE_VIEW(tree4));
				
	GtkWidget *tree8 = glade_xml_get_widget(xml, widget_audiocd_tree);
	audiocd_setup_list(GTK_TREE_VIEW(tree8));

	GtkWidget *tree12 = glade_xml_get_widget(xml, widget_datadvd_tree);
	datadvd_setup_list(GTK_TREE_VIEW(tree12));		
	
	/* get the currently selected cd size */
	gnomebaker_get_datacd_size();	
	
	/* Get and set the default toolbar style */
	gnomebaker_on_toolbar_style_changed(NULL, 0, NULL, NULL);
	preferences_register_notify(GNOME_TOOLBAR_STYLE, gnomebaker_on_toolbar_style_changed);
	
	g_main_context_iteration(NULL, TRUE);
	
	/*
	This is disabled so the DataDVD tab is shown. TODO: remove VideoCD tab.

	GtkWidget *notebook = glade_xml_get_widget(xml, widget_datacd_notebook);
	if(notebook != NULL)
		gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), -1);*/
	
	GtkWidget* gb = glade_xml_get_widget(xml, widget_gnomebaker);
	/*gtk_widget_hide(gb);
	gtk_widget_show_all(gb);*/
	return gb;
}


void
gnomebaker_delete(GtkWidget* self)
{
	GB_LOG_FUNC
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_free(xml);
}


gint
gnomebaker_show_msg_dlg(GtkMessageType type, GtkButtonsType buttons,
		  			GtkButtonsType additional, const gchar * message)
{
	GB_LOG_FUNC
	
	g_message( "MessageDialog message [%s]", message);		
	
	GtkWidget *dialog = gtk_message_dialog_new(
		GTK_WINDOW(glade_xml_get_widget(xml, widget_gnomebaker)), 
		GTK_DIALOG_DESTROY_WITH_PARENT, type, buttons, message);
	
	/*if(additional != GTK_BUTTONS_NONE)
		gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);*/

	gint result = gtk_dialog_run(GTK_DIALOG(dialog));	
	gtk_widget_destroy(dialog);

	return result;
}


void
gnomebaker_on_quit(GtkMenuItem * menuitem, gpointer user_data)
{
	GB_LOG_FUNC
	
	switch(gnomebaker_show_msg_dlg(GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
		 "Are you sure you want to quit?"))
	{
	case GTK_RESPONSE_OK:
	
		/* Clean out the temporary files directory if we're told to */		
		if(preferences_get_bool(GB_CLEANTEMPDIR))
		{
			gchar* copydataiso = preferences_get_copy_data_cd_image();
			gchar* createdataiso = preferences_get_create_data_cd_image();
			gchar* audiodir = preferences_get_convert_audio_track_dir();
			
			gchar* tmp = preferences_get_string(GB_TEMP_DIR);
			gchar* cmd = g_strdup_printf("rm -fr %s %s %s %s/gbtrack*", 
				copydataiso, createdataiso, audiodir, tmp);
			system(cmd);
			
			g_free(tmp);
			g_free(cmd);
			g_free(copydataiso);
			g_free(createdataiso);
			g_free(audiodir);
		}
		
		gtk_main_quit();
		break;
	case GTK_RESPONSE_CANCEL:
		break;
	default:
		break;
	}
}


void
gnomebaker_on_create_datacd(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	/* Here we should get a glist of data files to burn to the cd.
	 * or something like that */	
	GtkWidget *datatree = glade_xml_get_widget(xml, widget_datacd_tree);
	g_return_if_fail(datatree != NULL);
	
	GtkTreeModel* datamodel = gtk_tree_view_get_model(GTK_TREE_VIEW(datatree));
	g_return_if_fail(datamodel != NULL);

	burn_create_data_cd(datamodel);	
}


void
gnomebaker_on_create_audiocd(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	/* Here we should get a glist of data files to burn to the cd.
	 * or something like that */	
	GtkWidget *audiotree = glade_xml_get_widget(xml, widget_audiocd_tree);
	g_return_if_fail(audiotree != NULL);
	
	GtkTreeModel* audiomodel = gtk_tree_view_get_model(GTK_TREE_VIEW(audiotree));
	g_return_if_fail(audiomodel != NULL);
		
	burn_create_audio_cd(audiomodel);
}


void
gnomebaker_on_blank_cdrw(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC

	burn_blank_cdrw();	
}


void
gnomebaker_on_burn_iso(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
		
	GtkWidget *filesel = gtk_file_selection_new("Please select an iso file...");
	
	gtk_file_selection_set_select_multiple(GTK_FILE_SELECTION(filesel), FALSE);

	const gint result = gtk_dialog_run(GTK_DIALOG(filesel));
	const gchar *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));

	gtk_widget_destroy(filesel);
	
	/* Not using this at the moment as I'm not sure if it's backward compatible
	   with gtk < 2.4...!
	GtkWidget *filesel = gtk_file_chooser_dialog_new(
		"Please select an iso file...", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, NULL);
	
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filesel), FALSE);

	const gint result = gtk_dialog_run(GTK_DIALOG(filesel));
	const gchar *file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel));

	gtk_widget_destroy(filesel);
	*/

	if(result == GTK_RESPONSE_OK)
	{
		g_message( "file is %s", file);
		
		gchar* mime = gnome_vfs_get_mime_type(file);
		g_return_if_fail(mime != NULL);
		g_message("mime type is %s for %s", mime, file);
		
		/* Check that the mime type is iso */
		if(g_ascii_strcasecmp(mime, "application/x-cd-image") == 0)
		{
			burn_iso(file);
		}
		else
		{
			gnomebaker_show_msg_dlg(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE,
			  "The file you have selected is not a cd image. Please select a cd image to burn.");
		}
		
		g_free(mime);
	}
}


void
gnomebaker_on_preferences(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	GtkWidget *dlg = prefsdlg_new();
	gtk_dialog_run(GTK_DIALOG(dlg));
	prefsdlg_delete(dlg);
}


void 
gnomebaker_on_copy_datacd(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	burn_copy_data_cd();
}


void 
gnomebaker_on_copy_audiocd(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	burn_copy_audio_cd();
}


gboolean
gnomebaker_on_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	GB_LOG_FUNC
	gnomebaker_on_quit(NULL, NULL);
	return TRUE;
}


void
gnomebaker_on_about(GtkMenuItem * menuitem, gpointer user_data)
{
	GB_LOG_FUNC
	
	const gchar* authors[] = {"Luke Biddell", "Isak Savo", NULL};
	const gchar* documenters[] = {"Milen Dzhumerov", NULL};
	
	GtkWidget* about = gnome_about_new(_("GnomeBaker"), VERSION, "LGPL", 
		_("Simple CD Burning for Gnome"), authors, documenters, _(""), 
		gdk_pixbuf_new_from_file(PACKAGE_PIXMAPS_DIR"/splash_2.png", NULL));
	
	gtk_widget_show(about);	
}


GladeXML* 
gnomebaker_getxml()
{
	GB_LOG_FUNC
	return xml;
}


void 
gnomebaker_show_busy_cursor(gboolean isbusy)
{	
	GB_LOG_FUNC
	if(isbusy)
		gbcommon_start_busy_cursor1(xml, widget_gnomebaker);
	else
		gbcommon_end_busy_cursor1(xml, widget_gnomebaker);
}


gint 
gnomebaker_get_datacd_size()
{
	GB_LOG_FUNC
	GtkWidget* optmen = glade_xml_get_widget(xml, widget_datacd_size);
	g_return_val_if_fail(optmen != NULL, 0);
	
	if(gtk_option_menu_get_history(GTK_OPTION_MENU(optmen)) == 1)
		datacdsize = 650;
	else
		datacdsize = 700;
	
	return datacdsize;
}


void 
gnomebaker_on_datacd_size_changed(GtkOptionMenu *optionmenu, gpointer user_data)
{
	GB_LOG_FUNC
		
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_datacd_progressbar);
	g_return_if_fail(progbar != NULL);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));	
	gint previoussize = datacdsize;
	datacdsize = gnomebaker_get_datacd_size();
		
	fraction = (fraction * previoussize)/datacdsize;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);
	
	gchar* buf = g_strdup_printf("%d%%", (gint)(fraction * 100));
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
	g_free(buf);
}


void 
gnomebaker_on_audiocd_size_changed(GtkOptionMenu *optionmenu, gpointer user_data)
{
	GB_LOG_FUNC
		
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_audiocd_progressbar);
	g_return_if_fail(progbar != NULL);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));	
	gint previoussize = audiocdsize;
	audiocdsize = gnomebaker_get_audiocd_size();
		
	fraction = (fraction * previoussize)/audiocdsize;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);	
}


gint 
gnomebaker_get_audiocd_size()
{
	GB_LOG_FUNC
	GtkWidget* optmen = glade_xml_get_widget(xml, widget_audiocd_size);
	g_return_val_if_fail(optmen != NULL, 0);
	
	if(gtk_option_menu_get_history(GTK_OPTION_MENU(optmen)) == 1)
		audiocdsize = 74;
	else
		audiocdsize = 80;
	
	return audiocdsize;
}


void 
gnomebaker_on_add_dir(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkWidget *dirtree = glade_xml_get_widget(xml, widget_browser_dirtree);
	g_return_if_fail(dirtree != NULL);	
	
	GtkSelectionData* selection_data = g_new0(GtkSelectionData, 1);	
	filebrowser_on_drag_data_get(dirtree, NULL, selection_data, 0, 0, NULL);
	
	GtkWidget *notebook = glade_xml_get_widget(xml, widget_datacd_notebook);
	g_return_if_fail(notebook != NULL);	
	
	switch(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)))
	{
		case 0:
		{
			GtkWidget *datatree = glade_xml_get_widget(xml, widget_datacd_tree);
			g_return_if_fail(datatree != NULL);			
			
			datacd_on_drag_data_received(
				datatree, NULL, 0, 0, selection_data, 0, 0, NULL);
			break;
		}
		default:
		{
		}
	};	
	
	gtk_selection_data_free(selection_data);
}


void 
gnomebaker_on_add_files(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkWidget *filetree = glade_xml_get_widget(xml, widget_browser_filelist);
	g_return_if_fail(filetree != NULL);	
	
	GtkSelectionData* selection_data = g_new0(GtkSelectionData, 1);	
	filebrowser_on_drag_data_get(filetree, NULL, selection_data, 0, 0, NULL);
	
	GtkWidget *notebook = glade_xml_get_widget(xml, widget_datacd_notebook);
	g_return_if_fail(notebook != NULL);	
	
	switch(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)))
	{
		case 0:
		{
			GtkWidget *datatree = glade_xml_get_widget(xml, widget_datacd_tree);
			g_return_if_fail(datatree != NULL);			
			
			datacd_on_drag_data_received(
				datatree, NULL, 0, 0, selection_data, 0, 0, NULL);
			break;
		}
		case 1:
		{
			GtkWidget *audiotree = glade_xml_get_widget(xml, widget_audiocd_tree);
			g_return_if_fail(audiotree != NULL);			
				
			audiocd_on_drag_data_received(
				audiotree, NULL, 0, 0, selection_data, 0, 0, NULL);
			break;
		}
		default:
		{
		}
	};
	
	gtk_selection_data_free(selection_data);
}


void 
gnomebaker_on_remove(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkWidget *notebook = glade_xml_get_widget(xml, widget_datacd_notebook);
	g_return_if_fail(notebook != NULL);	
	
	switch(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)))
	{
		case 0:
		{			
			datacd_on_remove_clicked(NULL, NULL);		
			break;
		}
		case 1:
		{
			audiocd_on_remove_clicked(NULL, NULL);	
			break;
		}
		default:
		{
		}
	};
}


void 
gnomebaker_on_clear(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkWidget *notebook = glade_xml_get_widget(xml, widget_datacd_notebook);
	g_return_if_fail(notebook != NULL);	
	
	switch(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)))
	{
		case 0:			
		{
			datacd_on_clear_clicked(NULL, NULL);
			break;
		}
		case 1:
		{
			audiocd_on_clear_clicked(NULL, NULL);
			break;
		}		
		default:
		{
		}	
	};
}


void 
gnomebaker_update_status(const gchar* status)
{
	GB_LOG_FUNC
	GtkWidget *appbar = glade_xml_get_widget(xml, widget_appbar);
	g_return_if_fail(appbar != NULL);	
	
	if(status == NULL || strlen(status) == 0)
		gnome_appbar_pop(GNOME_APPBAR(appbar));
	else
		gnome_appbar_push(GNOME_APPBAR(appbar), status);	
}


void 
gnomebaker_enable_widget(const gchar* widgetname, gboolean enabled)
{
	GB_LOG_FUNC
	g_return_if_fail(widgetname != NULL);
	g_return_if_fail(xml != NULL);
	GtkWidget* widget = glade_xml_get_widget(xml, widgetname);
	g_return_if_fail(widget != NULL);	
	gtk_widget_set_sensitive(widget, enabled);
}


void 
gnomebaker_on_refresh(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	filebrowser_refresh();	
}


void 
gnomebaker_on_import(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkWidget *notebook = glade_xml_get_widget(xml, widget_datacd_notebook);
	g_return_if_fail(notebook != NULL);	
	
	switch(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)))
	{
		case 0:			
		{
			datacd_import_session();
			break;
		}
		case 1:
		{
			break;
		}		
		default:
		{
		}	
	};
}

gint 
gnomebaker_get_datadvd_size()
{
	GB_LOG_FUNC
	GtkWidget* optmen = glade_xml_get_widget(xml, widget_datadvd_size);
	g_return_val_if_fail(optmen != NULL, 0);
	
	if(gtk_option_menu_get_history(GTK_OPTION_MENU(optmen)) == 1)
		datadvdsize = 8500;
	else
		datadvdsize = 4700;
	
	g_message("Returning datadvdsize %d",datadvdsize);
	
	return datadvdsize;
}

void 
gnomebaker_on_datadvd_size_changed(GtkOptionMenu *optionmenu, gpointer user_data)
{
	GB_LOG_FUNC
		
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_datadvd_progressbar);
	g_return_if_fail(progbar != NULL);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));	
	gint previoussize = datadvdsize;
	datadvdsize = gnomebaker_get_datadvd_size();
		
	fraction = (fraction * previoussize)/datadvdsize;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);
	
	gchar* buf = g_strdup_printf("%d%%", (gint)(fraction * 100));
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
	g_free(buf);
}

void gnomebaker_on_format_dvdrw(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	burn_format_dvdrw();	
}

void
gnomebaker_on_create_datadvd(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkWidget *datatree = glade_xml_get_widget(xml, widget_datadvd_tree);
	g_return_if_fail(datatree != NULL);
	
	GtkTreeModel* datamodel = gtk_tree_view_get_model(GTK_TREE_VIEW(datatree));
	g_return_if_fail(datamodel != NULL);

	burn_create_data_dvd(datamodel);	
}

