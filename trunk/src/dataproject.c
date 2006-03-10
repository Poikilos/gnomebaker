#include "dataproject.h"
#include "gbcommon.h"

G_DEFINE_TYPE(DataProject, dataproject, PROJECT_TYPE_WIDGET);

static void
dataproject_destroy(GtkObject *object)
{
    GB_LOG_FUNC
    g_return_if_fail(object != NULL);
    g_return_if_fail(DATAPROJECT_IS_WIDGET(object));

    DataProject *dial = DATAPROJECT_WIDGET(object);
    /*if(GTK_CONTAINER_CLASS(dial->parent_class)->destroy)
       (* GTK_CONTAINER_CLASS(dial>parent_class)->destroy)(object);*/
}


static void
dataproject_class_init(DataProjectClass *klass)
{
    GB_LOG_FUNC
    g_return_if_fail(klass != NULL);
    
    GtkObjectClass *object_class = (GtkObjectClass*)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass*)klass;
    /*klass->parent_class = (ProjectClass)gtk_type_class(gtk_widget_get_type());*/
    object_class->destroy = dataproject_destroy;
}


static void
dataproject_init(DataProject *project)
{
    GB_LOG_FUNC
    g_return_if_fail(project != NULL);
    
    GtkWidget *hpaned4 = gtk_hpaned_new();
    gtk_widget_show(hpaned4);
    gtk_box_pack_start(GTK_BOX(project), hpaned4, TRUE, TRUE, 0);
    gtk_paned_set_position(GTK_PANED(hpaned4), 250);

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
    gtk_box_pack_start(GTK_BOX(hbox23), label258, FALSE, FALSE, 0);

    project_set_title(PROJECT_WIDGET(project), _("<b>Data Disk</b>"));
}


GtkWidget*
dataproject_new(void)
{
    GB_LOG_FUNC
    return GTK_WIDGET(g_object_new(DATAPROJECT_TYPE_WIDGET, NULL));
}


