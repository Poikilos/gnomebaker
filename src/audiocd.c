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


static const gchar* const widget_audiocd_tree = "treeview8";
static const gchar* const widget_audiocd_size = "optionmenu2";
static const gchar* const widget_audiocd_progressbar = "progressbar3";
static const gchar* const widget_audiocd_create = "button14";


static gdouble audiocdsize = 0.0;

static DiskSize audiodisksizes[] = 
{
	{22, "22 min. CD"},
	{74, "74 min. CD"},
	{80, "80 min. CD"},
	{90, "90 min. CD"}
};


enum
{
    TARGET_STRING,
    TARGET_URL    
};


static GtkTargetEntry targetentries[] = 
{
    {"STRING", 0, TARGET_STRING},
    {"text/plain", 0, TARGET_STRING},
    {"text/uri-list", 0, TARGET_URL},
};


static void 
audiocd_move_selected(const gboolean up)
{
    GB_LOG_FUNC
    
    GtkWidget* tree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));        
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
    GList* list = gtk_tree_selection_get_selected_rows(selection, &model);    
    GList* current = NULL; 
    for((up ? (current = list) : (current = g_list_last(list))); 
        current != NULL; 
        (up ? (current = current->next) : (current = current->prev)))
    {
        GtkTreeIter from, to;
        GtkTreePath* path = (GtkTreePath*)current->data;
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
	GtkWidget* optmen = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_size);
	g_return_val_if_fail(optmen != NULL, 0);
	audiocdsize = audiodisksizes[gtk_option_menu_get_history(GTK_OPTION_MENU(optmen))].size;	
	return audiocdsize;
}


static gboolean 
audiocd_update_progress_bar(gboolean add, gdouble seconds)
{
	GB_LOG_FUNC
	gboolean ok = TRUE;
	
	/* Now update the progress bar with the cd size */
	GladeXML* xml = gnomebaker_getxml();
	g_return_val_if_fail(xml != NULL, FALSE);
	
	GtkWidget* progbar = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_progressbar);
	g_return_val_if_fail(progbar != NULL, FALSE);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));	
	const gdouble cdsize = audiocd_get_audiocd_size();	
	gdouble currentsecs = fraction * cdsize * 60;
	
	if(add)
		currentsecs += seconds;
	else
		currentsecs -= seconds;
	
	fraction = currentsecs / (cdsize * 60);
	
	if(fraction < 0.0 || fraction == -0.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), _("0 mins 0 secs"));
		gnomebaker_enable_widget(widget_audiocd_create, FALSE);
	}	
	/* If the file is too large then we don't allow the user to add it */
	else if(fraction <= 1.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);

		gint ss = ((gint)currentsecs)%60;
		gint m = (((gint)currentsecs)-ss)/60;
		
		gchar* buf = g_strdup_printf(_("%d mins %d secs"), m, ss);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
		g_free(buf);
		gnomebaker_enable_widget(widget_audiocd_create, TRUE);
	}
	else
	{
		gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
			_("Track is too large to fit in the remaining space on the CD"));
		ok = FALSE;
	}

	return ok;
}


static void
audiocd_foreach_fileselection(GtkTreeModel *filemodel,
								  GtkTreePath *path,
								  GtkTreeIter *iter,
								  gpointer userdata)
{
	GB_LOG_FUNC
	g_return_if_fail(filemodel != NULL);
	g_return_if_fail(iter != NULL);
	
	GList** rowref_list = (GList**)userdata;	
	GtkTreeRowReference* ref = gtk_tree_row_reference_new(filemodel, path);
	if(ref != NULL)
    	*rowref_list = g_list_append(*rowref_list, ref);
}


static void 
audiocd_on_remove_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC	
	audiocd_remove();
}


static void 
audiocd_on_clear_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC
	audiocd_clear();
}


static void 
audiocd_on_open(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(user_data != NULL);

	GtkTreeView* view = (GtkTreeView*)user_data;	
	GtkTreeModel* model = gtk_tree_view_get_model(view);
	GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
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
		GtkWidget* menu = gtk_menu_new();
		GtkTreeView* view = (GtkTreeView*)widget;
		GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
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
audiocd_add_file(const gchar* filename, GtkTreeModel* model)
{
	GB_LOG_FUNC
	g_return_val_if_fail(filename != NULL, FALSE);
	g_return_val_if_fail(model != NULL, FALSE);
    
    GB_TRACE("audiocd_add_file - [%s]", filename);
	/* ret is set to true as we want to ignore non audio files. */
	gboolean ret = TRUE;
	MediaInfo* info = media_info_new(filename);
	if(info != NULL)
	{     
		switch(info->status)
        {
            case INSTALLED:
            {
                if(audiocd_update_progress_bar(TRUE, (gdouble)info->duration))
                {
                    GdkPixbuf* icon = gbcommon_get_icon_for_mime(info->mimetype, 16);
            
                    GB_DECLARE_STRUCT(GtkTreeIter, iter);		
                    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                    gtk_list_store_set(
                        GTK_LIST_STORE(model), &iter, 
                        AUDIOCD_COL_ICON, icon, 
                        AUDIOCD_COL_FILE, (gchar*)filename, 
                        AUDIOCD_COL_DURATION, info->formattedduration->str,
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
audiocd_import_playlist_file(const gchar* playlistdir, const gchar* file, GtkTreeModel* model)
{
    GB_LOG_FUNC
    g_return_val_if_fail(file != NULL, FALSE);
    g_return_val_if_fail(model != NULL, FALSE);
    
    gboolean ret = TRUE;
    gchar* filename = NULL;
    if(!g_path_is_absolute(file)) /* This is a relative file, relative to the playlist */
        filename = g_build_filename(playlistdir, file, NULL);
    else 
        filename = g_strdup(file);
    
    if(g_file_test(filename, G_FILE_TEST_EXISTS))
        ret = audiocd_add_file(filename, model);
    else 
        /* TODO need to figure out what to do with files which don't exist, ignore them for now */
    g_free(filename);
    return ret;
}


static gboolean
audiocd_import_m3u_playlist(const gchar* m3ufile, GtkTreeModel* model)
{
    GB_LOG_FUNC
    g_return_val_if_fail(m3ufile != NULL, FALSE);
    g_return_val_if_fail(model != NULL, FALSE);
    
    gboolean ret = TRUE;
    gchar* m3udir = g_path_get_dirname(m3ufile);
    gchar** lines = gbcommon_get_file_as_list(m3ufile);
    gchar** line = lines;
    while((line != NULL) && (*line != NULL) && (ret == TRUE))
    {
        gchar* file = *line;
        g_strstrip(file);
        GB_TRACE("audiocd_import_m3u_playlist - [%s]", file);
        if((strlen(file) > 0) && (file[0] != '#') && !g_ascii_isspace(file[0]))
            ret = audiocd_import_playlist_file(m3udir, file, model);
        ++line;
    }
    g_free(m3udir);
    g_strfreev(lines);
    return ret;
}


static gboolean 
audiocd_import_pls_playlist(const gchar* plsfile, GtkTreeModel* model)
{
    GB_LOG_FUNC
    g_return_val_if_fail(plsfile != NULL, FALSE);
    g_return_val_if_fail(model != NULL, FALSE);
    
    gboolean ret = TRUE;
    gchar* plsdir = g_path_get_dirname(plsfile);
    gchar** lines = gbcommon_get_file_as_list(plsfile);
    gchar** line = lines;
    while((line != NULL) && (*line != NULL) && (ret == TRUE))
    {
        gchar* entry = *line;
        g_strstrip(entry);        
        if((entry[0] != '[') && (strstr(entry, "File") != NULL))
        {
            const gchar* file = strchr(entry, '=');
            if(file != NULL)
            {
                ++file;            
                GB_TRACE("audiocd_import_pls_playlist - [%s]", file);
                ret = audiocd_import_playlist_file(plsdir, file, model);
            }
            else
            {
                g_critical("audiocd_import_pls_playlist - file [%s] contains invalid line [%s]", plsfile, entry);   
            }
        }
        ++line;
    }
    g_free(plsdir);
    g_strfreev(lines);
    return ret;
}


static void
audiocd_on_drag_data_received(
    GtkWidget * widget,
    GdkDragContext * context,
    gint x,
    gint y,
    GtkSelectionData * seldata,
    guint info,
    guint time,
    gpointer userdata)
{
    GB_LOG_FUNC
    audiocd_add_selection(seldata);
}


static void 
audiocd_on_list_dbl_click(GtkTreeView* treeview, GtkTreePath* path,
                       	GtkTreeViewColumn* col, gpointer userdata)
{
	GB_LOG_FUNC
	g_return_if_fail(treeview != NULL);
	g_return_if_fail(path != NULL);
	
	GtkTreeModel* model = gtk_tree_view_get_model(treeview);
	g_return_if_fail(model != NULL);
	
	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	gtk_tree_model_get_iter(model, &iter, path);
	
	gchar* file = NULL;
	gtk_tree_model_get(model, &iter, AUDIOCD_COL_FILE, &file, -1);
	gbcommon_launch_app_for_file(file);
	g_free(file);
}


void /* libglade callback */
audiocd_on_audiocd_size_changed(GtkOptionMenu *optionmenu, gpointer user_data)
{
	GB_LOG_FUNC
		
	GtkWidget* progbar = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_progressbar);
	g_return_if_fail(progbar != NULL);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));	
	gdouble previoussize = audiocdsize;
	audiocdsize = audiocd_get_audiocd_size();
		
	fraction = (fraction * previoussize)/audiocdsize;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);	
}


void /* libglade callback */
audiocd_on_create_audiocd(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	/* Here we should get a glist of data files to burn to the cd.
	 * or something like that */	
	GtkWidget *audiotree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
	g_return_if_fail(audiotree != NULL);
	
	GtkTreeModel* audiomodel = gtk_tree_view_get_model(GTK_TREE_VIEW(audiotree));
	g_return_if_fail(audiomodel != NULL);
		
	burn_create_audio_cd(audiomodel);
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
audiocd_add_selection(GtkSelectionData* selection)
{
    GB_LOG_FUNC
    g_return_if_fail(selection != NULL);
    g_return_if_fail(selection->data != NULL);
    
    GtkTreeView* view = GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree));
    g_return_if_fail(view != NULL);                         
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    g_return_if_fail(model != NULL);
    
    gnomebaker_show_busy_cursor(TRUE);

    gboolean cont = TRUE;
    const gchar* file = strtok((gchar*)selection->data, "\n");
    while((file != NULL) && cont)
    {
        /* Get the file name that's been dropped and if there's a 
           file url at the start then strip it off */   
        gchar* filename = gbcommon_get_local_path(file);        
        
        /* if the file is really a directory then we open it
        and attempt to add any audio files in the directory */
        if(g_file_test(filename, G_FILE_TEST_IS_DIR))
        {
            GDir* dir = g_dir_open(filename, 0, NULL);
            const gchar *name = g_dir_read_name(dir);
            while((name != NULL) && cont)
            {
                gchar* fullname = g_build_filename(filename, name, NULL);
                if(!g_file_test(fullname, G_FILE_TEST_IS_DIR))
                    cont = audiocd_add_file(fullname, model);
                g_free(fullname);
                name = g_dir_read_name(dir);                    
            }
            g_dir_close(dir);
        }
        else 
        {
            /* Now try and find any playlist types and add them */
            gchar* mime = gbcommon_get_mime_type(filename);
            if(g_ascii_strcasecmp(mime, "audio/x-mpegurl") == 0)
                cont = audiocd_import_m3u_playlist(filename, model);
            else if(g_ascii_strcasecmp(mime, "audio/x-scpls") == 0)
                cont = audiocd_import_pls_playlist(filename, model);
            else 
                cont = audiocd_add_file(filename, model);
            g_free(mime);
        }
        g_free(filename);       
        file = strtok(NULL, "\n");
    }
    
    gnomebaker_show_busy_cursor(FALSE);
}


void 
audiocd_remove()
{
    GB_LOG_FUNC
    gnomebaker_show_busy_cursor(TRUE);
    
    GtkWidget *audiotree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
    GtkTreeModel* filemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(audiotree));    
    
    GList *rr_list = g_list_alloc();    /* list of GtkTreeRowReferences to remove */    
    
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(audiotree));
    gtk_tree_selection_selected_foreach(selection, audiocd_foreach_fileselection, &rr_list);        
    
    GList *node = rr_list;
    for (; node != NULL ; node = node->next)
    {
        if(node->data)
        {
            GtkTreePath* path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);
            
            if (path)
            {
                GB_DECLARE_STRUCT(GtkTreeIter, iter);
                if (gtk_tree_model_get_iter(filemodel, &iter, path))
                {
                    MediaInfo* info = NULL;
                    gtk_tree_model_get (filemodel, &iter, AUDIOCD_COL_INFO, &info, -1);
                    audiocd_update_progress_bar(FALSE, (gdouble)info->duration);  
                    gtk_list_store_remove(GTK_LIST_STORE(filemodel), &iter);
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
    
    GtkWidget *audiotree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
    GtkTreeModel* filemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(audiotree));    
    GtkWidget* progbar = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_progressbar);
    
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), _("0 mins 0 secs"));
    gnomebaker_enable_widget(widget_audiocd_create, FALSE);

    /* Now get each row and free the MediaInfo* we store in each */    
    GtkTreeIter iter;
    if(gtk_tree_model_get_iter_first(filemodel, &iter))
    {
        do
        {
            MediaInfo* info = NULL;
            gtk_tree_model_get (filemodel, &iter, AUDIOCD_COL_INFO, &info, -1);
            media_info_delete(info);
        } while (gtk_tree_model_iter_next(filemodel, &iter));
    }    
    gtk_list_store_clear(GTK_LIST_STORE(filemodel));
    gnomebaker_show_busy_cursor(FALSE); 
}


void
audiocd_new()
{
    GB_LOG_FUNC
    
    GtkTreeView *filelist = 
        GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree));
    g_return_if_fail(filelist != NULL);
    
    /* Create the list store for the file list */
    GtkListStore *store = gtk_list_store_new(AUDIOCD_NUM_COLS, 
        GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, /*G_TYPE_ULONG,*/
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    gtk_tree_view_set_model(filelist, GTK_TREE_MODEL(store));
    g_object_unref(store);

    /* One column which has an icon renderer and text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Track"));
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", AUDIOCD_COL_ICON, NULL);
    
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_FILE, NULL);
    gtk_tree_view_append_column(filelist, col);

    /* Second column to display the duration*/
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Duration"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_DURATION, NULL);
    gtk_tree_view_append_column(filelist, col);
    
    /* Third column to display the size 
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Size");
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_SIZE, NULL);
    gtk_tree_view_append_column(filelist, col);*/
        
    /* column to display the artist */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Artist"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_ARTIST, NULL);
    gtk_tree_view_append_column(filelist, col);
    
    /* column to display the album */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Album"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_ALBUM, NULL);
    gtk_tree_view_append_column(filelist, col);
    
    /* column to display the title */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Title"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_TITLE, NULL);
    gtk_tree_view_append_column(filelist, col);
    
    /* hidden column to store the MediaInfo pointer */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Info"));
    gtk_tree_view_append_column(filelist, col);
    gtk_tree_view_column_set_visible(col, FALSE);
    
    
    /* Set the selection mode of the file list */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(filelist), GTK_SELECTION_MULTIPLE);

    /* Enable the file list as a drag destination */    
    gtk_drag_dest_set(GTK_WIDGET(filelist), GTK_DEST_DEFAULT_ALL,
        targetentries, 3, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

    /* Connect the function to handle the drag data */
    g_signal_connect(filelist, "drag_data_received",
    G_CALLBACK(audiocd_on_drag_data_received), store);
    
    /* connect the signal to handle right click */
    g_signal_connect (G_OBJECT(filelist), "button-press-event",
        G_CALLBACK(audiocd_on_button_pressed), NULL);
        
    g_signal_connect(G_OBJECT(filelist), "row-activated", 
        G_CALLBACK(audiocd_on_list_dbl_click), NULL);
        
    GtkWidget* optmenu = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_size);
    gbcommon_populate_disk_size_option_menu(GTK_OPTION_MENU(optmenu), audiodisksizes, 
        (sizeof(audiodisksizes)/sizeof(DiskSize)), 2);
}

