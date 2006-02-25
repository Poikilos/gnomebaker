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
 * File: audiocd.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Fri May  7 22:37:25 2004
 */
 
#include "audiocd.h"
#include "gnomebaker.h"
#include "gbcommon.h"
#include "burn.h"
#include <stdio.h>
#include "media.h"
#include "preferences.h"


static const gchar *const widget_audiocd_tree = "treeview8";
static const gchar *const widget_audiocd_size = "optionmenu2";
static const gchar *const widget_audiocd_progress_bar = "progressbar3";
static const gchar *const widget_audiocd_create = "button14";

static gdouble audiocd_selected_size = 0.0;

/*I think this is better than relying in the progressbar to store the length*/
static gdouble audiocd_compilation_seconds = 0;

static DiskSize audio_disk_sizes[] = 
{
	{21.0, "21 min. CD"},
	{63.0, "63 min. CD"},
	{74.0, "74 min. CD"},
    {80.0, "80 min. CD"},
	{90.0, "90 min. CD"},
    {99.0, "99 min. CD"}
};


enum
{
    TARGET_URI_LIST,
    TARGET_STRING,
    TARGET_COUNT
};


static GtkTargetEntry target_entries[] = 
{
    {"text/uri-list", 0, TARGET_URI_LIST},
    {"text/plain", 0, TARGET_STRING}
};


static void 
audiocd_move_selected(const gboolean up)
{
    GB_LOG_FUNC
    
    GtkWidget *tree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));        
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
    GList *list = gtk_tree_selection_get_selected_rows(selection, &model);    
    GList *current = NULL; 
    for((up ? (current = list) : (current = g_list_last(list))); 
        current != NULL; 
        (up ? (current = current->next) : (current = current->prev)))
    {
        GtkTreeIter from, to;
        GtkTreePath *path = (GtkTreePath*)current->data;
        gtk_tree_model_get_iter(model, &from, path);
        
        if(up) gtk_tree_path_prev(path);
        else gtk_tree_path_next(path);
            
        if(gtk_tree_model_get_iter(model, &to, path))
            gtk_list_store_swap(GTK_LIST_STORE(model), &from, &to);
            
        gtk_tree_path_free(path);     
    }
    
    g_list_free (list);        
}


static gint 
audiocd_get_audiocd_size()
{
	GB_LOG_FUNC
	GtkWidget *option_menu = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_size);
	g_return_val_if_fail(option_menu != NULL, 0);
	audiocd_selected_size = audio_disk_sizes[gtk_option_menu_get_history(GTK_OPTION_MENU(option_menu))].size;	
	return audiocd_selected_size;
}


static gchar*
audiocd_format_progress_text(gdouble current_secs)
{
    GB_LOG_FUNC
    g_return_val_if_fail(current_secs < (audiocd_selected_size * 60), NULL);
    
    gint ss1 = ((gint)current_secs)%60;
    gint m1 = (((gint)current_secs)-ss1)/60;
    gint ss2 = ((gint)((audiocd_selected_size * 60) - current_secs))%60;
    gint m2 = (((gint)((audiocd_selected_size * 60) - current_secs))-ss2)/60;        
    return g_strdup_printf(_("%d mins %d secs used - %d mins %d secs remaining"), m1, ss1, m2, ss2);    
}   


static gboolean 
audiocd_update_progress_bar(gboolean add, gdouble seconds)
{
	GB_LOG_FUNC
	gboolean ok = TRUE;
	
	/* Now update the progress bar with the cd size */
	GladeXML *xml = gnomebaker_getxml();
	g_return_if_fail(xml != NULL);
	
	GtkWidget *progress_bar = glade_xml_get_widget(xml, widget_audiocd_progress_bar);
	g_return_if_fail(progress_bar != NULL);
	
	const gdouble disk_size = audiocd_get_audiocd_size() * 60;
	
	if(add)
	{
		if((audiocd_compilation_seconds +  seconds) <= disk_size)
		{
			audiocd_compilation_seconds += seconds;
		}
		else /*do not add*/
		{
			gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
			_("Track is too large to fit in the remaining space on the CD"));
			ok = FALSE;
		}
	}
	else
	{
		audiocd_compilation_seconds -= seconds;
	}	
    
	if (audiocd_compilation_seconds < 0.0 || audiocd_compilation_seconds == 0.0)
	{
		audiocd_compilation_seconds = 0;
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
		gchar *buf = audiocd_format_progress_text(audiocd_compilation_seconds);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), buf);
        g_free(buf);
		
		/* disable the create button as there's nothing on the disk */
		gnomebaker_enable_widget(widget_audiocd_create, FALSE);
	}	
	else
	{
		gdouble fraction = 0.0;
	
		if(disk_size > 0)
	 		fraction = (gdouble)audiocd_compilation_seconds/disk_size;
	 		
		if(audiocd_compilation_seconds > disk_size)
		{
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 1.0);
			gnomebaker_enable_widget(widget_audiocd_create, FALSE);
		}
		else
		{
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
            gnomebaker_enable_widget(widget_audiocd_create, TRUE);
		}
	
        gchar *buf = audiocd_format_progress_text(audiocd_compilation_seconds);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), buf);
        g_free(buf);
	}
	
	return ok;
}


static void
audiocd_foreach_fileselection(GtkTreeModel *file_model,
								  GtkTreePath *path,
								  GtkTreeIter *iter,
								  gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(file_model != NULL);
	g_return_if_fail(iter != NULL);
	
	GList **rowref_list = (GList**)user_data;	
	GtkTreeRowReference *ref = gtk_tree_row_reference_new(file_model, path);
	if(ref != NULL)
    	*rowref_list = g_list_append(*rowref_list, ref);
}


static void 
audiocd_on_remove_clicked(GtkWidget *menuitem, gpointer user_data)
{
	GB_LOG_FUNC	
	audiocd_remove();
}


static void 
audiocd_on_clear_clicked(GtkWidget *menuitem, gpointer user_data)
{
	GB_LOG_FUNC
	audiocd_clear();
}


static void 
audiocd_on_open(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(user_data != NULL);

	GtkTreeView *view = (GtkTreeView*)user_data;	
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	gtk_tree_selection_selected_foreach(selection, 
		(GtkTreeSelectionForeachFunc)gbcommon_get_first_selected_row, &iter);	
	gchar *file = NULL;
	gtk_tree_model_get(model, &iter, AUDIOCD_COL_FILE, &file, -1);
	gbcommon_launch_app_for_file(file);
	g_free(file);
}


static void 
audiocd_on_move_up(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC
    audiocd_move_selected(TRUE);
}


static void 
audiocd_on_move_down(gpointer widget, gpointer user_data)
{
    GB_LOG_FUNC
    audiocd_move_selected(FALSE);
}


static gboolean
audiocd_on_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(widget != NULL, FALSE);

	/* look for a right click */	
	if(event->button == 3)
	{
		GtkWidget *menu = gtk_menu_new();
		GtkTreeView *view = (GtkTreeView*)widget;
		GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
		const gint count = gtk_tree_selection_count_selected_rows(selection);
		if(count == 1)
		{
			gbcommon_append_menu_item_stock(menu, _("_Open"), GTK_STOCK_OPEN, 
				(GCallback)audiocd_on_open, view);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
		}
        
        gbcommon_append_menu_item_stock(menu, _("Move selected _up"), GTK_STOCK_GO_UP, 
            (GCallback)audiocd_on_move_up, widget);
        gbcommon_append_menu_item_stock(menu, _("Move selected _down"), GTK_STOCK_GO_DOWN,
            (GCallback)audiocd_on_move_down, widget);   
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());        
		gbcommon_append_menu_item_stock(menu, _("_Remove selected"), GTK_STOCK_REMOVE, 
			(GCallback)audiocd_on_remove_clicked, widget);	
		gbcommon_append_menu_item_stock(menu, _("_Clear"), GTK_STOCK_CLEAR,
			(GCallback)audiocd_on_clear_clicked, widget);	
            
		gtk_widget_show_all(menu);
	
		/* Note: event can be NULL here when called. However,
		 *  gdk_event_get_time() accepts a NULL argument */
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
					   (event != NULL) ? event->button : 0,
					   gdk_event_get_time((GdkEvent*)event));
		return TRUE;
	}
	return FALSE;	
}


static gboolean
audiocd_add_file(const gchar *file_name, GtkTreeModel *model)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file_name != NULL, FALSE);
	g_return_val_if_fail(model != NULL, FALSE);
    
    GB_TRACE("audiocd_add_file - [%s]\n", file_name);
	/* ret is set to true as we want to ignore non audio files. */
	gboolean ret = TRUE;
	MediaInfo *info = media_info_new(file_name);
	if(info != NULL)
	{     
		switch(info->status)
        {
            case INSTALLED:
            {
                if(audiocd_update_progress_bar(TRUE, (gdouble)info->duration))
                {
                    GdkPixbuf *icon = gbcommon_get_icon_for_mime(info->mime_type, 16);
            
                    GB_DECLARE_STRUCT(GtkTreeIter, iter);		
                    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                    gtk_list_store_set(
                        GTK_LIST_STORE(model), &iter, 
                        AUDIOCD_COL_ICON, icon, 
                        AUDIOCD_COL_FILE, (gchar*)file_name, 
                        AUDIOCD_COL_DURATION, info->formatted_duration->str,
                        /*AUDIOCD_COL_SIZE, info->filesize,*/
                        AUDIOCD_COL_ARTIST, info->artist->str, 
                        AUDIOCD_COL_ALBUM, info->album->str,
                        AUDIOCD_COL_TITLE, info->title->str, 
                        AUDIOCD_COL_INFO, info,
                        -1);
                
                    g_object_unref(icon);
                    
                }
                else
                {
                    media_info_delete(info);
                    ret = FALSE;
                }
                break;
            }
            default:
            {
                gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE, info->error->message);
                media_info_delete(info);
            }
        }
	}
	return ret;
}


static gboolean
audiocd_import_playlist_file(const gchar *play_list_dir, const gchar *file, GtkTreeModel *model)
{
    GB_LOG_FUNC
    g_return_val_if_fail(file != NULL, FALSE);
    g_return_val_if_fail(model != NULL, FALSE);
    
    gboolean ret = TRUE;
    gchar *file_name = NULL;
    if(!g_path_is_absolute(file)) /* This is a relative file, relative to the play_list */
        file_name = g_build_filename(play_list_dir, file, NULL);
    else 
        file_name = g_strdup(file);
    
    if(g_file_test(file_name, G_FILE_TEST_EXISTS))
        ret = audiocd_add_file(file_name, model);
    else 
    {
        gchar *message = g_strdup_printf(_("The file [%s] referred to in the play_list could not be imported as it does not exist."), file_name);
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
        g_free(message);                
    }
    g_free(file_name);
    return ret;
}


static gboolean
audiocd_import_m3u_playlist(const gchar *m3u_file, GtkTreeModel *model)
{
    GB_LOG_FUNC
    g_return_val_if_fail(m3u_file != NULL, FALSE);
    g_return_val_if_fail(model != NULL, FALSE);
    
    gboolean ret = TRUE;
    gchar *m3u_dir = g_path_get_dirname(m3u_file);
    gchar **lines = gbcommon_get_file_as_list(m3u_file);
    gchar **line = lines;
    while((line != NULL) && (*line != NULL) && (ret == TRUE))
    {
        gchar *file = *line;
        g_strstrip(file);
        GB_TRACE("audiocd_import_m3u_playlist - [%s]\n", file);
        if((strlen(file) > 0) && (file[0] != '#') && !g_ascii_isspace(file[0]))
            ret = audiocd_import_playlist_file(m3u_dir, file, model);
        ++line;
    }
    g_free(m3u_dir);
    g_strfreev(lines);
    return ret;
}


static gboolean 
audiocd_import_pls_playlist(const gchar *pls_file, GtkTreeModel *model)
{
    GB_LOG_FUNC
    g_return_val_if_fail(pls_file != NULL, FALSE);
    g_return_val_if_fail(model != NULL, FALSE);
    
    gboolean ret = TRUE;
    gchar *pls_dir = g_path_get_dirname(pls_file);
    gchar **lines = gbcommon_get_file_as_list(pls_file);
    gchar **line = lines;
    while((line != NULL) && (*line != NULL) && (ret == TRUE))
    {
        gchar *entry = *line;
        g_strstrip(entry);        
        if((entry[0] != '[') && (strstr(entry, "File") != NULL))
        {
            const gchar *file = strchr(entry, '=');
            if(file != NULL)
            {
                ++file;            
                GB_TRACE("audiocd_import_pls_playlist - [%s]\n", file);
                ret = audiocd_import_playlist_file(pls_dir, file, model);
            }
            else
            {
                g_critical("audiocd_import_pls_playlist - file [%s] contains invalid line [%s]", pls_file, entry);   
            }
        }
        ++line;
    }
    g_free(pls_dir);
    g_strfreev(lines);
    return ret;
}


static gboolean 
audiocd_import_supported_playlist(const gchar *mime, const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(mime != NULL, FALSE);
    g_return_val_if_fail(play_list != NULL, FALSE);
        
    GtkTreeView *view = GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree));
    GtkTreeModel *model = gtk_tree_view_get_model(view);    
    gboolean ret = FALSE;
    if(g_ascii_strcasecmp(mime, "audio/x-mpegurl") == 0)
        ret = audiocd_import_m3u_playlist(play_list, model);
    else if(g_ascii_strcasecmp(mime, "audio/x-scpls") == 0)      
        ret = audiocd_import_pls_playlist(play_list, model);
    return ret;
}


static void
audiocd_on_drag_data_received(
    GtkWidget  *widget,
    GdkDragContext  *context,
    gint x,
    gint y,
    GtkSelectionData  *selection_data,
    guint info,
    guint time,
    gpointer user_data)
{
    GB_LOG_FUNC
    audiocd_add_selection(selection_data);
}


static void 
audiocd_on_list_dbl_click(GtkTreeView *tree_view, GtkTreePath *path,
                       	GtkTreeViewColumn *col, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(tree_view != NULL);
	g_return_if_fail(path != NULL);
	
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	g_return_if_fail(model != NULL);
	
	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	gtk_tree_model_get_iter(model, &iter, path);
	
	gchar *file = NULL;
	gtk_tree_model_get(model, &iter, AUDIOCD_COL_FILE, &file, -1);
	gbcommon_launch_app_for_file(file);
	g_free(file);
}


void /* libglade callback */
audiocd_on_audiocd_size_changed(GtkOptionMenu *option_menu, gpointer user_data)
{
	GB_LOG_FUNC
		
	GtkWidget *progress_bar = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_progress_bar);
	g_return_if_fail(progress_bar != NULL);
	
	audiocd_selected_size = audiocd_get_audiocd_size();		
	gdouble fraction  = (audiocd_compilation_seconds)/(audiocd_selected_size*60);

    if(fraction>1.0)
    {
    	 gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
			_("Not enough space on the CD"));
		
		/* disable the create button*/
		gnomebaker_enable_widget(widget_audiocd_create, FALSE);	
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 1.0);	

	}
	else
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
		gnomebaker_enable_widget(widget_audiocd_create, TRUE);		
	}
	
    gchar *buf = audiocd_format_progress_text(audiocd_compilation_seconds);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), buf);
    preferences_set_int(GB_AUDIO_DISK_SIZE, gtk_option_menu_get_history(option_menu));
    g_free(buf);
    
}


void /* libglade callback */
audiocd_on_create_audiocd(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	/* Here we should get a glist of data files to burn to the cd.
	 * or something like that */	
	GtkWidget *audio_tree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
	g_return_if_fail(audio_tree != NULL);
	
	GtkTreeModel *audio_model = gtk_tree_view_get_model(GTK_TREE_VIEW(audio_tree));
	g_return_if_fail(audio_model != NULL);
		
	burn_create_audio_cd(audio_model);
}


void 
audiocd_import_session()
{
	GB_LOG_FUNC
}


void 
audiocd_move_selected_up()
{
    GB_LOG_FUNC
    audiocd_move_selected(TRUE);
}


void 
audiocd_move_selected_down()
{
    GB_LOG_FUNC
    audiocd_move_selected(FALSE);
}


void 
audiocd_add_selection(GtkSelectionData *selection)
{
    GB_LOG_FUNC
    g_return_if_fail(selection != NULL);
    g_return_if_fail(selection->data != NULL);
    
    GtkTreeView *view = GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree));
    g_return_if_fail(view != NULL);                         
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    g_return_if_fail(model != NULL);
    
    gnomebaker_show_busy_cursor(TRUE);

    gboolean cont = TRUE;
    const gchar *file = strtok((gchar*)selection->data, "\n");
    while((file != NULL) && cont)
    {
        /* Get the file name that's been dropped and if there's a 
           file url at the start then strip it off */   
        gchar *file_name = gbcommon_get_local_path(file);        
        
        /* if the file is really a directory then we open it
        and attempt to add any audio files in the directory */
        if(g_file_test(file_name, G_FILE_TEST_IS_DIR))
        {
            GDir *dir = g_dir_open(file_name, 0, NULL);
            const gchar *name = g_dir_read_name(dir);
            while((name != NULL) && cont)
            {
                gchar *full_name = g_build_filename(file_name, name, NULL);
                if(!g_file_test(full_name, G_FILE_TEST_IS_DIR))
                    cont = audiocd_add_file(full_name, model);
                g_free(full_name);
                name = g_dir_read_name(dir);                    
            }
            g_dir_close(dir);
        }
        else 
        {
            /* Now try and find any play_list types and add them */
            gchar *mime = gbcommon_get_mime_type(file_name);
            if(audiocd_is_supported_playlist(mime))
                cont = audiocd_import_supported_playlist(mime, file_name);
            else 
                cont = audiocd_add_file(file_name, model);
            g_free(mime);
        }
        g_free(file_name);       
        file = strtok(NULL, "\n");
    }
    
    gnomebaker_show_busy_cursor(FALSE);
}


void 
audiocd_remove()
{
    GB_LOG_FUNC
    gnomebaker_show_busy_cursor(TRUE);
    
    GtkWidget *audio_tree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
    GtkTreeModel *file_model = gtk_tree_view_get_model(GTK_TREE_VIEW(audio_tree));    
    
    GList *rr_list = g_list_alloc();    /* list of GtkTreeRowReferences to remove */    
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(audio_tree));
    gtk_tree_selection_selected_foreach(selection, audiocd_foreach_fileselection, &rr_list);        
    
    GList *node = rr_list;
    for (; node != NULL ; node = node->next)
    {
        if(node->data)
        {
            GtkTreePath *path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);
            
            if (path)
            {
                GB_DECLARE_STRUCT(GtkTreeIter, iter);
                if (gtk_tree_model_get_iter(file_model, &iter, path))
                {
                    MediaInfo *info = NULL;
                    gtk_tree_model_get (file_model, &iter, AUDIOCD_COL_INFO, &info, -1);
                    audiocd_update_progress_bar(FALSE, (gdouble)info->duration);  
                    gtk_list_store_remove(GTK_LIST_STORE(file_model), &iter);
                    media_info_delete(info);
                }           
                /* FIXME/CHECK: Do we need to free the path here? */
            }
        }
    }
    
    g_list_foreach(rr_list, (GFunc)gtk_tree_row_reference_free, NULL);
    g_list_free(rr_list);       
    
    gnomebaker_show_busy_cursor(FALSE);
}


void
audiocd_clear()
{
    GB_LOG_FUNC
    
    gnomebaker_show_busy_cursor(TRUE);
    
    audiocd_compilation_seconds = 0.0;
    
    GtkWidget *audio_tree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
    GtkTreeModel *file_model = gtk_tree_view_get_model(GTK_TREE_VIEW(audio_tree));    
    GtkWidget *progress_bar = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_progress_bar);
    
    gchar *buf = audiocd_format_progress_text(0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), buf);
    g_free(buf);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);

    gnomebaker_enable_widget(widget_audiocd_create, FALSE);

    /* Now get each row and free the MediaInfo* we store in each */    
    GtkTreeIter iter;
    if(gtk_tree_model_get_iter_first(file_model, &iter))
    {
        do
        {
            MediaInfo *info = NULL;
            gtk_tree_model_get (file_model, &iter, AUDIOCD_COL_INFO, &info, -1);
            media_info_delete(info);
        } while (gtk_tree_model_iter_next(file_model, &iter));
    }    
    gtk_list_store_clear(GTK_LIST_STORE(file_model));
    gnomebaker_show_busy_cursor(FALSE); 
}


gboolean 
audiocd_is_supported_playlist(const gchar *mime)
{
    GB_LOG_FUNC
    g_return_val_if_fail(mime != NULL, FALSE);           
    
    if((g_ascii_strcasecmp(mime, "audio/x-mpegurl") == 0) || 
            (g_ascii_strcasecmp(mime, "audio/x-scpls") == 0))
        return TRUE;
    return FALSE;
}


gboolean 
audiocd_import_playlist(const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(play_list != NULL, FALSE);
    
    gboolean ret = FALSE;
    gchar *mime = gbcommon_get_mime_type(play_list);
    if(audiocd_is_supported_playlist(mime))
        ret = audiocd_import_supported_playlist(mime, play_list);
    else
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE,
          _("The file you have selected is not a supported play_list. Please select a pls or m3u file."));
    g_free(mime);
    return ret;
}


static gboolean
audiocd_export_m3u(const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(play_list != NULL, FALSE);

    gboolean ret = FALSE;
    FILE *file = NULL;   
    if ((file = fopen(play_list, "w")) == 0)
    {
        g_critical("audiocd_export_m3u - Failed to write play_list [%s]", play_list);
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
            GTK_BUTTONS_NONE, _("Failed to write play_list file"));
    }
    else
    {
        GtkWidget *tree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
        while (valid)
        {
            gchar *playlist_item = NULL;
            gtk_tree_model_get (model, &iter,  AUDIOCD_COL_FILE, &playlist_item, -1);
            fprintf(file, "%s\n", playlist_item);
            g_free (playlist_item);
            valid = gtk_tree_model_iter_next (model, &iter);
        }
        
        fclose(file);
        ret = TRUE;
    }
    
    return ret;
}    


static gboolean
audiocd_export_pls(const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(play_list != NULL, FALSE);

    gboolean ret = FALSE;
    FILE *file;   
    if ((file = fopen(play_list, "w")) == 0)
    {
        g_critical("audiocd_export_pls - Failed to write play_list [%s]", play_list);
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, 
            GTK_BUTTONS_NONE, _("Failed to write play_list file"));
    }
    else
    {
        fprintf(file, "[play_list]\n");
        GtkWidget *tree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
        gint track = 1;
        while (valid)
        {
            gchar *playlist_item = NULL, *title = NULL;
            MediaInfo *info = NULL;
            gtk_tree_model_get (model, &iter, AUDIOCD_COL_FILE, &playlist_item, -1);
            gtk_tree_model_get (model, &iter, AUDIOCD_COL_TITLE, &title, -1);
            gtk_tree_model_get (model, &iter, AUDIOCD_COL_INFO, &info, -1);
            fprintf(file, "File%d=%s\n", track, playlist_item);
            fprintf(file, "Title%d=%s\n", track, title);
            fprintf(file, "Length%d=%ld\n", track, info->duration);
            g_free (playlist_item);
            g_free (title);
            /*g_free (info); we don't own the media info pointer */
            
            valid = gtk_tree_model_iter_next (model, &iter);
            track++;
        }

        fprintf(file, "NumberOfEntries=%d\n", track-1);
        fprintf(file, "Version=2");
        fclose(file);
        ret = TRUE;
    }
    
    return ret;
}    


gboolean 
audiocd_export_playlist(const gchar *play_list)
{
    GB_LOG_FUNC
    g_return_val_if_fail(play_list != NULL, FALSE);

    gboolean ret = FALSE;
    gint result = GTK_RESPONSE_YES;
    if(g_file_test(play_list, G_FILE_TEST_EXISTS))
    {
        result = gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                    GTK_BUTTONS_NONE, _("Playlist already exists.\n Do you want to replace it?"));
    }

    if(result == GTK_RESPONSE_YES)
    {
        if (strlen(play_list) < 4 || gbcommon_str_has_suffix(play_list, ".m3u"))
            ret = audiocd_export_m3u(play_list);
        else if (strlen(play_list) < 4 || gbcommon_str_has_suffix(play_list, ".pls"))
            ret = audiocd_export_pls(play_list);
    }   
    return ret;
}


void
audiocd_new()
{
    GB_LOG_FUNC
    
    audiocd_compilation_seconds = 0.0;
    
    GtkTreeView *file_list = 
        GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree));
    g_return_if_fail(file_list != NULL);
    
    /* Create the list store for the file list */
    GtkListStore *store = gtk_list_store_new(AUDIOCD_NUM_COLS, 
        GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, /*G_TYPE_ULONG,*/
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    gtk_tree_view_set_model(file_list, GTK_TREE_MODEL(store));
    g_object_unref(store);

    /* One column which has an icon renderer and text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Track"));
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", AUDIOCD_COL_ICON, NULL);
    
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_FILE, NULL);
    gtk_tree_view_append_column(file_list, col);

    /* Second column to display the duration*/
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Duration"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_DURATION, NULL);
    gtk_tree_view_append_column(file_list, col);
    
    /* Third column to display the size 
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Size");
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_SIZE, NULL);
    gtk_tree_view_append_column(file_list, col);*/
        
    /* column to display the artist */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Artist"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_ARTIST, NULL);
    gtk_tree_view_append_column(file_list, col);
    
    /* column to display the album */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Album"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_ALBUM, NULL);
    gtk_tree_view_append_column(file_list, col);
    
    /* column to display the title */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Title"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_TITLE, NULL);
    gtk_tree_view_append_column(file_list, col);
    
    /* hidden column to store the MediaInfo pointer */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Info"));
    gtk_tree_view_append_column(file_list, col);
    gtk_tree_view_column_set_visible(col, FALSE);
    
    /* Set the selection mode of the file list */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(file_list), GTK_SELECTION_MULTIPLE);

    /* Enable the file list as a drag destination */    
    gtk_drag_dest_set(GTK_WIDGET(file_list), GTK_DEST_DEFAULT_ALL,
        target_entries, TARGET_COUNT, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

    /* Connect the function to handle the drag data */
    g_signal_connect(file_list, "drag_data_received",
    G_CALLBACK(audiocd_on_drag_data_received), store);
    
    /* connect the signal to handle right click */
    g_signal_connect (G_OBJECT(file_list), "button-press-event",
        G_CALLBACK(audiocd_on_button_pressed), NULL);
        
    g_signal_connect(G_OBJECT(file_list), "row-activated", 
        G_CALLBACK(audiocd_on_list_dbl_click), NULL);
        
    GtkWidget *option_menu = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_size);
    gbcommon_populate_disk_size_option_menu(GTK_OPTION_MENU(option_menu), audio_disk_sizes, 
        (sizeof(audio_disk_sizes)/sizeof(DiskSize)), preferences_get_int(GB_AUDIO_DISK_SIZE));      
}

