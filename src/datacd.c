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
 * File: datacd.h
 * Created by: Luke Biddell <Email>
 * Created on: Sun May  9 15:16:08 2004
 */

#include "datacd.h"
#include "gnomebaker.h"
#include <sys/stat.h>
#include "filebrowser.h"
#include "gbcommon.h"
#include "devices.h"
#include "preferences.h"
#include "exec.h"


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
datacd_contents_cell_edited(GtkCellRendererText *cell,
							gchar* path_string,
							gchar* new_text,
							gpointer user_data)
{
	GB_LOG_FUNC	
	g_return_if_fail(cell != NULL);
	g_return_if_fail(path_string != NULL);
	g_return_if_fail(new_text != NULL);
	g_return_if_fail(user_data != NULL);
		
	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(user_data), &iter, path_string))
	{
		GValue val = {0};
		g_value_init(&val, G_TYPE_STRING);
		g_value_set_string(&val, new_text);

		gtk_list_store_set_value(GTK_LIST_STORE(user_data), &iter, DATACD_COL_FILE, &val);
		
		g_value_unset(&val);
	}
}


gboolean
datacd_on_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GB_LOG_FUNC

	/* look for a right click */	
	if(event->button == 3)
	{
		GtkWidget* menu = gtk_menu_new();	
		
		GtkWidget* menuitem = gtk_menu_item_new_with_label("Remove selected");	
		g_signal_connect(menuitem, "activate",
			(GCallback)datacd_on_remove_clicked, widget);	
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);	
		
		menuitem = gtk_menu_item_new_with_label("Clear");	
		g_signal_connect(menuitem, "activate",
			(GCallback)datacd_on_clear_clicked, widget);	
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
datacd_setup_list(GtkTreeView * filelist)
{
	GB_LOG_FUNC
	g_return_if_fail(filelist != NULL);
	
	/* Create the list store for the file list */
    GtkListStore *store = gtk_list_store_new(DATACD_NUM_COLS, G_TYPE_STRING, 
			G_TYPE_STRING, G_TYPE_ULONG, G_TYPE_STRING);
    gtk_tree_view_set_model(filelist, GTK_TREE_MODEL(store));
    g_object_unref(store);

	/* First column which has an icon renderer and a text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Contents");
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "stock-id", DATACD_COL_ICON, NULL);
	
	GValue value = { 0 };
	g_value_init(&value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&value, TRUE);	

    renderer = gtk_cell_renderer_text_new();
	g_object_set_property(G_OBJECT(renderer), "editable", &value);
	g_signal_connect(renderer, "edited", (GCallback)datacd_contents_cell_edited, 
		(gpointer)store);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_COL_FILE, NULL);
	gtk_tree_view_append_column(filelist, col);	
	
	g_value_unset(&value);
	
	/* Second column to display the file/dir size */
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Size");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_COL_SIZE, NULL);
	gtk_tree_view_append_column(filelist, col);
	
	/* Third column for the full path of the file/dir */
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Full Path");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_COL_PATH, NULL);
	gtk_tree_view_append_column(filelist, col);
	
	/* Set the selection mode of the file list */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(filelist),
		GTK_SELECTION_MULTIPLE /*GTK_SELECTION_BROWSE*/);

	/* Enable the file list as a drag destination */	
    gtk_drag_dest_set(GTK_WIDGET(filelist), GTK_DEST_DEFAULT_ALL,
		targetentries, 3, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

	/* Connect the function to handle the drag data */
    g_signal_connect(filelist, "drag_data_received",
		G_CALLBACK(datacd_on_drag_data_received), store);
	
	/* connect the signal to handle right click */
	g_signal_connect (G_OBJECT(filelist), "button-press-event",
        G_CALLBACK(datacd_on_button_pressed), NULL);
}


gboolean 
datacd_update_progress_bar(gboolean add, gdouble filesize)
{
	GB_LOG_FUNC
	gboolean ok = TRUE;
	
	/* Now update the progress bar with the cd size */
	GladeXML* xml = gnomebaker_getxml();
	g_return_val_if_fail(xml != NULL, FALSE);
	
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_datacd_progressbar);
	g_return_val_if_fail(progbar != NULL, FALSE);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));
	
	gint cdsize = gnomebaker_get_datacd_size();
	
	gdouble currentguchars = fraction * cdsize * 1024 * 1024;
	
	if(add)
		currentguchars += filesize;
	else
		currentguchars -= filesize;
	
	fraction = currentguchars / (cdsize * 1024 * 1024);
	
	g_message( "File size %f Fraction is %f", filesize, fraction);
	
	if(fraction < 0.0 || fraction == -0.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), "0%");
		
		/* disable the create button as there's nothing on the disk */
		gnomebaker_enable_widget(widget_datacd_create, FALSE);
		
		/* remove the multisession flag as there's nothing on the disk */
		GtkWidget* datatree = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree);
		GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(datatree)));	
		g_free((gchar*)g_object_get_data(G_OBJECT(model), DATACD_EXISTING_SESSION));
		g_object_set_data(G_OBJECT(model), DATACD_EXISTING_SESSION, NULL);
	}	
	/* If the file is too large then we don't allow the user to add it */
	else if(fraction <= 1.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);
		
		gchar* buf = g_strdup_printf("%d%%", (gint)(fraction * 100));
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
		g_free(buf);
		gnomebaker_enable_widget(widget_datacd_create, TRUE);
	}
	else
	{
		gnomebaker_show_msg_dlg(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
			"File or directory is too large to fit in the remaining space on the CD");
		ok = FALSE;
	}
	
	return ok;
}


gboolean  
datacd_add_to_compilation(const gchar* file, GtkListStore* liststore, gboolean existingsession)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, FALSE);
	g_return_val_if_fail(liststore != NULL, FALSE);
	gboolean ret = TRUE;
	
	gchar* filename = gbcommon_tidy_nautilus_dnd_file(file);
		
	GB_DECLARE_STRUCT(struct stat, s);
	gint statret = stat(filename, &s);
	if(statret == 0)
	{
		gulong size = s.st_size;				
		if(s.st_mode & S_IFDIR)
			size = gbcommon_calc_dir_size(filename);
		
		if(datacd_update_progress_bar(TRUE, (gdouble)size))
		{					
			GB_DECLARE_STRUCT(GtkTreeIter, iter);
			gtk_list_store_append(liststore, &iter);						
			gchar* basename = g_path_get_basename(filename);
						
			gtk_list_store_set(liststore, &iter, DATACD_COL_ICON, 
				existingsession ? DATACD_EXISTING_SESSION_ICON : 
					((s.st_mode & S_IFDIR) ? GTK_STOCK_OPEN : GTK_STOCK_DND), 
				DATACD_COL_FILE, basename, DATACD_COL_SIZE, size, 
				DATACD_COL_PATH, filename, -1);
			
			g_free(basename);
		}
		else
		{				
			ret = FALSE;
		}
	}
	else
	{
		g_warning("failed to stat file [%s] with ret code [%d] error no [%s]", 
			filename, statret, strerror(errno));
		
		ret = FALSE;
	}
	
	g_free(filename);
	return ret;
}


void
datacd_on_drag_data_received(
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
	
	gnomebaker_show_busy_cursor(TRUE);	    	
	gnomebaker_update_status("Calculating size");
	
	GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(widget)));

	g_message( "received sel %s", seldata->data);	
	const gchar* file = strtok((gchar*)seldata->data,"\n");
	while(file != NULL)
	{
		if(!datacd_add_to_compilation(file, model, FALSE)) 
			break;
		file = strtok(NULL, "\n");
	}
	
	gnomebaker_show_busy_cursor(FALSE);
	gnomebaker_update_status("");
}


void
datacd_foreach_fileselection(GtkTreeModel *filemodel,
								  GtkTreePath *path,
								  GtkTreeIter *iter,
								  gpointer userdata)
{
	GB_LOG_FUNC
	g_return_if_fail(filemodel != NULL);
	g_return_if_fail(iter != NULL);
	g_return_if_fail(userdata != NULL);
	
	GList** rowref_list = (GList**)userdata;	
	GtkTreeRowReference* ref = gtk_tree_row_reference_new(filemodel, path);
	if(ref != NULL)
    	*rowref_list = g_list_append(*rowref_list, ref);
}


void
datacd_on_remove_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC	
			
	gnomebaker_show_busy_cursor(TRUE);
	
	GtkWidget *datatree = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree);	
	GtkTreeModel* filemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(datatree));
	
	GList *rr_list = g_list_alloc();    /* list of GtkTreeRowReferences to remove */    
	
	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(datatree));
	gtk_tree_selection_selected_foreach(selection, datacd_foreach_fileselection, &rr_list);		
	
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
					GValue iconvalue = { 0 };
					gtk_tree_model_get_value(filemodel, &iter, DATACD_COL_ICON, &iconvalue);
					const gchar* icon = g_value_get_string(&iconvalue);
					if(g_ascii_strcasecmp(icon, DATACD_EXISTING_SESSION_ICON) != 0)
					{										
						GValue value = { 0 };
						gtk_tree_model_get_value(filemodel, &iter, DATACD_COL_SIZE, &value);
						
						datacd_update_progress_bar(FALSE, (gdouble)g_value_get_ulong(&value));
						
						g_value_unset(&value);	
						gtk_list_store_remove(GTK_LIST_STORE(filemodel), &iter);
					}
					g_value_unset(&iconvalue);	
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
datacd_on_clear_clicked(GtkWidget *menuitem, gpointer userdata)
{	
	GB_LOG_FUNC

	gnomebaker_show_busy_cursor(TRUE);
	
	GladeXML* xml = gnomebaker_getxml();
	GtkWidget *datatree = glade_xml_get_widget(xml, widget_datacd_tree);	
	GtkTreeModel* filemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(datatree));
	gtk_list_store_clear(GTK_LIST_STORE(filemodel));

	GtkWidget* progbar = glade_xml_get_widget(xml, widget_datacd_progressbar);
	
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), "0%");
	gnomebaker_enable_widget(widget_datacd_create, FALSE);
	
	gnomebaker_show_busy_cursor(FALSE);
}


gboolean 
datacd_get_msinfo(gchar** msinfo)
{
	GB_LOG_FUNC
	gboolean ok = FALSE;
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	gchar* msinfocmd = g_strdup_printf("cdrecord -msinfo dev=%s", writer);
	GString* output = exec_run_cmd(msinfocmd);
	
	gint start = 0, end = 0;
	if((output == NULL ) || (sscanf(output->str, "%d,%d\n", &start, &end) != 2))
	{
		gchar* message = g_strdup_printf("Error getting session information.\n\n%s", 
			output != NULL ? output->str : "unknown error");
		gnomebaker_show_msg_dlg(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
		g_free(message);
	}
	else
	{
		*msinfo = g_strdup_printf("%d,%d", start, end);
		g_message("datacd next session is [%s]", *msinfo);		
		ok = TRUE;
	}
	
	g_string_free(output, TRUE);
	g_free(writer);
	g_free(msinfocmd);
	return ok;	
}


void 
datacd_import_session()
{
	GB_LOG_FUNC
	
	gnomebaker_show_busy_cursor(TRUE);
	
	datacd_on_clear_clicked(NULL, NULL);

	gchar* mountpoint = NULL;	
	gchar* msinfo = NULL;
	if(!datacd_get_msinfo(&msinfo))
	{
		g_critical("Error getting msinfo");
	}
	else if(!devices_mount_device(GB_WRITER, &mountpoint))
	{
		g_critical("Error mounting writer device");
	}
	else	
	{							
		/* try and open the mountpoint and read in the cd contents */
		GDir *dir = g_dir_open(mountpoint, 0, NULL);	
		if(dir != NULL)
		{
			GtkWidget* datatree = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree);
			GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(datatree)));		
			g_object_set_data(G_OBJECT(model), DATACD_EXISTING_SESSION, g_strdup(msinfo));
			
			/* Make sure we turn multisession on in preferences */
			preferences_set_bool(GB_MULTI_SESSION, TRUE);
			
			const gchar *name = g_dir_read_name(dir);					
			while(name != NULL)
			{
				gchar* fullname = g_build_filename(mountpoint, name, NULL);
				if(!datacd_add_to_compilation(fullname, model, TRUE))
					break;
				g_free(fullname);
				name = g_dir_read_name(dir);				
			}
	
			g_dir_close(dir);
		}
		else
		{
			gchar* message = g_strdup_printf("Error importing session from [%s]. "
				"Please check the device configuration in preferences.", mountpoint);
			gnomebaker_show_msg_dlg(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
			g_free(message);			
		}				
		
		if(!devices_mount_device(GB_WRITER, NULL))
			g_critical("Error unmounting writer device");	
	}
	
	g_free(mountpoint);	
	g_free(msinfo);
	
	gnomebaker_show_busy_cursor(FALSE);
}
