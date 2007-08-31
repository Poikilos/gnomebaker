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
 * File: dataproject.c
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun Mar  19 21:19:51 2006
 */

#include "dataproject.h"
#include "gbcommon.h"
#include "devices.h"
#include "gnomebaker.h"
#include "selectdevicedlg.h"
#include "filebrowser.h"
#include "preferences.h"
#include "exec.h"
#include "burn.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libxml/xpath.h>


G_DEFINE_TYPE(DataProject, dataproject, PROJECT_TYPE_WIDGET);


static const gdouble overburn_percent = 1.02;


/*THINGS TO DO: -improve DnD within the datacd
 *              -when we create a foder, we must start editing its name
 *              -when we add data to a folder, check for repeated names
 *              -use the new cairo capacity widget(tm) :P
 *              -check the issue of row references. I don´t like the idea of having to store gpointer
 * */


enum
{
    STATUS_IS_FOLDER        = 1 << 0,
    STATUS_EXISTING_SESSION = 1 << 1,
    STATUS_FOLDER_MODIFIED  = 1 << 2
};


enum
{
    DATA_LIST_COL_ICON = 0,
    DATA_LIST_COL_FILE,
    DATA_LIST_COL_SIZE,
    DATA_LIST_COL_HUMANSIZE,
    DATA_LIST_COL_PATH,
    DATA_LIST_COL_STATUS,
    DATA_LIST_COL_ROWREFERENCE,
    DATA_LIST_NUM_COLS
};


enum
{
    DATA_TREE_COL_ICON = 0,
    DATA_TREE_COL_FILE,
    DATA_TREE_COL_SIZE,
    DATA_TREE_COL_HUMANSIZE,
    DATA_TREE_COL_PATH,
    DATA_TREE_COL_STATUS,
    DATA_TREE_NUM_COLS
};


enum
{
    CD_200MB = 0,
    CD_650MB,
    CD_700MB,
    CD_800MB,
    DVD_4GB,
    DVD_8GB
};

static DiskSize data_cd_disk_sizes[] =
{
    /* http://www.cdrfaq.org/faq07.html#S7-6
        http://www.osta.org/technology/dvdqa/dvdqa6.htm */
    {94500.0 * 2048, "200MB CD"},
    {333000.0 * 2048, "650MB CD"},
    {360000.0 * 2048, "700MB CD"},
    {405000.0 * 2048, "800MB CD"}
};


static DiskSize data_dvd_disk_sizes[] =
{
    {2294922.0 * 2048, "4.7GB DVD"},
    {8.5 * 1000 * 1000 * 1000, "8.5GB DVD"} /* DVDs are salesman's MegaByte ie 1000 not 1024 */
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

/*
 * utility function to get the root of the compilation
 * All the elements hang off it. This will contain
 * the name of the compilation. This should be considered when burning.
 */
static void
dataproject_compilation_root_get_iter(DataProject *data_project, GtkTreeIter *iter)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(data_project->dataproject_compilation_store), iter);
}


/*returns the Volume Id of current datacd compilation*/
gchar*
dataproject_compilation_get_volume_id(DataProject *data_project)
{
    GB_LOG_FUNC
      g_return_val_if_fail(data_project != NULL, NULL);

    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(data_project->dataproject_compilation_store),&iter))
    {
        gchar *vol_id = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(data_project->dataproject_compilation_store),&iter,DATA_TREE_COL_FILE, &vol_id,-1);
        return vol_id;
    }
    return NULL;
}


/*should only be called after reseting the list store*/
static void
dataproject_compilation_root_add(DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);

    /* Add the compilation label as the base of our tree */
    GdkPixbuf *icon = gbcommon_get_icon_for_name("gnome-dev-disc-cdr", 16);
    GB_DECLARE_STRUCT(GtkTreeIter, root_iter);
    gtk_tree_store_append(data_project->dataproject_compilation_store, &root_iter, NULL);
    gtk_tree_store_set(data_project->dataproject_compilation_store, &root_iter, DATA_TREE_COL_ICON, icon,
            DATA_TREE_COL_FILE, _("GnomeBaker data disk"),  DATA_TREE_COL_SIZE, (guint64)0,  DATA_TREE_COL_HUMANSIZE,
            "", DATA_TREE_COL_PATH, "", DATA_TREE_COL_STATUS, 0L, -1);
    g_object_unref(icon);
}


/*we change this, making it a bit faster, but this way it is safe, I think*/
static gboolean
dataproject_compilation_is_root(DataProject *data_project, GtkTreeIter *global_iter)
{
    GB_LOG_FUNC
      g_return_val_if_fail(data_project != NULL, FALSE);
    g_return_val_if_fail(global_iter != NULL, FALSE);

    gboolean ret = FALSE;
    /*check if iter is the root*/
    GB_DECLARE_STRUCT(GtkTreeIter, root);

    /*the list should not be empty*/
    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(data_project->dataproject_compilation_store),&root))
    {
        GtkTreePath *path_root = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &root );
        GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store), global_iter );

        if(gtk_tree_path_compare(path_root,path) == 0)
            ret = TRUE;

        gtk_tree_path_free(path_root);
        gtk_tree_path_free(path);
    }
    return ret;
}


/*selects the node given by iter in the tree view. This causes listview to updated*/
static void
dataproject_set_current_node(DataProject* data_project, GtkTreeIter *iter)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);
    g_return_if_fail(iter != NULL);

    GtkTreePath *child_path = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store), iter);
    /* select the row in the tree view */
    GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER(gtk_tree_view_get_model(data_project->tree));
    GtkTreePath *path = gtk_tree_model_filter_convert_child_path_to_path(filter, child_path);
    if(path != NULL)
    {
        gtk_tree_view_expand_to_path(data_project->tree, path);
        gtk_tree_view_set_cursor(data_project->tree, path, NULL, FALSE);
        gtk_tree_path_free(path);

        /* only update current node if the path in the tree view is valid (if not, it is not a dir)*/
        gtk_tree_row_reference_free(data_project->dataproject_current_node);
        data_project->dataproject_current_node = gtk_tree_row_reference_new (GTK_TREE_MODEL(data_project->dataproject_compilation_store), child_path);
    }

    gtk_tree_path_free(child_path);
}


static void
dataproject_get_current_node(DataProject *data_project, GtkTreeIter *iter)
{
    GB_LOG_FUNC

    GtkTreePath *path = gtk_tree_row_reference_get_path(data_project->dataproject_current_node);
    GtkTreeModel *model = gtk_tree_row_reference_get_model(data_project->dataproject_current_node);
    gtk_tree_model_get_iter (model,iter, path);
    gtk_tree_path_free(path);
}


static void
dataproject_list_view_clear(GtkTreeView *file_list)
{
    GB_LOG_FUNC
    g_return_if_fail(file_list != NULL);

    GtkListStore *store = GTK_LIST_STORE( gtk_tree_view_get_model(file_list) );

    /* TODO/CHECK: this code below was itended to free all the row references. It should work but it makes the app to crash
     * whenever we delete something. Check this. We may use G_TYPE_TREE_ROW_REFERENCE instead of G_TYPE_POINTER
     */

/*  GB_DECLARE_STRUCT(GtkTreeIter, next_iter);
    int count = 0;
    while( gtk_tree_model_iter_nth_child (GTK_TREE_MODEL(store), &next_iter,NULL ,count))
    {
        GValue reference_val = {0};

        gtk_tree_model_get_value(GTK_TREE_MODEL(store),&next_iter,DATA_LIST_COL_ROWREFERENCE, &reference_val);
        GtkTreeRowReference * row_reference = (GtkTreeRowReference*)g_value_get_pointer(&reference_val);

        g_value_unset(&reference_val);

        if(row_reference != NULL)
        {
            gtk_tree_row_reference_free(row_reference);
        }

        ++count;

    }
*/
    gtk_list_store_clear(store);
}


static void
dataproject_list_view_update(DataProject *data_project, GtkTreeIter *parent_iter)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);
    g_return_if_fail(parent_iter != NULL);

    GtkListStore *store = GTK_LIST_STORE( gtk_tree_view_get_model(data_project->list) );
    dataproject_list_view_clear(data_project->list);

    GB_DECLARE_STRUCT(GtkTreeIter, child_iter);
    int child_number = 0;
    /* Add the childrens of the parent iter to the */
    while(gtk_tree_model_iter_nth_child (GTK_TREE_MODEL(data_project->dataproject_compilation_store),
            &child_iter, parent_iter, child_number))
    {
        GdkPixbuf *icon = NULL;
        gchar *base_name = NULL, *human_readable = NULL, *file_name = NULL;
        guint64 size = 0;
        gulong status = 0L;

        gtk_tree_model_get(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &child_iter,
                DATA_TREE_COL_ICON, &icon, DATA_TREE_COL_FILE, &base_name,
                DATA_TREE_COL_SIZE, &size, DATA_TREE_COL_HUMANSIZE, &human_readable,
                DATA_TREE_COL_PATH, &file_name, DATA_TREE_COL_STATUS, &status, -1);

        GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store),&child_iter);
        GtkTreeRowReference *row_reference = gtk_tree_row_reference_new(GTK_TREE_MODEL(data_project->dataproject_compilation_store),path);
        gtk_tree_path_free(path);

        GB_DECLARE_STRUCT(GtkTreeIter, store_iter);
        gtk_list_store_insert(store, &store_iter, child_number);
        gtk_list_store_set(store, &store_iter,
                DATA_LIST_COL_ICON, icon, DATA_LIST_COL_FILE, base_name,
                DATA_LIST_COL_SIZE, size, DATA_LIST_COL_HUMANSIZE, human_readable,
                DATA_LIST_COL_PATH, file_name, DATA_LIST_COL_STATUS, status,
                DATA_LIST_COL_ROWREFERENCE, row_reference, -1);
        g_object_unref(icon);

        /*row_reference should not be unreferenced as the set function just stores the pointer value */
        /*we have to do it manually when clearing the list view*/
        ++child_number;
    }
}


static void
dataproject_set_multisession(DataProject* data_project, const gchar *msinfo)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);

    if(data_project->msinfo != NULL)
        g_free(data_project->msinfo);
    data_project->msinfo = (msinfo == NULL ? NULL : g_strdup(msinfo));
}


static void
dataproject_on_show_humansize_changed( GConfClient *client,
                                    guint cnxn_id,
                                    GConfEntry *entry,
                                    DataProject* data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);

    GtkTreeViewColumn *size_column = gtk_tree_view_get_column(data_project->list, DATA_TREE_COL_SIZE-1);
    GtkTreeViewColumn *humansize_column = gtk_tree_view_get_column(data_project->list, DATA_TREE_COL_HUMANSIZE-1);
    const gboolean show_human_size = preferences_get_bool(GB_SHOWHUMANSIZE);
    gtk_tree_view_column_set_visible(size_column, !show_human_size);
    gtk_tree_view_column_set_visible(humansize_column, show_human_size);
}


static gdouble
dataproject_get_datadisk_size(DataProject* data_project)
{
    GB_LOG_FUNC
      g_return_val_if_fail(data_project != NULL, 0.0);
    if(data_project->is_dvd)
        data_project->data_disk_size = data_dvd_disk_sizes[gtk_option_menu_get_history(PROJECT_WIDGET(data_project)->menu)].size;
    else
        data_project->data_disk_size = data_cd_disk_sizes[gtk_option_menu_get_history(PROJECT_WIDGET(data_project)->menu)].size;
    return data_project->data_disk_size;
}


#ifndef CAIRO_WIDGETS
static gchar*
dataproject_format_progress_text(DataProject* data_project, gdouble current_size)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);

    const gboolean is_cd = data_project->data_disk_size < data_disk_sizes[DVD_4GB].size;

    gchar *current = NULL, *remaining = NULL, *buf = NULL;
    if(is_cd && (current_size > data_project->data_disk_size) && (current_size < (data_project->data_disk_size * overburn_percent)))
    {
        current = gbcommon_humanreadable_filesize((guint64)current_size);
        remaining = gbcommon_humanreadable_filesize((guint64)(data_project->data_disk_size));
        buf = g_strdup_printf(_("%s of %s used. Overburning."), current, remaining);
    }
    else if(current_size < data_project->data_disk_size)
    {
        current = gbcommon_humanreadable_filesize((guint64)current_size);
        remaining = gbcommon_humanreadable_filesize((guint64)(data_project->data_disk_size - current_size));
        buf = g_strdup_printf(_("%s used - %s remaining."), current, remaining);
    }
    else
    {
        current = gbcommon_humanreadable_filesize((guint64)current_size);
        remaining = gbcommon_humanreadable_filesize((guint64)(data_project->data_disk_size));
        buf = g_strdup_printf(_("%s of %s used. Disk full."), current, remaining);
    }
    g_free(current);
    g_free(remaining);
    return buf;
}
#endif


static void
dataproject_update_progress_bar(DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);

#ifndef CAIRO_WIDGETS
    /* Now update the progress bar with the cd size */
    GtkProgressBar *progress_bar = PROJECT_WIDGET(data_project)->progress_bar;
#else
	GBCairoFillBar *progress_bar = PROJECT_WIDGET(data_project)->progress_bar;
#endif

    const gdouble disk_size = dataproject_get_datadisk_size(data_project);

    if (data_project->dataproject_compilation_size < 0 || data_project->dataproject_compilation_size == 0)
    {
        data_project->dataproject_compilation_size = 0;
#ifndef CAIRO_WIDGETS
        gtk_progress_bar_set_fraction(progress_bar, 0.0);
        gchar *buf = dataproject_format_progress_text(data_project, 0.0);
        gtk_progress_bar_set_text(progress_bar, buf);
        g_free(buf);
#else
		gb_cairo_fillbar_set_project_total_size(progress_bar, 0);
#endif

        /* disable the create button as there's nothing on the disk */
        gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(data_project)->button), FALSE);

        /* remove the multisession flag as there's nothing on the disk */
        dataproject_set_multisession(data_project, NULL);
    }
    else
    {
        gdouble fraction = 0.0;
#ifndef CAIRO_WIDGETS
        if(disk_size > 0)
            fraction = (gdouble)data_project->dataproject_compilation_size/disk_size;
#endif
        if(!data_project->is_dvd && (data_project->dataproject_compilation_size > disk_size) &&
                (data_project->dataproject_compilation_size < (disk_size * overburn_percent)))
        {
#ifndef CAIRO_WIDGETS
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 1.0);
#endif
            gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(data_project)->button), TRUE);
        }
        else if(data_project->dataproject_compilation_size > disk_size)
        {
#ifndef CAIRO_WIDGETS
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 1.0);
#endif
            gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(data_project)->button), FALSE);
        }
        else
        {
#ifndef CAIRO_WIDGETS
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
#endif
            gtk_widget_set_sensitive(GTK_WIDGET(PROJECT_WIDGET(data_project)->button), TRUE);
        }
#ifndef CAIRO_WIDGETS
        gchar *buf = dataproject_format_progress_text(data_project, data_project->dataproject_compilation_size);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), buf);
        g_free(buf);
#else
		gb_cairo_fillbar_set_project_total_size(progress_bar,data_project->dataproject_compilation_size);
#endif
    }
}


static GtkTreeIter
dataproject_add_to_model(DataProject *data_project, const gboolean is_folder, const gchar *file_name,
        const gchar *base_name, GtkTreeIter *parent_node, gboolean existing_session, guint64 size)
{
    GB_LOG_FUNC
    /*TODO/FIX:what to do with the size of the folders?
     * We could leave as before, showing the size of its contents,
     * but we cannot call gbcommon_calc_dir_size because it would be very slow.
     * We have to perform the loop through the contents before
     * setting the values to the store (but after adding a new element)
     * and dataproject_add_to_compilation should have another argument, a pointer that returns
     * the accumulated size. If we use this method, if,
     * whe we delete items, we would have to go up through the tree updating the parents
     */
    gchar *path_to_show = NULL;
    if(is_folder)
    {
        /*Is there any sense storing the file name for folders?*/
        /*we don´t want to show path for folders, so the easiest way is to store nothing*/
        path_to_show = g_strdup("");
    }
    else
    {
        path_to_show = g_strdup(file_name);
    }
    data_project->dataproject_compilation_size += size;

    /*
     * we allow to add more size than the capacity of the disc,
     * and show the error dialog only before burning. The progress bar should
     * reflect somehow this situation
     *
     * updating progress bar for every file, makes it slow
     * (and for sure the filter_func in the tree_view, but that
     * is something we cannot avoid, even disconnecting it from the store, I think)
     * */

    GdkPixbuf *icon = NULL;
    if(existing_session)
    {
        icon = gbcommon_get_icon_for_name("gnome-dev-cdrom", 16);
    }
    else if (!strcmp(file_name, ""))
    {
        icon = gbcommon_get_icon_for_name("gnome-fs-directory", 16);
    }
    else
    {
        gchar *mime = gbcommon_get_mime_type(file_name);
        icon = gbcommon_get_icon_for_mime(mime, 16);
        g_free(mime);
    }

    gulong status = 0L;
    if(is_folder) status |= STATUS_IS_FOLDER;
    if(existing_session) status |= STATUS_EXISTING_SESSION;

    gchar *human_readable = gbcommon_humanreadable_filesize(size);
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    gtk_tree_store_append(data_project->dataproject_compilation_store, &iter, parent_node);
    gtk_tree_store_set(data_project->dataproject_compilation_store, &iter,
            DATA_TREE_COL_ICON, icon, DATA_TREE_COL_FILE, base_name,
            DATA_TREE_COL_SIZE, size, DATA_TREE_COL_HUMANSIZE, human_readable,
            DATA_TREE_COL_PATH, path_to_show, DATA_TREE_COL_STATUS, status, -1);
    g_free(human_readable);
    g_object_unref(icon);
    return iter;
}


static gboolean
dataproject_add_to_compilation(DataProject *data_project, const gchar *file, GtkTreeIter *parent_node, gboolean existing_session)
{
    GB_LOG_FUNC
      g_return_val_if_fail(data_project != NULL, FALSE);
    g_return_val_if_fail(file != NULL, FALSE);
    g_return_val_if_fail(parent_node != NULL, FALSE);
    gboolean ret = TRUE;

    gchar *file_name = gbcommon_get_local_path(file);
    gchar *path_to_show = NULL;
    GB_TRACE("dataproject_add_to_compilation - Adding file [%s]\n", file_name);

    GB_DECLARE_STRUCT(struct stat, s);
    const gint statret = lstat(file_name, &s);
    if(statret == 0)
    {
        const gboolean is_folder = S_ISDIR(s.st_mode);
        if(S_ISREG(s.st_mode) || is_folder)
        {
            gchar *base_name = g_path_get_basename(file_name);
            GtkTreeIter iter = dataproject_add_to_model(data_project, is_folder,
                    file_name, base_name, parent_node, existing_session, (guint64)s.st_size);
            g_free(base_name);

            /*recursively add contents*/
            if(is_folder)
            {
                GDir *dir = g_dir_open(file_name, 0, NULL);
                if(dir != NULL)
                {
                    const gchar *name = g_dir_read_name(dir);
                    while(name != NULL)
                    {
                        /* build up the full path to the name */
                        gchar *full_name = g_build_filename(file_name, name, NULL);
                        /*if the disc is full, do not add more files! (for now)*/
                        if(!dataproject_add_to_compilation(data_project, full_name, &iter, existing_session))
                        {
                            g_free(full_name);
                            break;
                        }
                        g_free(full_name);
                        name = g_dir_read_name(dir);
                    }
                    g_dir_close(dir);
                }
            }
        }
    }
    else
    {
        g_warning("dataproject_add_to_compilation - failed to stat file [%s] with ret code [%d] error no [%s]",
            file_name, statret, strerror(errno));
        ret = FALSE;
    }
    g_free(path_to_show);
    g_free(file_name);
    return ret;
}


static void
dataproject_add_selection(Project *project, GtkSelectionData *selection)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
    g_return_if_fail(selection != NULL);

    gnomebaker_show_busy_cursor(TRUE);

    DataProject *data_project = DATAPROJECT_WIDGET(project);
    GB_TRACE("dataproject_add_selection - received sel [%s]\n", selection->data);

    GB_DECLARE_STRUCT(GtkTreeIter, parent_iter);
    if(!gtk_tree_row_reference_valid(data_project->dataproject_current_node))
    {
        dataproject_compilation_root_get_iter(data_project, &parent_iter);
        dataproject_set_current_node(data_project, &parent_iter);
    }

    dataproject_get_current_node(data_project, &parent_iter);
   /* Do not disconnect the model from the view.
    * Disconnecting the tree model causes the tree view not to
    * behave as we want when we add elements
    */
    const gchar *file = strtok((gchar*)selection->data,"\n");
    while(file != NULL)
    {
        /* We pass the current node. It depends of the selection in the tree view */
        if(!dataproject_add_to_compilation(data_project, file, &parent_iter, FALSE))
            break;
        file = strtok(NULL, "\n");
    }

    dataproject_update_progress_bar(data_project);

    /*expand node if posible*/
    GtkTreePath *global_path = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &parent_iter);
    GtkTreeModel *model = gtk_tree_view_get_model(data_project->tree);
    GtkTreePath *path = gtk_tree_model_filter_convert_child_path_to_path(GTK_TREE_MODEL_FILTER(model), global_path);
    gtk_tree_view_expand_to_path(data_project->tree,path);
    gtk_tree_path_free(global_path);
    gtk_tree_path_free(path);
    dataproject_list_view_update(data_project, &parent_iter);

    gnomebaker_show_busy_cursor(FALSE);
}


static void
dataproject_on_drag_data_received(
    GtkWidget  *widget,
    GdkDragContext  *context,
    gint x,
    gint y,
    GtkSelectionData  *selection_data,
    guint info,
    guint time,
    DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);
    g_return_if_fail(widget != NULL);

    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
    if(GTK_IS_TREE_MODEL_FILTER(model))
    {
        GtkTreePath *path = NULL;
        GtkTreeViewDropPosition pos;

        gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW(widget), x, y, &path, NULL);
        if(path)
        {
            GB_DECLARE_STRUCT(GtkTreeIter, iter);
            gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter,path);
            GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
            gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &global_iter, &iter);
            dataproject_set_current_node(data_project, &global_iter);
            gtk_tree_path_free(path);
        }
    }
    dataproject_add_selection(PROJECT_WIDGET(data_project), selection_data);
    if(GTK_IS_TREE_MODEL_FILTER(model))
    {
        g_signal_stop_emission_by_name(G_OBJECT(widget), "drag_data_received");
    }
    project_set_dirty(PROJECT_WIDGET(data_project), TRUE);
}


static gboolean
dataproject_drag_motion_expand_timeout(DataProject *data_project)
{
    GB_LOG_FUNC
      g_return_val_if_fail(data_project != NULL, FALSE);

    gdk_threads_enter();
    gtk_tree_view_expand_row(data_project->tree, data_project->last_path, FALSE);
    gdk_threads_leave();
    return FALSE; /* only call once */
}


static gboolean
dataproject_drag_motion_scroll_timeout(DataProject *data_project)
{
    GB_LOG_FUNC
      g_return_val_if_fail(data_project != NULL, FALSE);

    GdkRectangle visible_rect;
    gint y;
    gint offset;
    gfloat value;
    gdouble adjust_value;
    gdouble adjust_upper;
    gdouble adjust_page_size;

    gdk_threads_enter();
    gdk_window_get_pointer (gtk_tree_view_get_bin_window(data_project->tree), NULL, &y, NULL);
    GtkAdjustment* adjust = gtk_tree_view_get_vadjustment(data_project->tree);
    g_object_get(G_OBJECT(adjust), "value",&adjust_value, "upper",&adjust_upper, "page-size", &adjust_page_size, NULL);

    y += adjust_value;
    gtk_tree_view_get_visible_rect (data_project->tree, &visible_rect);

      /* see if we are near the edge. */
    offset = y - (visible_rect.y + 2 * 15); /*15 is the edge size, acording to gtktreeview.c*/
    if (offset > 0)
    {
        offset = y - (visible_rect.y + visible_rect.height - 2 * 15);
        if (offset < 0)
        {
            data_project->autoscroll_timeout_id = 0;
            gdk_threads_leave();
            return FALSE;
        }
    }

    value =  CLAMP(adjust_value + offset, 0.0, adjust_upper - adjust_page_size);
    gtk_adjustment_set_value (adjust, value);

    /*reset*/
    data_project->autoscroll_timeout_id = 0;
    gdk_threads_leave();
    return FALSE; /* only call once */
}


static gboolean
dataproject_on_drag_motion(GtkWidget *widget,
                        GdkDragContext *context,
                        gint x,guint y,
                        guint time,
                        DataProject *data_project)

{
    GB_LOG_FUNC
      g_return_val_if_fail(data_project != NULL, FALSE);

    data_project->expand_timeout_id = 0;
    data_project->autoscroll_timeout_id = 0;
    GtkTreePath *path = NULL;

    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, NULL);
    if (path)
    {
        if (!data_project->last_path || ((data_project->last_path) && gtk_tree_path_compare(data_project->last_path, path) != 0))
        {
            if(data_project->expand_timeout_id != 0)
            {
                g_source_remove(data_project->expand_timeout_id);
                data_project->expand_timeout_id = 0;
            }

            gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW(widget),path,GTK_TREE_VIEW_DROP_INTO_OR_AFTER);

            if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
                data_project->expand_timeout_id = g_timeout_add(750, (GSourceFunc) dataproject_drag_motion_expand_timeout, data_project);

            if(data_project->autoscroll_timeout_id == 0)
            {
                data_project->autoscroll_timeout_id = g_timeout_add(150,
                        (GSourceFunc)dataproject_drag_motion_scroll_timeout, data_project);
            }
        }
    }
    else
    {
        if(data_project->expand_timeout_id != 0)
        {
            g_source_remove(data_project->expand_timeout_id);
            data_project->expand_timeout_id = 0;
        }
        gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW(widget),NULL,0);
    }
    if (data_project->last_path)
        gtk_tree_path_free(data_project->last_path);
    data_project->last_path = path;
    return TRUE;

}


/* adds to the list all the children of parent_iter*/
static void
dataproject_add_recursive_reference_list(DataProject *data_project, GtkTreeIter *parent_iter,
                                  GList **rref_list)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);
    g_return_if_fail(parent_iter != NULL);
    g_return_if_fail(rref_list != NULL);

    int count=0;
    GB_DECLARE_STRUCT(GtkTreeIter, child_iter)
    while(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(data_project->dataproject_compilation_store),&child_iter,parent_iter,count))
    {
        /*add the row reference of each child to the list */
        /*obtain the path from the iter*/
        GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store),&child_iter);
        /*obtain the row reference from the path */
        GtkTreeRowReference *row_reference = gtk_tree_row_reference_new(GTK_TREE_MODEL(data_project->dataproject_compilation_store), path);
        /*and finally add*/
        if(row_reference != NULL)
        {
            GB_TRACE("dataproject_add_recursive_reference_list - Added to remove [%s]\n",gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &child_iter));
            *rref_list = g_list_prepend(*rref_list, row_reference);
        }

        gtk_tree_path_free(path);

        /* add the children of the child recursively*/
        /* it is important the order in which we add the elements to the list
         * otherwise we can delete a row before its children are deleted*/
        dataproject_add_recursive_reference_list(data_project, &child_iter, rref_list);
        ++count;
    }
}


static void
dataproject_foreach_fileselection(GtkTreeModel *file_model,
                                  GtkTreePath *path,
                                  GtkTreeIter *iter,
                                  DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(file_model != NULL);
    g_return_if_fail(iter != NULL);
    g_return_if_fail(data_project != NULL);

    /*obtain the row reference*/
    if(GTK_IS_LIST_STORE(file_model))
    {
        GtkTreeRowReference *row_reference = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(file_model),iter,DATA_LIST_COL_ROWREFERENCE, &row_reference,-1);

        GtkTreePath *global_path = gtk_tree_row_reference_get_path (row_reference);
        if(global_path != NULL)
        {
            data_project->rowref_list = g_list_prepend(data_project->rowref_list, row_reference);

            /*Get the iter. Use this as parent iter to add all the children recursively to the list*/
            GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
            if(gtk_tree_model_get_iter(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &global_iter, global_path))
            {
                GB_TRACE("dataproject_foreach_fileselection - Added to remove [%s]\n", gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &global_iter));
                dataproject_add_recursive_reference_list(data_project, &global_iter, &data_project->rowref_list);
            }
            gtk_tree_path_free(global_path);
        }
    }
}


static void
dataproject_clear(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));

    gnomebaker_show_busy_cursor(TRUE);

    DataProject *data_project = DATAPROJECT_WIDGET(project);

    /* Fastest way, clear all and add a new root node */
    gtk_tree_store_clear(data_project->dataproject_compilation_store);
    dataproject_compilation_root_add(data_project);

    data_project->dataproject_compilation_size = 0;

    /*update list view*/
    GB_DECLARE_STRUCT(GtkTreeIter, root);
    dataproject_compilation_root_get_iter(data_project, &root);
    dataproject_set_current_node(data_project, &root);
    dataproject_list_view_update(data_project, &root);
    dataproject_update_progress_bar(data_project);

    /* clear any multisession flags */
    dataproject_set_multisession(data_project, NULL);

    gnomebaker_show_busy_cursor(FALSE);
}


static void
dataproject_remove(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
    DataProject *data_project = DATAPROJECT_WIDGET(project);

    gnomebaker_show_busy_cursor(TRUE);

    data_project->rowref_list = NULL;   /* list of GtkTreeRowReferences to remove */

    /* TODO this used to handle both the tree and list selection */
    GtkTreeSelection *selection = gtk_tree_view_get_selection(data_project->list);
    GtkTreeModel *model = gtk_tree_view_get_model(data_project->list);

    g_return_if_fail(model != NULL);

    if(GTK_IS_LIST_STORE(model))
    {
        /*this behaves differently, since it has to retrieve the references to g_compilationStore*/
        gtk_tree_selection_selected_foreach(selection, (GtkTreeSelectionForeachFunc)dataproject_foreach_fileselection, data_project);
    }
    else
    {
        /*check that the selected item is not the root*/
        GtkTreeModel *tree_model = NULL;
        GB_DECLARE_STRUCT(GtkTreeIter, iter);
        GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
        gtk_tree_selection_get_selected (selection, &tree_model ,&iter);
        gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(tree_model),&global_iter,&iter);

        if(!dataproject_compilation_is_root(data_project, &global_iter))
        {
            GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store),&global_iter);
            GtkTreeRowReference *row_reference = gtk_tree_row_reference_new(GTK_TREE_MODEL(data_project->dataproject_compilation_store), path);

            data_project->rowref_list = g_list_prepend(data_project->rowref_list, row_reference);

            /*add recursively the children*/
            dataproject_add_recursive_reference_list(data_project, &global_iter, &data_project->rowref_list);

            gtk_tree_path_free(path);

            /*if we remove the selected node from the tree, we have to select a new node. The parent may be? (we always will have root)*/
            GB_DECLARE_STRUCT(GtkTreeIter, global_parent_iter);
            if(gtk_tree_model_iter_parent (GTK_TREE_MODEL(data_project->dataproject_compilation_store),&global_parent_iter,&global_iter))
            {
                dataproject_set_current_node(DATAPROJECT_WIDGET(project), &global_parent_iter);
            }
        }
    }

    GList *node;
    for (node = data_project->rowref_list; node != NULL; node = node->next )
    {
        if(node->data!=NULL)
        {
            GtkTreePath  *path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);
            if (path!=NULL)
            {
                GB_DECLARE_STRUCT(GtkTreeIter, iter);
                if (gtk_tree_model_get_iter(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &iter, path))
                {
                    GValue status = { 0 };
                    gtk_tree_model_get_value(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &iter, DATA_TREE_COL_STATUS, &status);
                    if(!(g_value_get_ulong(&status) & STATUS_EXISTING_SESSION))
                    {
                        GValue value = { 0 };
                        gtk_tree_model_get_value(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &iter, DATA_TREE_COL_SIZE, &value);

                        /*compilation size*/
                        data_project->dataproject_compilation_size-=g_value_get_uint64(&value);

                        g_value_unset(&value);

                        GB_TRACE("dataproject_remove - Removed [%s]\n",gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &iter));
                        gtk_tree_store_remove(data_project->dataproject_compilation_store, &iter);
                    }
                    g_value_unset(&status);
                }
                gtk_tree_path_free(path);
            }
        }
    }

    g_list_foreach(data_project->rowref_list, (GFunc)gtk_tree_row_reference_free, NULL);
    g_list_free(data_project->rowref_list);
    data_project->rowref_list = NULL;

    /*update list view*/
    GB_DECLARE_STRUCT(GtkTreeIter, parent_iter);
    if(!gtk_tree_row_reference_valid(data_project->dataproject_current_node))
    {
        dataproject_compilation_root_get_iter(data_project, &parent_iter);
        dataproject_set_current_node(data_project, &parent_iter);
    }
    dataproject_get_current_node(data_project, &parent_iter);
    dataproject_list_view_update(data_project, &parent_iter);

    dataproject_update_progress_bar(data_project);

    gnomebaker_show_busy_cursor(FALSE);
}


static void
dataproject_on_remove_clicked(GtkWidget *menuitem, DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);
    dataproject_remove(PROJECT_WIDGET(data_project));
}


static void
dataproject_on_clear_clicked(GtkWidget *menuitem, DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);
    dataproject_clear(PROJECT_WIDGET(data_project));
}


static void
dataproject_list_contents_cell_edited(GtkCellRendererText *cell,
                            gchar *path_string,
                            gchar *new_text,
                            DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(cell != NULL);
    g_return_if_fail(path_string != NULL);
    g_return_if_fail(new_text != NULL);
    g_return_if_fail(data_project != NULL);

    GtkTreeModel *model = gtk_tree_view_get_model(data_project->list);
    /*Read the row reference and edit the value in g_compilationStore*/
    GB_DECLARE_STRUCT(GtkTreeIter, iter1);
    if(gtk_tree_model_get_iter_from_string(model, &iter1, path_string))
    {
        GValue reference_val = {0};
        gtk_tree_model_get_value(model, &iter1, DATA_LIST_COL_ROWREFERENCE, &reference_val);
        GtkTreeRowReference  *row_reference = (GtkTreeRowReference*)g_value_get_pointer(&reference_val);
        g_value_unset(&reference_val);

        /*we cannot free this reference because it continues existing in the list store*/
        GtkTreePath *path = gtk_tree_row_reference_get_path (row_reference);

        if(path != NULL)
        {
            GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
            gtk_tree_model_get_iter(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &global_iter, path);

            GValue val = {0};
            g_value_init(&val, G_TYPE_STRING);
            g_value_set_string(&val, new_text);
            gtk_tree_store_set_value(data_project->dataproject_compilation_store, &global_iter, DATA_TREE_COL_FILE, &val);
            g_value_unset(&val);
            gtk_tree_path_free(path);
        }
    }

    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    dataproject_get_current_node(data_project, &iter);
    dataproject_list_view_update(data_project, &iter);

    /*disable editing*/
    GValue value = { 0 };
    g_value_init(&value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&value, FALSE);

    g_object_set_property(G_OBJECT(cell), "editable", &value);
}


static void
dataproject_tree_contents_cell_edited(GtkCellRendererText *cell,
                            gchar *path_string,
                            gchar *new_text,
                            DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(cell != NULL);
    g_return_if_fail(path_string != NULL);
    g_return_if_fail(new_text != NULL);
    g_return_if_fail(data_project != NULL);

    /* path_string is relative to the GtkTreeModelFilter. We have to convert it to a iter first and then to
     * a iter relative to the child GtkTreeModel (in out case g_compilationStore) */
    GB_DECLARE_STRUCT(GtkTreeIter, iter1);
    if(gtk_tree_model_get_iter_from_string(gtk_tree_view_get_model(data_project->tree), &iter1, path_string))
    {
        GB_DECLARE_STRUCT(GtkTreeIter, child_iter);
        gtk_tree_model_filter_convert_iter_to_child_iter(
                GTK_TREE_MODEL_FILTER(gtk_tree_view_get_model(data_project->tree)), &child_iter, &iter1);

        GValue val = {0};
        g_value_init(&val, G_TYPE_STRING);
        g_value_set_string(&val, new_text);
        gtk_tree_store_set_value(data_project->dataproject_compilation_store, &child_iter,
                DATA_TREE_COL_FILE, &val);
        g_value_unset(&val);
    }

    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    dataproject_get_current_node(data_project, &iter);
    dataproject_list_view_update(data_project, &iter);

    /*disable editing*/
    GValue value = { 0 };
    g_value_init(&value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&value, FALSE);

    g_object_set_property(G_OBJECT(cell), "editable", &value);
}


static void
dataproject_on_edit(gpointer widget, GtkTreeView *tree_view)
{
    GB_LOG_FUNC
    g_return_if_fail(tree_view != NULL);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GList *selected_path_list =  gtk_tree_selection_get_selected_rows(selection, NULL);
    GtkTreePath *path = g_list_nth_data(selected_path_list, 0);

    if(path != NULL)
    {
        GtkTreeViewColumn *column = gtk_tree_view_get_column(tree_view, 0);
        GList *renderer_list = gtk_tree_view_column_get_cell_renderers(column);
        /*get the second renderer*/
        GtkCellRenderer *text_renderer =  GTK_CELL_RENDERER(g_list_nth_data(renderer_list,1));

        /*enable editing*/
        GValue value = { 0 };
        g_value_init(&value, G_TYPE_BOOLEAN);
        g_value_set_boolean(&value, TRUE);
        g_object_set_property(G_OBJECT(text_renderer), "editable", &value);
        g_value_unset(&value);
        gtk_tree_view_set_cursor(tree_view, path, column, TRUE);
    }
    g_list_foreach (selected_path_list, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (selected_path_list);
}


static void
dataproject_on_add_folder(gpointer widget, DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);

    GB_DECLARE_STRUCT(GtkTreeIter, parent_iter);
    dataproject_get_current_node(data_project, &parent_iter);

    /*TODO: what to do with the size?*/
    guint64 size = 0;
    gchar *human_readable = gbcommon_humanreadable_filesize(size);

    /*we will use this mime: x-directory/normal*/
    GdkPixbuf *icon = gbcommon_get_icon_for_mime("x-directory/normal", 16);

    /*Add a folder to the current node  */
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    gtk_tree_store_append(data_project->dataproject_compilation_store, &iter, &parent_iter);
    gtk_tree_store_set(data_project->dataproject_compilation_store, &iter,
            DATA_TREE_COL_ICON, icon, DATA_TREE_COL_FILE, _("New Folder"),
            DATA_TREE_COL_SIZE, size, DATA_TREE_COL_HUMANSIZE, human_readable,
            DATA_TREE_COL_PATH, "", DATA_TREE_COL_STATUS, STATUS_IS_FOLDER, -1);

    g_object_unref(icon);

    /*expand node if posible*/
    GtkTreeModel *model = gtk_tree_view_get_model(data_project->tree);
    GtkTreePath *global_path = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &parent_iter);
    GtkTreePath *path = gtk_tree_model_filter_convert_child_path_to_path(GTK_TREE_MODEL_FILTER(model), global_path);
    gtk_tree_view_expand_row (data_project->tree, path, FALSE);

    gtk_tree_path_free(global_path);
    gtk_tree_path_free(path);

    dataproject_list_view_update(data_project, &parent_iter);

    /*we must edit the name of the new folder*/
    GtkTreeViewColumn* column =NULL;

    column = gtk_tree_view_get_column (data_project->tree,0);

    GList * renderer_list = gtk_tree_view_column_get_cell_renderers(column);
    /*get the second renderer*/
    GtkCellRenderer *text_renderer =  GTK_CELL_RENDERER(g_list_nth_data(renderer_list,1));

    /*enable editing*/
    GValue value = { 0 };
    g_value_init(&value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&value, TRUE);

    g_object_set_property(G_OBJECT(text_renderer), "editable", &value);

    g_value_unset(&value);

    global_path = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store),&iter);
    path = gtk_tree_model_filter_convert_child_path_to_path(GTK_TREE_MODEL_FILTER(model),global_path);

    gtk_tree_view_set_cursor(data_project->tree, path, column, TRUE);

    gtk_tree_path_free(global_path);
    gtk_tree_path_free(path);
}


static void
dataproject_on_list_open(GtkTreeModel *model, GtkTreeIter *iter, DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(model != NULL);
    g_return_if_fail(iter != NULL);
    g_return_if_fail(data_project != NULL);

    GValue status = {0};
    gtk_tree_model_get_value(model, iter, DATA_LIST_COL_STATUS, &status);
    if(g_value_get_ulong(&status) & STATUS_IS_FOLDER)
    {
        GValue reference_val = {0};
        gtk_tree_model_get_value(model, iter, DATA_LIST_COL_ROWREFERENCE, &reference_val);
        GtkTreeRowReference  *row_reference = (GtkTreeRowReference*)g_value_get_pointer(&reference_val);
        g_value_unset(&reference_val);

        GtkTreePath *global_path = gtk_tree_row_reference_get_path(row_reference);

        GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
        if(gtk_tree_model_get_iter(GTK_TREE_MODEL(data_project->dataproject_compilation_store),&global_iter,global_path))
        {
            dataproject_set_current_node(data_project, &global_iter);
        }
        gtk_tree_path_free(global_path);
    }
    else
    {
        gchar *file = NULL;
        gtk_tree_model_get(model, iter, DATA_LIST_COL_PATH, &file, -1);
        gbcommon_launch_app_for_file(file);
        g_free(file);
    }
    g_value_unset(&status);
}


static void
dataproject_on_list_dbl_click(GtkTreeView *tree_view,
                         GtkTreePath *path,
                         GtkTreeViewColumn *column,
                         DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(tree_view != NULL);
    g_return_if_fail(path != NULL);
    g_return_if_fail(column != NULL);
    g_return_if_fail(data_project != NULL);

    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    if(gtk_tree_model_get_iter(model, &iter, path))
        dataproject_on_list_open(model, &iter, data_project);
}


static void
dataproject_on_open(gpointer widget, DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);

    GtkTreeModel *model = gtk_tree_view_get_model(data_project->list);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(data_project->list);
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    gtk_tree_selection_selected_foreach(selection,
            (GtkTreeSelectionForeachFunc)gbcommon_get_first_selected_row, &iter);
    dataproject_on_list_open(model, &iter, data_project);
}


static gboolean
dataproject_on_button_pressed(GtkWidget *widget, GdkEventButton *event, DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(data_project != NULL, FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    /* look for a right click */
    if(event->button == 3)
    {
        GtkTreeView *view = (GtkTreeView*)widget;
        GtkTreeModel *model = gtk_tree_view_get_model(view);

        if(GTK_IS_LIST_STORE(model))
        {
            GtkWidget *menu = gtk_menu_new();
            GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
            const gint count = gtk_tree_selection_count_selected_rows(selection);
            if(count == 1)
            {
                gbcommon_append_menu_item_stock(menu, _("_Open"), GTK_STOCK_OPEN,
                        (GCallback)dataproject_on_open, data_project);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            }
            gbcommon_append_menu_item_stock(menu, _("_Add Folder"), GTK_STOCK_NEW,
                    (GCallback)dataproject_on_add_folder, data_project);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            if(count == 1)
            {
                gbcommon_append_menu_item_stock(menu, _("_Edit name"), GTK_STOCK_DND,
                        (GCallback)dataproject_on_edit, data_project->list);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            }
            gbcommon_append_menu_item_stock(menu, _("_Remove selected"), GTK_STOCK_REMOVE,
                    (GCallback)dataproject_on_remove_clicked, data_project);
            gbcommon_append_menu_item_stock(menu, _("Clear"), GTK_STOCK_CLEAR,
                    (GCallback)dataproject_on_clear_clicked, data_project);
            gtk_widget_show_all(menu);

            /* Note: event can be NULL here when called. However,
             *  gdk_event_get_time() accepts a NULL argument */
            gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                    (event != NULL) ? event->button : 0, gdk_event_get_time((GdkEvent*)event));
            return TRUE;
        }
        else if(model != NULL)
        {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
            const gint count = gtk_tree_selection_count_selected_rows(selection);
            if(count == 1)
            {
                /*Do not show the menu if the selected item is the root*/
                GtkTreeModel *tree_model = NULL;
                GB_DECLARE_STRUCT(GtkTreeIter, iter);
                GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
                gtk_tree_selection_get_selected (selection, &tree_model ,&iter);
                gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(tree_model),&global_iter,&iter);

                GtkWidget *menu = gtk_menu_new();

                gbcommon_append_menu_item_stock(menu, _("_Add Folder"), GTK_STOCK_NEW,
                        (GCallback)dataproject_on_add_folder, data_project);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

                gbcommon_append_menu_item_stock(menu, _("_Edit name"), GTK_STOCK_DND,
                        (GCallback)dataproject_on_edit, data_project->tree);

                gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
                if(!dataproject_compilation_is_root(data_project, &global_iter))
                {
                    gbcommon_append_menu_item_stock(menu, _("_Remove selected"), GTK_STOCK_REMOVE,
                            (GCallback)dataproject_on_remove_clicked, data_project);
                }

                gbcommon_append_menu_item_stock(menu, _("Clear"), GTK_STOCK_CLEAR,
                        (GCallback)dataproject_on_clear_clicked, data_project);

                gtk_widget_show_all(menu);
                gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                        (event != NULL) ? event->button : 0, gdk_event_get_time((GdkEvent*)event));
                return TRUE;
            }
        }
    }
    return FALSE;
}


/* returns true if the element in the model pointed by iter is a directory*/
static gboolean
dataproject_treeview_filter_func(GtkTreeModel *model, GtkTreeIter *iter, DataProject *data_project)
{
    GB_LOG_FUNC
      g_return_val_if_fail(model != NULL, FALSE);
    g_return_val_if_fail(iter != NULL, FALSE);
    g_return_val_if_fail(data_project != NULL, FALSE);

    gboolean ret = TRUE;
    GValue status = {0};
    gtk_tree_model_get_value(model,iter,DATA_TREE_COL_STATUS, &status);
    /*check if it is a directory. If so,show. Otherwise, don´t*/
    if(!(g_value_get_ulong(&status) & STATUS_IS_FOLDER))
        ret = dataproject_compilation_is_root(data_project, iter);
    g_value_unset(&status);
    return ret;
}


/*changed selection in the dir tree*/
static void
dataproject_tree_sel_changed(GtkTreeSelection *selection, DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(selection != NULL);
    g_return_if_fail(data_project != NULL);

    GtkTreeModel *tree_model = NULL;
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    /* Block the signal so that it doesn't trigger recursively (saving CPU cycles here :-))*/
    g_signal_handler_block (selection, data_project->sel_changed_id);

    /* The selection in the dir tree has changed so get that selection */
    if(gtk_tree_selection_get_selected(selection, &tree_model, &iter))
    {
        /*get the corresponding iter in g_compilationStore */
        GB_DECLARE_STRUCT(GtkTreeIter, global_iter);
        gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER(tree_model), &global_iter, &iter);
        GtkTreePath *global_path_curr = gtk_tree_model_get_path(GTK_TREE_MODEL(data_project->dataproject_compilation_store), &global_iter);
        GtkTreePath *global_path_prev = NULL;
        /*stop glib giving warnings*/
        if(data_project->dataproject_current_node != NULL)
        {
            global_path_prev = gtk_tree_row_reference_get_path (data_project->dataproject_current_node);
        }

        /*only update list view if if the selection has really changed*/
        if((global_path_prev == NULL) || (gtk_tree_path_compare(global_path_prev, global_path_curr) != 0))
        {
            gtk_tree_row_reference_free(data_project->dataproject_current_node);
            data_project->dataproject_current_node = gtk_tree_row_reference_new (GTK_TREE_MODEL(data_project->dataproject_compilation_store), global_path_curr);
            dataproject_list_view_update(data_project, &global_iter);
        }

        gtk_tree_path_free(global_path_prev);
        gtk_tree_path_free(global_path_curr);
    }
    g_signal_handler_unblock (selection, data_project->sel_changed_id);
}


/*show folders first
 * TODO: should we also group by mime?
 * */
static gint
dataproject_list_sortfunc(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
    /* <0, a BEFORE b
     *  0,  a WITH b (undefined)
     * >0,  a AFTER b
     */

    gchar *a_name = NULL, *b_name = NULL;
    gulong a_status = 0, b_status = 0;

    gtk_tree_model_get (model, a, DATA_LIST_COL_FILE, &a_name, DATA_LIST_COL_STATUS, &a_status, -1);
    gtk_tree_model_get (model, b, DATA_LIST_COL_FILE, &b_name, DATA_LIST_COL_STATUS, &b_status, -1);

    gint result = 0;
    if((a_status & STATUS_IS_FOLDER) && !(b_status & STATUS_IS_FOLDER))
        result = -1;
    else if(!(a_status & STATUS_IS_FOLDER) && (b_status & STATUS_IS_FOLDER))
        result = 1;
    else
        result = g_ascii_strcasecmp(a_name, b_name);

    g_free(a_name);
    g_free(b_name);

    return result;
}


static gboolean
dataproject_get_msinfo(gchar **msinfo)
{
    GB_LOG_FUNC
    gboolean ok = FALSE;
    devices_unmount_device(GB_WRITER);
    gchar *writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
    gchar *msinfo_cmd = g_strdup_printf("cdrecord -msinfo dev=%s", writer);
    gchar *output = NULL;
    exec_run_cmd(msinfo_cmd, &output);

    gint start = 0, end = 0;
    if((output == NULL ) || (sscanf(output, "%d,%d\n", &start, &end) != 2))
    {
        gchar *message = g_strdup_printf(_("Error getting session information.\n\n%s"),
                output != NULL ? output : _("unknown error"));
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
        g_free(message);
    }
    else
    {
        *msinfo = g_strdup_printf("%d,%d", start, end);
        GB_TRACE("dataproject_get_msinfo - next session is [%s]\n", *msinfo);
        ok = TRUE;
    }

    g_free(output);
    g_free(writer);
    g_free(msinfo_cmd);
    return ok;
}


void
dataproject_on_datadisk_size_changed(GtkOptionMenu *option_menu, DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(option_menu != NULL);
    g_return_if_fail(data_project != NULL);

#ifdef CAIRO_WIDGETS
    gb_cairo_fillbar_set_disk_size(PROJECT_WIDGET(data_project)->progress_bar,
            dataproject_get_datadisk_size(data_project), FALSE, overburn_percent, TRUE);
#endif

    dataproject_update_progress_bar(data_project);
    preferences_set_int(GB_DATA_DISK_SIZE, gtk_option_menu_get_history(option_menu));
}


/*
 * Escapes file paths adding '\' before any '\' or '=' character
 */
static gchar *
escape_filepath(gchar *path)
{
    GB_LOG_FUNC
    g_return_val_if_fail(path != NULL, NULL);

    int count = 0;
    gchar *p = path;
    gchar *pr = NULL;
    gchar *result = NULL;

    while (*p != 0) {
        if (*p == '\\' || *p == '=') {
            ++count;
        }
        ++p;
    }

    if (count) {
        result = g_malloc0(strlen(path) + count + 1);
        p = path;
        pr = result;
        while (*p != 0) {
            if (*p == '\\' || *p == '=') {
                *pr++ = '\\';
            }
            *pr++ = *p++;
        }
    } else {
        result = g_strdup(path);
    }
    return result;
}

/* Adds to the burning list all the contents recursively. This could be improved checking for depth,
 * as mkisofs only accepts a max depth o 6 directories */
static void
dataproject_build_paths_file_recursive(GtkTreeModel *model, GtkTreeIter *parent_iter, const char *file_path, GBTempFile *tmp_file)
{
    GB_LOG_FUNC
    g_return_if_fail(model != NULL);
    g_return_if_fail(parent_iter != NULL);
    g_return_if_fail(file_path != NULL);
    g_return_if_fail(tmp_file != NULL);    

    int count = 0;
    GB_DECLARE_STRUCT(GtkTreeIter, iter);
    while(gtk_tree_model_iter_nth_child(model,&iter,parent_iter,count))
    {
        gchar *file_name = NULL, *path_in_system = NULL;
        gulong status = 0L;

        gtk_tree_model_get(model, &iter, DATA_TREE_COL_FILE, &file_name,
                DATA_TREE_COL_PATH, &path_in_system, DATA_TREE_COL_STATUS, &status, -1);

        if(status & STATUS_IS_FOLDER)
        {
            /*it is a folder, add its contents recursively building the filepaths to burn*/
            gchar *file_path_new = g_build_filename(file_path, file_name,NULL);
            dataproject_build_paths_file_recursive(model, &iter, file_path_new, tmp_file);
            g_free(file_path_new);
        }
        else
        {
            gchar *full_path = g_build_filename(file_path, file_name, NULL);
            gchar *escaped_full_path = escape_filepath(full_path);
            gchar *escaped_path_in_system = escape_filepath(path_in_system);
            fprintf(tmp_file->file_stream, "%s=%s\n", escaped_full_path, escaped_path_in_system);
            GB_TRACE("dataproject_build_paths_file_recursive - [%s]\n", full_path);
            g_free(full_path);
            g_free(escaped_full_path);
            g_free(escaped_path_in_system);
        }

        g_free(path_in_system);
        g_free(file_name);
        ++count;
    }
}


/*
 * Creates a temporary file wich stores the graft-points.
 * This way we avoid the "too many arguments" error.
 */
static const GBTempFile*
dataproject_build_paths_file(GtkTreeModel *model)
{
    GB_LOG_FUNC
      g_return_val_if_fail(model != NULL, NULL);

    GBTempFile *tmp_file = gbcommon_create_open_temp_file();
    if(tmp_file == NULL)
    {
        g_warning("dataproject_build_paths_file - Error. Temp file was not created. Image will not be created");
        return NULL;
    }
    GB_TRACE("dataproject_build_paths_file - created file [%s]\n", tmp_file->file_name);
    
    gnomebaker_show_busy_cursor(TRUE);
    
    /* Get the root iter.It should be the name of the compilation,
     * but for the moment we don´t do anything with it
     * All will start from here */
    GB_DECLARE_STRUCT(GtkTreeIter, root_iter);
    gtk_tree_model_get_iter_first(model, &root_iter);
    if(gtk_tree_model_iter_has_child(model, &root_iter))
        dataproject_build_paths_file_recursive(model, &root_iter, "", tmp_file);
    /*else
        compilation is empty! What should we do?*/

    gbcommon_close_temp_file(tmp_file); /*close, but don´t delete. It must be readed by mkisofs*/
    
    gnomebaker_show_busy_cursor(FALSE);
    
    return tmp_file;
}


void
dataproject_on_create_datadisk(gpointer widget, DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);
    g_return_if_fail(data_project->dataproject_compilation_store != NULL);

    if(data_project->dataproject_compilation_size <= 0 || data_project->dataproject_compilation_size > dataproject_get_datadisk_size(data_project))
    {
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
           _("Compilation exeeds the capacity or the remaining space of the disk"));
       return;
    }

    const GBTempFile *tmp_file = dataproject_build_paths_file(GTK_TREE_MODEL(data_project->dataproject_compilation_store));

    if (tmp_file != NULL) 
    {
        if(data_project->is_dvd)
        {
            if(data_project->msinfo != NULL)
                burn_append_data_dvd(tmp_file->file_name, data_project->msinfo);
            else
                burn_create_data_dvd(tmp_file->file_name);
        }
        else if(data_project->msinfo != NULL)
            burn_append_data_cd(tmp_file->file_name, data_project->msinfo);
        else
            burn_create_data_cd(tmp_file->file_name);
        /* TODO - we should delete the temp file here */
    }
    else
    {
      /* TODO - Display an error message */
        gchar *temp_dir = preferences_get_string(GB_TEMP_DIR);
        gchar *message = g_strdup_printf(_("Could not create temporary file. \n"
                                           "Please make sure your temporary "
                                           "directory [%s] is writable "
                                           "and try again."), temp_dir);
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
        g_free(message);
        g_free(temp_dir);
        return;
    }


}


static void
dataproject_import_session(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));

    DataProject *data_project = DATAPROJECT_WIDGET(project);
    GtkWidget *dlg = selectdevicedlg_new();
    const gint response = gtk_dialog_run(GTK_DIALOG(dlg));
    selectdevicedlg_delete(dlg);

    if(response == GTK_RESPONSE_CANCEL)
        return;
    if(devices_prompt_for_disk(gnomebaker_get_window(), GB_WRITER) == GTK_RESPONSE_CANCEL)
        return;

    gnomebaker_show_busy_cursor(TRUE);

    devices_unmount_device(GB_WRITER);
    dataproject_clear(project);

    gchar *mount_point = NULL;
    gchar *msinfo = NULL;
    if(!dataproject_get_msinfo(&msinfo))
    {
        g_warning("dataproject_import_session - Error getting msinfo");
    }
    else if(!devices_mount_device(GB_WRITER, &mount_point))
    {
        g_warning("dataproject_import_session - Error mounting writer device");
    }
    else
    {
        /* try and open the mount_point and read in the cd contents */
        GDir *dir = g_dir_open(mount_point, 0, NULL);
        if(dir != NULL)
        {
            dataproject_set_multisession(data_project, msinfo);

            const gchar *name = g_dir_read_name(dir);
            while(name != NULL)
            {
                gchar *full_name = g_build_filename(mount_point, name, NULL);
                gchar *uri = gbcommon_get_uri(full_name);
                /*
                 * TODO:Should we also import the volume id of the disc?
                 */
                GB_DECLARE_STRUCT(GtkTreeIter, root);
                dataproject_compilation_root_get_iter(data_project, &root);
                if(!dataproject_add_to_compilation(data_project, uri, &root, TRUE))
                    break;
                g_free(uri);
                g_free(full_name);
                name = g_dir_read_name(dir);
            }

            dataproject_update_progress_bar(data_project);
            g_dir_close(dir);
        }
        else
        {
            gchar *message = g_strdup_printf(_("Error importing session from [%s]. "
                    "Please check the device configuration in preferences."), mount_point);
            gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
            g_free(message);
        }

        devices_unmount_device(GB_WRITER);
    }

    g_free(mount_point);
    g_free(msinfo);

    project_set_dirty(PROJECT_WIDGET(data_project), TRUE);

    gnomebaker_show_busy_cursor(FALSE);
}


static void
dataproject_open_recursive(DataProject *project, GtkTreeModel *model, xmlNodePtr parent_node, GtkTreeIter *parent_iter)
{
    GB_LOG_FUNC
    struct _xmlAttr *p = NULL;
    xmlChar *file_path = NULL;
    xmlChar *display_name = NULL;
    guint64 file_size = 0;
    gboolean is_folder = FALSE;

    if (!strcmp((gchar*)parent_node->name, "file"))
    {
        for (p = parent_node->properties; p != NULL; p=p->next)
        {
            if (!strcmp((gchar*)p->name, "path"))
            {
                file_path = p->children->content;
            }
            else if (!strcmp((gchar*)p->name, "graft"))
            {
                display_name = p->children->content;
            }
        }

        if (!strcmp((gchar*)file_path, ""))
        {
            file_size = 4096;
            is_folder = TRUE;
        }
        else
        {
            is_folder = FALSE;
            GB_DECLARE_STRUCT(struct stat, s);
            const gint statret = lstat((gchar*)file_path, &s);
            if (statret == 0)
                file_size = s.st_size;
        }

        GtkTreeIter new_node = dataproject_add_to_model(project, is_folder, (gchar*)file_path, (gchar*)display_name, parent_iter, FALSE, file_size);
        parent_iter = &new_node;
    }
    xmlNodePtr child;
    for ( child = parent_node->children; child != NULL && parent_node->last != NULL && child != parent_node->last->next; child = child->next) 
    {
        if (!strcmp((gchar*)child->name, "file"))
        {
          dataproject_open_recursive(project, model, child, parent_iter);
        }
    }
}


static void
dataproject_open(Project *project, xmlDocPtr doc)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
    g_return_if_fail(doc != NULL);

    DataProject *data_project = DATAPROJECT_WIDGET(project);
    GB_DECLARE_STRUCT(GtkTreeIter, root);
    dataproject_compilation_root_get_iter(data_project, &root);

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr result = xmlXPathEvalExpression((const xmlChar*)"/project/data", context);
    if(result != NULL)
    {
        xmlNodeSetPtr data_nodes = result->nodesetval;
        gint i = 0;
        for (; i < data_nodes->nodeNr; i++)
        {
          dataproject_open_recursive((DataProject*)project, GTK_TREE_MODEL(data_project->dataproject_compilation_store),
                    data_nodes->nodeTab[i], &root);
        }
        xmlXPathFreeObject(result);
    }
    else
    {
        g_warning("dataproject_open - Error in xmlXPathEvalExpression");
    }
    xmlXPathFreeContext(context);
    dataproject_update_progress_bar((DataProject *)project);
    dataproject_list_view_update((DataProject *)project, &root);
}


static void
dataproject_save_recursive(GtkTreeModel *model, GtkTreeIter *parent_iter, xmlNodePtr parent_node)
{
    GB_DECLARE_STRUCT(GtkTreeIter, child_iter);
    int child_number = 0;
    /* Add the childrens of the parent iter to the */
    while(gtk_tree_model_iter_nth_child(model, &child_iter, parent_iter, child_number))
    {
        /* TODO - We need to record if a folder is modified in the model. Then when we
         * save we only need to record the folder rather than the entirety of its
         * contents. We must do this for projects where a bunch of top level folders
         * are added to the project only so it effectively forms a backup set
         * and all of the referenced folders contents get added and new files
         * in those folders do not get ignored */
        gchar *file = NULL, *file_path = NULL;
        gulong status = 0L;

        gtk_tree_model_get(model, &child_iter,
                DATA_TREE_COL_FILE, &file, DATA_TREE_COL_PATH, &file_path,
                DATA_TREE_COL_STATUS, &status, -1 );

        if(!(status & STATUS_EXISTING_SESSION))
        {
            xmlNodePtr file_node = xmlNewTextChild(parent_node, NULL, (const xmlChar*)"file", NULL);
            xmlNewProp(file_node, (const xmlChar*)"path", (const xmlChar*)file_path);
            xmlNewProp(file_node, (const xmlChar*)"graft", (const xmlChar*)file);

            if(gtk_tree_model_iter_has_child (model, &child_iter))
                dataproject_save_recursive(model, &child_iter, file_node);
        }

        g_free(file);
        g_free(file_path);

        ++child_number;
    }
}


static void
dataproject_save(Project *project, xmlNodePtr project_node)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));

    xmlNodePtr cur = project_node->xmlChildrenNode;
    while (cur != NULL)
    {
        if(xmlStrcmp(cur->name, (const xmlChar *)"data") == 0)
        {
            DataProject *data_project = DATAPROJECT_WIDGET(project);
            GB_DECLARE_STRUCT(GtkTreeIter, root);
            dataproject_compilation_root_get_iter(data_project, &root);
            GtkTreeModel *model = GTK_TREE_MODEL(data_project->dataproject_compilation_store);
            dataproject_save_recursive(model, &root, cur);
        }
        cur = cur->next;
    }
}


static void
dataproject_close(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
    DataProject *data_project = DATAPROJECT_WIDGET(project);
    if(data_project->msinfo != NULL)
        g_free(data_project->msinfo);
}


static void
dataproject_class_init(DataProjectClass *klass)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET_CLASS(klass));

    ProjectClass *project_class = PROJECT_WIDGET_CLASS(klass);
    project_class->clear = dataproject_clear;
    project_class->remove = dataproject_remove;
    project_class->add_selection = dataproject_add_selection;
    project_class->import_session = dataproject_import_session;
    project_class->open = dataproject_open;
    project_class->save = dataproject_save;
    project_class->close = dataproject_close;
}


static void
dataproject_setup_list(DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project->dataproject_compilation_store != NULL);
    /* Create the list store for the file list */
    /* Add a column field for a row reference to rows in DatacdCompilationStore */
    /* Instead of a pointer to row reference we could use G_TYPE_TREE_ROW_REFERENCE */
    /* so that we do not need to delete it manually*/
    GtkListStore *store = gtk_list_store_new(DATA_LIST_NUM_COLS, GDK_TYPE_PIXBUF,
            G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG, G_TYPE_POINTER);
    gtk_tree_view_set_model(data_project->list, GTK_TREE_MODEL(store));

    /*show folders first*/
    gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(store), dataproject_list_sortfunc, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

    g_object_unref(store);

    /* First column which has an icon renderer and a text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Contents"));
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", DATA_LIST_COL_ICON, NULL);

    GValue value = { 0 };
    g_value_init(&value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&value, FALSE);

    renderer = gtk_cell_renderer_text_new();
    g_object_set_property(G_OBJECT(renderer), "editable", &value);
    g_signal_connect(renderer, "edited", (GCallback)dataproject_list_contents_cell_edited, (gpointer)data_project);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATA_LIST_COL_FILE, NULL);
    gtk_tree_view_append_column(data_project->list, col);

    g_value_unset(&value);

    const gboolean show_human_size = preferences_get_bool(GB_SHOWHUMANSIZE);

    /* Second column to display the file/dir size */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Size"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATA_LIST_COL_SIZE, NULL);
    gtk_tree_view_column_set_visible(col, !show_human_size);
    gtk_tree_view_append_column(data_project->list, col);

    /* Third column to display the human size of file/dir */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Size"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATA_LIST_COL_HUMANSIZE, NULL);
    gtk_tree_view_column_set_visible(col, show_human_size);
    gtk_tree_view_append_column(data_project->list, col);

    /* Fourth column for the full path of the file/dir */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_title(col, _("Full Path"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATA_LIST_COL_PATH, NULL);
    gtk_tree_view_append_column(data_project->list, col);

    /* Set the selection mode of the file list */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(data_project->list),
            GTK_SELECTION_MULTIPLE /*GTK_SELECTION_BROWSE*/);

    /* Enable the file list as a drag destination */
    gtk_drag_dest_set(GTK_WIDGET(data_project->list), GTK_DEST_DEFAULT_ALL, target_entries,
            TARGET_COUNT, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

    /* Connect the function to handle the drag data */
    g_signal_connect(data_project->list, "drag_data_received",
            G_CALLBACK(dataproject_on_drag_data_received), data_project);

    /* connect the signal to handle right click */
    g_signal_connect (G_OBJECT(data_project->list), "button-press-event",
            G_CALLBACK(dataproject_on_button_pressed), data_project);

    /* handle double clicks. This should allow us to navigate through the contents */
    g_signal_connect(G_OBJECT(data_project->list), "row-activated", G_CALLBACK(dataproject_on_list_dbl_click), data_project);
}


static void
dataproject_setup_tree(DataProject *data_project)
{
    GB_LOG_FUNC
    g_return_if_fail(data_project != NULL);
    g_return_if_fail(data_project->dataproject_compilation_store != NULL);

    /* Create the tree store for the dir tree */
    /*GtkTreeStore *store = gtk_tree_store_new(DT_NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING);*/
    GtkTreeModel *store = gtk_tree_model_filter_new(GTK_TREE_MODEL(data_project->dataproject_compilation_store),NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER(store),
            (GtkTreeModelFilterVisibleFunc)dataproject_treeview_filter_func, (gpointer)data_project, NULL);
    gtk_tree_view_set_model(data_project->tree, store);
    g_object_unref(store);

    /* One column which has an icon renderer and a text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Contents"));
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", DATA_TREE_COL_ICON, NULL);

    GValue value = { 0 };
    g_value_init(&value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&value, FALSE);

    renderer = gtk_cell_renderer_text_new();
    g_object_set_property(G_OBJECT(renderer), "editable", &value);
    g_signal_connect(renderer, "edited", (GCallback)dataproject_tree_contents_cell_edited, (gpointer)data_project);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DATA_TREE_COL_FILE, NULL);
    gtk_tree_view_append_column(data_project->tree, col);
    g_value_unset(&value);

    /* Enable the file list as a drag destination */
    gtk_drag_dest_set(GTK_WIDGET(data_project->tree), GTK_DEST_DEFAULT_ALL, target_entries,
            TARGET_COUNT, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

    /* Connect the function to handle the drag data */
    g_signal_connect(data_project->tree, "drag_data_received",
            G_CALLBACK(dataproject_on_drag_data_received), data_project);
    /* Connect the function to handle the drag data */
    g_signal_connect(data_project->tree, "drag-motion",
            G_CALLBACK(dataproject_on_drag_motion), data_project);

    /* Set the selection mode of the dir tree */
    GtkTreeSelection *selection = gtk_tree_view_get_selection(data_project->tree);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

    /* Connect up the changed signal so we can populate the file list according to the selection in the dir tree */
    data_project->sel_changed_id = g_signal_connect((gpointer) selection, "changed", G_CALLBACK(dataproject_tree_sel_changed), data_project);

    /* connect the signal to handle right click */
    g_signal_connect (G_OBJECT(data_project->tree), "button-press-event",
            G_CALLBACK(dataproject_on_button_pressed), data_project);

    /*TODO double clicks should expand the node of the tree*/
    /* handle double clicks */
    /*g_signal_connect(G_OBJECT(data_project->tree), "row-activated", G_CALLBACK(dataproject_on_tree_dbl_click), NULL);*/
}


static void
dataproject_init(DataProject *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));

    GtkWidget *hpaned4 = gtk_hpaned_new();
    gtk_widget_show(hpaned4);
    gtk_paned_set_position(GTK_PANED(hpaned4), 250);
    gtk_box_pack_start(GTK_BOX(project), hpaned4, TRUE, TRUE, 0);
    gtk_box_reorder_child(GTK_BOX(project), hpaned4, 0);

    GtkWidget *scrolledwindow18 = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(scrolledwindow18);
    gtk_paned_pack1(GTK_PANED(hpaned4), scrolledwindow18, FALSE, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow18), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    project->tree = GTK_TREE_VIEW(gtk_tree_view_new());
    gtk_widget_show(GTK_WIDGET(project->tree));
    gtk_container_add(GTK_CONTAINER(scrolledwindow18), GTK_WIDGET(project->tree));

    GtkWidget *scrolledwindow14 = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(scrolledwindow14);
    gtk_paned_pack2(GTK_PANED(hpaned4), scrolledwindow14, TRUE, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow14), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    project->list = GTK_TREE_VIEW(gtk_tree_view_new());
    gtk_widget_show(GTK_WIDGET(project->list));
    gtk_container_add(GTK_CONTAINER(scrolledwindow14), GTK_WIDGET(project->list));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(project->list), TRUE);


        /* Create the global store. It has the same structure as the list view, but it is a tree (*obvious*)
     * 1-icon 2-name
     * 3-size
     * 4-human size
     * 5-full path
     * 6-session (if it comes from another session
     * 7-is a folder
     */
    project->dataproject_compilation_store = gtk_tree_store_new(DATA_TREE_NUM_COLS, GDK_TYPE_PIXBUF,
            G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG);

    project->dataproject_compilation_size = 0;

    dataproject_setup_list(project);
    dataproject_setup_tree(project);
    dataproject_compilation_root_add(project);

    GB_DECLARE_STRUCT(GtkTreeIter, root);
    dataproject_compilation_root_get_iter(project, &root);
    dataproject_set_current_node(DATAPROJECT_WIDGET(project), &root);

    preferences_register_notify(GB_SHOWHUMANSIZE,
            (GConfClientNotifyFunc)dataproject_on_show_humansize_changed, project);

    g_signal_connect(G_OBJECT(PROJECT_WIDGET(project)->button), "clicked",
            G_CALLBACK(dataproject_on_create_datadisk), project);

    g_signal_connect(G_OBJECT(PROJECT_WIDGET(project)->menu), "changed",
            G_CALLBACK(dataproject_on_datadisk_size_changed), project);

#ifdef CAIRO_WIDGETS
	gb_cairo_fillbar_set_disk_size(PROJECT_WIDGET(project)->progress_bar,
            dataproject_get_datadisk_size(project), FALSE, overburn_percent, TRUE);
#else
	dataproject_get_datadisk_size(project);
#endif

	dataproject_update_progress_bar(project);
}


GtkWidget*
dataproject_new(const gboolean is_dvd)
{
    GB_LOG_FUNC

    DataProject* data_project = (DataProject*)g_object_new(DATAPROJECT_TYPE_WIDGET, NULL);
    PROJECT_WIDGET(data_project)->type = is_dvd ? DATA_DVD : DATA_CD;
    data_project->is_dvd = is_dvd;
    if(is_dvd)
    {
        gbcommon_populate_disk_size_option_menu(PROJECT_WIDGET(data_project)->menu, data_dvd_disk_sizes, 2, 0);
        project_set_title(PROJECT_WIDGET(data_project), _("Data DVD"));
    }
    else
    {
        gbcommon_populate_disk_size_option_menu(PROJECT_WIDGET(data_project)->menu, data_cd_disk_sizes, 4,
                /*preferences_get_int(GB_DATA_DISK_SIZE)*/2);
        project_set_title(PROJECT_WIDGET(data_project), _("Data CD"));
    }
    return GTK_WIDGET(data_project);
}


