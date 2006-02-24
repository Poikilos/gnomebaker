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
 * File: preferences.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Wed Jan 22 23:45:46 2003
 */

#ifndef _PREFS_H
#define _PREFS_H

#include <gnome.h>
#include <gconf/gconf-client.h>

#define GB_BASE_KEY "/apps/GnomeBaker"
#define GB_READER 					GB_BASE_KEY "/ReadDevice"
#define GB_WRITER 					GB_BASE_KEY "/WriteDevice"
#define GB_CDWRITE_SPEED 			GB_BASE_KEY "/WriteSpeed"
#define GB_DVDWRITE_SPEED 			GB_BASE_KEY "/DvdWriteSpeed"
#define GB_DUMMY 					GB_BASE_KEY "/Dummy"
#define GB_EJECT 					GB_BASE_KEY "/Eject"
#define GB_TEMP_DIR 				GB_BASE_KEY "/TempDir"
#define GB_FAST_BLANK 				GB_BASE_KEY "/FastBlank"
#define GB_BURNFREE 				GB_BASE_KEY "/BurnFree"
#define GB_SHOWHIDDEN 				GB_BASE_KEY "/ShowHiddenFiles"
#define GB_SHOWHUMANSIZE			GB_BASE_KEY "/ShowHumanFilesizes"
#define GB_CLEANTEMPDIR 			GB_BASE_KEY "/CleanTempDir"
#define GB_CREATEISOONLY 			GB_BASE_KEY "/CreateISOOnly"
#define GB_WRITE_MODE 				GB_BASE_KEY "/WriteMode"
#define GB_DVDWRITE_MODE            GB_BASE_KEY "/DvdWriteMode"
#define GB_ALWAYS_SCAN				GB_BASE_KEY "/AlwaysScanOnStartup"
#define GB_FORCE					GB_BASE_KEY "/Force"
#define GB_FINALIZE					GB_BASE_KEY "/Finalize"
#define GB_SHOW_FILE_BROWSER		GB_BASE_KEY "/ShowFileBrowser"
#define GB_FAST_FORMAT 				GB_BASE_KEY "/FastFormat"
#define GB_JOLIET                   GB_BASE_KEY "/Joliet"
#define GB_ROCKRIDGE                GB_BASE_KEY "/RockRidge"
#define GB_ONTHEFLY                 GB_BASE_KEY "/OnTheFly"
#define GB_ASK_ON_QUIT				GB_BASE_KEY "/AskOnQuit"
#define GB_PLAY_SOUND               GB_BASE_KEY "/PlaySound"
#define GB_DATA_DISK_SIZE           GB_BASE_KEY "/DataDiskSize"
#define GB_AUDIO_DISK_SIZE          GB_BASE_KEY "/AudioDiskSize"
#define GB_CDRECORD_FORCE		    GB_BASE_KEY "/CDRecordForce"
#define GB_LAST_ISO                 GB_BASE_KEY "/LastISO"

#define GB_MAIN_WINDOW_HEIGHT		GB_BASE_KEY "/UI/MainWindowHeight"
#define GB_MAIN_WINDOW_WIDTH		GB_BASE_KEY "/UI/MainWindowWidth"
#define GB_MAIN_WINDOW_POSITION_X	GB_BASE_KEY "/UI/MainWindowPositionX"
#define GB_MAIN_WINDOW_POSITION_Y	GB_BASE_KEY "/UI/MainWindowPositionY"
#define GB_MAIN_WINDOW_MAXIMIZED	GB_BASE_KEY "/UI/MainWindowMaximized"
#define GB_SCROLL_OUTPUT		    GB_BASE_KEY "/UI/ScrollOutput"

#define GB_DEVICE_FORMAT 			"Device%.2d"
#define GB_DEVICES_KEY				GB_BASE_KEY "/Devices" 
#define GB_DEVICE_KEY				GB_DEVICES_KEY "/" GB_DEVICE_FORMAT

#define GB_DEVICE_NAME_LABEL		"/DeviceName"
#define GB_DEVICE_NAME				GB_DEVICE_KEY GB_DEVICE_NAME_LABEL
#define GB_DEVICE_ID_LABEL			"/DeviceId"
#define GB_DEVICE_ID				GB_DEVICE_KEY GB_DEVICE_ID_LABEL
#define GB_DEVICE_NODE_LABEL		"/DeviceNode"
#define GB_DEVICE_NODE				GB_DEVICE_KEY GB_DEVICE_NODE_LABEL
#define GB_DEVICE_MOUNT_LABEL		"/MountPoint"
#define GB_DEVICE_MOUNT				GB_DEVICE_KEY GB_DEVICE_MOUNT_LABEL
#define GB_DEVICE_CAPABILITIES_LABEL	"/Capabilities"
#define GB_DEVICE_CAPABILITIES		GB_DEVICE_KEY GB_DEVICE_CAPABILITIES_LABEL

#define GNOME_TOOLBAR_STYLE		 "/desktop/gnome/interface/toolbar_style"
#define GNOME_TOOLBAR_DETACHABLE "/desktop/gnome/interface/toolbar_detachable"

gboolean preferences_init();
void preferences_finalise();

gchar *preferences_get_copy_data_cd_image();
gchar *preferences_get_create_data_cd_image();
gchar *preferences_get_convert_audio_track_dir();
gchar *preferences_get_default_iso();
GtkToolbarStyle preferences_get_toolbar_style();

gchar *preferences_get_string(const gchar *key);
gboolean preferences_get_bool(const gchar *key);
gint preferences_get_int(const gchar *key);
gboolean preferences_set_string(const gchar *key, const gchar *value);
gboolean preferences_set_bool(const gchar *key, const gboolean value);
gboolean preferences_set_int(const gchar *key, const gint value);
gboolean preferences_key_exists(const gchar *key);
GSList* preferences_get_key_subkeys(const gchar *key);
gboolean preferences_delete_key(const gchar *key);
void preferences_register_notify(const gchar *key, GConfClientNotifyFunc func);

#endif
