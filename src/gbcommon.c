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
#include <sys/stat.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>


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
}


void 
gbcommon_start_busy_cursor(GtkWidget* window)
{
	GB_LOG_FUNC
	g_return_if_fail(window != NULL);
    if(GTK_WIDGET_REALIZED(window))
    {
    	GdkCursor* cursor = gdk_cursor_new(GDK_WATCH);
    	gdk_window_set_cursor(GDK_WINDOW(window->window), cursor);
    	gdk_cursor_destroy(cursor);/* safe because cursor is just a handle */
    	gdk_flush();
    }
}


void 
gbcommon_start_busy_cursor1(GladeXML* xml, const gchar* windowname)
{
	GB_LOG_FUNC
    g_return_if_fail(xml != NULL);
    g_return_if_fail(windowname != NULL);    
	GtkWidget* dlg = glade_xml_get_widget(xml, windowname);
	g_return_if_fail(dlg != NULL);
	gbcommon_start_busy_cursor(dlg);
}


void 
gbcommon_end_busy_cursor(GtkWidget* window)
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
gbcommon_end_busy_cursor1(GladeXML* xml, const gchar* windowname)
{
	GB_LOG_FUNC
    g_return_if_fail(xml != NULL);
    g_return_if_fail(windowname != NULL);        
	GtkWidget* dlg = glade_xml_get_widget(xml, windowname);
	g_return_if_fail(dlg != NULL);
	gbcommon_end_busy_cursor(dlg);
}


guint64
gbcommon_calc_dir_size(const gchar* dirname)
{
	/*GB_LOG_FUNC*/
    g_return_val_if_fail(dirname != NULL, 0);
    
	guint64 size = 0;	
	GDir *dir = g_dir_open(dirname, 0, NULL);
	if(dir != NULL)
	{
		const gchar *name = g_dir_read_name(dir);	
		while(name != NULL)
		{
			/* build up the full path to the name */
			gchar* fullname = g_build_filename(dirname, name, NULL);
	
			GB_DECLARE_STRUCT(struct stat, s);
			if(stat(fullname, &s) == 0)
			{
				/* see if the name is actually a directory or a regular file */
				if(s.st_mode & S_IFDIR)
					size += gbcommon_calc_dir_size(fullname);
				else if(s.st_mode & S_IFREG)
					size += (guint64)s.st_size;
			}
			
			g_free(fullname);			
			name = g_dir_read_name(dir);
		}
	
		g_dir_close(dir);
	}
	
	return size;
}

void 
gbcommon_mkdir(const gchar* dirname)
{
	GB_LOG_FUNC
	g_return_if_fail(dirname != NULL);
		
	GB_TRACE("creating [%s]", dirname);

	gchar *dirs = g_strdup(dirname);
	GString *dir = g_string_new("");
	
	gchar* currentdir = strtok(dirs, "/");
	while(currentdir != NULL)
	{
		g_string_append_printf(dir, "/%s", currentdir);
		if((g_file_test(dir->str, G_FILE_TEST_IS_DIR) == FALSE) && 
				(mkdir(dir->str, 0775) == -1))
			g_critical("failed to create temp %d", errno);

		currentdir = strtok(NULL, "/");
	}
	
	g_string_free(dir, TRUE);
	g_free(dirs);	
}


gchar** 
gbcommon_get_file_as_list(const gchar* file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, NULL);
	
	gchar** ret = NULL;	
	gchar* contents = NULL;	
	if(g_file_get_contents(file, &contents, NULL, NULL))
		ret = g_strsplit(contents, "\n", 0);
	else
		g_critical("Failed to get contents of file [%s]", file);

	g_free(contents);	
	return ret;
}


gchar* 
gbcommon_get_option_menu_selection(GtkOptionMenu* optmen)
{
	GB_LOG_FUNC
	g_return_val_if_fail(optmen != NULL, NULL);
	
	gchar* ret = NULL;
	GtkWidget* mode = GTK_BIN(optmen)->child;
	if(mode != NULL && GTK_IS_LABEL(mode))
	{
		gchar* text = NULL;
		gtk_label_get(GTK_LABEL(mode), &text);
		/*g_free(text);*/
		ret = g_strdup(text);
	}
	return ret;
}


void 
gbcommon_set_option_menu_selection(GtkOptionMenu* optmen, const gchar* selection)
{	
	GB_LOG_FUNC
	g_return_if_fail(optmen != NULL);
	g_return_if_fail(selection != NULL);
	
	GList* items = GTK_MENU_SHELL(gtk_option_menu_get_menu(optmen))->children;	
	gint index = 0;
	for(; items != NULL; items = items->next)
	{
		if(GTK_BIN(items->data)->child)
		{
			GtkWidget *child = GTK_BIN(items->data)->child;				
			if (GTK_IS_LABEL(child))
			{
				gchar *text = NULL;			
				gtk_label_get(GTK_LABEL(child), &text);
				if(g_ascii_strcasecmp(text, selection) == 0)
				{
					gtk_option_menu_set_history(optmen, index);	
					break;
				}
			}
		}
		++index;
	}
}

gchar*
gbcommon_humanreadable_filesize(guint64 size)
{
	GB_LOG_FUNC
	gchar* ret = NULL;
	const gchar* unit_list[5] = {"B ", "KB", "MB", "GB", "TB"};
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
gbcommon_get_icon_for_mime(const gchar* mime, gint size)
{
	GB_LOG_FUNC	
    g_return_val_if_fail(mime != NULL, NULL);
    
	GtkIconTheme* theme = gtk_icon_theme_get_default();
	g_return_val_if_fail(theme != NULL, NULL);

	gchar* icon_name = gnome_icon_lookup(
		theme ,NULL, NULL, NULL, NULL, mime, GNOME_ICON_LOOKUP_FLAGS_NONE, NULL);
	GdkPixbuf* ret = gtk_icon_theme_load_icon(theme, icon_name, size, 0, NULL);
	g_free(icon_name);

	if (ret == NULL) 
		ret = gbcommon_get_icon_for_name("gnome-fs-regular", size);
	return ret;
}


GdkPixbuf*
gbcommon_get_icon_for_name(const gchar* icon, gint size)
{
	GB_LOG_FUNC
    g_return_val_if_fail(icon != NULL, NULL);
	GtkIconTheme* theme = gtk_icon_theme_get_default();
	g_return_val_if_fail(theme != NULL, NULL);
	return gtk_icon_theme_load_icon(theme, icon, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
}


void
gbcommon_launch_app_for_file(const gchar* file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);
	
	gchar* mime = gbcommon_get_mime_type(file);
	if(mime != NULL)
	{		
		GnomeVFSMimeApplication* app = gnome_vfs_mime_get_default_application(mime);
		if(app != NULL)
		{			
			gchar* uri = gnome_vfs_get_uri_from_local_path(file);
			GList* uris = g_list_append(NULL, uri);
			gnome_vfs_mime_application_launch(app, uris);
			g_free(uri);
			g_list_free(uris);
			gnome_vfs_mime_application_free(app);
		}
	}
	g_free(mime);	
}


void 
gbcommon_populate_disk_size_option_menu(GtkOptionMenu* optmen, DiskSize sizes[], 
										const gint sizecount, const gint history)
{
	GB_LOG_FUNC
	g_return_if_fail(optmen != NULL);
	g_return_if_fail(sizes != NULL);
	
	GtkWidget* menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmen));
	if(menu != NULL)
		gtk_widget_destroy(menu);
	menu = gtk_menu_new();
	gtk_widget_show(menu);
	
	gint i = 0; 
	for(; i < sizecount; ++i)
	{
		GtkWidget* menuitem = gtk_menu_item_new_with_label(sizes[i].label);
		gtk_widget_show(menuitem);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmen), menu);	
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmen), history);
}


gchar* 
gbcommon_get_mime_description(const gchar* mime)
{
	GB_LOG_FUNC
	g_return_val_if_fail(mime != NULL, NULL);
	
	gchar* ret = NULL;
	const gchar* desc = gnome_vfs_mime_get_description(mime);
	if(desc != NULL)
		ret = g_strdup(desc);
	else
		ret = g_strdup(mime);
	
	GB_TRACE("gbcommon_get_mime_description - mime [%s] description [%s]", mime, ret);	
	return ret;
}


gchar*
gbcommon_get_mime_type(const gchar* file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, NULL);	
	
	gchar* uri = gbcommon_get_uri(file);	
	gchar* mime = gnome_vfs_get_mime_type(uri);
	GB_TRACE("gbcommon_get_mime_type - uri [%s] mime [%s]", uri, mime);
	g_free(uri);	
	return mime;
}


gchar* 
gbcommon_get_local_path(const gchar* uri)
{
	GB_LOG_FUNC
	g_return_val_if_fail(uri != NULL, NULL);	
	
	gchar* fileuri = g_strdup(uri);
	g_strstrip(fileuri); /* We sometimes get files with /r etc on the end */
	if(g_ascii_strncasecmp(fileuri, "file:", 5) == 0)
	{
		gchar* localpath = gnome_vfs_get_local_path_from_uri(fileuri);
		g_free(fileuri);
		return localpath;
	}
	return fileuri;		
}


gchar* 
gbcommon_get_uri(const gchar* localpath)
{
	GB_LOG_FUNC
	g_return_val_if_fail(localpath != NULL, NULL);	
	return gnome_vfs_get_uri_from_local_path(localpath);
}


gboolean
gbcommon_get_first_selected_row(GtkTreeModel *model,
								GtkTreePath  *path,
								GtkTreeIter  *iter,
								gpointer      user_data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(user_data != NULL, TRUE);
	GtkTreeIter* treeiter = (GtkTreeIter*)user_data;
	*treeiter = *iter;
	return TRUE; /* only process the first selected item */
}


void 
gbcommon_append_menu_item(GtkWidget* menu, const gchar* menuitemlabel, GtkWidget* image,
						  GCallback activated, gpointer userdata)
{
	GB_LOG_FUNC
	g_return_if_fail(menu != NULL);
	g_return_if_fail(menuitemlabel != NULL);
	g_return_if_fail(image != NULL);
	g_return_if_fail(activated != NULL);
	g_return_if_fail(userdata != NULL);
	
	GtkWidget* menuitem = gtk_image_menu_item_new_with_mnemonic(menuitemlabel);	
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect(menuitem, "activate",
		(GCallback)activated, userdata);	
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);	
}


void 
gbcommon_append_menu_item_stock(GtkWidget* menu, const gchar* menuitemlabel, const gchar* stockid,
								GCallback activated, gpointer userdata)
{
	GB_LOG_FUNC
	g_return_if_fail(stockid != NULL);
	
	GtkWidget* image = gtk_image_new_from_stock(stockid, GTK_ICON_SIZE_MENU);
	gbcommon_append_menu_item(menu, menuitemlabel, image, activated, userdata);	
}


void 
gbcommon_append_menu_item_file(GtkWidget* menu, const gchar* menuitemlabel, const gchar* filename,
								GCallback activated, gpointer userdata)
{
	GB_LOG_FUNC
	g_return_if_fail(filename != NULL);
	
	gchar* fullfilename = g_build_filename(PACKAGE_PIXMAPS_DIR, filename, NULL);
	GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(fullfilename, NULL);
	GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
	gbcommon_append_menu_item(menu, menuitemlabel, image, activated, userdata);	
	g_free(fullfilename);
	g_object_unref(pixbuf);
}


gchar*
gbcommon_show_file_chooser(const gchar* title, GtkFileFilter* imagefilter)
{
	GB_LOG_FUNC
    g_return_val_if_fail(title != NULL, NULL);
    g_return_val_if_fail(imagefilter != NULL, NULL);
    
	gchar* file = NULL;
	GtkWidget *filesel = gtk_file_chooser_dialog_new( title , NULL, GTK_FILE_CHOOSER_ACTION_OPEN, 
	    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filesel), FALSE);
	GtkFileFilter *allfilter = gtk_file_filter_new();
	gtk_file_filter_add_pattern (allfilter, "*");
	gtk_file_filter_set_name(allfilter,_("All files"));
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filesel), imagefilter);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filesel), allfilter);
	const gint result = gtk_dialog_run(GTK_DIALOG(filesel));
	file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel));
	gtk_widget_destroy(filesel);
	if(result != GTK_RESPONSE_OK)
		file = NULL;
	return file;
}


void 
gbcommon_center_window_on_parent(GtkWidget* window)
{
    GB_LOG_FUNC    
    g_return_if_fail(window != NULL);
    
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(gnomebaker_get_window()));
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_widget_realize(window);
    gdk_window_set_functions(window->window, GDK_FUNC_RESIZE|GDK_FUNC_MOVE|GDK_FUNC_CLOSE);
    gtk_widget_show(window);
}


gboolean 
gbcommon_str_has_suffix(const gchar* str, const gchar* suffix)
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


