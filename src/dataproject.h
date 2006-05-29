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
 * File: dataproject.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun Mar  19 21:19:51 2006
 */

#ifndef _DATAPROJECT_H
#define _DATAPROJECT_H

#include <gtk/gtk.h>
#include "project.h"

G_BEGIN_DECLS

#define DATAPROJECT_TYPE_WIDGET dataproject_get_type()

#define DATAPROJECT_WIDGET(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    DATAPROJECT_TYPE_WIDGET, DataProject))

#define DATAPROJECT_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), \
    DATAPROJECT_TYPE_WIDGET, DataProjectClass))

#define DATAPROJECT_IS_WIDGET(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    DATAPROJECT_TYPE_WIDGET))

#define DATAPROJECT_IS_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), \
    DATAPROJECT_TYPE_WIDGET))

#define DATAPROJECT_WIDGET_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
    DATAPROJECT_TYPE_WIDGET, DataProjectClass))


typedef struct
{
    Project parent;

    GtkTreeView *tree;
    GtkTreeView *list;

    /* This will store all the data. The tree view will use a GtkTreeModelFilter and the globalStore as a child node.
     * It will use a filter function to hide non-dir elements.
     * This way all the changes done in the tree view are automatically performed on the globalStore.
     * Iters can be mapped betwenn both models, so we can update correctly the list view.
     */
    GtkTreeStore *dataproject_compilation_store;

    /* As the filtering trick doesn´t work for the list view, it needs its own GtkListStore.
     * We fill it with the children of the row selected in the tree view. The problem is that we need to be able
     * to perform the changes we do in the list view also in the g_compilationStore, so we need a row reference for the current node
     * and all its child elements in the list view. This is achived storing in the list view a gpointer to a GtkTreeRowReference.
     * For all the changes in list view, extract the row reference and perform the same change in the g_compilationStore. the tree view
     * is updated then automatically (since it only filters g_compilationStore). When adding new elements, first add them
     * to g_compilationStore, and then add them to the list with the row_reference
     * */

    GtkTreeRowReference *dataproject_current_node;

    /*I think this is better than relying in the progressbar to store the size*/
    guint64 dataproject_compilation_size;
    gchar *msinfo;
    gdouble data_disk_size;

    /* callback id, so we can block it! */
    gulong sel_changed_id;

    /* TODO these are something to do with drag motion - make private */
    GtkTreePath *last_path; /* NULL */
    guint expand_timeout_id;
    guint autoscroll_timeout_id;
    GList *rowref_list;
    gboolean is_dvd;

} DataProject;


typedef struct
{
    ProjectClass parent_class;
} DataProjectClass;


GType dataproject_get_type();
GtkWidget *dataproject_new(const gboolean is_dvd);
gchar* dataproject_compilation_get_volume_id(DataProject *data_project);

G_END_DECLS

#endif /*_DATAPROJECT_H*/
