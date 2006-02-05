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
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun May  9 15:16:08 2004
 */

#include "datacd.h"
#include "gnomebaker.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "filebrowser.h"
#include "gbcommon.h"
#include "devices.h"
#include "preferences.h"
#include "exec.h"
#include "burn.h"
#include <unistd.h>
#include <string.h>
#include "selectdevicedlg.h"
#include <libgnomevfs/gnome-vfs-mime-utils.h>


static const gchar* const widget_datacd_tree = "treeview7";
static const gchar* const widget_datacd_size = "optionmenu1";
static const gchar* const widget_datacd_progressbar = "progressbar2";
static const gchar* const widget_datacd_create = "createDataCDBtn";


gdouble datadisksize = 0.0;
GtkCellRenderer* contentrenderer = NULL;

enum
{
    CD_200MB = 0,
    CD_650MB,
    CD_700MB,
    CD_800MB,
    DVD_4GB,
    DVD_8GB,
    DISK_SIZE_COUNT
};


static DiskSize datadisksizes[DISK_SIZE_COUNT] = 
{
    /* http://www.cdrfaq.org/faq07.html#S7-6 
        http://www.osta.org/technology/dvdqa/dvdqa6.htm */
	{94500.0 * 2048, "200MB CD"},
	{333000.0 * 2048, "650MB CD"},
	{360000.0 * 2048, "700MB CD"},
	{405000.0 * 2048, "800MB CD"},
	{2294922.0 * 2048, "4.7GB DVD"}, 
	{8.5 * 1000 * 1000 * 1000, "8.5GB DVD"} /* DVDs are salesman's MegaByte ie 1000 not 1024 */
};


enum
{
    TARGET_URI_LIST,
    TARGET_STRING,
    TARGET_COUNT
};


static GtkTargetEntry targetentries[] = 
{
    {"text/uri-list", 0, TARGET_URI_LIST},
    {"text/plain", 0, TARGET_STRING}
};


static void 
datacd_set_multisession(const gchar* msinfo)
{
	GB_LOG_FUNC
		
	GtkWidget* datatree = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree);
	GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(datatree)));	
	if(msinfo == NULL)
		g_free((gchar*)g_object_get_data(G_OBJECT(model), DATACD_EXISTING_SESSION));
	g_object_set_data(G_OBJECT(model), DATACD_EXISTING_SESSION, 
		msinfo == NULL ? NULL : g_strdup(msinfo));	
}


static void
datacd_on_show_humansize_changed(GConfClient *client,
                                   guint cnxn_id,
								   GConfEntry *entry,
								   gpointer user_data)
{
	GB_LOG_FUNC

	GtkTreeView* filelist = 
		GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree));
	g_return_if_fail(filelist != NULL);

	GtkTreeViewColumn* size_column = 
		gtk_tree_view_get_column(filelist, DATACD_COL_SIZE-1);
	g_return_if_fail(size_column != NULL);

	GtkTreeViewColumn* humansize_column = 
		gtk_tree_view_get_column(filelist, DATACD_COL_HUMANSIZE-1);
	g_return_if_fail(humansize_column != NULL);

	const gboolean showhumansize = preferences_get_bool(GB_SHOWHUMANSIZE);

	gtk_tree_view_column_set_visible(size_column, !showhumansize);
	gtk_tree_view_column_set_visible(humansize_column, showhumansize);
}


static gdouble 
datacd_get_datadisk_size()
{
	GB_LOG_FUNC
	GtkWidget* optmen = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_size);
	g_return_val_if_fail(optmen != NULL, 0);	
	datadisksize = datadisksizes[gtk_option_menu_get_history(GTK_OPTION_MENU(optmen))].size;
	return datadisksize;
}


static gchar*
datacd_format_progress_text(gdouble currentsize)
{
    GB_LOG_FUNC
    g_return_val_if_fail(currentsize < datadisksize, NULL);
    
    gchar* current = gbcommon_humanreadable_filesize((guint64)currentsize);
    gchar* remaining = gbcommon_humanreadable_filesize((guint64)(datadisksize - currentsize));
    gchar* buf = g_strdup_printf(_("%s used - %s remaining"), current, remaining);
    g_free(current);
    g_free(remaining);
    return buf;
}   


static gboolean 
datacd_update_progress_bar(gboolean add, guint64 filesize)
{
	GB_LOG_FUNC
	gboolean ok = TRUE;
	
	/* Now update the progress bar with the cd size */
	GladeXML* xml = gnomebaker_getxml();
	g_return_val_if_fail(xml != NULL, FALSE);
	
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_datacd_progressbar);
	g_return_val_if_fail(progbar != NULL, FALSE);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));
	const gdouble disksize = datacd_get_datadisk_size();
	gdouble currentsize = fraction * disksize;
	
	if(add)
		currentsize += (gdouble)filesize;
	else
		currentsize -= (gdouble)filesize;
	
	fraction = currentsize / disksize;
    
	if(fraction < 0.0 || fraction == -0.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
		gchar* buf = datacd_format_progress_text(0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
        g_free(buf);
		
		/* disable the create button as there's nothing on the disk */
		gnomebaker_enable_widget(widget_datacd_create, FALSE);
		
		/* remove the multisession flag as there's nothing on the disk */
		datacd_set_multisession(NULL);
	}	
	/* If the file is too large then we don't allow the user to add it */
	else if(fraction <= 1.0)
	{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);		
        gchar* buf = datacd_format_progress_text(currentsize);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
        g_free(buf);
		gnomebaker_enable_widget(widget_datacd_create, TRUE);
	}
	else
	{
		gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
			_("File or directory is too large to fit in the remaining space on the CD"));
		ok = FALSE;
	}
	return ok;
}


static gboolean  
datacd_add_to_compilation(const gchar* file, GtkListStore* liststore, gboolean existingsession)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, FALSE);
	g_return_val_if_fail(liststore != NULL, FALSE);
	gboolean ret = TRUE;
	
	gchar* filename = gbcommon_get_local_path(file);
		
	GB_DECLARE_STRUCT(struct stat, s);
	gint statret = stat(filename, &s);
	if(statret == 0)
	{
		guint64 size = (guint64)s.st_size;				
		if(s.st_mode & S_IFDIR)
			size = gbcommon_calc_dir_size(filename);
		
		if(datacd_update_progress_bar(TRUE, size))
		{					
			GB_DECLARE_STRUCT(GtkTreeIter, iter);
			gtk_list_store_append(liststore, &iter);						
			gchar* basename = g_path_get_basename(filename);
			
			GdkPixbuf* icon = NULL;
			if(existingsession) 
			{
				icon = gbcommon_get_icon_for_name("gnome-dev-cdrom", 16);
			} 
			else 
			{
				gchar* mime = gbcommon_get_mime_type(filename);
				icon = gbcommon_get_icon_for_mime(mime, 16);
				g_free(mime);
			}
			gchar* humanreadable = gbcommon_humanreadable_filesize(size);
			gtk_list_store_set(liststore, &iter, DATACD_COL_ICON, icon,	DATACD_COL_FILE, basename, 
				DATACD_COL_SIZE, size, DATACD_COL_HUMANSIZE, humanreadable,
				DATACD_COL_PATH, filename, DATACD_COL_SESSION, existingsession, -1);
			g_free(humanreadable);
			g_object_unref(icon);
			g_free(basename);
		}
		else
		{				
			ret = FALSE;
		}
	}
	else
	{
		g_warning("datacd_add_to_compilation - failed to stat file [%s] with ret code [%d] error no [%s]", 
			filename, statret, strerror(errno));
		
		ret = FALSE;
	}
	
	g_free(filename);
	return ret;
}


void 
datacd_add_selection(GtkSelectionData* selection)
{
	GB_LOG_FUNC
	g_return_if_fail(selection != NULL);
	g_return_if_fail(selection->data != NULL);
	
	GtkTreeView *datatree = GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree));
	g_return_if_fail(datatree != NULL);			
	GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(datatree));
	g_return_if_fail(model != NULL);
	
	gnomebaker_show_busy_cursor(TRUE);	    	

	GB_TRACE("datacd_add_selection - received sel [%s]\n", selection->data);	
	const gchar* file = strtok((gchar*)selection->data,"\n");
	while(file != NULL)
	{
		if(!datacd_add_to_compilation(file, model, FALSE)) 
			break;
		file = strtok(NULL, "\n");
	}
	
	gnomebaker_show_busy_cursor(FALSE);	
}


static void
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
 	datacd_add_selection(seldata);
}


static void
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
datacd_remove()
{
	GB_LOG_FUNC
	gnomebaker_show_busy_cursor(TRUE);
	
	GtkWidget *datatree = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree);	
	GtkTreeModel* filemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(datatree));
	
	GList *rr_list = g_list_alloc();    /* list of GtkTreeRowReferences to remove */    
	
	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(datatree));
	gtk_tree_selection_selected_foreach(selection, datacd_foreach_fileselection, &rr_list);		
	
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
					GValue sessionvalue = { 0 };
					gtk_tree_model_get_value(filemodel, &iter, DATACD_COL_SESSION, &sessionvalue);
					if(g_value_get_boolean(&sessionvalue) == FALSE)
					{										
						GValue value = { 0 };
						gtk_tree_model_get_value(filemodel, &iter, DATACD_COL_SIZE, &value);
						
						datacd_update_progress_bar(FALSE, g_value_get_uint64(&value));
						
						g_value_unset(&value);	
						gtk_list_store_remove(GTK_LIST_STORE(filemodel), &iter);
					}
					g_value_unset(&sessionvalue);
				}			
				/* FIXME/CHECK: Do we need to free the path here? */
			}
		}
	}
	
	g_list_foreach(rr_list, (GFunc)gtk_tree_row_reference_free, NULL);
	g_list_free(rr_list);		
	
	gnomebaker_show_busy_cursor(FALSE);
}


static void
datacd_on_remove_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC	
	datacd_remove();		
}


void
datacd_clear()
{
	GB_LOG_FUNC
	gnomebaker_show_busy_cursor(TRUE);
	
	GladeXML* xml = gnomebaker_getxml();
	GtkWidget *datatree = glade_xml_get_widget(xml, widget_datacd_tree);	
	GtkTreeModel* filemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(datatree));
	gtk_list_store_clear(GTK_LIST_STORE(filemodel));

	GtkWidget* progbar = glade_xml_get_widget(xml, widget_datacd_progressbar);
	
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
	gchar* buf = datacd_format_progress_text(0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
    g_free(buf);
	gnomebaker_enable_widget(widget_datacd_create, FALSE);
	
	/* clear any multisession flags */	
	datacd_set_multisession(NULL);
	
	gnomebaker_show_busy_cursor(FALSE);
}


static void
datacd_on_clear_clicked(GtkWidget *menuitem, gpointer userdata)
{	
	GB_LOG_FUNC
	datacd_clear();
}


static void
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


static void 
datacd_on_edit(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(user_data != NULL);        
    /*g_signal_emit_by_name (contentrenderer, "edited", NULL, NULL);*/    
}


static gboolean
datacd_on_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GB_LOG_FUNC

	/* look for a right click */	
	if(event->button == 3)
	{
		GtkWidget* menu = gtk_menu_new();			
        GtkTreeView* view = (GtkTreeView*)widget;
		GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
		const gint count = gtk_tree_selection_count_selected_rows(selection);
		if(count == 1)
		{
			gbcommon_append_menu_item_stock(menu, _("_Edit name"), GTK_STOCK_DND, 
				(GCallback)datacd_on_edit, view);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
		}
		gbcommon_append_menu_item_stock(menu, _("_Remove selected"), GTK_STOCK_REMOVE, 
			(GCallback)datacd_on_remove_clicked, widget);	
		gbcommon_append_menu_item_stock(menu, _("Clear"), GTK_STOCK_CLEAR, 
			(GCallback)datacd_on_clear_clicked, widget);		
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


void
datacd_new()
{
	GB_LOG_FUNC
	
	GtkTreeView* filelist = GTK_TREE_VIEW(glade_xml_get_widget(
		gnomebaker_getxml(), widget_datacd_tree));
	g_return_if_fail(filelist != NULL);
	
	/* Create the list store for the file list */
    GtkListStore *store = gtk_list_store_new(DATACD_NUM_COLS, GDK_TYPE_PIXBUF, 
			G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_tree_view_set_model(filelist, GTK_TREE_MODEL(store));
    g_object_unref(store);

	/* First column which has an icon renderer and a text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Contents"));
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", DATACD_COL_ICON, NULL);
	
	GValue value = { 0 };
	g_value_init(&value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&value, TRUE);	

    contentrenderer = gtk_cell_renderer_text_new();
	g_object_set_property(G_OBJECT(contentrenderer), "editable", &value);
	g_signal_connect(contentrenderer, "edited", (GCallback)datacd_contents_cell_edited, 
		(gpointer)store);
    gtk_tree_view_column_pack_start(col, contentrenderer, TRUE);
    gtk_tree_view_column_set_attributes(col, contentrenderer, "text", DATACD_COL_FILE, NULL);
	gtk_tree_view_append_column(filelist, col);	
	
	g_value_unset(&value);

	const gboolean showhumansize = preferences_get_bool(GB_SHOWHUMANSIZE);
	
	/* Second column to display the file/dir size */
	col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_title(col, _("Size"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_COL_SIZE, NULL);
	gtk_tree_view_column_set_visible(col, !showhumansize);
	gtk_tree_view_append_column(filelist, col);

	/* Third column to display the human size of file/dir */
	col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_title(col, _("Size"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_COL_HUMANSIZE, NULL);
	gtk_tree_view_column_set_visible(col, showhumansize);
	gtk_tree_view_append_column(filelist, col);
	
	/* Fourth column for the full path of the file/dir */
	col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_title(col, _("Full Path"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_COL_PATH, NULL);
	gtk_tree_view_append_column(filelist, col);
	
	/* Fifth column for the session bool */
	col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_title(col, _("Session"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_COL_SESSION, NULL);
	gtk_tree_view_column_set_visible(col, FALSE);
	gtk_tree_view_append_column(filelist, col);

	/* Set the selection mode of the file list */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(filelist),
		GTK_SELECTION_MULTIPLE /*GTK_SELECTION_BROWSE*/);

	/* Enable the file list as a drag destination */	
    gtk_drag_dest_set(GTK_WIDGET(filelist), GTK_DEST_DEFAULT_ALL,
		targetentries, TARGET_COUNT, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

	/* Connect the function to handle the drag data */
    g_signal_connect(filelist, "drag_data_received",
		G_CALLBACK(datacd_on_drag_data_received), store);
	
	/* connect the signal to handle right click */
	g_signal_connect (G_OBJECT(filelist), "button-press-event",
        G_CALLBACK(datacd_on_button_pressed), NULL);

	preferences_register_notify(GB_SHOWHUMANSIZE, datacd_on_show_humansize_changed);
	
	GtkWidget* optmenu = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_size);
	gbcommon_populate_disk_size_option_menu(GTK_OPTION_MENU(optmenu), datadisksizes, 
		DISK_SIZE_COUNT, preferences_get_int(GB_DATA_DISK_SIZE));
}


static gboolean 
datacd_get_msinfo(gchar** msinfo)
{
	GB_LOG_FUNC
	gboolean ok = FALSE;
    devices_unmount_device(GB_WRITER);
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	gchar* msinfocmd = g_strdup_printf("cdrecord -msinfo dev=%s", writer);    
	gchar* output = NULL;
    exec_run_cmd(msinfocmd, &output);
	
	gint start = 0, end = 0;
	if((output == NULL ) || (sscanf(output, "%d,%d\n", &start, &end) != 2))
	{
		gchar* message = g_strdup_printf(_("Error getting session information.\n\n%s"), 
			output != NULL ? output : _("unknown error"));
		gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
		g_free(message);
	}
	else
	{
		*msinfo = g_strdup_printf("%d,%d", start, end);
		GB_TRACE("datacd_get_msinfo - next session is [%s]\n", *msinfo);		
		ok = TRUE;
	}
	
	g_free(output);
	g_free(writer);
	g_free(msinfocmd);
	return ok;	
}


void 
datacd_import_session()
{
	GB_LOG_FUNC
	
	GtkWidget* dlg = selectdevicedlg_new();
	const gint response = gtk_dialog_run(GTK_DIALOG(dlg));
	selectdevicedlg_delete(dlg);
	
	if(response == GTK_RESPONSE_CANCEL)
		return;
    if(devices_prompt_for_disk(gnomebaker_get_window(), GB_WRITER) == GTK_RESPONSE_CANCEL)
        return;
	
	gnomebaker_show_busy_cursor(TRUE);
	
    devices_unmount_device(GB_WRITER);
	datacd_clear();

	gchar* mountpoint = NULL;	
	gchar* msinfo = NULL;    
    if(!datacd_get_msinfo(&msinfo))
	{
		g_critical("datacd_import_session - Error getting msinfo");
	}
	else if(!devices_mount_device(GB_WRITER, &mountpoint))
	{
		g_critical("datacd_import_session - Error mounting writer device");
	}
	else	
	{							
		/* try and open the mountpoint and read in the cd contents */
		GDir *dir = g_dir_open(mountpoint, 0, NULL);	
		if(dir != NULL)
		{
			datacd_set_multisession(msinfo);
						
			GtkWidget* datatree = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree);
			GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(datatree)));	
			
			const gchar *name = g_dir_read_name(dir);					
			while(name != NULL)
			{
				gchar* fullname = g_build_filename(mountpoint, name, NULL);
				gchar* uri = gbcommon_get_uri(fullname);
				if(!datacd_add_to_compilation(uri, model, TRUE))
					break;
				g_free(uri);
				g_free(fullname);
				name = g_dir_read_name(dir);				
			}
	
			g_dir_close(dir);
		}
		else
		{
			gchar* message = g_strdup_printf(_("Error importing session from [%s]. "
				"Please check the device configuration in preferences."), mountpoint);
			gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
			g_free(message);			
		}				
		
		devices_unmount_device(GB_WRITER);
	}
	
	g_free(mountpoint);	
	g_free(msinfo);
	
	gnomebaker_show_busy_cursor(FALSE);
}


void /* libglade callback */
datacd_on_datadisk_size_changed(GtkOptionMenu *optionmenu, gpointer user_data)
{
	GB_LOG_FUNC
		
	GtkWidget* progbar = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_progressbar);
	g_return_if_fail(progbar != NULL);
	
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar));	
	const gdouble previoussize = datadisksize;
	datadisksize = datacd_get_datadisk_size();
		
	fraction = (fraction * previoussize)/datadisksize;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);
	gchar* buf = datacd_format_progress_text(fraction * datadisksize);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
	g_free(buf);
    
    preferences_set_int(GB_DATA_DISK_SIZE, gtk_option_menu_get_history(optionmenu));
}


void /* libglade callback */
datacd_on_create_datadisk(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkWidget *datatree = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree);
	g_return_if_fail(datatree != NULL);
	GtkTreeModel* datamodel = gtk_tree_view_get_model(GTK_TREE_VIEW(datatree));
	g_return_if_fail(datamodel != NULL);
    gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datamodel), DATACD_EXISTING_SESSION);

	if(datadisksize >= datadisksizes[DVD_4GB].size)
    {
        if(msinfo != NULL)
            burn_append_data_dvd(datamodel);    
        else
		    burn_create_data_dvd(datamodel);
    }
	else if(msinfo != NULL)
        burn_append_data_cd(datamodel);
    else
	    burn_create_data_cd(datamodel);	

}
