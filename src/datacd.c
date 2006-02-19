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

static const gchar* const widget_datacd_tree = "treeview13";
static const gchar* const widget_datacd_list = "treeview14";
static const gchar* const widget_datacd_size = "optionmenu1";
static const gchar* const widget_datacd_progressbar = "progressbar2";
static const gchar* const widget_datacd_create = "createDataCDBtn";

static const gchar* const DATACD_EXISTING_SESSION = "msinfo";

gdouble datadisksize = 0.0;
GtkCellRenderer* contentrenderer = NULL;


enum
{    
    DATACD_COL_ICON = 0,
    DATACD_COL_FILE,
    DATACD_COL_SIZE,
    DATACD_COL_HUMANSIZE,
    DATACD_COL_PATH,
    DATACD_COL_SESSION,
    DATACD_COL_ISFOLDER,
    DATACD_NUM_COLS
};


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

/*THINGS TO DO: -improve DnD within the datacd
 * 				-when we create a foder, we must start editing its name
 * 				-when we add data to a folder, check for repeated names
 * 				-use the new cairo capacity widget(tm) :P
 * 				-check the issue of row references. I don´t like the idea of having to store gpointer
 * */


/* This will store all the data. The tree view will use a GtkTreeModelFilter and the globalStore as a child node.
 * It will use a filter function to hide non-dir elements.
 * This way all the changes done in the tree view are automatically performed on the globalStore.
 * Iters can be mapped betwenn both models, so we can update correctly the list view.
 */
static GtkTreeStore *datacd_compilation_store = NULL;

/* As the filtering trick doesn´t work for the list view, it needs its own GtkListStore.
 * We fill it with the children of the row selected in the tree view. The problem is that we need to be able
 * to perform the changes we do in the list view also in the g_compilationStore, so we need a row reference for the current node
 * and all its child elements in the list view. This is achived storing in the list view a gpointer to a GtkTreeRowReference.
 * For all the changes in list view, extract the row reference and perform the same change in the g_compilationStore. the tree view
 * is updated then automatically (since it only filters g_compilationStore). When adding new elements, first add them
 * to g_compilationStore, and then add them to the list with the rowreference
 * */

static GtkTreeRowReference * datacd_current_node = NULL;

/*I think this is better than relying in the progressbar to store the size*/
static guint64 datacd_compilation_size = 0;

/* callback id, so we can block it! */
static gulong sel_changed_id;

enum
{    
	DATACD_LIST_COL_ICON = 0,
    DATACD_LIST_COL_FILE,
	DATACD_LIST_COL_SIZE,
	DATACD_LIST_COL_HUMANSIZE,
	DATACD_LIST_COL_PATH,
	DATACD_LIST_COL_SESSION,
	DATACD_LIST_COL_ISFOLDER,
	DATACD_LIST_COL_ROWREFERENCE,
    DATACD_LIST_NUM_COLS
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

/*
 * utility function to get the root of the compilation
 * All the elements hang off it. This will contain
 * the name of the compilation. This should be considered when burning.
 */
static void
datacd_compilation_root_get_iter(GtkTreeIter *iter)
{
	GB_LOG_FUNC
	
	/*g_return_if_fail(iter != NULL);*/
	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(datacd_compilation_store),iter);
}


/*returns the Volume Id of current datacd compilation*/
gchar* 
datacd_compilation_get_volume_id()
{
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(datacd_compilation_store),&iter))
    {
		gchar *vol_id = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(datacd_compilation_store),&iter,DATACD_COL_FILE, &vol_id,-1);
		return vol_id;

    }
    return NULL;
}


/*should only be called after reseting the list store*/
static void
datacd_compilation_root_add()
{
	GB_LOG_FUNC
	
	/* Add the compilation label as the base of our tree */
	GdkPixbuf *icon = gbcommon_get_icon_for_name("gnome-dev-disc-cdr", 16);
	GB_DECLARE_STRUCT(GtkTreeIter, rootiter);
    gtk_tree_store_append(datacd_compilation_store, &rootiter, NULL);           
	gtk_tree_store_set(datacd_compilation_store, &rootiter, DATACD_COL_ICON, icon, 
						DATACD_COL_FILE, _("GnomeBaker data disk"), 
						DATACD_COL_SIZE, (guint64)0, DATACD_COL_HUMANSIZE, "",
						DATACD_COL_PATH, "", DATACD_COL_SESSION, FALSE, DATACD_COL_ISFOLDER, TRUE ,-1);
	g_object_unref(icon);
}


/*we change this, making it a bit faster, but this way it is safe, I think*/
static gboolean
datacd_compilation_is_root(GtkTreeIter * global_iter)
{
	GB_LOG_FUNC
	
	gboolean ret = FALSE;
	/*check if iter is the root*/
	GB_DECLARE_STRUCT(GtkTreeIter, root);

	/*the list should not be empty*/
	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(datacd_compilation_store),&root))
	{
		GtkTreePath *path_root = gtk_tree_model_get_path(GTK_TREE_MODEL(datacd_compilation_store), &root );
		GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(datacd_compilation_store), global_iter );
			
		if(gtk_tree_path_compare(path_root,path) == 0)
			ret = TRUE;
			
		gtk_tree_path_free(path_root);
		gtk_tree_path_free(path);		
	}
	return ret;
}


/*selects the node given by iter in the tree view. This causes listview to updated*/
static void
datacd_current_node_update(GtkTreeIter *iter)
{
	GB_LOG_FUNC

	GtkTreePath* child_path = gtk_tree_model_get_path(GTK_TREE_MODEL(datacd_compilation_store), iter);
	/* select the row in the tree view */
	GtkTreeView* dirtree = GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree));
	GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER( gtk_tree_view_get_model(dirtree) ); 	
	GtkTreePath *path = gtk_tree_model_filter_convert_child_path_to_path(filter, child_path);
	if(path!=NULL)
	{
		gtk_tree_view_expand_to_path(dirtree, path);
		gtk_tree_view_set_cursor(dirtree, path, NULL, FALSE);	
		gtk_tree_path_free(path);
		
		/* only update current node if the path in the tree view is valid (if not, it is not a dir)*/
		gtk_tree_row_reference_free(datacd_current_node);
		datacd_current_node = gtk_tree_row_reference_new (GTK_TREE_MODEL(datacd_compilation_store), child_path);
	}	

	gtk_tree_path_free(child_path);
}


static void
datacd_current_node_get_iter(GtkTreeIter *iter)
{
	GB_LOG_FUNC
	
	GtkTreePath *path = gtk_tree_row_reference_get_path(datacd_current_node);
	GtkTreeModel *model = gtk_tree_row_reference_get_model(datacd_current_node);
	gtk_tree_model_get_iter (model,iter, path);
	gtk_tree_path_free(path);
}


static void
datacd_list_view_clear(GtkTreeView *filelist)
{
	GB_LOG_FUNC

	GtkListStore *store = GTK_LIST_STORE( gtk_tree_view_get_model(filelist) ); 

	/* TODO/CHECK: this code below was itended to free all the row references. It should work but it makes the app to crash
	 * whenever we delete something. Check this. We may use G_TYPE_TREE_ROW_REFERENCE instead of G_TYPE_POINTER
	 */
		
/*	GB_DECLARE_STRUCT(GtkTreeIter, next_iter);
	int nCount = 0; 	
	while( gtk_tree_model_iter_nth_child (GTK_TREE_MODEL(store), &next_iter,NULL ,nCount))
	{
		GValue reference_val = {0};

		gtk_tree_model_get_value(GTK_TREE_MODEL(store),&next_iter,DATACD_LIST_COL_ROWREFERENCE, &reference_val);
		GtkTreeRowReference * rowreference = (GtkTreeRowReference*)g_value_get_pointer(&reference_val);
		
		g_value_unset(&reference_val);
		
		if(rowreference != NULL)
		{
			gtk_tree_row_reference_free(rowreference);
		}
		
		++nCount;
				
	}
*/
	gtk_list_store_clear(store);
}


static void
datacd_list_view_update(GtkTreeIter *parent_iter)
{
	GB_LOG_FUNC
	
	GtkTreeView *filelist = GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_list));
	GtkListStore *store = GTK_LIST_STORE( gtk_tree_view_get_model(filelist) ); 	
	datacd_list_view_clear(filelist);
	
	GB_DECLARE_STRUCT(GtkTreeIter, child_iter);	
	int nChild = 0;
	/* Add the childrens of the parent iter to the */
	while(gtk_tree_model_iter_nth_child (GTK_TREE_MODEL(datacd_compilation_store),
                                             &child_iter,
                                             parent_iter,
 	                                        nChild))
    {
    	GdkPixbuf *icon = NULL;
    	gchar *basename = NULL, *humanreadable = NULL, *filename = NULL;
    	guint64 size = 0;
    	gboolean existingsession, isFolder;
    	
    	gtk_tree_model_get(GTK_TREE_MODEL(datacd_compilation_store), &child_iter,
    						DATACD_COL_ICON, &icon, DATACD_COL_FILE, &basename, 
							DATACD_COL_SIZE, &size, DATACD_COL_HUMANSIZE, &humanreadable,
							DATACD_COL_PATH, &filename, DATACD_COL_SESSION, &existingsession,
							DATACD_COL_ISFOLDER, &isFolder ,-1);
							
		
		GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(datacd_compilation_store),&child_iter);
		GtkTreeRowReference *rowreference = gtk_tree_row_reference_new(GTK_TREE_MODEL(datacd_compilation_store),path);		
		gtk_tree_path_free(path);					
		  	
    	GB_DECLARE_STRUCT(GtkTreeIter, store_iter);
    	gtk_list_store_insert(store, &store_iter, nChild);
    	gtk_list_store_set(store, &store_iter,
    						DATACD_LIST_COL_ICON, icon,	DATACD_LIST_COL_FILE, basename, 
							DATACD_LIST_COL_SIZE, size, DATACD_LIST_COL_HUMANSIZE, humanreadable,
							DATACD_LIST_COL_PATH, filename, DATACD_LIST_COL_SESSION, existingsession,
							DATACD_LIST_COL_ISFOLDER, isFolder ,DATACD_LIST_COL_ROWREFERENCE, rowreference,
							-1);
		g_object_unref(icon);
		
		/*rowreference should not be unreferenced as the set function just stores the pointer value */
		/*we have to do it manually when clearing the list view*/
		++nChild;
    }
}


static void 
datacd_set_multisession(const gchar* msinfo)
{
	GB_LOG_FUNC
		
	if(msinfo == NULL)
		g_free((gchar*)g_object_get_data(G_OBJECT(datacd_compilation_store), DATACD_EXISTING_SESSION));
	g_object_set_data(G_OBJECT(datacd_compilation_store), DATACD_EXISTING_SESSION, 
	msinfo == NULL ? NULL : g_strdup(msinfo));	
}


static void
datacd_on_show_humansize_changed( GConfClient *client,
                               		guint cnxn_id,
								   	GConfEntry *entry,
								   	gpointer user_data)
{
	GB_LOG_FUNC

	GtkTreeView* filelist = 
		GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_list));
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
    
    gchar *current = NULL, *remaining = NULL, *buf = NULL;    
    if(currentsize < datadisksize)
    {
   		current = gbcommon_humanreadable_filesize((guint64)currentsize);
    	remaining = gbcommon_humanreadable_filesize((guint64)(datadisksize - currentsize));
    	buf = g_strdup_printf(_("%s used - %s remaining"), current, remaining);

    }
    else
    {
   	   	current = gbcommon_humanreadable_filesize((guint64)currentsize);
    	remaining = gbcommon_humanreadable_filesize((guint64)(datadisksize));
    	buf = g_strdup_printf(_("%s of %s used. Disk full"), current, remaining);
    }
    g_free(current);
    g_free(remaining);
    return buf;
}   


static void 
datacd_update_progress_bar()
{
	GB_LOG_FUNC
	
	/* Now update the progress bar with the cd size */
	GladeXML* xml = gnomebaker_getxml();
	g_return_if_fail(xml != NULL);
	
	GtkWidget* progbar = glade_xml_get_widget(xml, widget_datacd_progressbar);
	g_return_if_fail(progbar != NULL);
	
	const gdouble disksize = datacd_get_datadisk_size();	
    
	if (datacd_compilation_size < 0 || datacd_compilation_size == 0)
	{
		datacd_compilation_size = 0;
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.0);
		gchar* buf = datacd_format_progress_text(0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
        g_free(buf);
		
		/* disable the create button as there's nothing on the disk */
		gnomebaker_enable_widget(widget_datacd_create, FALSE);
		
		/* remove the multisession flag as there's nothing on the disk */
		datacd_set_multisession(NULL);
	}	
	else
	{
		gdouble fraction = 0.0;
	
		if(disksize > 0)
	 		fraction = (gdouble)datacd_compilation_size/disksize;
	 		
		if(datacd_compilation_size > disksize)
		{
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 1.0);	
		}
		else
		{
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), fraction);
		}
	
        gchar *buf = datacd_format_progress_text(datacd_compilation_size);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), buf);
        g_free(buf);
		gnomebaker_enable_widget(widget_datacd_create, TRUE);
	}

}


static gboolean  
datacd_add_to_compilation(const gchar* file, GtkTreeIter* parentNode, gboolean existingsession)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, FALSE);
	g_return_val_if_fail(parentNode != NULL, FALSE);
	gboolean ret = TRUE;
	
	gchar* filename = gbcommon_get_local_path(file);
	gchar* path_to_show = NULL;
		
	GB_DECLARE_STRUCT(struct stat, s);
	gint statret = stat(filename, &s);
	if(statret == 0)
	{
		gboolean isFolder = s.st_mode & S_IFDIR;
		/*TODO/FIX:what to do with the size of the folders?
		 * We could leave as before, showing the size of its contents,
		 * but we cannot call gbcommon_calc_dir_size because it would be very slow.
		 * We have to perform the loop through the contents before
		 * setting the values to the store (but after adding a new element)
		 * and datacd_add_to_compilation should have another argument, a pointer that returns
		 * the accumulated size. If we use this method, if,
		 * whe we delete items, we would have to go up through the tree updating the parents
		 */
		
		guint64 size = 0;
		if(isFolder)
		{
			/*Is there any sense storing the file name for folders?*/
			/*we don´t want to show path for folders, so the easiest way is to store nothing*/
			path_to_show = g_strdup("");
			
		}
		else
		{
			size = (guint64)s.st_size;			
			path_to_show = g_strdup(filename);
			datacd_compilation_size += size;
		}

		/* 
		 * we allow to add more size than the capacity of the disc,
		 * and show the error dialog only before burning. The progress bar should
		 * reflect somehow this situation
		 * 
		 * updating progress bar for every file, makes it slow
		 * (and for sure the filter_func in the treeview, but that
		 * is something we cannot avoid, even disconnecting it from the store, I think)
		 *  
		 * */
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

		GB_DECLARE_STRUCT(GtkTreeIter, iter);
		gtk_tree_store_append(datacd_compilation_store, &iter, parentNode);	
		gtk_tree_store_set(datacd_compilation_store, &iter,
							DATACD_COL_ICON, icon, DATACD_COL_FILE, basename, 
							DATACD_COL_SIZE, size, DATACD_COL_HUMANSIZE, humanreadable,
							DATACD_COL_PATH, path_to_show, DATACD_COL_SESSION, existingsession,
							DATACD_COL_ISFOLDER, isFolder,-1);
		
		g_free(humanreadable);
		g_object_unref(icon);
		g_free(basename);
		
		/*recursively add contents*/
		if(isFolder)
		{ 
			GDir *dir = g_dir_open(filename, 0, NULL);
			if(dir != NULL)
			{
				const gchar *name = g_dir_read_name(dir);					
				while(name != NULL)
				{
					/* build up the full path to the name */
					gchar* fullname = g_build_filename(filename, name, NULL);
					GB_DECLARE_STRUCT(struct stat, s);
					
					/*if the disc is full, do not add more files! (for now)*/
					if(!datacd_add_to_compilation(fullname, &iter, existingsession))
					{
						g_free(fullname);
						break;							
					}
					
					g_free(fullname);
					name = g_dir_read_name(dir);
				}
				g_dir_close(dir);
			}
		}
	}
	else
	{
		g_warning("datacd_add_to_compilation - failed to stat file [%s] with ret code [%d] error no [%s]", 
			filename, statret, strerror(errno));
		
		ret = FALSE;
	}
	g_free(path_to_show);
	g_free(filename);
	return ret;
}


void 
datacd_add_selection(GtkSelectionData* selection)
{
	GB_LOG_FUNC
	g_return_if_fail(selection != NULL);
	g_return_if_fail(selection->data != NULL);
	
	gnomebaker_show_busy_cursor(TRUE);	    	

	GB_TRACE("datacd_add_selection - received sel [%s]\n", selection->data);	
	const gchar* file = strtok((gchar*)selection->data,"\n");
    
    GB_DECLARE_STRUCT(GtkTreeIter, parent_iter);
    if(!gtk_tree_row_reference_valid(datacd_current_node))
    {
        datacd_compilation_root_get_iter(&parent_iter);
        datacd_current_node_update(&parent_iter);
    }
   
    datacd_current_node_get_iter(&parent_iter);
   
    GtkTreeView* dirtree = GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree));
    GtkTreeModel *model = gtk_tree_view_get_model(dirtree);
   
   /* Do not disconnect the model from the view.
    * Disconnecting the tree model causes the tree view not to
    * behave as we want when we add elements
    */ 
	while(file != NULL)
	{
        /* We pass the current node. It depends of the selection in the tree view */
        if(!datacd_add_to_compilation(file, &parent_iter, FALSE)) 
            break;
		file = strtok(NULL, "\n");
	}
    
    datacd_update_progress_bar();

    /*expand node if posible*/
    GtkTreePath *global_path = gtk_tree_model_get_path(GTK_TREE_MODEL(datacd_compilation_store),&parent_iter);
    GtkTreePath *path = gtk_tree_model_filter_convert_child_path_to_path(GTK_TREE_MODEL_FILTER(model),global_path);
    gtk_tree_view_expand_to_path(dirtree,path);   
    gtk_tree_path_free(global_path);
    gtk_tree_path_free(path);   
    datacd_list_view_update(&parent_iter);  
	
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
 	/*this model does not support automatic dnd. Disable the annoying message*/
 	if(GTK_IS_TREE_MODEL_FILTER(userdata))
 	{
 		g_signal_stop_emission_by_name(G_OBJECT(widget), "drag_data_received");
 	}
}


/* adds to the list all the children of parent_iter*/
static void
datacd_add_recursive_reference_list(GtkTreeIter *parent_iter,
								  GList **rref_list)
{
	GB_LOG_FUNC
	
	int nCount=0;
	GB_DECLARE_STRUCT(GtkTreeIter, child_iter)
	while(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(datacd_compilation_store),&child_iter,parent_iter,nCount))
	{
		/*add the row reference of each child to the list */
		/*obtain the path from the iter*/
		GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(datacd_compilation_store),&child_iter);
        /*obtain the row reference from the path */
		GtkTreeRowReference *rowreference = gtk_tree_row_reference_new(GTK_TREE_MODEL(datacd_compilation_store), path);
		/*and finally add*/
		if(rowreference!=NULL)
		{
			GB_TRACE("Added to remove: %s",gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(datacd_compilation_store), &child_iter));
			*rref_list = g_list_prepend(*rref_list, rowreference);
		}

		gtk_tree_path_free(path);
		
		/* add the children of the child recursively*/
		/* it is important the order in which we add the elements to the list
		 * otherwise we can delete a row before its children are deleted*/
		datacd_add_recursive_reference_list(&child_iter,rref_list);
		++nCount;
	}
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

	/*obtain the row reference*/
	if(GTK_IS_LIST_STORE(filemodel))
	{
		GtkTreeRowReference *rowreference = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(filemodel),iter,DATACD_LIST_COL_ROWREFERENCE, &rowreference,-1);

		GtkTreePath *global_path = gtk_tree_row_reference_get_path (rowreference);
		if(global_path!=NULL)
		{
			*rowref_list = g_list_prepend(*rowref_list, rowreference);			

			/*Get the iter. Use this as parent iter to add all the children recursively to the list*/
			GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
			if(gtk_tree_model_get_iter(GTK_TREE_MODEL(datacd_compilation_store),&global_iter, global_path))
			{
				GB_TRACE("Added to remove: %s",gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(datacd_compilation_store), &global_iter));
				datacd_add_recursive_reference_list(&global_iter,rowref_list);
			}
			gtk_tree_path_free(global_path);
		}		
	}
}


void 
datacd_remove(GtkTreeView *widget)
{
	GB_LOG_FUNC
	gnomebaker_show_busy_cursor(TRUE);
	
	GtkTreeView *view = widget;
	/*get list by default*/
	if(view == NULL)
	{
		view = GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_list));
	}
	
	GList *rr_list = NULL;   /* list of GtkTreeRowReferences to remove */
	
	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	
	g_return_if_fail(model != NULL);
	
	if(GTK_IS_LIST_STORE(model))
	{
		/*this behaves differently, since it has to retrieve the references to g_compilationStore*/
		gtk_tree_selection_selected_foreach(selection, datacd_foreach_fileselection, &rr_list);
	}
	else
	{
		/*check that the selected item is not the root*/
		GtkTreeModel *treemodel = NULL;
    	GB_DECLARE_STRUCT(GtkTreeIter, iter);
    	GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
		gtk_tree_selection_get_selected (selection, &treemodel ,&iter);
		gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(treemodel),&global_iter,&iter);
				
		if(!datacd_compilation_is_root(&global_iter))
		{
			GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(datacd_compilation_store),&global_iter);
 			GtkTreeRowReference *rowreference = gtk_tree_row_reference_new(GTK_TREE_MODEL(datacd_compilation_store), path);
			
			rr_list = g_list_prepend(rr_list, rowreference);	
			
			/*add recursively the children*/
			datacd_add_recursive_reference_list(&global_iter, &rr_list);
			
			gtk_tree_path_free(path);
			
			/*if we remove the selected node from the tree, we have to select a new node. The parent may be? (we always will have root)*/
			GB_DECLARE_STRUCT(GtkTreeIter, global_parent_iter);
			if(gtk_tree_model_iter_parent (GTK_TREE_MODEL(datacd_compilation_store),&global_parent_iter,&global_iter))
			{
				datacd_current_node_update(&global_parent_iter);
			}	
		}
	}
	
	GList *node;		
	for (node = rr_list; node != NULL; node = node->next )
	{
		if(node->data!=NULL)
		{
			GtkTreePath * path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);
			if (path!=NULL)
			{
				GB_DECLARE_STRUCT(GtkTreeIter, iter);
				if (gtk_tree_model_get_iter(GTK_TREE_MODEL(datacd_compilation_store), &iter, path))
				{
					GValue sessionvalue = { 0 };
					gtk_tree_model_get_value(GTK_TREE_MODEL(datacd_compilation_store), &iter, DATACD_COL_SESSION, &sessionvalue);
					if(g_value_get_boolean(&sessionvalue) == FALSE)
					{										
						GValue value = { 0 };
						gtk_tree_model_get_value(GTK_TREE_MODEL(datacd_compilation_store), &iter, DATACD_COL_SIZE, &value);
					
						/*compilation size*/
						datacd_compilation_size-=g_value_get_uint64(&value);
						
						g_value_unset(&value);
						
						GB_TRACE("Removed: %s",gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(datacd_compilation_store), &iter));									
						gtk_tree_store_remove(datacd_compilation_store, &iter);
					}
					g_value_unset(&sessionvalue);
				}			
				gtk_tree_path_free(path);
			}
		}
	}
	
	g_list_foreach(rr_list, (GFunc)gtk_tree_row_reference_free, NULL);
	g_list_free(rr_list);
	
	/*update list view*/
	GB_DECLARE_STRUCT(GtkTreeIter, parent_iter);
	if(!gtk_tree_row_reference_valid(datacd_current_node))
	{
		datacd_compilation_root_get_iter(&parent_iter);
		datacd_current_node_update(&parent_iter);
	}
	datacd_current_node_get_iter(&parent_iter);			
	datacd_list_view_update(&parent_iter);
	
	datacd_update_progress_bar();
	
	gnomebaker_show_busy_cursor(FALSE);
}


static void
datacd_on_remove_clicked(GtkWidget *menuitem, gpointer userdata)
{
	GB_LOG_FUNC
	
	g_return_if_fail(userdata != NULL);
	datacd_remove(GTK_TREE_VIEW(userdata));		
}


void
datacd_clear()
{
	GB_LOG_FUNC
	gnomebaker_show_busy_cursor(TRUE);
	
	GladeXML* xml = gnomebaker_getxml();

	/* Fastest way, clear all and add a new root node */
	GB_TRACE("clearing the whow tree store");
	gtk_tree_store_clear(datacd_compilation_store);	
	datacd_compilation_root_add();
	
	datacd_compilation_size = 0;
	
	/*update list view*/
	GB_DECLARE_STRUCT(GtkTreeIter, root);
	datacd_compilation_root_get_iter(&root);
	datacd_current_node_update(&root);
	datacd_list_view_update(&root);

	datacd_update_progress_bar();
	
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
	
	if(GTK_IS_LIST_STORE(user_data))
	{
		/*Read the row reference and edit the value in g_compilationStore*/
		GB_DECLARE_STRUCT(GtkTreeIter, iter);
		if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(user_data), &iter, path_string))
		{
			GValue reference_val = {0};
			gtk_tree_model_get_value(GTK_TREE_MODEL(user_data),&iter,DATACD_LIST_COL_ROWREFERENCE, &reference_val);			
			GtkTreeRowReference * rowreference = g_value_get_pointer(&reference_val);			
			g_value_unset(&reference_val);		
			
			/*we cannot free this reference because it continues existing in the list store*/
			GtkTreePath *path = gtk_tree_row_reference_get_path (rowreference);
			if(path!=NULL)
			{
				GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
				gtk_tree_model_get_iter(GTK_TREE_MODEL(datacd_compilation_store),&global_iter, path);
				
				GValue val = {0};
				g_value_init(&val, G_TYPE_STRING);
				g_value_set_string(&val, new_text);
				gtk_tree_store_set_value(datacd_compilation_store, &global_iter, DATACD_COL_FILE, &val);
				g_value_unset(&val);
				gtk_tree_path_free(path);				
			}			
		}
	}
	else
	{
		/* path_string is relative to the GtkTreeModelFilter. We have to convert it to a iter first and then to
	 	 * a iter relative to the child GtkTreeModel (in out case g_compilationStore) */	
		GB_DECLARE_STRUCT(GtkTreeIter, iter);
		if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(user_data), &iter, path_string))
		{
			GB_DECLARE_STRUCT(GtkTreeIter, child_iter);
			gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER(user_data),&child_iter, &iter);
			
			GValue val = {0};
			g_value_init(&val, G_TYPE_STRING);
			g_value_set_string(&val, new_text);
			gtk_tree_store_set_value(datacd_compilation_store, &child_iter, DATACD_COL_FILE, &val);
			g_value_unset(&val);
		}
	}
	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	datacd_current_node_get_iter(&iter);
	datacd_list_view_update(&iter);
}


static void 
datacd_on_edit(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(user_data != NULL);        
    /*g_signal_emit_by_name (contentrenderer, "edited", NULL, NULL);*/    
}


static void 
datacd_on_add_folder(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC
	
	GB_DECLARE_STRUCT(GtkTreeIter, parent_iter);
	datacd_current_node_get_iter(&parent_iter);
	
	/*TODO: what to do with the size?*/
	guint64 size = 0;
	gchar* humanreadable = gbcommon_humanreadable_filesize(size);
	
	/*we will use this mime: x-directory/normal*/
	GdkPixbuf *icon = gbcommon_get_icon_for_mime("x-directory/normal", 16);
	
	/*Add a folder to the current node	*/
	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	gtk_tree_store_append(datacd_compilation_store, &iter, &parent_iter);	
	gtk_tree_store_set(datacd_compilation_store, &iter,
						DATACD_COL_ICON, icon,
						DATACD_COL_FILE, _("New Folder"),
						DATACD_COL_SIZE, size,
						DATACD_COL_HUMANSIZE, humanreadable,
						DATACD_COL_PATH, "",
						DATACD_COL_SESSION, FALSE,
						DATACD_COL_ISFOLDER, TRUE,
						-1);
						
	g_object_unref(icon);

	/*expand node if posible*/
	GtkTreeView* dirtree = GTK_TREE_VIEW(glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree));
	GtkTreeModel *model = gtk_tree_view_get_model(dirtree);
	GtkTreePath *global_path = gtk_tree_model_get_path(GTK_TREE_MODEL(datacd_compilation_store),&parent_iter);
	GtkTreePath *path = gtk_tree_model_filter_convert_child_path_to_path(GTK_TREE_MODEL_FILTER(model),global_path);
	gtk_tree_view_expand_row (dirtree, path, FALSE);
	
	gtk_tree_path_free(global_path);
	gtk_tree_path_free(path);

	datacd_list_view_update(&parent_iter);

	/*TODO: we should edit the name of the new folder*/
}

static gboolean
datacd_on_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GB_LOG_FUNC
	
	/* look for a right click */	
	if(event->button == 3)
	{
        GtkTreeView* view = (GtkTreeView*)widget;
        GtkTreeModel *model = gtk_tree_view_get_model(view);
        
        if(GTK_IS_LIST_STORE(model))
        {
        	GtkWidget* menu = gtk_menu_new();
        				
	        gbcommon_append_menu_item_stock(menu, _("_Add Folder"), GTK_STOCK_NEW, 
					(GCallback)datacd_on_add_folder, view);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	        
			GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
			const gint count = gtk_tree_selection_count_selected_rows(selection);
			if(count == 1)
			{
				gbcommon_append_menu_item_stock(menu, _("_Edit name"), GTK_STOCK_DND, 
					(GCallback)datacd_on_edit, view);
	            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
			}
			gbcommon_append_menu_item_stock(menu, _("_Remove selected"), GTK_STOCK_REMOVE, 
				(GCallback)datacd_on_remove_clicked, view);	
			gbcommon_append_menu_item_stock(menu, _("Clear"), GTK_STOCK_CLEAR, 
				(GCallback)datacd_on_clear_clicked, view);		
			gtk_widget_show_all(menu);
		
			/* Note: event can be NULL here when called. However,
			 *  gdk_event_get_time() accepts a NULL argument */
			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			     (event != NULL) ? event->button : 0, gdk_event_get_time((GdkEvent*)event));
			return TRUE;
        }
        else if(model != NULL)
        {
        	GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
        	const gint count = gtk_tree_selection_count_selected_rows(selection);
        	if(count == 1)
			{
				/*Do not show the menu if the selected item is the root*/
				GtkTreeModel *treemodel = NULL;
    			GB_DECLARE_STRUCT(GtkTreeIter, iter);
    			GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
				gtk_tree_selection_get_selected (selection, &treemodel ,&iter);
				gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(treemodel),&global_iter,&iter);
				
				if(!datacd_compilation_is_root(&global_iter))
				{
					GtkWidget* menu = gtk_menu_new();
					gbcommon_append_menu_item_stock(menu, _("_Remove selected"), GTK_STOCK_REMOVE, 
													(GCallback)datacd_on_remove_clicked, view);
					gtk_widget_show_all(menu);
					gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
						   			(event != NULL) ? event->button : 0,
					gdk_event_get_time((GdkEvent*)event));
					
				    return TRUE;
				}
			}	
        }
	}
	return FALSE;
}


static void
datacd_on_tree_dbl_click  (	GtkTreeView *treeview,
							GtkTreePath *path,
							GtkTreeViewColumn *column,
							gpointer user_data)
{
	GB_LOG_FUNC
	
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	if(model != NULL)
	{
		GB_DECLARE_STRUCT(GtkTreeIter,iter);		
		if(gtk_tree_model_get_iter(model,&iter,path))
		{
			GValue isFolder = {0};
			gtk_tree_model_get_value(GTK_TREE_MODEL(model),&iter,DATACD_LIST_COL_ISFOLDER, &isFolder);
			
			if(g_value_get_boolean(&isFolder))
			{
				GValue reference_val = {0};
				gtk_tree_model_get_value(GTK_TREE_MODEL(model),&iter,DATACD_LIST_COL_ROWREFERENCE, &reference_val);
				GtkTreeRowReference * rowreference = (GtkTreeRowReference*)g_value_get_pointer(&reference_val);
				g_value_unset(&reference_val);
			
				GtkTreePath *global_path = gtk_tree_row_reference_get_path(rowreference);
				
				GB_DECLARE_STRUCT(GtkTreeIter,global_iter);	
				if(gtk_tree_model_get_iter(GTK_TREE_MODEL(datacd_compilation_store),&global_iter,global_path))
				{
					datacd_current_node_update(&global_iter);
				}
				gtk_tree_path_free(global_path);
			}
			g_value_unset(&isFolder);			
		}
	}
}
                                            

/* returns true if the element in the model pointed by iter is a directory*/
static gboolean
datacd_treeview_filter_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	GB_LOG_FUNC
	g_return_if_fail(model != NULL);
	g_return_if_fail(iter != NULL);
	
	gboolean ret = TRUE;
	GValue isFolder = {0};
	gtk_tree_model_get_value(model,iter,DATACD_COL_ISFOLDER, &isFolder);
	/*check if it is a directory. If so,show. Otherwise, don´t*/
	if(!g_value_get_boolean(&isFolder))
		ret = datacd_compilation_is_root(iter);	
	g_value_unset(&isFolder);	
	return ret;
}


/*changed selection in the dir tree*/
static void
datacd_tree_sel_changed(GtkTreeSelection* selection, gpointer userdata)
{
	GB_LOG_FUNC
	g_return_if_fail(selection != NULL);
	
    GtkTreeModel *treemodel = NULL;
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    /* Block the signal so that it doesn't trigger recursively (saving CPU cycles here :-))*/
    g_signal_handler_block (selection, sel_changed_id);

    /* The selection in the dir tree has changed so get that selection */
    if(gtk_tree_selection_get_selected(selection, &treemodel, &iter))
    {
    	/*get the corresponding iter in g_compilationStore */
    	GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
		gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER(treemodel), &global_iter, &iter);
		GtkTreePath* global_path_curr = gtk_tree_model_get_path(GTK_TREE_MODEL(datacd_compilation_store), &global_iter);
		GtkTreePath* global_path_prev = NULL;
		/*stop glib giving warnings*/
		if(datacd_current_node!=NULL)
		{
			global_path_prev = gtk_tree_row_reference_get_path (datacd_current_node);
		}

		/*only update list view if if the selection has really changed*/				
		if((global_path_prev == NULL) || (gtk_tree_path_compare(global_path_prev, global_path_curr) != 0))
		{
			gtk_tree_row_reference_free(datacd_current_node);
			datacd_current_node = gtk_tree_row_reference_new (GTK_TREE_MODEL(datacd_compilation_store), global_path_curr);
			datacd_list_view_update(&global_iter);
    	}

		gtk_tree_path_free(global_path_prev);
    	gtk_tree_path_free(global_path_curr);

	}
    g_signal_handler_unblock (selection, sel_changed_id);
}


static void
datacd_setup_tree( GtkTreeView *dirtree)
{
	GB_LOG_FUNC
	g_return_if_fail(dirtree != NULL);
	g_return_if_fail(datacd_compilation_store != NULL);
	
	/* Create the tree store for the dir tree */
    /*GtkTreeStore *store = gtk_tree_store_new(DT_NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING);*/
    GtkTreeModel *store = gtk_tree_model_filter_new(GTK_TREE_MODEL(datacd_compilation_store),NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER(store), datacd_treeview_filter_func, NULL, NULL);
    gtk_tree_view_set_model(dirtree, store);
    g_object_unref(store);
    
	/* One column which has an icon renderer and a text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Contents"));
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", DATACD_COL_ICON, NULL);

	GValue value = { 0 };
	g_value_init(&value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&value, TRUE);	

    renderer = gtk_cell_renderer_text_new();
    g_object_set_property(G_OBJECT(renderer), "editable", &value);
	g_signal_connect(renderer, "edited", (GCallback)datacd_contents_cell_edited, (gpointer)store);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_COL_FILE, NULL);
    gtk_tree_view_append_column(dirtree, col);
    g_value_unset(&value);
    
	/* Enable the file list as a drag destination */	
    gtk_drag_dest_set(GTK_WIDGET(dirtree), GTK_DEST_DEFAULT_ALL,
		targetentries, TARGET_COUNT, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

	/* Connect the function to handle the drag data */
    g_signal_connect(dirtree, "drag_data_received",
		G_CALLBACK(datacd_on_drag_data_received), store);

	/* Set the selection mode of the dir tree */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(dirtree);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	/* Connect up the changed signal so we can populate the file list according to the selection in the dir tree */
	sel_changed_id = g_signal_connect((gpointer) selection, "changed", G_CALLBACK(datacd_tree_sel_changed), NULL);
				
	/* connect the signal to handle right click */
	g_signal_connect (G_OBJECT(dirtree), "button-press-event",
        G_CALLBACK(datacd_on_button_pressed), NULL);
        
	/*TODO double clicks should expand the node of the tree*/
	/* handle double clicks */	
	/*g_signal_connect(G_OBJECT(dirtree), "row-activated", G_CALLBACK(datacd_on_tree_dbl_click), NULL);*/
}

/*show folders first
 * TODO: should we also group by mime?
 * */
static gint
datacd_list_sortfunc(	GtkTreeModel *model,
						GtkTreeIter *a,
						GtkTreeIter *b,
                        gpointer user_data)
{
	/* <0, a BEFORE b
	 *  0,  a WITH b (undefined)
	 * >0,  a AFTER b
	 */
	 
	gchar *aname = NULL, *bname = NULL;
	gboolean aIsFolder = FALSE, bIsFolder = FALSE;
	
	gtk_tree_model_get (model, a, DATACD_LIST_COL_FILE, &aname, DATACD_LIST_COL_ISFOLDER, &aIsFolder, -1);
	gtk_tree_model_get (model, b, DATACD_LIST_COL_FILE, &bname, DATACD_LIST_COL_ISFOLDER, &bIsFolder, -1);
	
	gint result = 0;
	if(aIsFolder && !bIsFolder)
		result = -1;
	else if(!aIsFolder && bIsFolder)
		result = 1;
	else
		result = g_ascii_strcasecmp(aname, bname);
		
	g_free(aname);
	g_free(bname);
	
	return result;
}


static void
datacd_setup_list( GtkTreeView *filelist)
{
	GB_LOG_FUNC
	g_return_if_fail(datacd_compilation_store != NULL);
	/* Create the list store for the file list */
	/* Add a column field for a row reference to rows in DatacdCompilationStore */
	/* Instead of a pointer to row reference we could use G_TYPE_TREE_ROW_REFERENCE
	/* so that we do not need to delete it manually*/
    GtkListStore *store = gtk_list_store_new(DATACD_LIST_NUM_COLS, GDK_TYPE_PIXBUF, 
			G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_POINTER);
    gtk_tree_view_set_model(filelist, GTK_TREE_MODEL(store));
    
    /*show folders first*/
	gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(store), datacd_list_sortfunc, NULL, NULL);	
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
		
    g_object_unref(store);

	/* First column which has an icon renderer and a text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Contents"));
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", DATACD_LIST_COL_ICON, NULL);
	
	GValue value = { 0 };
	g_value_init(&value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&value, TRUE);	

    contentrenderer = gtk_cell_renderer_text_new();
	g_object_set_property(G_OBJECT(contentrenderer), "editable", &value);
	g_signal_connect(contentrenderer, "edited", (GCallback)datacd_contents_cell_edited, 
		(gpointer)store);
    gtk_tree_view_column_pack_start(col, contentrenderer, TRUE);
    gtk_tree_view_column_set_attributes(col, contentrenderer, "text", DATACD_LIST_COL_FILE, NULL);
	gtk_tree_view_append_column(filelist, col);	
	
	g_value_unset(&value);

	const gboolean showhumansize = preferences_get_bool(GB_SHOWHUMANSIZE);
	
	/* Second column to display the file/dir size */
	col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_title(col, _("Size"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_LIST_COL_SIZE, NULL);
	gtk_tree_view_column_set_visible(col, !showhumansize);
	gtk_tree_view_append_column(filelist, col);

	/* Third column to display the human size of file/dir */
	col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_title(col, _("Size"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_LIST_COL_HUMANSIZE, NULL);
	gtk_tree_view_column_set_visible(col, showhumansize);
	gtk_tree_view_append_column(filelist, col);
	
	/* Fourth column for the full path of the file/dir */
	col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_title(col, _("Full Path"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_LIST_COL_PATH, NULL);
	gtk_tree_view_append_column(filelist, col);
	
	/* Fifth column for the session bool */
	col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_title(col, _("Session"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATACD_LIST_COL_SESSION, NULL);
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
        
    /* handle double clicks. This should allow us to navigate through the contents */	
	g_signal_connect(G_OBJECT(filelist), "row-activated", G_CALLBACK(datacd_on_tree_dbl_click), NULL);
}


void
datacd_new()
{
	GB_LOG_FUNC
	
	/* Create the global store. It has the same structure as the list view, but it is a tree (*obvious*)
	 * 1-icon 2-name
	 * 3-size
	 * 4-human size
	 * 5-full path
	 * 6-session (if it comes from another session
	 * 7-is a folder
	 */	
	datacd_compilation_store = gtk_tree_store_new(DATACD_NUM_COLS, GDK_TYPE_PIXBUF, 
			G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_STRING,G_TYPE_BOOLEAN,G_TYPE_BOOLEAN);
			
	datacd_compilation_size = 0;

	GtkTreeView* dirtree = GTK_TREE_VIEW(glade_xml_get_widget(
		gnomebaker_getxml(), widget_datacd_tree));
	GtkTreeView* filelist = GTK_TREE_VIEW(glade_xml_get_widget(
		gnomebaker_getxml(), widget_datacd_list));
	
	g_return_if_fail(dirtree != NULL);	
	g_return_if_fail(filelist != NULL);	
	
	datacd_setup_list(filelist);
	datacd_setup_tree(dirtree);
	datacd_compilation_root_add();
	
	GB_DECLARE_STRUCT(GtkTreeIter, root);
	datacd_compilation_root_get_iter(&root);
	datacd_current_node_update(&root);

	preferences_register_notify(GB_SHOWHUMANSIZE, datacd_on_show_humansize_changed);
	
	GtkWidget* optmenu = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_size);
	gbcommon_populate_disk_size_option_menu(GTK_OPTION_MENU(optmenu), datadisksizes, 
		DISK_SIZE_COUNT, preferences_get_int(GB_DATA_DISK_SIZE));
		
	datacd_update_progress_bar();
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
						
			GtkWidget* datalist = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_list);
			
			const gchar *name = g_dir_read_name(dir);					
			while(name != NULL)
			{
				gchar* fullname = g_build_filename(mountpoint, name, NULL);
				gchar* uri = gbcommon_get_uri(fullname);
				/*
				 * TODO:Should we also import the volume id of the disc?
				 */
				GB_DECLARE_STRUCT(GtkTreeIter, root);
				datacd_compilation_root_get_iter(&root);
				if(!datacd_add_to_compilation(uri, &root, TRUE))
					break;
				g_free(uri);
				g_free(fullname);
				name = g_dir_read_name(dir);				
			}
			
			datacd_update_progress_bar();
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
		
	datacd_update_progress_bar();
    preferences_set_int(GB_DATA_DISK_SIZE, gtk_option_menu_get_history(optionmenu));
}



/* this should be better to be moved to datacd
 * so that from outside of datacd only the Glist is
 * available
 * */

struct BurnItem
{
    guint64 size;
    gboolean existing_session;
    gchar *path_to_burn;
    gchar *path_in_filesystem;
};



/* Adds  to the burning list all the contents recursively. This could be improved checking for depth,
 * as mkisofs only accepts a max depth o 6 directories */
static void
datacd_build_filepaths_recursive(GtkTreeModel *model, GtkTreeIter *parent_iter, const char * filepath ,GList **compilation_list)
{
    GB_LOG_FUNC

    int nCount = 0;
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    while(gtk_tree_model_iter_nth_child(model,&iter,parent_iter,nCount))
    {
        guint64 size;
        gboolean isFolder = FALSE, existing_session = FALSE;
        gchar *filename = NULL, *path_in_system = NULL;

        gtk_tree_model_get(model, &iter,
                           DATACD_COL_FILE, &filename,
                           DATACD_COL_SIZE, &size,
                           DATACD_COL_PATH, &path_in_system,
                           DATACD_COL_SESSION, &existing_session,
                           DATACD_COL_ISFOLDER, & isFolder,
                           -1 );
        
        /*it is not a folder, add it to the list*/
        if(!isFolder)
        {
            struct BurnItem *item = (struct BurnItem*)g_new0(struct BurnItem, 1);
            
            /*we will free memory when we delete the list*/
            item->existing_session = existing_session;
            item->path_in_filesystem = path_in_system; 
            item->path_to_burn = g_build_filename(filepath, filename, NULL); 
            item->size = size;
            
            /*this is faster than g_list_append*/
            *compilation_list = g_list_prepend(*compilation_list,item);             
        }
        else /*it is a folder, add its contents recursively building the filepaths to burn*/
        {
            gchar * filepath_new = g_build_filename(filepath,filename,NULL); 
            datacd_build_filepaths_recursive(model, &iter, filepath_new ,compilation_list);
            
            /*delete the data as it´s not used*/
            g_free(filepath_new);
            g_free(path_in_system);
            g_free(filename);
        }       
        ++nCount;
    }
}


/*
 * Creates a temporary file wich stores the graft-points.
 * This way we avoid the "too many arguments" error.
 */
static const GBTempFile* 
datacd_build_filepaths(GtkTreeModel *model)
{
    GB_LOG_FUNC
        
    GList *compilation_list = NULL;
    
    GBTempFile *tmpFile = gbcommon_create_open_temp_file("gnomebaker");
    if(tmpFile == NULL)
    {
        GB_TRACE("Error. Temp file was not created. Image will not be created");
        return NULL;
    }

    /* Get the root iter.It should be the name of the compilation,
     * but for the moment we don´t do anything with it
     * All will start from here
     */
     
     GB_DECLARE_STRUCT(GtkTreeIter, root_iter);
     gtk_tree_model_get_iter_first(model, &root_iter);
     if(gtk_tree_model_iter_has_child(model, &root_iter))
     {
        int nCount = 0;
        GB_DECLARE_STRUCT(GtkTreeIter, iter);
        while(gtk_tree_model_iter_nth_child(model,&iter,&root_iter,nCount))
        {
            guint64 size;
            gboolean isFolder = FALSE, existing_session = FALSE;
            gchar *filename = NULL, *path_in_system = NULL;

            gtk_tree_model_get(model, &iter,
                               DATACD_COL_FILE, &filename,
                               DATACD_COL_SIZE, &size,
                               DATACD_COL_PATH, &path_in_system,
                               DATACD_COL_SESSION, &existing_session,
                               DATACD_COL_ISFOLDER, & isFolder,
                               -1 );
            
            /*it is not a folder, add it to the list*/
            if(!isFolder)
            {
                struct BurnItem *item = (struct BurnItem*)g_new0(struct BurnItem, 1);
                
                /*we will free memory when we delete the list*/
                item->existing_session = existing_session;
                item->path_in_filesystem = path_in_system; 
                item->path_to_burn = filename;/*this is the first level, do not concatenate nothing*/
                item->size = size;
                
                /*this is faster than g_list_append*/
                compilation_list = g_list_prepend(compilation_list,item);
            }
            else /*it is a folder, add its contents recursively building the filepaths to burn*/
            {
                datacd_build_filepaths_recursive(model, &iter, filename ,&compilation_list);
                
                /*delete the data as it´s not used*/
                g_free(path_in_system);
                g_free(filename);
            }
            ++nCount;
        }
    }
    else
    {
        /*compilation is empty! What should we do?*/
    }    
    
    /* once we have all the items, transverse all and add them to the arguments.
     * Calculate also the total size. We could aso look for invalid paths */     
    GList *node =NULL;
    for (node = compilation_list; node != NULL; node = node->next )
    {
        if(node->data!=NULL)
        {
            struct BurnItem *item = (struct BurnItem *)node->data;
            /*Only add files that are not part of an existing session*/
            if(!item->existing_session)
            {
                /*GB_TRACE("Data to Burn: %s=%s",item->path_to_burn , item->path_in_filesystem);*/
                fprintf(tmpFile->fileStream, "%s=%s\n", item->path_to_burn , item->path_in_filesystem);
                /*free memory*/
                g_free(item->path_in_filesystem);
                g_free(item->path_to_burn);
                g_free(item);
            }   
        }
    }
    g_list_free(compilation_list);
    gbcommon_close_temp_file(tmpFile); /*close, but don´t delete. It must be readed by mkisofs*/
    return tmpFile;
}


void /* libglade callback */
datacd_on_create_datadisk(gpointer widget, gpointer user_data)
{
	GB_LOG_FUNC	
    g_return_if_fail(datacd_compilation_store != NULL);
    
    if(datacd_compilation_size <= 0 || datacd_compilation_size > datacd_get_datadisk_size())
    {
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
           _("Compilation exeeds the capacity or the remaining space of the disk"));       
       return;
    }
    
    const GBTempFile* tmp_file = datacd_build_filepaths(GTK_TREE_MODEL(datacd_compilation_store));    
    gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datacd_compilation_store), DATACD_EXISTING_SESSION);

	if(datadisksize >= datadisksizes[DVD_4GB].size)
    {
        if(msinfo != NULL)
            burn_append_data_dvd(tmp_file->fileName, msinfo);
        else
		    burn_create_data_dvd(tmp_file->fileName);
    }
	else if(msinfo != NULL)
        burn_append_data_cd(tmp_file->fileName, msinfo);
    else
	    burn_create_data_cd(tmp_file->fileName);
}


void 
datacd_open_project()
{
    GB_LOG_FUNC
}


static gboolean
datacd_foreach_save_project(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, FILE* project)
{    
    gchar *file = NULL, *filepath = NULL;
    gboolean existingsession = FALSE;
        
    gtk_tree_model_get (model, iter, DATACD_COL_FILE, &file,
        DATACD_COL_PATH, &filepath, DATACD_COL_SESSION, &existingsession, -1);
    
    /* Only add files that are not part of an existing session */
    if(!existingsession)
        fprintf(project, "<File Name=\"%s\" Path=\"%s\"/>\n", file, filepath);

    g_free(file);   
    g_free(filepath);
    return FALSE;
}


void 
datacd_save_project()
{
    GB_LOG_FUNC
    
    GtkWidget *datatree = glade_xml_get_widget(gnomebaker_getxml(), widget_datacd_tree);
    g_return_if_fail(datatree != NULL);
    GtkTreeModel* datamodel = gtk_tree_view_get_model(GTK_TREE_VIEW(datatree));
    g_return_if_fail(datamodel != NULL);
    
    /* TODO - show a save dialog and select a file 
    FILE *project = fopen("test.gbp", "w");
    fprintf(project, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<GnomeBakerProject Version=\""PACKAGE_VERSION"\" Type=\"0\">\n");
    gtk_tree_model_foreach(datamodel, (GtkTreeModelForeachFunc) datacd_foreach_save_project, project);
    fprintf(project, "</GnomeBakerProject>\n");
    fclose(project);
    */
}

