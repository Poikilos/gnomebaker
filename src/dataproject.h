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


typedef struct {
    Project parent;
    
    GtkTreeView *tree;
    GtkTreeView *list;
    
    gdouble data_disk_size;
} DataProject;


typedef struct {
    ProjectClass parent_class;
} DataProjectClass;


GType dataproject_get_type();
GtkWidget *dataproject_new();


G_END_DECLS

#endif /*_DATAPROJECT_H*/
