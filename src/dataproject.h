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
    DATAPROJECT_TYPE_WIDGET, DataProjectwClass))

#define DATAPROJECT_IS_WIDGET(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    DATAPROJECT_TYPE_WIDGET))

#define DATAPROJECT_IS_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), \
    DATAPROJECT_TYPE_WIDGET))

#define DATAPROJECT_WIDGET_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
    DATAPROJECT_TYPE_WIDGET, DataProjectwClass))


typedef struct {
    Project parent;
    
    GtkTreeView *tree;
    GtkTreeView *list;
    GtkOptionMenu *menu;
    GtkButton *button;
    GtkProgressBar *progress_bar;
    
    gdouble data_disk_size;
} DataProject;


typedef struct {
    ProjectClass parent_class;
} DataProjectClass;


GType dataproject_get_type(void);
GtkWidget *dataproject_new(void);


G_END_DECLS

#endif /*_DATAPROJECT_H*/
