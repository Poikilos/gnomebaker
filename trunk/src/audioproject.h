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
 * File: audioproject.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun Mar  19 21:19:51 2006
 */

#ifndef _AUDIOPROJECT_H
#define _AUDIOPROJECT_H

#include <gtk/gtk.h>
#include "project.h"

G_BEGIN_DECLS

#define AUDIOPROJECT_TYPE_WIDGET audioproject_get_type()

#define AUDIOPROJECT_WIDGET(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    AUDIOPROJECT_TYPE_WIDGET, AudioProject))

#define AUDIOPROJECT_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), \
    AUDIOPROJECT_TYPE_WIDGET, AudioProjectClass))

#define AUDIOPROJECT_IS_WIDGET(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    AUDIOPROJECT_TYPE_WIDGET))

#define AUDIOPROJECT_IS_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), \
    AUDIOPROJECT_TYPE_WIDGET))

#define AUDIOPROJECT_WIDGET_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
    AUDIOPROJECT_TYPE_WIDGET, AudioProjectClass))


typedef struct {
    Project parent;
    
    GtkTreeView *tree;    
    gdouble selected_size;
    gdouble compilation_seconds;
    
} AudioProject;


typedef struct {
    ProjectClass parent_class;
} AudioProjectClass;


GType audioproject_get_type();
GtkWidget *audioproject_new();
gboolean audioproject_import_playlist(AudioProject *audio_project, const gchar *play_list);
gboolean audioproject_export_playlist(AudioProject *audio_project, const gchar *play_list);

G_END_DECLS

#endif /*_AUDIOPROJECT_H*/
