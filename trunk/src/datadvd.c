/***************************************************************************
 *            datadvd.c
 *
 *  Sat Nov 27 15:27:10 2004
 *  Copyright  2004  Christoffer Sørensen	
 *  Email	christoffer@curo.dk
 ****************************************************************************/

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
 
#include "datadvd.h"
#include "gnomebaker.h"
#include <sys/stat.h>
#include "filebrowser.h"
#include "gbcommon.h"


gboolean
datadvd_on_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

gboolean 
datadvd_update_progress_bar(gboolean add, gdouble filesize);


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
datadvd_setup_list(GtkTreeView * filelist)
{
	GB_LOG_FUNC
	/* Create the list store for the file list */
    GtkListStore *store = gtk_list_store_new(DATADVD_NUM_COLS, G_TYPE_STRING, 
			G_TYPE_STRING, G_TYPE_ULONG, G_TYPE_STRING);
    gtk_tree_view_set_model(filelist, GTK_TREE_MODEL(store));
    g_object_unref(store);

	/* First column which has an icon renderer and a text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Contents");
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "stock-id", DATADVD_COL_ICON, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATADVD_COL_FILE, NULL);
	gtk_tree_view_append_column(filelist, col);	
	
	/* Second column to display the file/dir size */
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Size");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATADVD_COL_SIZE, NULL);
	gtk_tree_view_append_column(filelist, col);
	
	/* Third column for the full path of the file/dir */
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Full Path");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATADVD_COL_PATH, NULL);
	gtk_tree_view_append_column(filelist, col);

	
	/* Set the selection mode of the file list */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(filelist),
		GTK_SELECTION_MULTIPLE /*GTK_SELECTION_BROWSE*/);

	/* Enable the file list as a drag destination */	
    gtk_drag_dest_set(GTK_WIDGET(filelist), GTK_DEST_DEFAULT_ALL,
		targetentries, 3, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

	/* Connect the function to handle the drag data */
    g_signal_connect(filelist, "drag_data_received",
		G_CALLBACK(datadvd_on_drag_data_received), store);
	
	/* connect the signal to handle right click */
	g_signal_connect (G_OBJECT(filelist), "button-press-event",
        G_CALLBACK(datadvd_on_button_pressed), NULL);
}


void datadvd_on_drag_data_received(
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
	gnomebaker_show_busy_cursor(TRUE);	    
	
	gnomebaker_update_status("Calculating size");

    g_return_if_fail(seldata != NULL);
	g_return_if_fail(seldata->data != NULL);
    g_return_if_fail(GTK_IS_TREE_VIEW(widget));
		
	g_message( "received sel %s", seldata->data);
	
	const gchar* file = strtok((gchar*)seldata->data,"\n");
	while(file != NULL)
	{
		g_message( "%s", file);
		
		gchar* filename = gbcommon_tidy_nautilus_dnd_file(file);
		
		struct stat s;
		if(stat(filename, &s) == 0)
		{
			gulong size = s.st_size;
				
			if(s.st_mode & S_IFDIR)
				size = gbcommon_calc_dir_size(filename);			
			
			if(datadvd_update_progress_bar(TRUE, (gdouble)size))
			{					
				/* Get the model and add the file to it */
				GtkTreeView *view = GTK_TREE_VIEW(widget);
				GtkTreeModel *model = gtk_tree_view_get_model(view);
			
				if(GTK_IS_LIST_STORE(model))
				{		
					GtkTreeIter iter;
					gtk_list_store_append(GTK_LIST_STORE(model), &iter);										
					gchar* basename = g_path_get_basename(filename);
					
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, DATADVD_COL_ICON, 
						(s.st_mode & S_IFDIR) ? GTK_STOCK_OPEN : GTK_STOCK_DND, 
						DATADVD_COL_FILE, basename, DATADVD_COL_SIZE, size, DATADVD_COL_PATH, filename, -1);
					
					g_free(basename);
				}		
			}
			else
			{				
				break;
			}
		}
		
		g_free(filename);
	
		file = strtok(NULL, "\n");
	}
	
	gnomebaker_show_busy_cursor(FALSE);
	gnomebaker_update_status("");
}


gboolean 
datadvd_update_progress_bar(gboolean add, gdouble filesize)
{
	GB_LOG_FUNC
	gboolean ok = TRUE;
	
	/* Now update the progress bar with the DVD size */
	GladeXML* xml = gnomebaker_getxml();
	g_return_val_if_fail(xml != NULL, FALSE);
	
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_datadvd_progressbar);
	g_return_val_if_fail(progbar != NULL, FALSE);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));
	
	gint dvdsize = gnomebaker_get_datadvd_size();
	
	gdouble gdvdsize = (gdouble)(dvdsize) * (gdouble)1024 * (gdouble) 1024;
	
	
	gdouble currentguchars = fraction * gdvdsize;
	
	if(add)
		currentguchars += filesize;
	else
		currentguchars -= filesize;
	
	fraction = currentguchars / gdvdsize;
	
	g_message( "gdvdsize %f",gdvdsize);
	
	g_message( "File size %f Fraction is %f", filesize, fraction);
	
	if(fraction < 0.0 || fraction == -0.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), "0%");
		gnomebaker_enable_widget(widget_datacd_create, FALSE);
	}	
	/* If the file is too large then we don't allow the user to add it */
	else if(fraction <= 1.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);
		
		gchar* buf = g_strdup_printf("%d%%", (gint)(fraction * 100));
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
		g_free(buf);
		gnomebaker_enable_widget(widget_datadvd_create, TRUE);
	}
	else
	{
		gnomebaker_show_msg_dlg(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
			"File or directory is too large to fit in the remaining space on the DVD");
		ok = FALSE;
	}
	
	return ok;
}


gboolean
datadvd_on_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GB_LOG_FUNC

	/* look for a right click */	
	if(event->button == 3)
	{
		GtkWidget* menu = gtk_menu_new();	
		
		GtkWidget* menuitem = gtk_menu_item_new_with_label("Remove selected");	
		g_signal_connect(menuitem, "activate",
			(GCallback)datadvd_on_remove_clicked, widget);	
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);	
		
		menuitem = gtk_menu_item_new_with_label("Clear");	
		g_signal_connect(menuitem, "activate",
			(GCallback)datadvd_on_clear_clicked, widget);	
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
datadvd_foreach_fileselection(GtkTreeModel *filemodel,
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
datadvd_on_remove_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC
	
	g_return_if_fail(userdata != NULL);
	gnomebaker_show_busy_cursor(TRUE);
	
	GtkTreeView* fileview = GTK_TREE_VIEW(userdata);
	GtkTreeModel* filemodel = gtk_tree_view_get_model(fileview);
	
	GList *rr_list = g_list_alloc();    /* list of GtkTreeRowReferences to remove */    
	
	GtkTreeSelection* selection = gtk_tree_view_get_selection(fileview);
	gtk_tree_selection_selected_foreach(selection, datadvd_foreach_fileselection, &rr_list);		
	
	GList *node = rr_list;
	while(node != NULL)
	{
		if(node->data)
		{
			GtkTreePath* path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);
			
			if (path)
			{
				GtkTreeIter iter;			
				if (gtk_tree_model_get_iter(filemodel, &iter, path))
				{
					GValue value = { 0 };
					gtk_tree_model_get_value(filemodel, &iter, DATADVD_COL_SIZE, &value);
					
					datadvd_update_progress_bar(FALSE, (gdouble)g_value_get_ulong(&value));
					
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
datadvd_on_clear_clicked(GtkWidget *menuitem, gpointer userdata)
{	
	GB_LOG_FUNC
	g_return_if_fail(userdata != NULL);
	gnomebaker_show_busy_cursor(TRUE);
	
	GtkTreeView* fileview = GTK_TREE_VIEW(userdata);
	GtkTreeModel* filemodel = gtk_tree_view_get_model(fileview);	
	gtk_list_store_clear(GTK_LIST_STORE(filemodel));
	
	GladeXML* xml = gnomebaker_getxml();
	g_return_if_fail(xml != NULL);
	
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_datadvd_progressbar);
	g_return_if_fail(progbar != NULL);
	
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), "0%");
	gnomebaker_enable_widget(widget_datadvd_create, FALSE);
	
	gnomebaker_show_busy_cursor(FALSE);
}
