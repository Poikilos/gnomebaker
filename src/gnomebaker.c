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
 * Copyright: luke_biddell@yahoo.com
 * Created on: Tue Apr  6 23:28:51 2004
 */
 
/* This is for bonobo_dock_item_set_locked */
#define BONOBO_UI_INTERNAL

#include "gnomebaker.h"
#include "filebrowser.h"
#include "burn.h"
#include "preferences.h"
#include "devices.h"
#include "prefsdlg.h"
#include "splashdlg.h"
#include "gbcommon.h"
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <gst/gst.h>
#include "media.h"
#include "dataproject.h"
#include "audioproject.h"


static const gchar *const widget_gnomebaker = "GnomeBaker";
static const gchar *const widget_project_notebook = "notebook1";
static const gchar *const widget_appbar = "appbar1";
static const gchar *const widget_menu_import = "import_session1";
static const gchar *const widget_up = "toolbutton8";
static const gchar *const widget_up_alt = "toolbutton15";
static const gchar *const widget_menu_up = "move_selection_up1";
static const gchar *const widget_down = "toolbutton9";
static const gchar *const widget_down_alt = "toolbutton16";
static const gchar *const widget_menu_down = "move_selection_down1";
static const gchar *const widget_top_toolbar_dock = "bonobodockitem4";
static const gchar *const widget_top_toolbar = "toolbar3";
static const gchar *const widget_middle_toolbar = "toolbar4";
static const gchar *const widget_show_browser_menu = "show_file_browser1";
static const gchar *const widget_show_hidden_files = "show_hidden_files1";
static const gchar *const widget_show_human_sizes = "show_human-readable_file_sizes1";
static const gchar *const widget_browser_hpane = "hpaned3";
static const gchar *const widget_add_button = "buttonAddFiles";
static const gchar *const widget_refresh_menu = "refresh1";
static const gchar *const widget_refresh_button = "toolbutton4";
static const gchar *const widget_add_button_alt = "toolbutton12";
static const gchar *const widget_remove_button_alt = "toolbutton13";
static const gchar *const widget_clear_button_alt = "toolbutton14";


/* Comment this out to use gb's internal file browser rather than the standard gtk widget */
/*#define USE_GTK_FILE_CHOOSER 1*/

static GladeXML *xml = NULL;

#ifdef USE_GTK_FILE_CHOOSER
static GtkWidget *file_chooser = NULL;
static GtkFileFilter *audio_filter = NULL;
#endif


void /* libglade callback */
gnomebaker_on_show_human_readable_file_sizes(GtkCheckMenuItem *check_menu_item, gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(check_menu_item != NULL);
    
    gboolean show = gtk_check_menu_item_get_active(check_menu_item);
    preferences_set_bool(GB_SHOWHUMANSIZE, show);    
}


void /* libglade callback */
gnomebaker_on_show_hidden_files(GtkCheckMenuItem *check_menu_item, gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(check_menu_item != NULL);
    
    gboolean show = gtk_check_menu_item_get_active(check_menu_item);
    preferences_set_bool(GB_SHOWHIDDEN, show);    
}


void /* libglade callback */
gnomebaker_on_show_file_browser(GtkCheckMenuItem *check_menu_item, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(check_menu_item != NULL);
	
	gboolean show = gtk_check_menu_item_get_active(check_menu_item);
	preferences_set_bool(GB_SHOW_FILE_BROWSER, show);
    
    if(show) gtk_widget_show(glade_xml_get_widget(xml, widget_middle_toolbar));
    else gtk_widget_hide(glade_xml_get_widget(xml, widget_middle_toolbar));
    
    if(!show) gtk_widget_show(glade_xml_get_widget(xml, "separatortoolitem8"));
    else gtk_widget_hide(glade_xml_get_widget(xml, "separatortoolitem8"));
    
    if(!show) gtk_widget_show(glade_xml_get_widget(xml, widget_add_button_alt));
    else gtk_widget_hide(glade_xml_get_widget(xml, widget_add_button_alt));
    
    if(!show) gtk_widget_show(glade_xml_get_widget(xml, widget_remove_button_alt));
    else gtk_widget_hide(glade_xml_get_widget(xml, widget_remove_button_alt));
    
    if(!show) gtk_widget_show(glade_xml_get_widget(xml, widget_clear_button_alt));
    else gtk_widget_hide(glade_xml_get_widget(xml, widget_clear_button_alt));
    
    if(!show) gtk_widget_show(glade_xml_get_widget(xml, "separatortoolitem9"));
    else gtk_widget_hide(glade_xml_get_widget(xml, "separatortoolitem9"));
    
    if(!show) gtk_widget_show(glade_xml_get_widget(xml, widget_up_alt));
    else gtk_widget_hide(glade_xml_get_widget(xml, widget_up_alt));
    
    if(!show) gtk_widget_show(glade_xml_get_widget(xml, widget_down_alt));
    else gtk_widget_hide(glade_xml_get_widget(xml, widget_down_alt));
				
	gtk_widget_set_sensitive(glade_xml_get_widget(xml, widget_refresh_menu), show);
	gtk_widget_set_sensitive(glade_xml_get_widget(xml, widget_refresh_button), show);	
#ifndef USE_GTK_FILE_CHOOSER       	
	GtkWidget *hpaned3 = glade_xml_get_widget(xml, widget_browser_hpane);	
	if(show) gtk_widget_show(hpaned3);
	else gtk_widget_hide(hpaned3);
#else
    if(show) gtk_widget_show(file_chooser);
    else gtk_widget_hide(file_chooser);
#endif    
}


static void 
gnomebaker_on_toolbar_style_changed(GConfClient *client,
                                   guint cnxn_id,
                                   GConfEntry *entry,
                                   gpointer user_data)
{
	GB_LOG_FUNC
	GtkWidget *top_toolbar = glade_xml_get_widget(xml, widget_top_toolbar);
	gtk_toolbar_set_style(GTK_TOOLBAR(top_toolbar), preferences_get_toolbar_style());
	
	GtkWidget *middle_toolbar = glade_xml_get_widget(xml, widget_middle_toolbar);	
	gtk_toolbar_set_style(GTK_TOOLBAR(middle_toolbar), preferences_get_toolbar_style());

	GtkWidget *top_toolbar_dock = glade_xml_get_widget(xml, widget_top_toolbar_dock);
	bonobo_dock_item_set_locked(BONOBO_DOCK_ITEM(top_toolbar_dock),
		!preferences_get_bool(GNOME_TOOLBAR_DETACHABLE));
}


#ifdef USE_GTK_FILE_CHOOSER
static gboolean
gnomebaker_audio_file_filter(const GtkFileFilterInfo *filter_info, gpointer data)
{
    GB_LOG_FUNC
    return (media_get_plugin_status(filter_info->mime_type) == INSTALLED);
}
#endif


static Project*
gnomebaker_get_current_project()
{
    GB_LOG_FUNC
    GtkNotebook *notebook = GTK_NOTEBOOK(glade_xml_get_widget(xml, widget_project_notebook));
    g_return_if_fail(notebook != NULL); 
    Project *project = PROJECT_WIDGET(gtk_notebook_get_nth_page(notebook,
            gtk_notebook_get_current_page(notebook)));
    return project;
}            


GtkWidget* 
gnomebaker_new()
{
	GB_LOG_FUNC	
    
    gbcommon_init();
	
	splashdlg_set_text(_("Loading preferences..."));
	preferences_init();
	
	splashdlg_set_text(_("Detecting devices..."));
	devices_init();

	splashdlg_set_text(_("Registering gstreamer plugins..."));
	media_init();

	splashdlg_set_text(_("Loading GUI..."));
	xml = glade_xml_new(glade_file, widget_gnomebaker, NULL);
    
	/* This is important */
	glade_xml_signal_autoconnect(xml);			

	/* set up the tree and lists */	
#ifndef USE_GTK_FILE_CHOOSER             
	filebrowser_new();
#else     
    file_chooser = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(file_chooser), TRUE);
    gtk_widget_show(file_chooser);
    GtkWidget *vpane = glade_xml_get_widget(xml, "vpaned1");
    GtkWidget *hpaned3 = glade_xml_get_widget(xml, widget_browser_hpane);   
    GtkWidget *tabs = glade_xml_get_widget(xml, "vbox18");
    g_object_ref(tabs);
    gtk_container_remove(GTK_CONTAINER(vpane), hpaned3);
    gtk_container_remove(GTK_CONTAINER(vpane), tabs);            
    gtk_paned_pack1(GTK_PANED(vpane), file_chooser, TRUE, TRUE);
    gtk_paned_pack2(GTK_PANED(vpane), tabs, TRUE, TRUE);
    
    audio_filter = gtk_file_filter_new();
    gtk_file_filter_add_custom(audio_filter, GTK_FILE_FILTER_MIME_TYPE,
        gnomebaker_audio_file_filter, NULL, NULL);
    gtk_file_filter_set_name(audio_filter,_("Audio files"));
    
    gtk_widget_hide(glade_xml_get_widget(xml, "separator4"));
    gtk_widget_hide(glade_xml_get_widget(xml, widget_refresh_menu));
    gtk_widget_hide(glade_xml_get_widget(xml, widget_refresh_button));   
    
#endif    
        
	/* Get and set the default toolbar style */
	gnomebaker_on_toolbar_style_changed(NULL, 0, NULL, NULL);
	preferences_register_notify(GNOME_TOOLBAR_STYLE, gnomebaker_on_toolbar_style_changed);
	preferences_register_notify(GNOME_TOOLBAR_DETACHABLE, gnomebaker_on_toolbar_style_changed);
	
	/* Resize and move the window to saved settings */
	GtkWidget *main_window = glade_xml_get_widget(xml, widget_gnomebaker);
	const gint x = preferences_get_int(GB_MAIN_WINDOW_POSITION_X);
	const gint y = preferences_get_int(GB_MAIN_WINDOW_POSITION_Y);
	const gint width = preferences_get_int(GB_MAIN_WINDOW_WIDTH);
	const gint height = preferences_get_int(GB_MAIN_WINDOW_HEIGHT);

	if((x > 0) && (y > 0))
		gtk_window_move(GTK_WINDOW(main_window), x, y);
	if((width > 0) && (height > 0))
		gtk_window_resize(GTK_WINDOW(main_window), width, height);
	if(preferences_get_bool(GB_MAIN_WINDOW_MAXIMIZED) == TRUE)
		gtk_window_maximize(GTK_WINDOW(main_window));

	g_main_context_iteration(NULL, TRUE);
	
	GtkWidget *notebook = glade_xml_get_widget(xml, widget_project_notebook);
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), -1);
    gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), -1);
    /*datacd_new();*/
    
    gtk_widget_show_all(main_window);
	
    /* Check preferences to see if we'll show/hide the file browser */
	GtkWidget *check_menu_item = glade_xml_get_widget(xml, widget_show_browser_menu);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_menu_item),
		preferences_get_bool(GB_SHOW_FILE_BROWSER));
	g_signal_emit_by_name(check_menu_item, "toggled", check_menu_item, NULL);	

    check_menu_item = glade_xml_get_widget(xml, widget_show_hidden_files);
#ifdef USE_GTK_FILE_CHOOSER
    gtk_widget_hide(check_menu_item);
#else        
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_menu_item),
        preferences_get_bool(GB_SHOWHIDDEN));
    g_signal_emit_by_name(check_menu_item, "toggled", check_menu_item, NULL);   
#endif    
    
    check_menu_item = glade_xml_get_widget(xml, widget_show_human_sizes);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_menu_item),
        preferences_get_bool(GB_SHOWHUMANSIZE));
    g_signal_emit_by_name(check_menu_item, "toggled", check_menu_item, NULL);   
    
	return main_window;
}


void
gnomebaker_delete(GtkWidget *self)
{
	GB_LOG_FUNC
    media_finalise();
    preferences_finalise();
    gb_common_finalise();
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_object_unref(xml);
}


gint
gnomebaker_show_msg_dlg(GtkWindow *parent, GtkMessageType type, 
    GtkButtonsType buttons, GtkButtonsType additional, const gchar  *message)
{
	GB_LOG_FUNC
	GB_TRACE("gnomebaker_show_msg_dlg - message [%s]\n", message);		
	
	GtkWidget *dialog = gtk_message_dialog_new(
		parent == NULL ? GTK_WINDOW(glade_xml_get_widget(xml, widget_gnomebaker)) : parent, 
		GTK_DIALOG_DESTROY_WITH_PARENT, type, buttons, message);
	
	/*if(additional != GTK_BUTTONS_NONE)
		gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);*/

	gint result = gtk_dialog_run(GTK_DIALOG(dialog));	
	gtk_widget_destroy(dialog);

	return result;
}


void /* libglade callback */
gnomebaker_on_quit(GtkMenuItem *menu_item, gpointer user_data)
{
	GB_LOG_FUNC
	
	gint response = GTK_RESPONSE_OK;	
	if(preferences_get_bool(GB_ASK_ON_QUIT))
	{
        response = gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
		 _("Are you sure you want to quit?"));
    }
    switch(response)
    {
    case GTK_RESPONSE_OK:
    
        /* Clean out the temporary files directory if we're told to */		
        if(preferences_get_bool(GB_CLEANTEMPDIR))
        {
            gchar *copy_data_iso = preferences_get_copy_data_cd_image();
            gchar *create_data_iso = preferences_get_create_data_cd_image();
            gchar *audio_dir = preferences_get_convert_audio_track_dir();
            
            gchar *tmp = preferences_get_string(GB_TEMP_DIR);
            gchar *cmd = g_strdup_printf("rm -fr %s %s %s %s/gbtrack*", 
                copy_data_iso, create_data_iso, audio_dir, tmp);
            system(cmd);
            
            g_free(tmp);
            g_free(cmd);
            g_free(copy_data_iso);
            g_free(create_data_iso);
            g_free(audio_dir);
        }
        
        /* Save main window position and size if not maximized */
    	if(preferences_get_bool(GB_MAIN_WINDOW_MAXIMIZED) != TRUE)
    	{
            GtkWidget *main_window = glade_xml_get_widget(xml, widget_gnomebaker);
    
        	gint width, height, x, y;
            gtk_window_get_size(GTK_WINDOW(main_window), &width, &height);
            gtk_window_get_position(GTK_WINDOW(main_window), &x, &y);
    
            preferences_set_int(GB_MAIN_WINDOW_WIDTH, width);
            preferences_set_int(GB_MAIN_WINDOW_HEIGHT, height);
            preferences_set_int(GB_MAIN_WINDOW_POSITION_X, x);
            preferences_set_int(GB_MAIN_WINDOW_POSITION_Y, y);
    	}
        
        gtk_main_quit();
        break;
    case GTK_RESPONSE_CANCEL:
        break;
    default:
        break;
    }	
}


void /* libglade callback */
gnomebaker_on_blank_cdrw(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	burn_blank_cdrw();	
}


static gboolean
gnomebaker_cd_image_file_filter(const GtkFileFilterInfo *filter_info, gpointer data)
{
    GB_LOG_FUNC
    if(gbcommon_str_has_suffix(filter_info->filename, ".cue") || 
            gbcommon_str_has_suffix(filter_info->filename, ".toc") || 
            (g_ascii_strcasecmp(filter_info->mime_type, "application/x-cd-image") == 0))
        return TRUE;
    return FALSE;
}


void /* libglade callback */
gnomebaker_on_burn_iso(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
    GtkFileFilter *image_filter = gtk_file_filter_new();
    gtk_file_filter_add_custom(image_filter, GTK_FILE_FILTER_FILENAME | GTK_FILE_FILTER_MIME_TYPE,
        gnomebaker_cd_image_file_filter, NULL, NULL);
    gtk_file_filter_set_name(image_filter,_("CD Image files"));
	gchar *file = gbcommon_show_file_chooser(_("Please select a CD image file."), 
        GTK_FILE_CHOOSER_ACTION_OPEN, image_filter, TRUE, NULL);
	if(file != NULL)
		burn_cd_image_file(file);
    g_free(file);
}


void /* libglade callback */
gnomebaker_on_burn_dvd_iso(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
    GtkFileFilter *image_filter = gtk_file_filter_new();
    gtk_file_filter_add_custom(image_filter, GTK_FILE_FILTER_MIME_TYPE,
        gbcommon_iso_file_filter, NULL, NULL);
    gtk_file_filter_set_name(image_filter,_("DVD Image files"));
	gchar *file = gbcommon_show_file_chooser(_("Please select a DVD image file."), 
        GTK_FILE_CHOOSER_ACTION_OPEN, image_filter, TRUE, NULL);
	if(file != NULL)
        burn_dvd_iso(file);
    g_free(file);
}


void /* libglade callback */
gnomebaker_on_preferences(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	GtkWidget *dlg = prefsdlg_new();
	gtk_dialog_run(GTK_DIALOG(dlg));
	prefsdlg_delete(dlg);
}


void /* libglade callback */
gnomebaker_on_copy_datacd(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	burn_copy_data_cd();
}


void /* libglade callback */
gnomebaker_on_copy_audiocd(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	burn_copy_audio_cd();
}


gboolean
gnomebaker_on_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	GB_LOG_FUNC
	gnomebaker_on_quit(NULL, NULL);
	return TRUE;
}


gboolean
gnomebaker_window_state_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	GB_LOG_FUNC
	if((event->type) == (GDK_WINDOW_STATE))
	{
		if((((GdkEventWindowState*)event)->new_window_state) & GDK_WINDOW_STATE_MAXIMIZED)
		    preferences_set_bool(GB_MAIN_WINDOW_MAXIMIZED, TRUE);
		else
		    preferences_set_bool(GB_MAIN_WINDOW_MAXIMIZED, FALSE);
	}
	return FALSE;
}


void /* libglade callback */
gnomebaker_on_about(GtkMenuItem *menu_item, gpointer user_data)
{
	GB_LOG_FUNC
	static GtkWidget *about = NULL;
	if (about != NULL)
    {
		gtk_window_present(GTK_WINDOW(about));
    }
	else
	{
		const gchar *authors[] = {"Luke Biddell", "Ignacio Martín", "Milen Dzhumerov", 
            "Christoffer Sørensen", "Razvan Gavril", "Isak Savo", "Adam Biddell (Sounds)", NULL};
		const gchar *documenters[] = {"Milen Dzhumerov", NULL};
		about = gnome_about_new(_("GnomeBaker"), PACKAGE_VERSION, "GPL", 
			_("Simple CD/DVD Burning for Gnome"), authors, documenters, _("translator_credits"), 
			gdk_pixbuf_new_from_file(IMAGEDIR"/splash_2.png", NULL));
		g_object_add_weak_pointer(G_OBJECT(about), (void**)&about);
		gtk_widget_show(about);	
	}
}


GladeXML* 
gnomebaker_getxml()
{
	GB_LOG_FUNC
	return xml;
}


void 
gnomebaker_show_busy_cursor(gboolean is_busy)
{	
	GB_LOG_FUNC
	if(is_busy)
		gbcommon_start_busy_cursor1(xml, widget_gnomebaker);
	else
		gbcommon_end_busy_cursor1(xml, widget_gnomebaker);
}


void /* libglade callback */
gnomebaker_on_add_dir(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC
    
    GtkSelectionData *selection_data = filebrowser_get_selection(TRUE);
    project_add_selection(gnomebaker_get_current_project(), selection_data);        
    gtk_selection_data_free(selection_data);
}


void /* libglade callback */
gnomebaker_on_add_files(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
    
    GtkSelectionData *selection_data = NULL;
    
    /* If we're not showing the file browser we popup an file selector dialog when
    we press + */
    if(preferences_get_bool(GB_SHOW_FILE_BROWSER))
    {
#ifndef USE_GTK_FILE_CHOOSER       
        selection_data = filebrowser_get_selection(FALSE);        
#else
        GSList *file = gtk_file_chooser_get_uris(GTK_FILE_CHOOSER(file_chooser));
        int index = 0;
        GString *text = g_string_new("");
        for(; file != NULL; file = file->next)
        {
            /* hand ownership of the data to the gchar** which we free later */
            GB_TRACE("gnomebaker_on_add_files - index [%d] file [%s]\n", index, (gchar*)file->data);
            g_string_append(text, (gchar*)file->data);
            g_string_append(text, "\n");
            g_free((gchar*)file->data);
            /*uris[index++] = (gchar*)file->data;*/
        }
        
        /*gtk_selection_data_set_uris(selection_data, uris);*/
        selection_data = g_new0(GtkSelectionData, 1);                    
        gtk_selection_data_set(selection_data, selection_data->target, 8, 
            (const guchar*)text->str, strlen(text->str) * sizeof(gchar));
        GB_TRACE("gnomebaker_on_add_files - [%s]\n", selection_data->data);
        g_slist_free(file);
        g_string_free(text, TRUE);
#endif
        if(selection_data != NULL)
        {
            project_add_selection(gnomebaker_get_current_project(), selection_data);        
            gtk_selection_data_free(selection_data);
        }
    }        
}


void /* libglade callback */
gnomebaker_on_remove(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC	
	project_remove(gnomebaker_get_current_project());
}


void /* libglade callback */
gnomebaker_on_clear(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
    project_clear(gnomebaker_get_current_project());
}


void 
gnomebaker_update_status(const gchar *status)
{
	GB_LOG_FUNC
	GtkWidget *app_bar = glade_xml_get_widget(xml, widget_appbar);
	g_return_if_fail(app_bar != NULL);	
	
	if(status == NULL || strlen(status) == 0)
		gnome_appbar_pop(GNOME_APPBAR(app_bar));
	else
		gnome_appbar_push(GNOME_APPBAR(app_bar), status);	
}


void 
gnomebaker_enable_widget(const gchar *widget_name, gboolean enabled)
{
	GB_LOG_FUNC
	g_return_if_fail(widget_name != NULL);
	g_return_if_fail(xml != NULL);
	GtkWidget *widget = glade_xml_get_widget(xml, widget_name);
	g_return_if_fail(widget != NULL);	
	gtk_widget_set_sensitive(widget, enabled);
}


void /* libglade callback */
gnomebaker_on_refresh(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	filebrowser_refresh();	
}


void /* libglade callback */
gnomebaker_on_import(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC	
	project_import_session(gnomebaker_get_current_project());
}


void /* libglade callback */
gnomebaker_on_format_dvdrw(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	burn_format_dvdrw();	
}


void /* libglade callback */
gnomebaker_on_help(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	GError *error = NULL;
	gnome_help_display("gnomebaker", NULL, &error);
	if(error != NULL)
	{
		gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
			GTK_BUTTONS_NONE, error->message);
		g_error_free(error);
	}	
}


void
gnomebaker_on_notebook_switch_page(GtkNotebook *notebook,
                                   GtkNotebookPage *page,
                                   gint page_num,
                                   gpointer user_data)
{
    GB_LOG_FUNC
    GtkWidget *menu_import = glade_xml_get_widget(xml, widget_menu_import);
    GtkWidget *up = glade_xml_get_widget(xml, widget_up);
    GtkWidget *down = glade_xml_get_widget(xml, widget_down);
    GtkWidget *up_alt = glade_xml_get_widget(xml, widget_up_alt);
    GtkWidget *down_alt = glade_xml_get_widget(xml, widget_down_alt);
    GtkWidget *menu_up = glade_xml_get_widget(xml, widget_menu_up);
    GtkWidget *menu_down = glade_xml_get_widget(xml, widget_menu_down);
    GtkWidget *project_menu = glade_xml_get_widget(xml, "project2");
    GtkWidget *middle_toolbar = glade_xml_get_widget(xml, "toolbar4");
    GtkWidget *export_menu = glade_xml_get_widget(xml, "export1");
    GtkWidget *save_menu = glade_xml_get_widget(xml, "save_project1");
    GtkWidget *save_as_menu = glade_xml_get_widget(xml, "save_project_as1");
    GtkWidget *save_all_menu = glade_xml_get_widget(xml, "save_all1");
    GtkWidget *add_alt = glade_xml_get_widget(xml, "toolbutton12");
    GtkWidget *remove_alt = glade_xml_get_widget(xml, "toolbutton13");
    GtkWidget *clear_alt = glade_xml_get_widget(xml, "toolbutton14");
    
    GtkWidget *widget = gtk_notebook_get_nth_page(notebook, page_num);
    if(PROJECT_IS_WIDGET(widget))
    {
        gtk_widget_set_sensitive(project_menu, TRUE);
        gtk_widget_set_sensitive(middle_toolbar, TRUE);
        gtk_widget_set_sensitive(save_menu, TRUE);
        gtk_widget_set_sensitive(save_as_menu, TRUE);
        gtk_widget_set_sensitive(save_all_menu, TRUE);
        gtk_widget_set_sensitive(add_alt, TRUE);
        gtk_widget_set_sensitive(remove_alt, TRUE);
        gtk_widget_set_sensitive(clear_alt, TRUE);
        
        Project *project = PROJECT_WIDGET(widget);
        if(DATAPROJECT_IS_WIDGET(project))
        {
            gtk_widget_set_sensitive(menu_import, TRUE);
            gtk_widget_set_sensitive(up, FALSE);
            gtk_widget_set_sensitive(down, FALSE);
            gtk_widget_set_sensitive(up_alt, FALSE);
            gtk_widget_set_sensitive(down_alt, FALSE);
            gtk_widget_set_sensitive(menu_up, FALSE);
            gtk_widget_set_sensitive(menu_down, FALSE);
            gtk_widget_set_sensitive(export_menu, FALSE);
#ifdef USE_GTK_FILE_CHOOSER    
            /* We must ref before remove otherwise our filter gets destroyed */
            g_object_ref(audio_filter);
            gtk_file_chooser_remove_filter(GTK_FILE_CHOOSER(file_chooser), audio_filter);            
#endif            
        } 
        else if(AUDIOPROJECT_IS_WIDGET(project))
        {
            gtk_widget_set_sensitive(menu_import, FALSE);
            gtk_widget_set_sensitive(up, TRUE);
            gtk_widget_set_sensitive(down, TRUE);
            gtk_widget_set_sensitive(up_alt, TRUE);
            gtk_widget_set_sensitive(down_alt, TRUE);
            gtk_widget_set_sensitive(menu_up, TRUE);
            gtk_widget_set_sensitive(menu_down, TRUE);
            gtk_widget_set_sensitive(export_menu, TRUE);
#ifdef USE_GTK_FILE_CHOOSER    
            /* When switching to an audio project we filter the files available in the
             * file chooser according to the gstreamer plugins we have installed */
            gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), audio_filter);
            gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(file_chooser), audio_filter);        
#endif
        }
    }
    else 
    {
        gtk_widget_set_sensitive(save_menu, FALSE);
        gtk_widget_set_sensitive(save_as_menu, FALSE);
        gtk_widget_set_sensitive(save_all_menu, FALSE);
        gtk_widget_set_sensitive(project_menu, FALSE);
        gtk_widget_set_sensitive(middle_toolbar, FALSE);
        gtk_widget_set_sensitive(menu_import, FALSE);
        gtk_widget_set_sensitive(up, FALSE);
        gtk_widget_set_sensitive(down, FALSE);
        gtk_widget_set_sensitive(up_alt, FALSE);
        gtk_widget_set_sensitive(down_alt, FALSE);
        gtk_widget_set_sensitive(menu_up, FALSE);
        gtk_widget_set_sensitive(menu_down, FALSE);
        gtk_widget_set_sensitive(export_menu, FALSE);
        gtk_widget_set_sensitive(add_alt, FALSE);
        gtk_widget_set_sensitive(remove_alt, FALSE);
        gtk_widget_set_sensitive(clear_alt, FALSE);
    }
}


void /* libglade callback */
gnomebaker_on_up(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC
    project_move_selected_up(gnomebaker_get_current_project());
}


void /* libglade callback */
gnomebaker_on_down(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC
    project_move_selected_down(gnomebaker_get_current_project());
}


GtkWindow* 
gnomebaker_get_window()
{
    GB_LOG_FUNC
    return GTK_WINDOW(glade_xml_get_widget(gnomebaker_getxml(), widget_gnomebaker));
}


static gboolean
gnomebaker_playlist_file_filter(const GtkFileFilterInfo *filter_info, gpointer data)
{
    GB_LOG_FUNC
    return audioproject_is_supported_playlist(filter_info->mime_type);
}


void /* libglade callback */
gnomebaker_on_export_playlist(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC
    
    Project *project = gnomebaker_get_current_project();
    if(AUDIOPROJECT_IS_WIDGET(project))
    {
        GtkFileFilter *image_filter = gtk_file_filter_new();
        gtk_file_filter_add_custom(image_filter, GTK_FILE_FILTER_FILENAME | GTK_FILE_FILTER_MIME_TYPE,
                gnomebaker_playlist_file_filter, NULL, NULL);
        gtk_file_filter_set_name(image_filter,_("Playlist files"));
        
        GtkWidget *filetypes_combo = gtk_combo_box_new_text();
        gtk_combo_box_append_text(GTK_COMBO_BOX(filetypes_combo), ".m3u");
        gtk_combo_box_append_text(GTK_COMBO_BOX(filetypes_combo), ".pls");
        gtk_combo_box_set_active(GTK_COMBO_BOX(filetypes_combo), 0);
                
        gchar *file = gbcommon_show_file_chooser(_("Save playlist as..."),
                GTK_FILE_CHOOSER_ACTION_SAVE, image_filter, FALSE, GTK_COMBO_BOX(filetypes_combo));
        audioproject_export_playlist(AUDIOPROJECT_WIDGET(project), file);
        g_free(file);
    }
}


static gboolean
gnomebaker_project_file_filter(const GtkFileFilterInfo *filter_info, gpointer data)
{
    GB_LOG_FUNC
    return gbcommon_str_has_suffix(filter_info->filename, ".gbp");
}


void /* libglade callback */
gnomebaker_on_open_project(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC  
    
    GtkFileFilter *project_filter = gtk_file_filter_new();
    gtk_file_filter_add_custom(project_filter, GTK_FILE_FILTER_FILENAME,
        gnomebaker_project_file_filter, NULL, NULL);
    gtk_file_filter_set_name(project_filter,_("Project files"));
    gchar *file = gbcommon_show_file_chooser(_("Please select a project file."), GTK_FILE_CHOOSER_ACTION_OPEN, project_filter, FALSE, NULL);
    if(file != NULL)
    {
        GB_TRACE("gnomebaker_on_open_project - opening [%s]\n", file);        
        project_open(gnomebaker_get_current_project(), file);
    }
    g_free(file);
}


void /* libglade callback */
gnomebaker_on_save_project(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC   
    project_save(gnomebaker_get_current_project());
}


void /* libglade callback */
gnomebaker_on_save_project_as(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC   
}


void /* libglade callback */
gnomebaker_on_save_all(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC   
}


void /* libglade callback */
gnomebaker_on_new_data_project(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC   
    GtkWidget *project = dataproject_new();
    gtk_widget_show(project);
    GtkWidget *notebook = glade_xml_get_widget(xml, widget_project_notebook);
    gint index = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), project, project_get_title_widget(PROJECT_WIDGET(project)));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), index);
}


void /* libglade callback */
gnomebaker_on_new_audio_project(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC
    GtkWidget *project = audioproject_new();
    gtk_widget_show(project);
    GtkWidget *notebook = glade_xml_get_widget(xml, widget_project_notebook);
    gint index = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), project, project_get_title_widget(PROJECT_WIDGET(project)));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), index);
}


void /* libglade callback */
gnomebaker_on_import_playlist(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC
        
    GtkFileFilter *image_filter = gtk_file_filter_new();
    gtk_file_filter_add_custom(image_filter, GTK_FILE_FILTER_FILENAME | GTK_FILE_FILTER_MIME_TYPE,
        gnomebaker_playlist_file_filter, NULL, NULL);
    gtk_file_filter_set_name(image_filter,_("Playlist files"));
    gchar *file = gbcommon_show_file_chooser(_("Please select a playlist."), 
            GTK_FILE_CHOOSER_ACTION_OPEN, image_filter, TRUE, NULL);
    if(file != NULL)
    {
        gnomebaker_on_new_audio_project(NULL, NULL);
        audioproject_import_playlist(AUDIOPROJECT_WIDGET(gnomebaker_get_current_project()), file);
    }
    g_free(file);
}


void 
gnomebaker_on_close_project(gpointer widget, Project *project)
{
    GB_LOG_FUNC   
    
    GtkNotebook *notebook = GTK_NOTEBOOK(glade_xml_get_widget(xml, widget_project_notebook));
    g_return_if_fail(notebook != NULL); 
    if(project == NULL)
        project = gnomebaker_get_current_project();
    if(PROJECT_IS_WIDGET(project))
    {
        gint index = gtk_notebook_page_num(notebook, GTK_WIDGET(project));
        /*project_delete(project);*/
        gtk_notebook_remove_page(notebook, index);
    }
}


static void 
gnomebaker_select_files_or_folders(const gchar* text, GtkFileChooserAction action)
{
    GB_LOG_FUNC
    
    GtkSelectionData *selection_data = NULL;
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new(text, NULL, action, GTK_STOCK_CANCEL, 
            GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
    
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(file_chooser), TRUE);     
    if(gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_OK)
    {
        
        GSList *files = gtk_file_chooser_get_uris(GTK_FILE_CHOOSER(file_chooser));
        /*gchar **uris = g_malloc0(g_slist_length(files) * sizeof(gchar*));*/
        
        GSList *file = files;
        int index = 0;
        GString *text = g_string_new("");
        for(; file != NULL; file = file->next)
        {
            /* hand ownership of the data to the gchar** which we free later */
            GB_TRACE("gnomebaker_select_files_or_folders - index [%d] file [%s]\n", index, (gchar*)file->data);
            g_string_append(text, (gchar*)file->data);
            g_string_append(text, "\n");
            g_free((gchar*)file->data);
            /*uris[index++] = (gchar*)file->data;*/
        }
        
        /*gtk_selection_data_set_uris(selection_data, uris);*/
        selection_data = g_new0(GtkSelectionData, 1);                    
        gtk_selection_data_set(selection_data, selection_data->target, 8, 
            (const guchar*)text->str, strlen(text->str) * sizeof(gchar));
        GB_TRACE("gnomebaker_select_files_or_folders - [%s]\n", selection_data->data);
        g_slist_free(files);
        /*g_strfreev(uris);*/
        g_string_free(text, TRUE);
    }
    gtk_widget_destroy(file_chooser);
    
    if(selection_data != NULL)
    {
        project_add_selection(gnomebaker_get_current_project(), selection_data);        
        gtk_selection_data_free(selection_data);
    }   
}


static void
gnomebaker_on_select_files(GtkWidget *menuitem, gpointer user_data)
{   
    GB_LOG_FUNC
    gnomebaker_select_files_or_folders(_("Please select files to add to the disk."), 
            GTK_FILE_CHOOSER_ACTION_OPEN);
}


static void
gnomebaker_on_select_folders(GtkWidget *menuitem, gpointer user_data)
{   
    GB_LOG_FUNC
    gnomebaker_select_files_or_folders(_("Please select folders to add to the disk."), 
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
}


void /* libglade callback */
gnomebaker_on_add_files_alt(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC    
    GtkWidget *menu = gtk_menu_new();
    gbcommon_append_menu_item_stock(menu, _("_Add Folders"), GTK_STOCK_NEW, 
            (GCallback)gnomebaker_on_select_folders, widget);
    gbcommon_append_menu_item_stock(menu, _("_Add Files"), GTK_STOCK_FILE, 
            (GCallback)gnomebaker_on_select_files, widget);
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gdk_event_get_time(NULL));
}
