/***************************************************************************
 *            gbcommon.h
 *
 *  Sun Jul  4 23:23:55 2004
 *  Copyright  2004  User
 *  Email
 ****************************************************************************/

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
 
 
#ifndef _GBCOMMON_H
#define _GBCOMMON_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glade/glade.h>
#include <gnome.h>


void gbcommon_start_busy_cursor(GtkWidget* window);
void gbcommon_start_busy_cursor1(GladeXML* xml, const gchar* windowname);
void gbcommon_end_busy_cursor(GtkWidget* window);
void gbcommon_end_busy_cursor1(GladeXML* xml, const gchar* windowname);
gulong gbcommon_calc_dir_size(const gchar* dirname);
gchar* gbcommon_tidy_nautilus_dnd_file(const gchar* file);
void gbcommon_mkdir(const gchar* dir);
gchar** gbcommon_get_file_as_list(const gchar* file);

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

static const gchar* const widget_audiocd_tree = "treeview8";
static const gchar* const widget_audiocd_size = "optionmenu2";
static const gchar* const widget_audiocd_progressbar = "progressbar3";
static const gchar* const widget_audiocd_create = "button14";

static const gchar* const widget_top_toolbar = "toolbar3";
static const gchar* const widget_middle_toolbar = "toolbar4";


/* Start dialog glade widget names */
static const gchar* const widget_startdlg = "startDlg";
static const gchar* const widget_startdlg_reader = "optmenReader";
static const gchar* const widget_startdlg_writer = "optmenWriter";
static const gchar* const widget_startdlg_speed = "spinSpeed";
static const gchar* const widget_startdlg_dummy = "checkDummy";
static const gchar* const widget_startdlg_multisession = "checkMultisession";
static const gchar* const widget_startdlg_eject = "checkEject";
static const gchar* const widget_startdlg_fasterase = "checkFastErase";
static const gchar* const widget_startdlg_overburn = "checkOverburn";
static const gchar* const widget_startdlg_readlabel = "labelReader";
static const gchar* const widget_startdlg_burnfree = "checkBurnFree";
static const gchar* const widget_startdlg_isoonly = "checkISOOnly";
static const gchar* const widget_startdlg_writemode = "optionmenu7";
static const gchar* const widget_startdlg_modelabel = "label265";


/* Progress dialog glade widget names */
static const gchar* const widget_progdlg = "progDlg";
static const gchar* const widget_progdlg_progbar = "progressbar1";
static const gchar* const widget_progdlg_output = "textview1";
static const gchar* const widget_progdlg_output_scroll = "textviewScroll";
static const gchar* const widget_progdlg_stop = "buttonStop";
static const gchar* const widget_progdlg_close = "buttonClose";

/* ISOfs dialog glade widget names */
static const gchar* const widget_isofsdlg = "isofsDlg";
static const gchar* const widget_isofsdlg_createdby = "entryCreated";
static const gchar* const widget_isofsdlg_volume = "entryVolume";

/* Prefs dialog glade widget names */
static const gchar* const widget_prefsdlg = "prefsDlg";
static const gchar* const widget_prefsdlg_tempdir = "tmpDirEntry";
static const gchar* const widget_prefsdlg_cleantempdir = "checkCleanTmp";
static const gchar* const widget_prefsdlg_showhidden = "checkHiddenFiles";
static const gchar* const widget_prefsdlg_alwaysscan = "checkAlwaysScan";
static const gchar* const widget_prefsdlg_devicelist = "treeview12";

/* Splash dialog glade widget names */
static const gchar* const widget_splashdlg = "splashWnd";
static const gchar* const widget_splashdlg_label = "splashLabel";


#define GB_LOG_FUNC											\
	if(showtrace) g_print("Entering [%s] [%s] [%d]\n", __FUNCTION__, __FILE__, __LINE__);	\

#define GB_DECLARE_STRUCT(STRUCT, INSTANCE)			\
	STRUCT INSTANCE; memset(&INSTANCE, 0x0, sizeof(STRUCT));	\

/* nautilus drag and drop has this prepended */
static const gchar* const fileurl = "file://";


#endif /* _GBCOMMON_H */
