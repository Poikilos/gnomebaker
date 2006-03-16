#include "project.h"
#include "gbcommon.h"

G_DEFINE_ABSTRACT_TYPE(Project, project, GTK_TYPE_VBOX);


static void
project_class_init(ProjectClass *klass)
{
    g_return_if_fail(PROJECT_IS_WIDGET_CLASS(klass));
    GB_LOG_FUNC
}


static void
project_init(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    
    project->title = GTK_LABEL(gtk_label_new(""));    
    
    GtkWidget *hbox5 = gtk_hbox_new(FALSE, 10);
    gtk_widget_show(hbox5);
    gtk_box_pack_start(GTK_BOX(project), hbox5, FALSE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox5), 5);
    
    project->progress_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_widget_show(GTK_WIDGET(project->progress_bar));
    gtk_box_pack_start(GTK_BOX(hbox5), GTK_WIDGET(project->progress_bar), TRUE, TRUE, 0);
    gtk_progress_bar_set_text(project->progress_bar, _("0%"));

    project->menu = GTK_OPTION_MENU(gtk_option_menu_new());
    gtk_widget_show(GTK_WIDGET(project->menu));
    gtk_box_pack_start(GTK_BOX(hbox5), GTK_WIDGET(project->menu), FALSE, FALSE, 0);

    GtkWidget *menu1 = gtk_menu_new();
/*    gnome_app_fill_menu(GTK_MENU_SHELL(menu1), menu1_uiinfo,
                       accel_group, FALSE, 0);*/

    gtk_option_menu_set_menu(GTK_OPTION_MENU(project->menu), menu1);

    project->button = GTK_BUTTON(gtk_button_new());
    gtk_widget_show(GTK_WIDGET(project->button));
    gtk_box_pack_start(GTK_BOX(hbox5), GTK_WIDGET(project->button), FALSE, FALSE, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(project->button), FALSE);
    /*gtk_tooltips_set_tip(tooltips, dial->button, _("Click to begin the process of creating a data disk."), NULL);*/

    GtkWidget *alignment13 = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_widget_show(alignment13);
    gtk_container_add(GTK_CONTAINER(project->button), alignment13);

    GtkWidget *hbox23 = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(hbox23);
    gtk_container_add(GTK_CONTAINER(alignment13), hbox23);

    GdkPixbuf *image105 = gdk_pixbuf_new_from_file(IMAGEDIR"/baker-cd.png", NULL);
    GtkWidget *image = gtk_image_new_from_pixbuf(image105);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox23), image, FALSE, FALSE, 0);
    g_object_unref(image105);

    GtkWidget *label258 = gtk_label_new_with_mnemonic(_("Create Data Disk"));
    gtk_widget_show(label258);
    gtk_box_pack_end(GTK_BOX(hbox23), label258, FALSE, FALSE, 0);            
}


GtkWidget*
project_new()
{
    GB_LOG_FUNC
    return GTK_WIDGET(g_object_new(PROJECT_TYPE_WIDGET, NULL));
}


void 
project_set_title(Project *project, const gchar *title)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    g_return_if_fail(title != NULL);
    
    gtk_label_set_label(project->title, title);
    gtk_widget_show(GTK_WIDGET(project->title));
    gtk_label_set_use_markup(project->title, TRUE);
}


void 
project_clear(Project *project)
{
    GB_LOG_FUNC   
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->clear(project);
}


void 
project_remove(Project *project)
{
    GB_LOG_FUNC   
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->remove(project);
}


void 
project_add_selection(Project *project, GtkSelectionData *selection)
{
    GB_LOG_FUNC   
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    g_return_if_fail(selection != NULL);
    PROJECT_WIDGET_GET_CLASS(project)->add_selection(project, selection);
}


void 
project_import_session(Project *project)
{
    GB_LOG_FUNC   
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->import_session(project);
}


void 
project_open(Project *project, const gchar *file_name)
{
    GB_LOG_FUNC   
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    g_return_if_fail(file_name != NULL);
    PROJECT_WIDGET_GET_CLASS(project)->open(project, file_name);
}


void 
project_save(Project *project)
{
    GB_LOG_FUNC   
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->save(project);
}


void 
project_close(Project *project)
{
    GB_LOG_FUNC   
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->close(project);
}

