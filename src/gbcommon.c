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
 * File: gbcommon.c
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun Jul  4 23:23:55 2004
 */

#include "gbcommon.h"
#include "gnomebaker.h"
#include "preferences.h"
#include <sys/stat.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

static GList *temp_file_list = NULL;


/* creates and opens a temp file*/
/* when it is no longer used it can be deleted with gbcommon_delete_temp_file
 * or at the end of the program it will be deleted as long as gbcommon_delete_all_temp_files*/
GBTempFile*
gbcommon_create_open_temp_file()
{
	GB_LOG_FUNC

    GBTempFile *tmp_file = NULL;
    gchar *temp_dir = preferences_get_string(GB_TEMP_DIR);
	gchar *tmp_file_name = g_build_filename(temp_dir, "gnomebaker-XXXXXX", NULL);
    g_free(temp_dir);
    FILE *stream = NULL;

	gint fd = g_mkstemp(tmp_file_name);
	if(fd == -1)
	{
		g_warning("gbcommon_create_open_temp_file - Failed when trying to create the temporary file [%s]",
                 tmp_file_name);
	}
	else if((stream = fdopen (fd, "w")) == NULL)
	{
		g_warning("gbcommon_create_open_temp_file - Could not open [%s] for writing", tmp_file_name);
        close(fd);
	}
    else
    {
    	tmp_file = (GBTempFile*)g_new0(GBTempFile, 1);
    	tmp_file->file_descriptor = fd;
    	tmp_file->file_stream = stream;
    	tmp_file->file_name = g_strdup(tmp_file_name); /*necesary for unlink*/
    	temp_file_list = g_list_prepend(temp_file_list, tmp_file);
    }
    g_free(tmp_file_name);

	return tmp_file;
}


/*this only closes the file*/
void
gbcommon_close_temp_file(GBTempFile *tmp_file)
{
	GB_LOG_FUNC
	g_return_if_fail(tmp_file != NULL);

	if(tmp_file->file_stream != NULL)
	{
		if(fclose(tmp_file->file_stream) != 0)
			g_warning("gbcommon_close_temp_file - Temporary file stream [%s] could not be closed", tmp_file->file_name);
	}
	if(tmp_file->file_descriptor >= 0)
	{
		if(close(tmp_file->file_descriptor) != 0)
			g_warning("gbcommon_close_temp_file - Temporary file descriptor [%s] could not be closed", tmp_file->file_name);
	}

    tmp_file->file_stream = NULL;
    tmp_file->file_descriptor = -1;
}


/*closes and deletes all the temp files created with gbcommon_create_temp_file*/
void
gbcommon_delete_all_temp_files()
{
	GB_LOG_FUNC
	GList *node = NULL;

	for(node = temp_file_list; node != NULL; node = node->next)
	{
		if(node->data != NULL)
		{
			GBTempFile *tmp_file = (GBTempFile*)node->data;
			gbcommon_close_temp_file(tmp_file);
			if(tmp_file->file_name != NULL)
			{
                /* TODO g_unlink requires gtk 2.6 */
				if(unlink(tmp_file->file_name) == -1)
				{
					g_warning("gbcommon_delete_all_temp_files - File [%s] could not be deleted", tmp_file->file_name);
				}
				g_free(tmp_file->file_name);
			}
			g_free(tmp_file);
		}
	}
	g_list_free(temp_file_list);
}


gboolean
gbcommon_init()
{
    GB_LOG_FUNC
    return TRUE;
}


void
gb_common_finalise()
{
    GB_LOG_FUNC
    /*delete temp files*/
	gbcommon_delete_all_temp_files();
}


void
gbcommon_start_busy_cursor(GtkWidget *window)
{
	GB_LOG_FUNC
	g_return_if_fail(window != NULL);
    if(GTK_WIDGET_REALIZED(window))
    {
    	GdkCursor *cursor = gdk_cursor_new(GDK_WATCH);
    	gdk_window_set_cursor(GDK_WINDOW(window->window), cursor);
    	gdk_cursor_destroy(cursor);/* safe because cursor is just a handle */
    	gdk_flush();
    }
}


void
gbcommon_start_busy_cursor1(GladeXML *xml, const gchar *window_name)
{
	GB_LOG_FUNC
    g_return_if_fail(xml != NULL);
    g_return_if_fail(window_name != NULL);
	GtkWidget *dlg = glade_xml_get_widget(xml, window_name);
	g_return_if_fail(dlg != NULL);
	gbcommon_start_busy_cursor(dlg);
}


void
gbcommon_end_busy_cursor(GtkWidget *window)
{
	GB_LOG_FUNC
	g_return_if_fail(window != NULL);
    if(GTK_WIDGET_REALIZED(window))
    {
    	gdk_window_set_cursor(GDK_WINDOW(window->window), NULL); /* set back to default cursor */
    	gdk_flush();
    }
}


void
gbcommon_end_busy_cursor1(GladeXML *xml, const gchar *window_name)
{
	GB_LOG_FUNC
    g_return_if_fail(xml != NULL);
    g_return_if_fail(window_name != NULL);
	GtkWidget *dlg = glade_xml_get_widget(xml, window_name);
	g_return_if_fail(dlg != NULL);
	gbcommon_end_busy_cursor(dlg);
}


guint64
gbcommon_calc_dir_size(const gchar *dir_name)
{
	/*GB_LOG_FUNC*/
    g_return_val_if_fail(dir_name != NULL, 0);

	guint64 size = 0;
	GDir *dir = g_dir_open(dir_name, 0, NULL);
	if(dir != NULL)
	{
		const gchar *name = g_dir_read_name(dir);
		while(name != NULL)
		{
			/* build up the full path to the name */
			gchar *full_name = g_build_filename(dir_name, name, NULL);

			GB_DECLARE_STRUCT(struct stat, s);
			if(stat(full_name, &s) == 0)
			{
				/* see if the name is actually a directory or a regular file */
				if(s.st_mode & S_IFDIR)
					size += gbcommon_calc_dir_size(full_name);
				else if(s.st_mode & S_IFREG)
					size += (guint64)s.st_size;
			}

			g_free(full_name);
			name = g_dir_read_name(dir);
		}

		g_dir_close(dir);
	}

	return size;
}

void
gbcommon_mkdir(const gchar *dir_name)
{
	GB_LOG_FUNC
	g_return_if_fail(dir_name != NULL);

	GB_TRACE("gbcommon_mkdir - creating [%s]\n", dir_name);

	gchar *dirs = g_strdup(dir_name);
	GString *dir = g_string_new("");

	gchar *current_dir = strtok(dirs, "/");
	while(current_dir != NULL)
	{
		g_string_append_printf(dir, "/%s", current_dir);
		if((g_file_test(dir->str, G_FILE_TEST_IS_DIR) == FALSE) &&
				(mkdir(dir->str, 0775) == -1))
			g_warning("gbcommon_mkdir - failed to create [%d]", errno);

		current_dir = strtok(NULL, "/");
	}

	g_string_free(dir, TRUE);
	g_free(dirs);
}


gchar**
gbcommon_get_file_as_list(const gchar *file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, NULL);

	gchar **ret = NULL;
	gchar *contents = NULL;
	if(g_file_get_contents(file, &contents, NULL, NULL))
		ret = g_strsplit(contents, "\n", 0);
	else
		g_warning("gbcommon_get_file_as_list - Failed to get contents of file [%s]", file);

	g_free(contents);
	return ret;
}


gchar*
gbcommon_get_option_menu_selection(GtkOptionMenu *option_menu)
{
	GB_LOG_FUNC
	g_return_val_if_fail(option_menu != NULL, NULL);

	GtkWidget *menu_item = GTK_BIN(option_menu)->child;
	if(menu_item != NULL && GTK_IS_LABEL(menu_item))
		return g_strdup(gtk_label_get_text(GTK_LABEL(menu_item)));
	return NULL;
}


void
gbcommon_set_option_menu_selection(GtkOptionMenu *option_menu, const gchar *selection)
{
	GB_LOG_FUNC
	g_return_if_fail(option_menu != NULL);
	g_return_if_fail(selection != NULL);

	GList *items = GTK_MENU_SHELL(gtk_option_menu_get_menu(option_menu))->children;
	gint index = 0;
	for(; items != NULL; items = items->next)
	{
		if(GTK_BIN(items->data)->child)
		{
			GtkWidget *child = GTK_BIN(items->data)->child;
			if (GTK_IS_LABEL(child) && 
                    (g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(child)), selection) == 0))
            {
				gtk_option_menu_set_history(option_menu, index);
				break;
            }
		}
		++index;
	}
}


gchar*
gbcommon_humanreadable_filesize(guint64 size)
{
	GB_LOG_FUNC
	gchar *ret = NULL;
	const gchar *unit_list[5] = {"B ", "KB", "MB", "GB", "TB"};
	gint unit = 0;
	gdouble human_size = (gdouble) size;

	while(human_size > 1024)
	{
		human_size = human_size / 1024;
		unit++;
	}

	if((human_size - (gulong)human_size) > 0.1)
		ret = g_strdup_printf("%.2f %s", human_size, unit_list[unit]);
	else
		ret = g_strdup_printf("%.0f %s", human_size, unit_list[unit]);
	return ret;
}


GdkPixbuf*
gbcommon_get_icon_for_mime(const gchar *mime, gint size)
{
	GB_LOG_FUNC
    g_return_val_if_fail(mime != NULL, NULL);

	GtkIconTheme *theme = gtk_icon_theme_get_default();
	g_return_val_if_fail(theme != NULL, NULL);

	gchar *icon_name = gnome_icon_lookup(theme ,NULL, NULL, NULL, NULL,
            mime, GNOME_ICON_LOOKUP_FLAGS_NONE, NULL);
	GdkPixbuf *ret = gtk_icon_theme_load_icon(theme, icon_name, size, 0, NULL);
	g_free(icon_name);

	if (ret == NULL)
		ret = gbcommon_get_icon_for_name("gnome-fs-regular", size);
	return ret;
}


GdkPixbuf*
gbcommon_get_icon_for_name(const gchar *icon, gint size)
{
	GB_LOG_FUNC
    g_return_val_if_fail(icon != NULL, NULL);
	GtkIconTheme *theme = gtk_icon_theme_get_default();
	g_return_val_if_fail(theme != NULL, NULL);
	return gtk_icon_theme_load_icon(theme, icon, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
}


void
gbcommon_launch_app_for_file(const gchar *file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);

	gchar *mime = gbcommon_get_mime_type(file);
	if(mime != NULL)
	{
		GnomeVFSMimeApplication *app = gnome_vfs_mime_get_default_application(mime);
		if(app != NULL)
		{
			gchar *uri = gnome_vfs_get_uri_from_local_path(file);
			GList *uris = g_list_append(NULL, uri);
			gnome_vfs_mime_application_launch(app, uris);
			g_free(uri);
			g_list_free(uris);
			gnome_vfs_mime_application_free(app);
		}
	}
	g_free(mime);
}


void
gbcommon_populate_disk_size_option_menu(GtkOptionMenu *option_menu, DiskSize sizes[],
										const gint size_count, const gint history)
{
	GB_LOG_FUNC
	g_return_if_fail(option_menu != NULL);
	g_return_if_fail(sizes != NULL);

	GtkWidget *menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
	if(menu != NULL)
		gtk_widget_destroy(menu);
	menu = gtk_menu_new();
	gtk_widget_show(menu);

	gint i = 0;
	for(; i < size_count; ++i)
	{
		GtkWidget *menu_item = gtk_menu_item_new_with_label(sizes[i].label);
		gtk_widget_show(menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), history);
}


gchar*
gbcommon_get_mime_description(const gchar *mime)
{
	GB_LOG_FUNC
	g_return_val_if_fail(mime != NULL, NULL);

	gchar *ret = NULL;
	const gchar *desc = gnome_vfs_mime_get_description(mime);
	if(desc != NULL)
		ret = g_strdup(desc);
	else
		ret = g_strdup(mime);

	GB_TRACE("gbcommon_get_mime_description - mime [%s] description [%s]\n", mime, ret);
	return ret;
}


gchar*
gbcommon_get_mime_type(const gchar *file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, NULL);

	gchar *uri = gbcommon_get_uri(file);
	gchar *mime = gnome_vfs_get_mime_type(uri);
	GB_TRACE("gbcommon_get_mime_type - uri [%s] mime [%s]\n", uri, mime);
	g_free(uri);
	return mime;
}


gchar*
gbcommon_get_local_path(const gchar *uri)
{
	GB_LOG_FUNC
	g_return_val_if_fail(uri != NULL, NULL);

	gchar *file_uri = g_strdup(uri);
	g_strstrip(file_uri); /* We sometimes get files with /r etc on the end */
	if(g_ascii_strncasecmp(file_uri, "file:", 5) == 0)
	{
		gchar* local_path = gnome_vfs_get_local_path_from_uri(file_uri);
		g_free(file_uri);
		return local_path;
	}
	return file_uri;
}


gchar*
gbcommon_get_uri(const gchar *local_path)
{
	GB_LOG_FUNC
	g_return_val_if_fail(local_path != NULL, NULL);
	return gnome_vfs_get_uri_from_local_path(local_path);
}


gboolean
gbcommon_get_first_selected_row(GtkTreeModel *model,
								GtkTreePath  *path,
								GtkTreeIter  *iter,
								gpointer      user_data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(user_data != NULL, TRUE);
	GtkTreeIter *treeiter = (GtkTreeIter*)user_data;
	*treeiter = *iter;
	return TRUE; /* only process the first selected item */
}


void
gbcommon_append_menu_item(GtkWidget *menu, const gchar *menu_item_label, GtkWidget *image,
						  GCallback activated, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(menu != NULL);
	g_return_if_fail(menu_item_label != NULL);
	g_return_if_fail(image != NULL);
	g_return_if_fail(activated != NULL);
	g_return_if_fail(user_data != NULL);

	GtkWidget *menu_item = gtk_image_menu_item_new_with_mnemonic(menu_item_label);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);
	g_signal_connect(menu_item, "activate", (GCallback)activated, user_data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
}


void
gbcommon_append_menu_item_stock(GtkWidget *menu, const gchar *menu_item_label, const gchar *stock_id,
								GCallback activated, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(stock_id != NULL);

	GtkWidget *image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);
	gbcommon_append_menu_item(menu, menu_item_label, image, activated, user_data);
}


void
gbcommon_append_menu_item_file(GtkWidget *menu, const gchar *menu_item_label, const gchar *file_name,
								GCallback activated, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(file_name != NULL);

	gchar *full_file_name = g_build_filename(IMAGEDIR, file_name, NULL);
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(full_file_name, NULL);
	GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
	gbcommon_append_menu_item(menu, menu_item_label, image, activated, user_data);
	g_free(full_file_name);
	g_object_unref(pixbuf);
}


gchar*
gbcommon_show_file_chooser(const gchar *title, GtkFileChooserAction action,
                           GtkFileFilter *custom_filter, const gboolean show_all_file_filter,
                           GtkComboBox *save_as_options)
{
    GB_LOG_FUNC
    g_return_val_if_fail(title != NULL, NULL);
    g_return_val_if_fail(action == GTK_FILE_CHOOSER_ACTION_OPEN ||
            action == GTK_FILE_CHOOSER_ACTION_SAVE, NULL);

    GtkWidget *file_chooser = gtk_file_chooser_dialog_new( title , NULL, action, GTK_STOCK_CANCEL,
            GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser), g_get_home_dir());
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(file_chooser), FALSE);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), custom_filter);
    if(show_all_file_filter || custom_filter == NULL)
    {
        GtkFileFilter *all_filter = gtk_file_filter_new();
        gtk_file_filter_add_pattern (all_filter, "*");
        gtk_file_filter_set_name(all_filter,_("All files"));
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), all_filter);
    }

    if(action == GTK_FILE_CHOOSER_ACTION_SAVE && save_as_options != NULL)
    {
        GtkWidget *pulldown_hbox = gtk_hbox_new(FALSE, 15);
        gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(file_chooser), pulldown_hbox);
        GtkWidget *filetypes_label = gtk_label_new(_("Save file as type:"));
        gtk_box_pack_start(GTK_BOX(pulldown_hbox),filetypes_label, FALSE, TRUE, 0);
        gtk_box_pack_end(GTK_BOX(pulldown_hbox), GTK_WIDGET(save_as_options), TRUE, TRUE, 0);
        gtk_widget_show_all(pulldown_hbox);
    }

    gchar *file = NULL;
    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_OK)
    {
        file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        if(action == GTK_FILE_CHOOSER_ACTION_SAVE && save_as_options != NULL)
        {
            gchar *ext = gtk_combo_box_get_active_text(save_as_options);
            if(strlen(file) > 3 && !gbcommon_str_has_suffix(file, ext))
            {
                gchar *tmp = file;
                file = g_strconcat(tmp, ext, NULL);
                g_free(tmp);
            }
            g_free(ext);
        }
    }
    gtk_widget_destroy(file_chooser);
    return file;
}


void
gbcommon_center_window_on_parent(GtkWidget *window)
{
    GB_LOG_FUNC
    g_return_if_fail(window != NULL);

    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(gnomebaker_get_window()));
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_widget_realize(window);
    gdk_window_set_functions(window->window, GDK_FUNC_RESIZE | GDK_FUNC_MOVE | GDK_FUNC_CLOSE);
    gtk_widget_show(window);
}


gboolean
gbcommon_str_has_suffix(const gchar *str, const gchar *suffix)
{
    GB_LOG_FUNC
    g_return_val_if_fail(str != NULL, FALSE);
    g_return_val_if_fail(suffix != NULL, FALSE);
    return (g_ascii_strcasecmp(str + strlen(str) - strlen(suffix), suffix) == 0);
}


gboolean
gbcommon_iso_file_filter(const GtkFileFilterInfo *filter_info, gpointer data)
{
    GB_LOG_FUNC
    if(g_ascii_strcasecmp(filter_info->mime_type, "application/x-cd-image") == 0)
        return TRUE;
    return FALSE;
}


