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

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
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


gint deviceadditionindex = 0;


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
devices_write_device_to_gconf(const gint devicenumber, const gchar* devicename, 
	const gchar* deviceid, const gchar* devicenode, const gchar* mountpoint,
	const gint capabilities)
{
	GB_LOG_FUNC
	gchar* devicenamekey = g_strdup_printf(GB_DEVICE_NAME, devicenumber);			
	gchar* deviceidkey = g_strdup_printf(GB_DEVICE_ID, devicenumber);	
	gchar* devicenodekey = g_strdup_printf(GB_DEVICE_NODE, devicenumber);	
	gchar* devicemountkey = g_strdup_printf(GB_DEVICE_MOUNT, devicenumber);	
	gchar* devicecapabilitieskey = g_strdup_printf(GB_DEVICE_CAPABILITIES, devicenumber);	
	
	preferences_set_string(devicenamekey, devicename);
	preferences_set_string(deviceidkey, deviceid);
	preferences_set_string(devicenodekey, devicenode);
	preferences_set_string(devicemountkey, mountpoint);
	preferences_set_int(devicecapabilitieskey, capabilities);
	
	g_free(devicenamekey);
	g_free(deviceidkey);
	g_free(devicenodekey);
	g_free(devicemountkey);
	g_free(devicecapabilitieskey);
	GB_TRACE("devices_write_device_to_gconf - Added [%s] [%s] [%s] [%s]", 
		devicename, deviceid, devicenode, mountpoint);
}


void
devices_add_device(const gchar* devicename, const gchar* deviceid, 
				   const gchar* devicenode, const gint capabilities)
{
	GB_LOG_FUNC
	g_return_if_fail(devicename != NULL);
	g_return_if_fail(deviceid != NULL);
	g_return_if_fail(devicenode != NULL);
	gchar* mountpoint = NULL;
		
	/* Look for the device in /etc/fstab */
	gchar** fstab = gbcommon_get_file_as_list("/etc/fstab");
	gchar** line = fstab;
	while((line != NULL) && (*line != NULL))
	{
		g_strstrip(*line);
		if((*line)[0] != '#') /* ignore commented out lines */
		{
			gchar node[64], mount[64];
			if(sscanf(*line, "%s\t%s", node, mount) == 2)
			{
				GB_TRACE("node [%s] mount [%s]", node, mount);
				if(g_ascii_strcasecmp(node, devicenode) == 0)
				{
					mountpoint = g_strdup(mount);
				}
				else
				{
					/* try to resolve the devicenode in case it's a
						symlink to the device we are looking for */					
					gchar* linktarget = g_new0(gchar, PATH_MAX);
					realpath(node, linktarget);					
					if(g_ascii_strcasecmp(linktarget, devicenode) == 0)
					{					
						GB_TRACE("node [%s] is link to [%s]", node, linktarget);
						mountpoint = g_strdup(mount);
					}
					g_free(linktarget);
				}
				
				if(mountpoint != NULL)
					break;
			}
		}
		++line;
	}

	g_strfreev(fstab);

	++deviceadditionindex;	
	devices_write_device_to_gconf(deviceadditionindex, devicename, deviceid,
		devicenode, mountpoint, capabilities);
	
	g_free(mountpoint);
}


gchar* 
devices_get_device_config(const gchar* devicekey, const gchar* deviceitem)
{
	GB_LOG_FUNC
	g_return_val_if_fail(devicekey != NULL, NULL);
	
	gchar* deviceitemvalue = NULL;
	gchar* devicekeylabel = preferences_get_string(devicekey);
	if(devicekeylabel != NULL);
	{
		gchar* devitemkey = g_strconcat(GB_DEVICES_KEY, "/", devicekeylabel, deviceitem, NULL);
		deviceitemvalue = preferences_get_string(devitemkey);		
		g_free(devitemkey);
		g_free(devicekeylabel);	
	}	
	
	return deviceitemvalue;
}


void
devices_populate_optionmenu(GtkWidget* option_menu, const gchar* devicekey)
{
	GB_LOG_FUNC
	g_return_if_fail(option_menu != NULL);
	g_return_if_fail(devicekey != NULL);
	
	gchar* defaultselect = preferences_get_string(devicekey);
																																		   
	GtkWidget* menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
	if(menu != NULL)
		gtk_widget_destroy(menu);
	menu = gtk_menu_new();
	gtk_widget_show(menu);
	
	gint index = 0, history = 0;
	GSList* devices = preferences_get_key_subkeys(GB_DEVICES_KEY);
	GSList* item = devices;	
	for(; item != NULL; item = item->next)
	{
		gchar* devicekey = (gchar*)item->data;		
		gchar* devicenamekey = g_strconcat(devicekey, GB_DEVICE_NAME_LABEL, NULL);
		gchar* devicename = preferences_get_string(devicenamekey);	
		if(devicename != NULL)
		{
			GtkWidget* menuitem = gtk_menu_item_new_with_label(devicename);
			gtk_widget_show(menuitem);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
			
			if(defaultselect != NULL)
			{
				gchar* devkeyid = g_strrstr(devicekey, defaultselect);
				if(devkeyid != NULL)
					history = index;
			}
			g_free(devicename);
		}
		
		g_free(devicekey);
		g_free(devicenamekey);		
		++index;
	}
	
	g_slist_free(devices);
	
	gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);	
	gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), history);
	
	g_free(defaultselect);
}


void 
devices_save_optionmenu(GtkOptionMenu* optmen, const gchar* devicekey)
{
	GB_LOG_FUNC
	g_return_if_fail(optmen != NULL);
	g_return_if_fail(devicekey != NULL);
	
	gint index = gtk_option_menu_get_history(optmen);
	gchar* device = g_strdup_printf(GB_DEVICE_FORMAT, index + 1);
	preferences_set_string(devicekey, device);
	g_free(device);
}


void
devices_clear_devicedata()
{
	GB_LOG_FUNC

	deviceadditionindex = 0;

	GSList* devices = preferences_get_key_subkeys(GB_DEVICES_KEY);
	GSList* item = devices;	
	for(;item != NULL; item = item->next)
	{
		gchar* devicekey = (gchar*)item->data;				
		gchar* devicenamekey = g_strconcat(devicekey, GB_DEVICE_NAME_LABEL, NULL);
		preferences_delete_key(devicenamekey);
		gchar* deviceidkey = g_strconcat(devicekey, GB_DEVICE_ID_LABEL, NULL);		
		preferences_delete_key(deviceidkey);				
		gchar* devicenodekey = g_strconcat(devicekey, GB_DEVICE_NODE_LABEL, NULL);		
		preferences_delete_key(devicenodekey);
		gchar* devicemountkey = g_strconcat(devicekey, GB_DEVICE_MOUNT_LABEL, NULL);		
		preferences_delete_key(devicemountkey);
		gchar* devicecapabilitieskey = g_strconcat(devicekey, GB_DEVICE_CAPABILITIES_LABEL, NULL);		
		preferences_delete_key(devicecapabilitieskey);
		
		preferences_delete_key(devicekey);
		
		g_free(deviceidkey);
		g_free(devicenamekey);		
		g_free(devicenodekey);
		g_free(devicemountkey);
		g_free(devicecapabilitieskey);
		g_free(devicekey);		
	}
	
	g_slist_free(devices);		
}


gboolean
devices_parse_cdrecord_output(const gchar* buffer, const gchar* busname)
{
	GB_LOG_FUNC
	g_return_val_if_fail(buffer != NULL, FALSE);
	g_return_val_if_fail(busname != NULL, FALSE);
	gboolean ok = TRUE;		
		
	gchar** lines = g_strsplit(buffer, "\n", 0);
	gchar** line = lines;
	while(*line != NULL)
	{
		/*	Ignore stuff like my camera which cdrecord detects...
		'OLYMPUS ' 'D-230           ' '1.00' Removable Disk */
		if(strstr(*line, "Removable Disk") == NULL)
		{
			gchar vendor[9], model[17], deviceid[6];
			
			if(sscanf(*line, "\t%5c\t  %*d) '%8c' '%16c'", deviceid, vendor, model) == 3) 
			{
				vendor[8] = '\0'; 
				model[16] = '\0';
				deviceid[5] = '\0';								
				
				gchar* device = NULL;
		
				/* Copy the bus id stuff ie 0,0,0 to the device struct. 
				   If the bus is NULL it's SCSI  */
				if(g_ascii_strncasecmp(busname, "SCSI", 4) == 0)
					device = g_strdup(deviceid);
				else if(g_ascii_strncasecmp(busname, "/dev", 4) == 0)
					device = g_strdup(busname);
				else
					device = g_strconcat(busname, ":", deviceid, NULL);
				
				gchar* displayname = g_strdup_printf("%s %s", g_strstrip(vendor), 
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
devices_parse_cdrecord_max_speed(const gchar* buffer, const gchar* busname)
{
	GB_LOG_FUNC
	g_return_val_if_fail(buffer != NULL, FALSE);
	g_return_val_if_fail(busname != NULL, FALSE);
	gboolean ok = TRUE;		
		
	gchar** lines = g_strsplit(buffer, "\n", 0);
	gchar** line = lines;
	while(*line != NULL)
	{
		
		const gchar* maxspeed = strstr(buf, "Maximum write speed:");
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
devices_probe_bus(const gchar* bus)
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
	
	gchar* buffer = NULL;
    exec_run_cmd(command, &buffer);
	if(buffer == NULL)
		g_critical("devices_probe_bus - Failed to scan the scsi bus");
	else if(!devices_parse_cdrecord_output(buffer, bus))
		g_critical("devices_probe_bus - failed to parse cdrecord output");
	else
		ok = TRUE;	
	g_free(buffer);

	return ok;
}


void 
devices_get_ide_device(const gchar* devicenode, const gchar* devicenodepath,
					   gchar** modelname, gchar** deviceid)
{
	GB_LOG_FUNC
	g_return_if_fail(devicenode != NULL);	
	g_return_if_fail(modelname != NULL);
	g_return_if_fail(deviceid != NULL);
	GB_TRACE("devices_get_ide_device - probing [%s]", devicenode);
	gchar* contents = NULL;
	gchar* file = g_strdup_printf("/proc/ide/%s/model", devicenode);
	if(g_file_get_contents(file, &contents, NULL, NULL))
	{
		g_strstrip(contents);
		*modelname = g_strdup(contents);
		*deviceid = g_strdup(devicenodepath);
		g_free(contents);
	}
	else
	{
		g_critical("Failed to open %s", file);
	}
	g_free(file);
}


void 
devices_get_scsi_device(const gchar* devicenode, const gchar* devicenodepath,
						gchar** modelname, gchar** deviceid)
{
	GB_LOG_FUNC
	g_return_if_fail(devicenode != NULL);
	g_return_if_fail(modelname != NULL);
	g_return_if_fail(deviceid != NULL);
	GB_TRACE("devices_add_scsi_device - probing [%s]", devicenode);
	
	gchar **device_strs = NULL, **devices = NULL;	
	if((devices = gbcommon_get_file_as_list("/proc/scsi/sg/devices")) == NULL)
	{
		g_critical("Failed to open /proc/scsi/sg/devices");
	}
	else if((device_strs = gbcommon_get_file_as_list("/proc/scsi/sg/device_strs")) == NULL)
	{
		g_critical("Failed to open /proc/scsi/sg/device_strs");
	}
	else
	{
		const gint scsicdromnum = atoi(&devicenode[strlen(devicenode) - 1]);
		gint cddevice = 0;
		gchar** device = devices;
		gchar** device_str = device_strs;		
		while((*device != NULL) && (*device_str) != NULL)
		{
			if((strcmp(*device, "<no active device>") != 0) && (strlen(*device) > 0))
			{
				gint scsihost, scsiid, scsilun, scsitype;
				if(sscanf(*device, "%d\t%*d\t%d\t%d\t%d", 
					&scsihost, &scsiid, &scsilun, &scsitype) != 4)
				{
					g_critical("Error reading scsi information from /proc/scsi/sg/devices");
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
							
							*modelname = g_strdup_printf("%s %s", vendor, model);
							*deviceid = g_strdup_printf("%d,%d,%d", scsihost, scsiid, scsilun);
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
	GB_TRACE("---- key [%s], value [%s]", (gchar*)key, (gchar*)value);
	g_free(key);
	g_free(value);
}


GHashTable* 
devices_get_cdrominfo(gchar** proccdrominfo, gint deviceindex)
{
	GB_LOG_FUNC
	g_return_val_if_fail(proccdrominfo != NULL, NULL);
	g_return_val_if_fail(deviceindex >= 1, NULL);
	
	GB_TRACE("looking for device [%d]", deviceindex);
	GHashTable* ret = NULL;
	gchar** info = proccdrominfo;
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
				gchar* key = NULL;
				gchar** columns = g_strsplit_set(*info, "\t", 0);				
				gchar** column = columns;
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
					GB_TRACE("Requested device index [%d] is out of bounds. "
						"All devices have been read.", deviceindex);
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
		g_critical("Failed to open /proc/sys/dev/cdrom/info");
	}
	else
	{
		gint devicenum = 1;
		GHashTable* devinfo = NULL;
		while((devinfo = devices_get_cdrominfo(info, devicenum)) != NULL)
		{
			const gchar* device = g_hash_table_lookup(devinfo, "drive name:");
			gchar* devicenodepath = g_strdup_printf("/dev/%s", device);
			
			gchar *modelname = NULL, *deviceid = NULL;
			
			if(device[0] == 'h')
				devices_get_ide_device(device, devicenodepath, &modelname, &deviceid);
			else
				devices_get_scsi_device(device, devicenodepath, &modelname, &deviceid);
			
			gint capabilities = 0;
			if(g_ascii_strcasecmp(g_hash_table_lookup(devinfo, "Can write CD-R:"), "1") == 0)
				capabilities |= DC_WRITE_CDR;
			if(g_ascii_strcasecmp(g_hash_table_lookup(devinfo, "Can write CD-RW:"), "1") == 0)
				capabilities |= DC_WRITE_CDRW;
			if(g_ascii_strcasecmp(g_hash_table_lookup(devinfo, "Can write DVD-R:"), "1") == 0)
				capabilities |= DC_WRITE_DVDR;
			if(g_ascii_strcasecmp(g_hash_table_lookup(devinfo, "Can write DVD-RAM:"), "1") == 0)
				capabilities |= DC_WRITE_DVDRAM;
			
			devices_add_device(modelname, deviceid, devicenodepath, capabilities);			
			
			g_free(modelname);
			g_free(deviceid);
			g_free(devicenodepath);			
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
devices_unmount_device(const gchar* devicekey)
{
    GB_LOG_FUNC
    g_return_if_fail(devicekey != NULL);
    
    gchar* node = devices_get_device_config(devicekey, GB_DEVICE_NODE_LABEL);
    gchar* mountcmd = g_strdup_printf("umount %s", node); 
    gchar* output = NULL;
    exec_run_cmd(mountcmd, &output);
    g_free(output);
    g_free(mountcmd);
    g_free(node);
}


gboolean 
devices_mount_device(const gchar* devicekey, gchar** mountpoint)
{
	GB_LOG_FUNC
	g_return_val_if_fail(devicekey != NULL, FALSE);
    g_return_val_if_fail(mountpoint != NULL, FALSE);
	gboolean ok = FALSE;
	
	gchar* mount = devices_get_device_config(devicekey, GB_DEVICE_MOUNT_LABEL);		
	if((mount == NULL) || (strlen(mount) == 0))
	{
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE,
            _("The mount point (e.g. /mnt/cdrom) for the writing device could not be obtained. "
            "Please check that the writing device has an entry in /etc/fstab and then go "
            "to preferences and rescan for devices."));
	}
	else
	{
		gchar* mountcmd = g_strdup_printf("mount %s", mount);	
		gchar* output = NULL;
		if(exec_run_cmd(mountcmd, &output) != 0)
		{
			gchar* message = g_strdup_printf(_("Error mounting %s.\n\n%s"), 
	            mount, output != NULL ? output : _("unknown error"));
			gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, GTK_BUTTONS_NONE, message);
			g_free(message);
		}
		else
		{
			ok = TRUE;			
    		*mountpoint = g_strdup(mount);
		}		
		g_free(output);
		g_free(mountcmd);
	}
	g_free(mount);
	
	return ok;
}

	
gboolean
devices_eject_disk(const gchar* devicekey)
{
    GB_LOG_FUNC
    g_return_val_if_fail(devicekey != NULL, FALSE);
    
	/* from http://leapster.org/linux/cdrom/ */
	gboolean ret = FALSE;
	gchar *device = devices_get_device_config(devicekey,GB_DEVICE_NODE_LABEL);
	GB_TRACE("Ejecting media in %s",device);
    int cdrom = open(device,O_RDONLY | O_NONBLOCK);
    g_free(device);
	if(cdrom < 0)
	{
        g_critical("Error opening device %s",device);
   	}
    else
    {			
        /* Use ioctl to send the CDROMEJECT (CDIOCEJECT on FreeBSD) command to the device */
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
        if(ioctl(cdrom, CDIOCEJECT, 0) < 0)
            ret = TRUE;
        else 
            g_critical("devices_eject_disk - ioctl failed");
#else        
        if(ioctl(cdrom, CDROMEJECT, 0) < 0)
            g_critical("devices_eject_disk - ioctl failed");
        else
            ret = TRUE;
#endif                
    }
	close(cdrom);
	return ret;
}
/*
gboolean 
devices_get_max_speed_for_drive(const gchar* drive)
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
	
	GString* buffer = exec_run_cmd(command);
	if(buffer == NULL)
		g_critical("devices_get_max_speed_for_drive - Failed to scan the scsi bus");
	else if(!devices_parse_cdrecord_output(buffer->str, bus))	
		g_critical("devices_get_max_speed_for_drive - failed to parse cdrecord output");
	else
		ok = TRUE;
	
	g_string_free(buffer, TRUE);

	return ok;
}
*/

static gboolean
devices_is_disk_inserted(const gchar* devicekey)
{
    GB_LOG_FUNC
	g_return_val_if_fail(devicekey != NULL, FALSE);
    
	gboolean retval = FALSE;
	gchar *device = devices_get_device_config(devicekey,GB_DEVICE_NODE_LABEL);
	int fd = open(device, O_RDONLY | O_NONBLOCK);
    g_free(device);
    const int ret = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    close(fd);
    if (ret == -1)
	{
		g_critical("devices_is_disk_inserted - ioctl failed");
    }
    else 
    {
    	switch (ret)
    	{
        	case CDS_NO_DISC:
    			retval = FALSE;
    				break;
    		case CDS_TRAY_OPEN:
    			retval = FALSE;
    				break;
    		case CDS_DRIVE_NOT_READY:
    			retval = FALSE;
    				break;
    		case CDS_DISC_OK:
    			retval = TRUE;
    				break;
            default:
                retval = FALSE;
    	}
    }    
	return retval;
}


gint 
devices_prompt_for_disk(GtkWindow* parent, const gchar* devicekey)
{
    GB_LOG_FUNC    
    g_return_val_if_fail(devicekey != NULL, GTK_RESPONSE_CANCEL);
    
    gchar* devicename = devices_get_device_config(devicekey, GB_DEVICE_NAME_LABEL);
    gchar* message = g_strdup_printf(_("Please insert a disk into the %s"), devicename);    
    gint ret = GTK_RESPONSE_OK;
    /*while(!devices_is_disk_inserted(devicekey) && (ret == GTK_RESPONSE_OK))*/
    if(!devices_is_disk_inserted(devicekey))
    {
        devices_eject_disk(devicekey);
        ret = gnomebaker_show_msg_dlg(parent, GTK_MESSAGE_INFO, 
            GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE, message);
    }
    g_free(devicename);
    g_free(message);
    return ret;
}


gboolean 
devices_reader_is_also_writer()
{
    GB_LOG_FUNC    
    gchar* reader = devices_get_device_config(GB_READER, GB_DEVICE_NODE_LABEL);
    gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_NODE_LABEL);
    gboolean ret = (g_ascii_strcasecmp(reader, writer) == 0);
    g_free(reader);
    g_free(writer);
    return ret;
}



