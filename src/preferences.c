/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * File: preferences.c
 * Copyright: luke_biddell@yahoo.com
 * Created on: Wed Jan 22 23:45:46 2003
 */

#include "preferences.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include "gbcommon.h"


GConfClient* gconf_client = NULL;
	

gboolean
preferences_init()
{
	GB_LOG_FUNC
	
	gboolean ok = FALSE;	
	
	gconf_client = gconf_client_get_default();
	if(gconf_client == NULL) 
	{
		g_critical(_("Failed to initialise gconf"));
	}
	else
	{
		gchar* tempdir = preferences_get_string(GB_TEMP_DIR);
		if(tempdir == NULL)
		{		
			tempdir = g_build_filename(g_get_tmp_dir(), "GnomeBaker", g_get_user_name(), NULL);		
					
			/* set up default values */			
			preferences_set_string(GB_READER, "Device01");
			preferences_set_string(GB_WRITER, "Device01");			
			preferences_set_int(GB_WRITE_SPEED, 4);
			preferences_set_bool(GB_DUMMY, FALSE);
			preferences_set_bool(GB_EJECT, TRUE);
			preferences_set_string(GB_TEMP_DIR, tempdir);
			preferences_set_bool(GB_FAST_BLANK, FALSE);
			preferences_set_bool(GB_BURNFREE, FALSE);
			preferences_set_bool(GB_SHOWHIDDEN, FALSE);
			preferences_set_bool(GB_SHOWHUMANSIZE, TRUE);
			preferences_set_bool(GB_CLEANTEMPDIR, TRUE);
			preferences_set_bool(GB_CREATEISOONLY, FALSE);
			preferences_set_string(GB_WRITE_MODE, "default");
			preferences_set_bool(GB_ALWAYS_SCAN, TRUE);
			preferences_set_bool(GB_SHOW_FILE_BROWSER, TRUE);
		}		
		
		gbcommon_mkdir(tempdir);
		g_free(tempdir);			
		ok = TRUE;
	}
		
	return ok;
}


gchar* 
preferences_get_copy_data_cd_image()
{
	GB_LOG_FUNC
	gchar* tempdir = preferences_get_string(GB_TEMP_DIR);
	gchar* ret = g_build_filename(tempdir, "gnomebaker_copy_data_cd.iso", NULL);
	g_free(tempdir);
	return ret;
}


gchar* 
preferences_get_create_data_cd_image()
{
	GB_LOG_FUNC
	gchar* tempdir = preferences_get_string(GB_TEMP_DIR);
	gchar* ret = g_build_filename(tempdir, "gnomebaker_create_data_cd.iso", NULL);
	g_free(tempdir);
	return ret;
}


gchar* 
preferences_get_convert_audio_track_dir()
{
	GB_LOG_FUNC
	gchar* tempdir = preferences_get_string(GB_TEMP_DIR);
	gchar* path = g_build_filename(tempdir, "create_audiocd", NULL);
	gbcommon_mkdir(path);
	g_free(tempdir);
	return path;
}


GtkToolbarStyle  
preferences_get_toolbar_style()
{
	GB_LOG_FUNC
	GtkToolbarStyle ret = GTK_TOOLBAR_BOTH;
	
	gchar* style = gconf_client_get_string(gconf_client, GNOME_TOOLBAR_STYLE, NULL);
	g_return_val_if_fail(style != NULL, ret);		
			
	if(g_ascii_strcasecmp(style, "both-horiz") == 0)
		ret = GTK_TOOLBAR_BOTH_HORIZ;
	else if(g_ascii_strcasecmp(style, "icons") == 0)
		ret = GTK_TOOLBAR_ICONS;
	else if(g_ascii_strcasecmp(style, "text") == 0)
		ret = GTK_TOOLBAR_TEXT;
	else if(g_ascii_strcasecmp(style, "both") == 0)
		ret = GTK_TOOLBAR_BOTH;
	
	GB_TRACE( _("toolbar style [%s] [%d]"), style, ret);
	g_free(style);
	
	return ret;
}


gchar* 
preferences_get_string(const gchar* key)
{
	GB_LOG_FUNC
	g_return_val_if_fail(key != NULL, NULL);
	return gconf_client_get_string(gconf_client, key, NULL);
}


gboolean 
preferences_get_bool(const gchar* key)
{
	GB_LOG_FUNC
	g_return_val_if_fail(key != NULL, FALSE);
	return gconf_client_get_bool(gconf_client, key, NULL);
}


gint 
preferences_get_int(const gchar* key)
{
	GB_LOG_FUNC
	g_return_val_if_fail(key != NULL, -1);
	return gconf_client_get_int(gconf_client, key, NULL);
}


gboolean 
preferences_set_string(const gchar* key, const gchar* value)
{
	GB_LOG_FUNC
	g_return_val_if_fail(key != NULL, FALSE);
	return gconf_client_set_string(gconf_client, key, value == NULL ? "" : value, NULL);
}


gboolean 
preferences_set_bool(const gchar* key, const gboolean value)
{
	GB_LOG_FUNC
	g_return_val_if_fail(key != NULL, FALSE);
	return gconf_client_set_bool(gconf_client, key, value, NULL);
}


gboolean 
preferences_set_int(const gchar* key, const gint value)
{
	GB_LOG_FUNC
	g_return_val_if_fail(key != NULL, FALSE);
	return gconf_client_set_int(gconf_client, key, value, NULL);	
}


gboolean 
preferences_key_exists(const gchar* key)
{
	GB_LOG_FUNC
	g_return_val_if_fail(key != NULL, FALSE);	
	return gconf_client_dir_exists(gconf_client, key, NULL);
}


GSList* 
preferences_get_key_subkeys(const gchar* key)
{
	GB_LOG_FUNC
	g_return_val_if_fail(key != NULL, NULL);	
	return gconf_client_all_dirs(gconf_client, key, NULL);
}


gboolean 
preferences_delete_key(const gchar* key)
{
	GB_LOG_FUNC
	g_return_val_if_fail(key != NULL, FALSE);	
	return gconf_client_unset(gconf_client, key, NULL);
}


void
preferences_register_notify(const gchar* key, GConfClientNotifyFunc func)
{
	GB_LOG_FUNC
	g_return_if_fail(key != NULL);	
	g_return_if_fail(func != NULL);	
	gconf_client_add_dir(gconf_client, key, GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add(gconf_client, key, func, NULL, NULL, NULL);	
}
