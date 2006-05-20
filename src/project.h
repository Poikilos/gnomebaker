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
 * File: project.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun Mar  19 21:19:51 2006
 */

#ifndef _PROJECT_H
#define _PROJECT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <libxml/parser.h>

#ifdef CAIRO_WIDGETS
#include "cairofillbar.h"
#endif

typedef enum
{
    DATA_CD = 0,
    DATA_DVD,
    AUDIO_CD      
} ProjectType;


G_BEGIN_DECLS

#define PROJECT_TYPE_WIDGET project_get_type()

#define PROJECT_WIDGET(obj) \
 (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  PROJECT_TYPE_WIDGET, Project))

#define PROJECT_WIDGET_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_CAST((klass), \
  PROJECT_TYPE_WIDGET, ProjectClass))

#define PROJECT_IS_WIDGET(obj) \
 (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
  PROJECT_TYPE_WIDGET))

#define PROJECT_IS_WIDGET_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_TYPE((klass), \
  PROJECT_TYPE_WIDGET))

#define PROJECT_WIDGET_GET_CLASS(obj) \
 (G_TYPE_INSTANCE_GET_CLASS((obj), \
  PROJECT_TYPE_WIDGET, ProjectClass))


typedef struct 
{
    GtkVBox parent;
    
    GtkOptionMenu *menu;
    GtkButton *button;
#ifdef CAIRO_WIDGETS
	GBCairoFillBar *progress_bar;
#else
    GtkProgressBar *progress_bar;
#endif
    GtkLabel *title;
    GtkButton *close_button;
    gboolean is_dirty;
    gchar *file;
    
} Project;


typedef struct 
{
    GtkVBoxClass parent_class;
    
    void (*clear)(Project *self);
    void (*remove)(Project *self);
    void (*add_selection)(Project *self, GtkSelectionData *selection);
    void (*import_session)(Project *self);
    void (*open)(Project *self, xmlDocPtr doc);
    void (*save)(Project *self);
    void (*close)(Project *self);
    void (*move_selected_up)(Project *self);
    void (*move_selected_down)(Project *self);
    
} ProjectClass;


GType project_get_type();
GtkWidget* project_new();
void project_set_title(Project *project, const gchar *title);
void project_clear(Project *project);
void project_remove(Project *project);
void project_add_selection(Project *project, GtkSelectionData *selection);
void project_import_session(Project *project);
void project_open(Project *project, xmlDocPtr doc);
void project_save(Project *project);
void project_close(Project *project);
void project_move_selected_up(Project *project);
void project_move_selected_down(Project *project);
GtkWidget *project_get_title_widget(Project *project);
const gchar *project_get_file(Project *project);
void project_set_file(Project *project, const gchar *file);


G_END_DECLS

#endif /*_PROJECT_H*/
