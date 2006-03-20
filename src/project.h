#ifndef _PROJECT_H
#define _PROJECT_H

#include <gtk/gtk.h>

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


typedef struct {
    GtkVBox parent;
    
    GtkOptionMenu *menu;
    GtkButton *button;
    GtkProgressBar *progress_bar;
    GtkLabel *title;
    GtkButton *close_button;
    gboolean is_dirty;
    
} Project;


typedef struct {
    GtkVBoxClass parent_class;
    
    void (*clear)(Project *self);
    void (*remove)(Project *self);
    void (*add_selection)(Project *self, GtkSelectionData *selection);
    void (*import_session)(Project *self);
    void (*open)(Project *self, const gchar *file_name);
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
void project_open(Project *project, const gchar *file_name);
void project_save(Project *project);
void project_close(Project *project);
void project_move_selected_up(Project *project);
void project_move_selected_down(Project *project);
GtkWidget *project_get_title_widget(Project *project);


G_END_DECLS

#endif /*_PROJECT_H*/
