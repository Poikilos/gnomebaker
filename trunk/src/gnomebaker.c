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

#include "gnomebaker.h"
#include <gnome.h>
#include "filebrowser.h"
#include "audiocd.h"
#include "datacd.h"
#include "burn.h"
#include "preferences.h"
#include "devices.h"
#include "prefsdlg.h"
#include "splashdlg.h"
#include "gbcommon.h"
#include <libgnomevfs/gnome-vfs-mime-utils.h>


GladeXML *xml = NULL;


void 
gnomebaker_on_show_file_browser(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(checkmenuitem != NULL);
	
	gboolean show = gtk_check_menu_item_get_active(checkmenuitem);
	preferences_set_bool(GB_SHOW_FILE_BROWSER, show);
				
	gtk_widget_set_sensitive(glade_xml_get_widget(xml, widget_refresh_menu), show);
	gtk_widget_set_sensitive(glade_xml_get_widget(xml, widget_refresh_button), show);
	gtk_widget_set_sensitive(glade_xml_get_widget(xml, widget_add_button), show);
	
	GtkWidget* hpaned3 = glade_xml_get_widget(xml, widget_browser_hpane);	
	if(show) gtk_widget_show(hpaned3);
	else gtk_widget_hide(hpaned3);
}


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
	
	splashdlg_set_text(_("Loading preferences..."));
	preferences_init();
	
	splashdlg_set_text(_("Detecting devices..."));
	devices_init();
		
	splashdlg_set_text(_("Loading GUI..."));
	xml = glade_xml_new(glade_file, widget_gnomebaker, NULL);

	/* This is important */
	glade_xml_signal_autoconnect(xml);			

	/* set up the tree and lists */	
	filebrowser_new();	
	datacd_new();
	audiocd_new();
	
	/* Get and set the default toolbar style */
	gnomebaker_on_toolbar_style_changed(NULL, 0, NULL, NULL);
	preferences_register_notify(GNOME_TOOLBAR_STYLE, gnomebaker_on_toolbar_style_changed);
	
	g_main_context_iteration(NULL, TRUE);
	
	GtkWidget *notebook = glade_xml_get_widget(xml, widget_datacd_notebook);
	if(notebook != NULL)
		gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), -1);
	
	GtkWidget* checkmenuitem = glade_xml_get_widget(xml, widget_show_browser_menu);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(checkmenuitem),
		preferences_get_bool(GB_SHOW_FILE_BROWSER));
	g_signal_emit_by_name(checkmenuitem, "toggled", checkmenuitem, NULL);
	
	return glade_xml_get_widget(xml, widget_gnomebaker);
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
	
	g_message( _("MessageDialog message [%s]"), message);		
	
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
		 _("Are you sure you want to quit?")))
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
gnomebaker_on_blank_cdrw(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC

	burn_blank_cdrw();	
}


void
gnomebaker_on_burn_iso(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
		
	GtkWidget *filesel = gtk_file_chooser_dialog_new(
		_("Please select an iso file..."), NULL, GTK_FILE_CHOOSER_ACTION_OPEN, 
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filesel), FALSE);

	const gint result = gtk_dialog_run(GTK_DIALOG(filesel));
	const gchar *file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel));

	gtk_widget_destroy(filesel);

	if(result == GTK_RESPONSE_OK)
	{
		g_message( _("file is %s"), file);
		
		gchar* mime = gnome_vfs_get_mime_type(file);
		g_return_if_fail(mime != NULL);
		g_message(_("mime type is %s for %s"), mime, file);
		
		/* Check that the mime type is iso */
		if(g_ascii_strcasecmp(mime, "application/x-cd-image") == 0)
		{
			burn_iso(file);
		}
		else
		{
			gnomebaker_show_msg_dlg(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE,
			  _("The file you have selected is not a cd image. Please select a cd image to burn."));
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
	
	const gchar* authors[] = {"Luke Biddell", "Isak Savo", "Christoffer Sørensen", NULL};
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


void 
gnomebaker_on_add_dir(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkSelectionData* selection_data = filebrowser_get_selection(TRUE);
	GtkWidget *notebook = glade_xml_get_widget(xml, widget_datacd_notebook);
	switch(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)))
	{
		case 0:
			datacd_add_selection(selection_data);
			break;
		default:{}
	};	
	
	gtk_selection_data_free(selection_data);
}


void 
gnomebaker_on_add_files(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkSelectionData* selection_data = filebrowser_get_selection(FALSE);
	GtkWidget *notebook = glade_xml_get_widget(xml, widget_datacd_notebook);	
	switch(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)))
	{
		case 0:
			datacd_add_selection(selection_data);
			break;
		case 1:
			audiocd_add_selection(selection_data);
			break;
		default:{}
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
			datacd_remove();
			break;
		case 1:
			audiocd_remove();
			break;
		default:{}
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
			datacd_clear();
			break;
		case 1:
			audiocd_clear();
			break;
		default:{}
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
			datacd_import_session();
			break;
		case 1:
			audiocd_import_session();
			break;
		default:{}	
	};
}


void 
gnomebaker_on_format_dvdrw(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	burn_format_dvdrw();	
}
