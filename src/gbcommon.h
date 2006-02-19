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

#ifndef _GBCOMMON_H
#define _GBCOMMON_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glade/glade.h>
#include <gnome.h>

typedef struct
{
	gdouble size;
	gchar* label;
} DiskSize;

typedef struct 
{
	int fileDescriptor;
	FILE * fileStream;
	char * fileName;
	
}GBTempFile;

GBTempFile * gbcommon_create_open_temp_file(const gchar * prefix);
void gbcommon_close_temp_file(GBTempFile *tmpFile);
void gbcommon_delete_all_temp_files();
void gbcommon_start_busy_cursor(GtkWidget* window);
void gbcommon_start_busy_cursor1(GladeXML* xml, const gchar* windowname);
void gbcommon_end_busy_cursor(GtkWidget* window);
void gbcommon_end_busy_cursor1(GladeXML* xml, const gchar* windowname);
guint64 gbcommon_calc_dir_size(const gchar* dirname);
void gbcommon_mkdir(const gchar* dir);
gchar** gbcommon_get_file_as_list(const gchar* file);
gchar* gbcommon_get_option_menu_selection(GtkOptionMenu* optmen);
void gbcommon_set_option_menu_selection(GtkOptionMenu* optmen, const gchar* selection);
gchar* gbcommon_humanreadable_filesize(guint64 size);
GdkPixbuf* gbcommon_get_icon_for_mime(const gchar* mime, gint size);
GdkPixbuf* gbcommon_get_icon_for_name(const gchar* mime, gint size);
void gbcommon_launch_app_for_file(const gchar* file);
void gbcommon_populate_disk_size_option_menu(GtkOptionMenu* optmen, DiskSize sizes[],
            const gint sizecount, const gint history);
gchar* gbcommon_get_mime_description(const gchar* mime);
gchar* gbcommon_get_mime_type(const gchar* file);
gchar* gbcommon_get_local_path(const gchar* uri);
gchar* gbcommon_get_uri(const gchar* localpath);
gboolean gbcommon_get_first_selected_row(GtkTreeModel *model, GtkTreePath  *path,
            GtkTreeIter  *iter, gpointer user_data);
void gbcommon_append_menu_item_file(GtkWidget* menu, const gchar* menuitemlabel,
			const gchar* filename, GCallback activated, gpointer userdata);
void gbcommon_append_menu_item_stock(GtkWidget* menu, const gchar* menuitemlabel,
			const gchar* stockid, GCallback activated, gpointer userdata);

gchar* gbcommon_show_file_chooser(const gchar* title, GtkFileChooserAction action, 
            GtkFileFilter* customfilter, const gboolean showallfilefilter, GtkComboBox* saveasoptions);
void gb_common_finalise();
gboolean gbcommon_init();
void gbcommon_center_window_on_parent(GtkWidget* window);
gboolean gbcommon_str_has_suffix(const gchar* str, const gchar* suffix);
gboolean gbcommon_iso_file_filter(const GtkFileFilterInfo *filter_info, gpointer data);

/* defined in main.c */
extern const gchar* glade_file;
extern gboolean showtrace;


#define GB_LOG_FUNC											\
	if(showtrace) g_print("[%p] [%s] [%s] [%d]\n", (gpointer)g_thread_self(), __FUNCTION__, __FILE__, __LINE__);	\

#define GB_TRACE			\
	if(showtrace) g_print	\

#define GB_DECLARE_STRUCT(STRUCT, INSTANCE)			\
	STRUCT INSTANCE; memset(&INSTANCE, 0x0, sizeof(STRUCT));	\

#endif /* _GBCOMMON_H */
