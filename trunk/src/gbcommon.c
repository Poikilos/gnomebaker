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


gulong 
gbcommon_calc_dir_size(const gchar* dirname)
{
	/*GB_LOG_FUNC*/
	gulong size = 0;
	GError *err = NULL;
	
	GDir *dir = g_dir_open(dirname, 0, &err);		
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
				if((s.st_mode & S_IFDIR)/* && (name[0] != '.')*/)
					size += gbcommon_calc_dir_size(fullname);
				else if((s.st_mode & S_IFREG)/* && (name[0] != '.')*/)
					size += s.st_size;
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
	
	g_message("returning [%s]", ret);
		
	return ret;
}


void 
gbcommon_mkdir(const gchar* dirname)
{
	GB_LOG_FUNC
	g_return_if_fail(dirname != NULL);
		
	g_message("creating [%s]", dirname);

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
