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
#include "gbcommon.h" 
#include <sys/stat.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>


void 
gbcommon_start_busy_cursor(GtkWidget* window)
{
	GB_LOG_FUNC
	g_return_if_fail(window != NULL);
	GdkCursor* cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(GDK_WINDOW(window->window), cursor);
	gdk_cursor_destroy(cursor);/* safe because cursor is just a handle */
	gdk_flush();
}


void 
gbcommon_start_busy_cursor1(GladeXML* xml, const gchar* windowname)
{
	GB_LOG_FUNC
	GtkWidget* dlg = glade_xml_get_widget(xml, windowname);
	g_return_if_fail(dlg != NULL);
	gbcommon_start_busy_cursor(dlg);
}


void 
gbcommon_end_busy_cursor(GtkWidget* window)
{
	GB_LOG_FUNC
	g_return_if_fail(window != NULL);
	gdk_window_set_cursor(GDK_WINDOW(window->window), NULL); /* set back to default cursor */
	gdk_flush();
}


void 
gbcommon_end_busy_cursor1(GladeXML* xml, const gchar* windowname)
{
	GB_LOG_FUNC
	GtkWidget* dlg = glade_xml_get_widget(xml, windowname);
	g_return_if_fail(dlg != NULL);
	gbcommon_end_busy_cursor(dlg);
}


guint64
gbcommon_calc_dir_size(const gchar* dirname)
{
	/*GB_LOG_FUNC*/
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


gchar* 
gbcommon_tidy_nautilus_dnd_file(const gchar* file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file, NULL);
	
	/* If there's a file url at the start then strip it off */	
	if(g_ascii_strncasecmp(file, fileurl, strlen(fileurl)) == 0)
		file += strlen(fileurl);
	
	guint length = strlen(file);
	gchar* ret = g_malloc((length + 1) * sizeof(gchar));
	memset(ret, 0x0, (length + 1) * sizeof(gchar));
		
	guint i = 0;
	for(; i < length; i++)
	{
		switch(file[i])
		{
		case '\r':
			strcat(ret, "\0");
			break;
		case '%':
			if(strncmp(&(file[i]), "%20", 3) == 0)
			{
				strcat(ret, " ");
				i += 2;
				break;
			}
		/* fall through and add the % as it's not a space */
		default:
			strncat(ret, &(file[i]), 1);
		};		
	}
	
	g_message(_("returning [%s]"), ret);
		
	return ret;
}


void 
gbcommon_mkdir(const gchar* dirname)
{
	GB_LOG_FUNC
	g_return_if_fail(dirname != NULL);
		
	g_message(_("creating [%s]"), dirname);

	gchar *dirs = g_strdup(dirname);
	GString *dir = g_string_new("");
	
	gchar* currentdir = strtok(dirs, "/");
	while(currentdir != NULL)
	{
		g_string_append_printf(dir, "/%s", currentdir);
		if((g_file_test(dir->str, G_FILE_TEST_IS_DIR) == FALSE) && 
				(mkdir(dir->str, 0775) == -1))
			g_critical(_("failed to create temp %d"), errno);

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
		g_critical(_("Failed to get contents of file [%s]"), file);

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
	while(items)
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
		items = items->next;
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
	GtkIconTheme* theme = gtk_icon_theme_get_default();
	g_return_val_if_fail(theme != NULL, NULL);
	return gtk_icon_theme_load_icon(theme, icon, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
}


void
gbcommon_launch_app_for_file(const gchar* file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);
	
	gchar* mime = gnome_vfs_get_mime_type(file);
	GnomeVFSMimeApplication* app = gnome_vfs_mime_get_default_application(mime);
	if(app != NULL)
	{			
		gchar* uri = gnome_vfs_get_uri_from_local_path(file);
		GList* uris = NULL;
		uris = g_list_append(uris, uri);
		gnome_vfs_mime_application_launch(app, uris);
		g_free(uri);
		g_list_free(uris);
		gnome_vfs_mime_application_free(app);
	}
	
	g_free(mime);	
}
