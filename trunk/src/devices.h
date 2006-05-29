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
 * File: devices.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Mon Feb 24 21:51:18 2003
 */

#ifndef _DEVICES_H_
#define _DEVICES_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>


/* Capabilities of devices */
static const gint DC_WRITE_CDR = 0x1;
static const gint DC_WRITE_CDRW = 0x2;
static const gint DC_WRITE_DVDR = 0x4;
static const gint DC_WRITE_DVDRAM = 0x8;


gboolean devices_probe_busses();
gboolean devices_init();
void devices_populate_optionmenu(GtkWidget *option_menu, const gchar *default_select, const gboolean add_writers_only);
gchar *devices_get_device_config(const gchar *device_key, const gchar *device_item);
void devices_write_device_to_gconf(const gint device_number, const gchar *device_name,
	const gchar *device_id, const gchar *device_node, const gchar *mount_point,
	const gint capabilities);
gboolean devices_mount_device(const gchar *device_key, gchar* *mount_point);
void devices_unmount_device(const gchar *device_key);
void devices_save_optionmenu(GtkOptionMenu *option_menu, const gchar *device_key);
gboolean devices_eject_disk(const gchar *device_key);
gint devices_prompt_for_disk(GtkWindow *parent, const gchar *device_key);
gint devices_prompt_for_blank_disk(GtkWindow *parent, const gchar *device_key);
gboolean devices_reader_is_also_writer();

#endif	/*_DEVICES_H_*/
