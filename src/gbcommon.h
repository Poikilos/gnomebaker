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

gchar* gbcommon_show_iso_dlg();
void gb_common_finalise();
gboolean gbcommon_init();
void gbcommon_memset(void* memory, gsize size);
void gbcommon_center_window_on_parent(GtkWidget* window);

/* defined in main.c */
extern const gchar* glade_file;
extern gboolean showtrace;

/* Main gui glade widget names */
static const gchar* const widget_gnomebaker = "GnomeBaker";
static const gchar* const widget_appbar = "appbar1";
static const gchar* const widget_browser_dirtree = "treeview4";
static const gchar* const widget_browser_filelist = "treeview5";

static const gchar* const widget_datacd_tree = "treeview7";
static const gchar* const widget_datacd_size = "optionmenu1";
static const gchar* const widget_datacd_progressbar = "progressbar2";
static const gchar* const widget_datacd_create = "createDataCDBtn";
static const gchar* const widget_datacd_notebook = "notebook1";

static const gchar* const widget_import = "toolbutton5";
static const gchar* const widget_up = "toolbutton8";
static const gchar* const widget_down = "toolbutton9";

static const gchar* const widget_audiocd_tree = "treeview8";
static const gchar* const widget_audiocd_size = "optionmenu2";
static const gchar* const widget_audiocd_progressbar = "progressbar3";
static const gchar* const widget_audiocd_create = "button14";

static const gchar* const widget_top_toolbar_dock = "bonobodockitem4";
static const gchar* const widget_top_toolbar = "toolbar3";
static const gchar* const widget_middle_toolbar = "toolbar4";

static const gchar* const widget_show_browser_menu = "show_file_browser1";
static const gchar* const widget_browser_hpane = "hpaned3";
static const gchar* const widget_add_button = "buttonAddFiles";
static const gchar* const widget_refresh_menu = "refresh1";
static const gchar* const widget_refresh_button = "toolbutton4";

/* Start dialog glade widget names */
static const gchar* const widget_startdlg = "startDlg";
static const gchar* const widget_startdlg_reader = "optmenReader";
static const gchar* const widget_startdlg_writer = "optmenWriter";
static const gchar* const widget_startdlg_speed = "spinSpeed";
static const gchar* const widget_startdlg_readlabel = "labelReader";
static const gchar* const widget_startdlg_writemode = "optionmenu7";
static const gchar* const widget_startdlg_modelabel = "label265";


/* Progress dialog glade widget names */
static const gchar* const widget_progdlg = "progDlg";
static const gchar* const widget_progdlg_progbar = "progressbar1";
static const gchar* const widget_progdlg_output = "textview1";
static const gchar* const widget_progdlg_output_scroll = "textviewScroll";
static const gchar* const widget_progdlg_stop = "buttonStop";
static const gchar* const widget_progdlg_close = "buttonClose";
static const gchar* const widget_progdlg_toggleoutputlabel = "label249";

/* ISOfs dialog glade widget names */
static const gchar* const widget_isofsdlg = "isofsDlg";
static const gchar* const widget_isofsdlg_createdby = "entryCreated";
static const gchar* const widget_isofsdlg_volume = "entryVolume";

/* Prefs dialog glade widget names */
static const gchar* const widget_prefsdlg = "prefsDlg";
static const gchar* const widget_prefsdlg_tempdir = "tmpDirEntry";
static const gchar* const widget_prefsdlg_cleantempdir = "checkCleanTmp";
static const gchar* const widget_prefsdlg_showhidden = "checkHiddenFiles";
static const gchar* const widget_prefsdlg_askonquit = "checkAskOnQuit";
static const gchar* const widget_prefsdlg_showhumansize = "checkShowHumanSizes";
static const gchar* const widget_prefsdlg_alwaysscan = "checkAlwaysScan";
static const gchar* const widget_prefsdlg_devicelist = "treeview12";

/* Splash dialog glade widget names */
static const gchar* const widget_splashdlg = "splashWnd";
static const gchar* const widget_splashdlg_label = "splashLabel";


/* Select device dialog widget names */
static const gchar* const widget_select_writer = "optmenWriter";
static const gchar* const widget_select_device_dlg = "selectDeviceDlg";



#define GB_LOG_FUNC											\
	if(showtrace) g_print("Entering [%s] [%s] [%d]\n", __FUNCTION__, __FILE__, __LINE__);	\
		
#define GB_TRACE			\
	if(showtrace) g_message	\

#define GB_DECLARE_STRUCT(STRUCT, INSTANCE)			\
	STRUCT INSTANCE; gbcommon_memset(&INSTANCE, sizeof(STRUCT));	\

#endif /* _GBCOMMON_H */
