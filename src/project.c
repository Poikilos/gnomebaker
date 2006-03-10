#include "project.h"
#include "gbcommon.h"

G_DEFINE_ABSTRACT_TYPE(Project, project, GTK_TYPE_VBOX);


static void
project_class_init(ProjectClass *klass)
{
    GB_LOG_FUNC
}


static void
project_init(Project *self)
{
    GB_LOG_FUNC
    g_return_if_fail(self != NULL);
    
    self->title = GTK_LABEL(gtk_label_new(""));    
}


GtkWidget*
project_new(void)
{
    GB_LOG_FUNC
    return GTK_WIDGET(g_object_new(PROJECT_TYPE_WIDGET, NULL));
}


void 
project_set_title(Project* project, const gchar* name)
{
    GB_LOG_FUNC
    g_return_if_fail(project != NULL);
    g_return_if_fail(name != NULL);
    
    gtk_label_set_label(project->title, name);
    gtk_widget_show(GTK_WIDGET(project->title));
    gtk_label_set_use_markup(project->title, TRUE);
}


