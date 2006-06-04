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
 * File: project.c
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun Mar  19 21:19:51 2006
 */

#include "project.h"
#include "gbcommon.h"
#include "gnomebaker.h"
#include <libxml/parser.h>

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
    project->is_dirty = FALSE;

    project->close_button = GTK_BUTTON(gtk_button_new());
    g_signal_connect(G_OBJECT(project->close_button), "clicked",
            G_CALLBACK(gnomebaker_on_close_project), project);
    gtk_button_set_relief(project->close_button, GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(project->close_button, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(project->close_button), 0);
    gtk_widget_show(GTK_WIDGET(project->close_button));

    GtkWidget *close_image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
    gtk_widget_show(GTK_WIDGET(close_image));
    gtk_container_add(GTK_CONTAINER(project->close_button), close_image);

    /* resize the close button so that it's only a couple of pixels bigger than the image */
    GtkRequisition req;
    gtk_widget_size_request(close_image, &req);
    gtk_widget_set_size_request(GTK_WIDGET(project->close_button), req.width + 2, req.height + 2);

    GtkWidget *hbox5 = gtk_hbox_new(FALSE, 10);
    gtk_widget_show(hbox5);
    gtk_box_pack_start(GTK_BOX(project), hbox5, FALSE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox5), 5);

#ifndef CAIRO_WIDGETS
    project->progress_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_widget_show(GTK_WIDGET(project->progress_bar));
    gtk_box_pack_start(GTK_BOX(hbox5), GTK_WIDGET(project->progress_bar), TRUE, TRUE, 0);
    gtk_progress_bar_set_text(project->progress_bar, _("0%"));
#else
	project->progress_bar =  GB_CAIRO_FILLBAR(gb_cairo_fillbar_new()) ;
	gtk_widget_show(GTK_WIDGET(project->progress_bar));
	gtk_box_pack_start(GTK_BOX(hbox5), GTK_WIDGET(project->progress_bar), TRUE, TRUE, 0);
#endif

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

    GdkPixbuf *image105 = gdk_pixbuf_new_from_file(IMAGEDIR"/gnomebaker-48.png", NULL);
    GdkPixbuf *pixbuf = gdk_pixbuf_scale_simple(image105, 22, 22, GDK_INTERP_HYPER);
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox23), image, FALSE, FALSE, 0);
    g_object_unref(image105);
    g_object_unref(pixbuf);

    GtkWidget *label258 = gtk_label_new_with_mnemonic(_("_Burn"));
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

    gchar *title_markup = NULL;
    if(gbcommon_str_has_suffix(title, ".gbp"))
    {
        gchar *title_short = g_strndup(title, strlen(title) - 4);
        title_markup = g_strdup_printf("<b>%s</b>", title_short);
        g_free(title_short);
    }
    else
        title_markup = g_strdup_printf("<b>%s</b>", title);
    gtk_label_set_label(project->title, title_markup);
    gtk_widget_show(GTK_WIDGET(project->title));
    gtk_label_set_use_markup(project->title, TRUE);
    g_free(title_markup);
}


GtkWidget*
project_get_title_widget(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));

    GtkWidget *hbox = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(project->title), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(project->close_button), FALSE, FALSE, 0);
    return hbox;
}


void
project_clear(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->clear(project);
    project_set_dirty(project, TRUE);
}


void
project_remove(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->remove(project);
    project_set_dirty(project, TRUE);
}


void
project_add_selection(Project *project, GtkSelectionData *selection)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    g_return_if_fail(selection != NULL);
    PROJECT_WIDGET_GET_CLASS(project)->add_selection(project, selection);
    project_set_dirty(project, TRUE);
}


void
project_import_session(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->import_session(project);
}


gboolean
project_is_dirty(Project *project)
{
    GB_LOG_FUNC
    g_return_val_if_fail(PROJECT_IS_WIDGET(project), FALSE);
    return project->is_dirty;
}


void
project_set_dirty(Project *project, gboolean dirty)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    if(project->is_dirty != dirty)
    {
        project->is_dirty = dirty;
        const gchar* title = gtk_label_get_text(project->title);
        gchar *new_title = NULL;
        if(dirty && title[0] != '*')
            new_title = g_strdup_printf("*%s", title);
        else if(!dirty && title[0] == '*')
            new_title = g_strdup(++title);
        if(new_title != NULL)
        {
            project_set_title(project, new_title);
            g_free(new_title);
        }
    }
}


void
project_open(Project *project, xmlDocPtr doc)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    g_return_if_fail(doc != NULL);
    PROJECT_WIDGET_GET_CLASS(project)->open(project, doc);
}


void
project_save(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));

    xmlDocPtr doc = xmlNewDoc((const xmlChar*)"1.0");

    xmlNodePtr root_node = xmlNewDocNode(doc, NULL, (const xmlChar*)"project", NULL);
    xmlDocSetRootElement(doc, root_node);
    gchar *type = g_strdup_printf("%d", project->type);
    xmlNewProp(root_node, (const xmlChar*)"type", (const xmlChar*)type);
    g_free(type);

    xmlNewTextChild(root_node, NULL, (const xmlChar*)"information", NULL);
    xmlNewTextChild(root_node, NULL, (const xmlChar*)"data", NULL);

    PROJECT_WIDGET_GET_CLASS(project)->save(project, root_node);
    xmlSaveFormatFile(project->file, doc, 1);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    project_set_dirty(project, FALSE);
}


void
project_close(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->close(project);
}


void
project_move_selected_up(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->move_selected_up(project);
}


void
project_move_selected_down(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    PROJECT_WIDGET_GET_CLASS(project)->move_selected_down(project);
}


void
project_set_file(Project *project, const gchar *file)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    if(project->file != NULL)
        g_free(project->file);
    project->file = g_strdup(file);
    gchar* base_name = g_path_get_basename(project->file);
    project_set_title(project, base_name);
    g_free(base_name);
}


const gchar*
project_get_file(Project *project)
{
    GB_LOG_FUNC
    g_return_if_fail(PROJECT_IS_WIDGET(project));
    return project->file;
}




