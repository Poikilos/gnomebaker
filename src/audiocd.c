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
 * Created by: luke_biddell@yahoo.com
 * Created on: Fri May  7 22:37:25 2004
 */
 
#include "audiocd.h"
#include "gnomebaker.h"
#include "gbcommon.h"
#include "audioinfo.h"
#include <stdio.h>



gboolean audiocd_on_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
gboolean audiocd_update_progress_bar(gboolean add, gdouble filesize);


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


void
audiocd_setup_list(GtkTreeView * filelist)
{
	GB_LOG_FUNC
	g_return_if_fail(filelist != NULL);
	
	/* Create the list store for the file list */
    GtkListStore *store = gtk_list_store_new(AUDIOCD_NUM_COLS, 
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, /*G_TYPE_ULONG,*/
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model(filelist, GTK_TREE_MODEL(store));
    /*g_object_unref(store);*/

	/* One column which has an icon renderer and text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Track");
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "stock-id", AUDIOCD_COL_ICON, NULL);
	
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_FILE, NULL);
    gtk_tree_view_append_column(filelist, col);

	/* Second column to display the duration*/
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Duration");
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
	gtk_tree_view_column_set_title(col, "Artist");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_ARTIST, NULL);
	gtk_tree_view_append_column(filelist, col);
	
	/* column to display the album */
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Album");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_ALBUM, NULL);
	gtk_tree_view_append_column(filelist, col);
	
	/* column to display the title */
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Title");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", AUDIOCD_COL_TITLE, NULL);
	gtk_tree_view_append_column(filelist, col);
	
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
}


void
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
    g_return_if_fail(seldata != NULL);
	g_return_if_fail(seldata->data != NULL);
    g_return_if_fail(GTK_IS_TREE_VIEW(widget));	
	
	const gchar* file = strtok((gchar*)seldata->data, "\n");
	while(file != NULL)
	{
		/* Get the file name that's been dropped and if there's a 
	   	   file url at the start then strip it off */	
		gchar* filename = gbcommon_tidy_nautilus_dnd_file(file);
		
		AudioInfo* info = audioinfo_new(filename);
		if(info != NULL)
		{
			if(audiocd_update_progress_bar(TRUE, (gdouble)info->duration))
			{
				GtkTreeView *view = GTK_TREE_VIEW(widget);
				GtkTreeModel *model = gtk_tree_view_get_model(view);
			
				if(GTK_IS_LIST_STORE(model))
				{
					GB_DECLARE_STRUCT(GtkTreeIter, iter);		
					gtk_list_store_append(GTK_LIST_STORE(model), &iter);
					gtk_list_store_set(
						GTK_LIST_STORE(model), &iter, 
						AUDIOCD_COL_ICON, GNOME_STOCK_MIDI, 
						AUDIOCD_COL_FILE, (gchar*)filename, 
						AUDIOCD_COL_DURATION, info->formattedduration->str,
						/*AUDIOCD_COL_SIZE, info->filesize,*/
						AUDIOCD_COL_ARTIST, info->artist->str, 
						AUDIOCD_COL_ALBUM, info->album->str,
						AUDIOCD_COL_TITLE, info->title->str, -1);
				}
			}
			else
			{
				audioinfo_delete(info);
				break;
			}
			
			audioinfo_delete(info);
		}
		
		g_free(filename);
		
		file = strtok(NULL, "\n");
	}
}



void
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


void 
audiocd_on_remove_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC	
	g_return_if_fail(userdata != NULL);
	
	gnomebaker_show_busy_cursor(TRUE);
	
	GtkTreeView* fileview = GTK_TREE_VIEW(userdata);
	GtkTreeModel* filemodel = gtk_tree_view_get_model(fileview);
	
	GList *rr_list = g_list_alloc();    /* list of GtkTreeRowReferences to remove */    
	
	GtkTreeSelection* selection = gtk_tree_view_get_selection(fileview);
	gtk_tree_selection_selected_foreach(selection, audiocd_foreach_fileselection, &rr_list);		
	
	GList *node = rr_list;
	while(node != NULL)
	{
		if(node->data)
		{
			GtkTreePath* path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);
			
			if (path)
			{
				GB_DECLARE_STRUCT(GtkTreeIter, iter);
				if (gtk_tree_model_get_iter(filemodel, &iter, path))
				{
					GValue value = { 0 };
					gtk_tree_model_get_value(filemodel, &iter, AUDIOCD_COL_DURATION, &value);
					
					/* Naff bit to turn our formatted duration back into
					   seconds. Probably should store the AudioInfo structure
					   against the row in the list and use that to get the 
						duration. */
					gint duration = 0;
					static const gint multipliers[2] = {60, 1};
					int index = 0;
					gchar* time = g_strdup(g_value_get_string(&value));
					const gchar* part = strtok(time, ":");
					while(part != NULL)
					{
						duration += multipliers[index] * atoi(part);					
						part = strtok(NULL, ":");
						index++;
					}
					
					audiocd_update_progress_bar(FALSE, (gdouble)duration);					
					
					g_free(time);
					g_value_unset(&value);
					gtk_list_store_remove(GTK_LIST_STORE(filemodel), &iter);
				}			
				/* FIXME/CHECK: Do we need to free the path here? */
			}
		}
		node = node->next;
	}
	
	g_list_foreach(rr_list, (GFunc)gtk_tree_row_reference_free, NULL);
	g_list_free(rr_list);		
	
	gnomebaker_show_busy_cursor(FALSE);
	
}


void 
audiocd_on_clear_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC
	g_return_if_fail(userdata != NULL);
	gnomebaker_show_busy_cursor(TRUE);
	
	GtkTreeView* fileview = GTK_TREE_VIEW(userdata);
	GtkTreeModel* filemodel = gtk_tree_view_get_model(fileview);	
	gtk_list_store_clear(GTK_LIST_STORE(filemodel));
	
	GladeXML* xml = gnomebaker_getxml();
	g_return_if_fail(xml != NULL);
	
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_audiocd_progressbar);
	g_return_if_fail(progbar != NULL);
	
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), "0 mins 0 secs");
	gnomebaker_enable_widget(widget_audiocd_create, FALSE);
	
	gnomebaker_show_busy_cursor(FALSE);	
}


gboolean
audiocd_on_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GB_LOG_FUNC

	/* look for a right click */	
	if(event->button == 3)
	{
		GtkWidget* menu = gtk_menu_new();	
		
		GtkWidget* menuitem = gtk_menu_item_new_with_label("Remove selected");	
		g_signal_connect(menuitem, "activate",
			(GCallback)audiocd_on_remove_clicked, widget);	
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);	
		
		menuitem = gtk_menu_item_new_with_label("Clear");	
		g_signal_connect(menuitem, "activate",
			(GCallback)audiocd_on_clear_clicked, widget);	
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);	
		
		gtk_widget_show_all(menu);
	
		/* Note: event can be NULL here when called. However,
		 *  gdk_event_get_time() accepts a NULL argument */
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
					   (event != NULL) ? event->button : 0,
					   gdk_event_get_time((GdkEvent*)event));
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


gboolean 
audiocd_update_progress_bar(gboolean add, gdouble seconds)
{
	GB_LOG_FUNC
	gboolean ok = TRUE;
	
	/* Now update the progress bar with the cd size */
	GladeXML* xml = gnomebaker_getxml();
	g_return_val_if_fail(xml != NULL, FALSE);
	
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_audiocd_progressbar);
	g_return_val_if_fail(progbar != NULL, FALSE);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));
	
	gint cdsize = gnomebaker_get_audiocd_size();
	
	gdouble currentsecs = fraction * cdsize * 60;
	
	if(add)
		currentsecs += seconds;
	else
		currentsecs -= seconds;
	
	fraction = currentsecs / (cdsize * 60);
	
	g_message( "Duration %f Fraction is %f", seconds, fraction);
	
	if(fraction < 0.0 || fraction == -0.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), "0 mins 0 secs");
		gnomebaker_enable_widget(widget_audiocd_create, FALSE);
	}	
	/* If the file is too large then we don't allow the user to add it */
	else if(fraction <= 1.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);

		gint ss = ((gint)currentsecs)%60;
		gint m = (((gint)currentsecs)-ss)/60;
		
		gchar* buf = g_strdup_printf("%d mins %d secs", m, ss);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
		g_free(buf);
		gnomebaker_enable_widget(widget_audiocd_create, TRUE);
	}
	else
	{
		gnomebaker_show_msg_dlg(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
			"Track is too large to fit in the remaining space on the CD");
		ok = FALSE;
	}

	return ok;
}
