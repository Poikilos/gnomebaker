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
 * File: devices.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Mon Feb 24 21:51:18 2003
 */


#include "devices.h"
#include "exec.h"
#include <string.h>
#include "gbcommon.h"
#include <glib/gprintf.h>
#include "preferences.h"
#include "gnomebaker.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__sun)
#include <sys/cdio.h>
#define CDROM_DRIVE_STATUS     0x5326
#define CDS_NO_DISC        1
#define CDS_TRAY_OPEN      2
#define CDS_DRIVE_NOT_READY    3
#define CDS_DISC_OK        4
#define CDSL_CURRENT       ((int) (~0U>>1))
#else
#include <linux/cdrom.h>
#endif


gint device_addition_index = 0;


gboolean
devices_init()
{
	GB_LOG_FUNC

	gboolean ok = TRUE;

	gchar* device = g_strdup_printf(GB_DEVICE_KEY, 1);
	if(!preferences_key_exists(device) || preferences_get_bool(GB_ALWAYS_SCAN))
		ok = devices_probe_busses();

	g_free(device);

	return ok;
}


void
devices_write_device_to_gconf(const gint device_number, const gchar *device_name,
	const gchar *device_id, const gchar *device_node, const gchar *mount_point,
	const gint capabilities)
{
	GB_LOG_FUNC
	gchar *device_name_key = g_strdup_printf(GB_DEVICE_NAME, device_number);
	gchar *device_id_key = g_strdup_printf(GB_DEVICE_ID, device_number);
	gchar *device_node_key = g_strdup_printf(GB_DEVICE_NODE, device_number);
	gchar *device_mount_key = g_strdup_printf(GB_DEVICE_MOUNT, device_number);
	gchar *device_capabilities_key = g_strdup_printf(GB_DEVICE_CAPABILITIES, device_number);

	preferences_set_string(device_name_key, device_name);
	preferences_set_string(device_id_key, device_id);
	preferences_set_string(device_node_key, device_node);
	preferences_set_string(device_mount_key, mount_point);
	preferences_set_int(device_capabilities_key, capabilities);

	g_free(device_name_key);
	g_free(device_id_key);
	g_free(device_node_key);
	g_free(device_mount_key);
	g_free(device_capabilities_key);
	GB_TRACE("devices_write_device_to_gconf - Added [%s] [%s] [%s] [%s]\n",
		  device_name, device_id, device_node, mount_point);
}


void
devices_add_device(const gchar *device_name, const gchar *device_id,
				   const gchar *device_node, const gint capabilities)
{
	GB_LOG_FUNC
	g_return_if_fail(device_name != NULL);
	g_return_if_fail(device_id != NULL);
	g_return_if_fail(device_node != NULL);
	gchar *mount_point = NULL;

	/* Look for the device in /etc/fstab */
	gchar* *fstab = gbcommon_get_file_as_list("/etc/fstab");
	gchar **line = fstab;
	while((line != NULL) && (*line != NULL))
	{
		g_strstrip(*line);
		if((*line)[0] != '#') /* ignore commented out lines */
		{
			gchar node[64], mount[64];
			if(sscanf(*line, "%s\t%s", node, mount) == 2)
			{
				GB_TRACE("devices_add_device - node [%s] mount [%s]\n", node, mount);
				if(g_ascii_strcasecmp(node, device_node) == 0)
				{
					mount_point = g_strdup(mount);
				}
				else
				{
					/* try to resolve the device_node in case it's a
						symlink to the device we are looking for */
					gchar *link_target = g_new0(gchar, PATH_MAX);
					realpath(node, link_target);
					if(g_ascii_strcasecmp(link_target, device_node) == 0)
					{
						GB_TRACE("devices_add_device - node [%s] is link to [%s]\n", node, link_target);
						mount_point = g_strdup(mount);
					}
					g_free(link_target);
				}

				if(mount_point != NULL)
					break;
			}
		}
		++line;
	}

	g_strfreev(fstab);

	++device_addition_index;
	devices_write_device_to_gconf(device_addition_index, device_name, device_id,
		  device_node, mount_point, capabilities);

	g_free(mount_point);
}


gchar*
devices_get_device_config(const gchar *device_key, const gchar *device_item)
{
	GB_LOG_FUNC
	g_return_val_if_fail(device_key != NULL, NULL);

	gchar *device_item_value = NULL;
	gchar *device_key_label = preferences_get_string(device_key);
	if(device_key_label != NULL);
	{
		gchar *device_item_key = g_strconcat(GB_DEVICES_KEY, "/", device_key_label, device_item, NULL);
		device_item_value = preferences_get_string(device_item_key);
		g_free(device_item_key);
		g_free(device_key_label);
	}

	return device_item_value;
}


void
devices_populate_optionmenu(GtkWidget *option_menu, const gchar *device_key, const gboolean add_writers_only)
{
	GB_LOG_FUNC
	g_return_if_fail(option_menu != NULL);
	g_return_if_fail(device_key != NULL);

	gchar *default_select = preferences_get_string(device_key);

	GtkWidget *menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
	if(menu != NULL)
		gtk_widget_destroy(menu);
	menu = gtk_menu_new();
	gtk_widget_show(menu);

	gint index = 0, history = 0;
	GSList *devices = preferences_get_key_subkeys(GB_DEVICES_KEY);
	GSList *item = devices;
	for(; item != NULL; item = item->next)
	{
		gchar *device_key = (gchar*)item->data;
		gchar *device_name_key = g_strconcat(device_key, GB_DEVICE_NAME_LABEL, NULL);
		gchar *device_name = preferences_get_string(device_name_key);
        gchar *device_capabilities_key = g_strconcat(device_key, GB_DEVICE_CAPABILITIES_LABEL, NULL);
        const gint capabilities = preferences_get_int(device_capabilities_key);
        /* Check the capabilities of the device and make sure that, if we are only adding
         * writers to the option menu, the device can actually write disks */
		if(device_name != NULL && (!add_writers_only ||
                (capabilities & DC_WRITE_CDR || capabilities & DC_WRITE_CDRW ||
                capabilities & DC_WRITE_DVDR || capabilities & DC_WRITE_DVDRAM)))
		{
			GtkWidget *menu_item = gtk_menu_item_new_with_label(device_name);
			gtk_widget_show(menu_item);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

			if(default_select != NULL)
			{
				gchar *device_key_id = g_strrstr(device_key, default_select);
				if(device_key_id != NULL)
					history = index;
			}
			g_free(device_name);
		}

        g_free(device_capabilities_key);
		g_free(device_key);
		g_free(device_name_key);
		++index;
	}

	g_slist_free(devices);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), history);

	g_free(default_select);
}


void
devices_save_optionmenu(GtkOptionMenu *option_menu, const gchar *device_key)
{
	GB_LOG_FUNC
	g_return_if_fail(option_menu != NULL);
	g_return_if_fail(device_key != NULL);

	gint index = gtk_option_menu_get_history(option_menu);
	gchar *device = g_strdup_printf(GB_DEVICE_FORMAT, index + 1);
	preferences_set_string(device_key, device);
	g_free(device);
}


void
devices_clear_devicedata()
{
	GB_LOG_FUNC

	device_addition_index = 0;

	GSList *devices = preferences_get_key_subkeys(GB_DEVICES_KEY);
	GSList *item = devices;
	for(;item != NULL; item = item->next)
	{
		gchar *device_key = (gchar*)item->data;
		gchar *device_name_key = g_strconcat(device_key, GB_DEVICE_NAME_LABEL, NULL);
		preferences_delete_key(device_name_key);
		gchar *device_id_key = g_strconcat(device_key, GB_DEVICE_ID_LABEL, NULL);
		preferences_delete_key(device_id_key);
		gchar *device_node_key = g_strconcat(device_key, GB_DEVICE_NODE_LABEL, NULL);
		preferences_delete_key(device_node_key);
		gchar *device_mount_key = g_strconcat(device_key, GB_DEVICE_MOUNT_LABEL, NULL);
		preferences_delete_key(device_mount_key);
		gchar *device_capabilities_key = g_strconcat(device_key, GB_DEVICE_CAPABILITIES_LABEL, NULL);
		preferences_delete_key(device_capabilities_key);

		preferences_delete_key(device_key);

		g_free(device_id_key);
		g_free(device_name_key);
		g_free(device_node_key);
		g_free(device_mount_key);
		g_free(device_capabilities_key);
		g_free(device_key);
	}

	g_slist_free(devices);
}


gboolean
devices_parse_cdrecord_output(const gchar *buffer, const gchar *bus_name)
{
	GB_LOG_FUNC
	g_return_val_if_fail(buffer != NULL, FALSE);
	g_return_val_if_fail(bus_name != NULL, FALSE);
	gboolean ok = TRUE;

	gchar **lines = g_strsplit(buffer, "\n", 0);
	gchar **line = lines;
	while(*line != NULL)
	{
		/*	Ignore stuff like my camera which cdrecord detects...
		'OLYMPUS ' 'D-230           ' '1.00' Removable Disk */
		if(strstr(*line, "Removable Disk") == NULL)
		{
			gchar vendor[9], model[17], device_id[6];

			if(sscanf(*line, "\t%5c\t  %*d) '%8c' '%16c'", device_id, vendor, model) == 3)
			{
				vendor[8] = '\0';
				model[16] = '\0';
				device_id[5] = '\0';

				gchar *device = NULL;

				/* Copy the bus id stuff ie 0,0,0 to the device struct.
				   If the bus is NULL it's SCSI  */
				if(g_ascii_strncasecmp(bus_name, "SCSI", 4) == 0)
					device = g_strdup(device_id);
				else if(g_ascii_strncasecmp(bus_name, "/dev", 4) == 0)
					device = g_strdup(bus_name);
				else
					device = g_strconcat(bus_name, ":", device_id, NULL);

				gchar *displayname = g_strdup_printf("%s %s", g_strstrip(vendor),
					   g_strstrip(model));

				devices_add_device(displayname, device, "", 0);

				g_free(displayname);
				g_free(device);
			}
		}
		++line;
	}

	g_strfreev (lines);

	return ok;
}
/*
gboolean
devices_parse_cdrecord_max_speed(const gchar *buffer, const gchar *bus_name)
{
	GB_LOG_FUNC
	g_return_val_if_fail(buffer != NULL, FALSE);
	g_return_val_if_fail(bus_name != NULL, FALSE);
	gboolean ok = TRUE;

	gchar **lines = g_strsplit(buffer, "\n", 0);
	gchar **line = lines;
	while(*line != NULL)
	{

		const gchar *maxspeed = strstr(buf, "Maximum write speed:");
		if(maxspeed != NULL)
		{
			gint maxwritespeed = 0;
			if(sscanf(*line, "%*s %d)", &maxwritespeed) == 1)
			{

			}
		}
		++line;
	}

	g_strfreev (lines);

	return ok;
}
*/

gboolean
devices_probe_bus(const gchar *bus)
{
	GB_LOG_FUNC
	gboolean ok = FALSE;
	g_return_val_if_fail(bus != NULL, FALSE);

	gchar command[32] = "cdrecord -scanbus";
	if(g_ascii_strncasecmp(bus, "SCSI", 4) != 0)
	{
		strcat(command, " dev=");
		strcat(command, bus);
	}

	gchar *buffer = NULL;
    exec_run_cmd(command, &buffer);
	if(buffer == NULL)
		g_warning("devices_probe_bus - Failed to scan the scsi bus");
	else if(!devices_parse_cdrecord_output(buffer, bus))
		g_warning("devices_probe_bus - failed to parse cdrecord output");
	else
		ok = TRUE;
	g_free(buffer);

	return ok;
}


void
devices_get_ide_device(const gchar *device_node, const gchar *device_node_path,
					   gchar **model_name, gchar **device_id)
{
	GB_LOG_FUNC
	g_return_if_fail(device_node != NULL);
	g_return_if_fail(model_name != NULL);
	g_return_if_fail(device_id != NULL);
	GB_TRACE("devices_get_ide_device - probing [%s]\n", device_node);
	gchar *contents = NULL;
	gchar *file = g_strdup_printf("/proc/ide/%s/model", device_node);
	if(g_file_get_contents(file, &contents, NULL, NULL))
	{
		g_strstrip(contents);
		*model_name = g_strdup(contents);
		*device_id = g_strdup(device_node_path);
		g_free(contents);
	}
	else
	{
		g_warning("devices_get_ide_device - Failed to open %s", file);
	}
	g_free(file);
}


void
devices_get_scsi_device(const gchar *device_node, const gchar *device_node_path,
						gchar **model_name, gchar **device_id)
{
	GB_LOG_FUNC
	g_return_if_fail(device_node != NULL);
	g_return_if_fail(model_name != NULL);
	g_return_if_fail(device_id != NULL);
	GB_TRACE("devices_add_scsi_device - probing [%s]\n", device_node);

	gchar **device_strs = NULL, **devices = NULL;
	if((devices = gbcommon_get_file_as_list("/proc/scsi/sg/devices")) == NULL)
	{
		g_warning("devices_get_scsi_device - Failed to open /proc/scsi/sg/devices");
	}
	else if((device_strs = gbcommon_get_file_as_list("/proc/scsi/sg/device_strs")) == NULL)
	{
		g_warning("devices_get_scsi_device - Failed to open /proc/scsi/sg/device_strs");
	}
	else
	{
		const gint scsicdromnum = atoi(&device_node[strlen(device_node) - 1]);
		gint cddevice = 0;
		gchar **device = devices;
		gchar **device_str = device_strs;
		while((*device != NULL) && (*device_str) != NULL)
		{
			if((strcmp(*device, "<no active device>") != 0) && (strlen(*device) > 0))
			{
				gint scsihost, scsiid, scsilun, scsitype;
				if(sscanf(*device, "%d\t%*d\t%d\t%d\t%d",
					   &scsihost, &scsiid, &scsilun, &scsitype) != 4)
				{
					g_warning("devices_get_scsi_device - Error reading scsi information from /proc/scsi/sg/devices");
				}
				/* 5 is the magic number according to lib-nautilus-burn */
				else if(scsitype == 5)
				{
					/* is the device the one we are looking for */
					if(cddevice == scsicdromnum)
					{
						gchar vendor[9], model[17];
						if(sscanf(*device_str, "%8c\t%16c", vendor, model) == 2)
						{
							vendor [8] = '\0';
							g_strstrip(vendor);

							model [16] = '\0';
							g_strstrip(model);

							*model_name = g_strdup_printf("%s %s", vendor, model);
							*device_id = g_strdup_printf("%d,%d,%d", scsihost, scsiid, scsilun);
							break;
						}
					}
					++cddevice;
				}
			}
			++device_str;
			++device;
		}
	}

	g_strfreev(devices);
	g_strfreev(device_strs);
}


void
devices_for_each(gpointer key, gpointer value, gpointer user_data)
{
	GB_TRACE("---- key [%s], value [%s]\n", (gchar*)key, (gchar*)value);
	g_free(key);
	g_free(value);
}


GHashTable*
devices_get_cdrominfo(gchar **proccdrominfo, gint deviceindex)
{
	GB_LOG_FUNC
	g_return_val_if_fail(proccdrominfo != NULL, NULL);
	g_return_val_if_fail(deviceindex >= 1, NULL);

	GB_TRACE("devices_get_cdrominfo - looking for device [%d]\n", deviceindex);
	GHashTable *ret = NULL;
	gchar **info = proccdrominfo;
	while(*info != NULL)
	{
		g_strstrip(*info);
		if(strlen(*info) > 0)
		{
			if(strstr(*info, "drive name:") != NULL)
				ret = g_hash_table_new(g_str_hash, g_str_equal);

			if(ret != NULL)
			{
				gint columnindex = 0;
				gchar *key = NULL;
				gchar **columns = g_strsplit_set(*info, "\t", 0);
				gchar **column = columns;
				while(*column != NULL)
				{
					g_strstrip(*column);
					if(strlen(*column) > 0)
					{
						if(columnindex == 0)
							key = *column;
						else if(columnindex == deviceindex)
							g_hash_table_insert(ret, g_strdup(key), g_strdup(*column));
						++columnindex;
					}
					++column;
				}

				/* We must check if we found the device index we were
				 looking for */
				if(columnindex <= deviceindex)
				{
					GB_TRACE("devices_get_cdrominfo - Requested device index [%d] is out of bounds. "
						  "All devices have been read.\n", deviceindex);
					g_hash_table_destroy(ret);
					ret = NULL;
					break;
				}

				g_strfreev(columns);
			}
		}
		++info;
	}

	return ret;
}


gboolean
devices_probe_busses()
{
	GB_LOG_FUNC

	gboolean ok = FALSE;

	devices_clear_devicedata();

#ifdef __linux__

	gchar **info = NULL;
	if((info = gbcommon_get_file_as_list("/proc/sys/dev/cdrom/info")) == NULL)
	{
		g_warning("devices_probe_busses - Failed to open /proc/sys/dev/cdrom/info");
	}
	else
	{
		gint devicenum = 1;
		GHashTable *devinfo = NULL;
		while((devinfo = devices_get_cdrominfo(info, devicenum)) != NULL)
		{
			const gchar *device = g_hash_table_lookup(devinfo, "drive name:");
			gchar *device_node_path = g_strdup_printf("/dev/%s", device);

			gchar *model_name = NULL, *device_id = NULL;

			if(device[0] == 'h')
				devices_get_ide_device(device, device_node_path, &model_name, &device_id);
			else
				devices_get_scsi_device(device, device_node_path, &model_name, &device_id);

			gint capabilities = 0;
			if(g_ascii_strcasecmp(g_hash_table_lookup(devinfo, "Can write CD-R:"), "1") == 0)
				capabilities |= DC_WRITE_CDR;
			if(g_ascii_strcasecmp(g_hash_table_lookup(devinfo, "Can write CD-RW:"), "1") == 0)
				capabilities |= DC_WRITE_CDRW;
			if(g_ascii_strcasecmp(g_hash_table_lookup(devinfo, "Can write DVD-R:"), "1") == 0)
				capabilities |= DC_WRITE_DVDR;
			if(g_ascii_strcasecmp(g_hash_table_lookup(devinfo, "Can write DVD-RAM:"), "1") == 0)
				capabilities |= DC_WRITE_DVDRAM;

			devices_add_device(model_name, device_id, device_node_path, capabilities);

			g_free(model_name);
			g_free(device_id);
			g_free(device_node_path);
			g_hash_table_foreach(devinfo, devices_for_each, NULL);
			g_hash_table_destroy(devinfo);
			devinfo = NULL;
			++devicenum;
		}
	}

	g_strfreev(info);

#else

	devices_probe_bus("SCSI");
	devices_probe_bus("ATAPI");
	devices_probe_bus("ATA");
	ok = TRUE;

#endif

	return ok;
}


void
devices_unmount_device(const gchar *device_key)
{
    GB_LOG_FUNC
    g_return_if_fail(device_key != NULL);

    gchar *node = devices_get_device_config(device_key, GB_DEVICE_NODE_LABEL);
    gchar *mount_cmd = g_strdup_printf("umount %s", node);
    gchar *output = NULL;
    exec_run_cmd(mount_cmd, &output);
    g_free(output);
    g_free(mount_cmd);
    g_free(node);
}


gboolean
devices_mount_device(const gchar *device_key, gchar* *mount_point)
{
	GB_LOG_FUNC
	g_return_val_if_fail(device_key != NULL, FALSE);
    g_return_val_if_fail(mount_point != NULL, FALSE);
	gboolean ok = FALSE;

	gchar *mount = devices_get_device_config(device_key, GB_DEVICE_MOUNT_LABEL);
	if((mount == NULL) || (strlen(mount) == 0))
	{
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
                _("The mount point (e.g. /mnt/cdrom) for the writing device could not be obtained. "
                "Please check that the writing device has an entry in /etc/fstab and then go "
                "to preferences and rescan for devices."));
	}
	else
	{
		gchar *mount_cmd = g_strdup_printf("mount %s", mount);
		gchar *output = NULL;
		if(exec_run_cmd(mount_cmd, &output) != 0)
		{
			gchar *message = g_strdup_printf(_("Error mounting %s.\n\n%s"),
	               mount, output != NULL ? output : _("unknown error"));
			gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
			g_free(message);
		}
		else
		{
			ok = TRUE;
    		*mount_point = g_strdup(mount);
		}
		g_free(output);
		g_free(mount_cmd);
	}
	g_free(mount);

	return ok;
}


gboolean
devices_eject_disk(const gchar *device_key)
{
    GB_LOG_FUNC
    g_return_val_if_fail(device_key != NULL, FALSE);

	/* from http://leapster.org/linux/cdrom/ */
	gboolean ret = FALSE;
	gchar *device = devices_get_device_config(device_key,GB_DEVICE_NODE_LABEL);
	GB_TRACE("devices_eject_disk - Ejecting media in [%s]\n",device);
    int cdrom = open(device,O_RDONLY | O_NONBLOCK);
    g_free(device);
	if(cdrom < 0)
	{
        g_warning("devices_eject_disk - Error opening device %s",device);
   	}
    else
    {
        /* Use ioctl to send the CDROMEJECT (CDIOCEJECT on FreeBSD) command to the device */
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
        if(ioctl(cdrom, CDIOCEJECT, 0) < 0)
            ret = TRUE;
        else
            g_warning("devices_eject_disk - ioctl failed");
#else
        if(ioctl(cdrom, CDROMEJECT, 0) < 0)
            g_warning("devices_eject_disk - ioctl failed");
        else
            ret = TRUE;
#endif
    }
	close(cdrom);
	return ret;
}
/*
gboolean
devices_get_max_speed_for_drive(const gchar *drive)
{
	GB_LOG_FUNC
	gboolean ok = FALSE;
	g_return_val_if_fail(bus != NULL, FALSE);

	gchar command[32] = "cdrecord -prcap";
	if(g_ascii_strncasecmp(bus, "SCSI", 4) != 0)
	{
		strcat(command, " dev=");
		strcat(command, bus);
	}

	GString *buffer = exec_run_cmd(command);
	if(buffer == NULL)
		g_warning("devices_get_max_speed_for_drive - Failed to scan the scsi bus");
	else if(!devices_parse_cdrecord_output(buffer->str, bus))
		g_warning("devices_get_max_speed_for_drive - failed to parse cdrecord output");
	else
		ok = TRUE;

	g_string_free(buffer, TRUE);

	return ok;
}
*/

static gboolean
devices_is_disk_inserted(const gchar *device_key)
{
    GB_LOG_FUNC
	g_return_val_if_fail(device_key != NULL, FALSE);

	gboolean ret_val = FALSE;
	gchar *device = devices_get_device_config(device_key,GB_DEVICE_NODE_LABEL);
	int fd = open(device, O_RDONLY | O_NONBLOCK);
    g_free(device);
    const int ret = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    close(fd);
    if (ret == -1)
	{
		g_warning("devices_is_disk_inserted - ioctl failed");
    }
    else
    {
    	switch (ret)
    	{
        	case CDS_NO_DISC:
    			ret_val = FALSE;
    				break;
    		case CDS_TRAY_OPEN:
    			ret_val = FALSE;
    				break;
    		case CDS_DRIVE_NOT_READY:
    			ret_val = FALSE;
    				break;
    		case CDS_DISC_OK:
    			ret_val = TRUE;
    				break;
            default:
                ret_val = FALSE;
    	}
    }
	return ret_val;
}


gint
devices_prompt_for_disk(GtkWindow *parent, const gchar *device_key)
{
    GB_LOG_FUNC
    g_return_val_if_fail(device_key != NULL, GTK_RESPONSE_CANCEL);

    gchar *device_name = devices_get_device_config(device_key, GB_DEVICE_NAME_LABEL);
    gchar *message = g_strdup_printf(_("Please insert a disk into the %s"), device_name);
    gint ret = GTK_RESPONSE_OK;
    /*while(!devices_is_disk_inserted(device_key) && (ret == GTK_RESPONSE_OK))*/
    if(!devices_is_disk_inserted(device_key))
    {
        devices_eject_disk(device_key);
        ret = gnomebaker_show_msg_dlg(parent, GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE, message);
    }
    g_free(device_name);
    g_free(message);
    return ret;
}


gboolean
devices_reader_is_also_writer()
{
    GB_LOG_FUNC
    gchar *reader = devices_get_device_config(GB_READER, GB_DEVICE_NODE_LABEL);
    gchar *writer = devices_get_device_config(GB_WRITER, GB_DEVICE_NODE_LABEL);
    gboolean ret = (g_ascii_strcasecmp(reader, writer) == 0);
    g_free(reader);
    g_free(writer);
    return ret;
}



