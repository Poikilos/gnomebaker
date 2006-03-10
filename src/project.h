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
  PROJECT_TYPE_WIDGET, ProjectwClass))

#define PROJECT_IS_WIDGET(obj) \
 (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
  PROJECT_TYPE_WIDGET))

#define PROJECT_IS_WIDGET_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_TYPE((klass), \
  PROJECT_TYPE_WIDGET))

#define PROJECT_WIDGET_GET_CLASS(obj) \
 (G_TYPE_INSTANCE_GET_CLASS((obj), \
  PROJECT_TYPE_WIDGET, ProjectwClass))


typedef struct {
    GtkVBox parent;
    
    GtkOptionMenu *menu;
    GtkButton *button;
    GtkProgressBar *progress_bar;
    GtkLabel *title;
    
} Project;


typedef struct {
    GtkVBoxClass parent_class;
    
    void (*clear)(Project *self);
    void (*remove)(Project *self);
    void (*add_selection)(Project *self, GtkSelectionData *selection);
    void (*import_session)(Project *self);
    void (*open_project)(Project *self);
    void (*save_project)(Project *self);
    
} ProjectClass;


GType project_get_type(void);
GtkWidget* project_new(void);
void project_set_title(Project* project, const gchar* name);

G_END_DECLS

#endif /*_PROJECT_H*/
