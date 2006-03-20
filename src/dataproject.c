#include "dataproject.h"
#include "gbcommon.h"

G_DEFINE_TYPE(DataProject, dataproject, PROJECT_TYPE_WIDGET);


static void
dataproject_clear(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
}


static void
dataproject_remove(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
}


static void
dataproject_add_selection(Project *project, GtkSelectionData *selection)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
    g_return_if_fail(selection != NULL);
}


static void
dataproject_import_session(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
}


static void
dataproject_open(Project *project, const gchar *file_name)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
    g_return_if_fail(file_name != NULL);
}


static void
dataproject_save(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
}


static void
dataproject_close(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(DATAPROJECT_IS_WIDGET(project));
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

    project_set_title(PROJECT_WIDGET(project), _("<b>Data project</b>"));
}


GtkWidget*
dataproject_new()
{
    GB_LOG_FUNC
    return GTK_WIDGET(g_object_new(DATAPROJECT_TYPE_WIDGET, NULL));
}


