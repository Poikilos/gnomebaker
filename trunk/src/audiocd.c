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
#include "burn.h"
#include <stdio.h>


gdouble audiocdsize = 0.0;

DiskSize audiodisksizes[] = 
{
	{22, "22 min. CD"},
	{74, "74 min. CD"},
	{80, "80 min. CD"}
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


gint 
audiocd_get_audiocd_size()
{
	GB_LOG_FUNC
	GtkWidget* optmen = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_size);
	g_return_val_if_fail(optmen != NULL, 0);
	audiocdsize = audiodisksizes[gtk_option_menu_get_history(GTK_OPTION_MENU(optmen))].size;	
	return audiocdsize;
}


gboolean 
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
		gnomebaker_show_msg_dlg(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
			_("Track is too large to fit in the remaining space on the CD"));
		ok = FALSE;
	}

	return ok;
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
audiocd_on_remove_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC	
	audiocd_remove();
}


void
audiocd_clear()
{
	GB_LOG_FUNC
	
	gnomebaker_show_busy_cursor(TRUE);
	
	GtkWidget *audiotree = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_tree);
	GtkTreeModel* filemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(audiotree));	
	gtk_list_store_clear(GTK_LIST_STORE(filemodel));
	
	GtkWidget* progbar = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_progressbar);
	
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), _("0 mins 0 secs"));
	gnomebaker_enable_widget(widget_audiocd_create, FALSE);
	
	gnomebaker_show_busy_cursor(FALSE);	
}


void 
audiocd_on_clear_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC
	audiocd_clear();
}


gboolean
audiocd_on_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GB_LOG_FUNC

	/* look for a right click */	
	if(event->button == 3)
	{
		GtkWidget* menu = gtk_menu_new();	
		
		GtkWidget* menuitem = gtk_menu_item_new_with_label(_("Remove selected"));	
		g_signal_connect(menuitem, "activate",
			(GCallback)audiocd_on_remove_clicked, widget);	
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);	
		
		menuitem = gtk_menu_item_new_with_label(_("Clear"));	
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

	const gchar* file = strtok((gchar*)selection->data, "\n");
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
					AUDIOCD_COL_TITLE, info->title->str, -1);
				
				g_object_unref(icon);
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
	
	gnomebaker_show_busy_cursor(FALSE);
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
    audiocd_add_selection(seldata);
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
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
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
		
	GtkWidget* optmenu = glade_xml_get_widget(gnomebaker_getxml(), widget_audiocd_size);
	gbcommon_populate_disk_size_option_menu(GTK_OPTION_MENU(optmenu), audiodisksizes, 
		(sizeof(audiodisksizes)/sizeof(DiskSize)), 2);
}


void 
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


void
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
