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
 * File: prefsdlg.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Fri Apr  9 12:04:30 2004
 */

#include "prefsdlg.h"
#include "preferences.h"
#include "gbcommon.h"
#include "filebrowser.h"
#include "devices.h"


static const gint DEVICELIST_COL_ICON = 0;
static const gint DEVICELIST_COL_NAME = 1;
static const gint DEVICELIST_COL_ID = 2;
static const gint DEVICELIST_COL_NODE = 3;
static const gint DEVICELIST_COL_MOUNT = 4;
static const gint DEVICELIST_WRITE_CDR = 5;
static const gint DEVICELIST_WRITE_CDRW = 6;
static const gint DEVICELIST_WRITE_DVDR = 7;
static const gint DEVICELIST_WRITE_DVDRAM = 8;
static const gint DEVICELIST_NUM_COLS = 9;


GladeXML* prefsdlg_xml = NULL;


void
prefsdlg_device_cell_edited(GtkCellRendererText *cell,
							gchar* path_string,
							gchar* new_text,
							gpointer user_data)
{
	GB_LOG_FUNC
	
	g_return_if_fail(cell != NULL);
	g_return_if_fail(path_string != NULL);
	g_return_if_fail(new_text != NULL);
	g_return_if_fail(user_data != NULL);
	
	gint* columnnum = (gint*)user_data;
	
	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget* devicelist = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(devicelist != NULL);
	GtkListStore* devicemodel = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(devicelist)));
	g_return_if_fail(devicemodel != NULL);	
	
	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(devicemodel), &iter, path_string))
	{
		GValue val = {0};
		g_value_init(&val, G_TYPE_STRING);
		g_value_set_string(&val, new_text);

		gtk_list_store_set_value(devicemodel, &iter, *columnnum, &val);
		
		g_value_unset(&val);
	}
}


void
prefsdlg_device_capability_edited(GtkCellRendererToggle *cell,
                                   gchar *path, gpointer user_data)
{
	GB_LOG_FUNC
	
	g_return_if_fail(cell != NULL);
	g_return_if_fail(path != NULL);
	g_return_if_fail(user_data != NULL);
	
	gint* columnnum = (gint*)user_data;

	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget* devicelist = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(devicelist != NULL);
	GtkListStore* devicemodel = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(devicelist)));
	g_return_if_fail(devicemodel != NULL);	
	
	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(devicemodel), &iter, path))
	{	
		GValue val = {0};
		g_value_init(&val, G_TYPE_BOOLEAN);
		g_value_set_boolean(&val, !gtk_cell_renderer_toggle_get_active(cell));

		gtk_list_store_set_value(devicemodel, &iter, *columnnum, &val);
		
		g_value_unset(&val);
	}
}


void 
prefsdlg_create_device_list()
{
	GB_LOG_FUNC
	
	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget* devicelist = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
		
	GtkListStore *store = gtk_list_store_new(DEVICELIST_NUM_COLS, 
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
		G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
    gtk_tree_view_set_model(GTK_TREE_VIEW(devicelist), GTK_TREE_MODEL(store));
    g_object_unref(store);
	
	GValue value = { 0 };
	g_value_init(&value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&value, TRUE);	

	/* One column which has an icon renderer and text renderer packed in */
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Name"));
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "stock-id", DEVICELIST_COL_ICON, NULL);
	
    renderer = gtk_cell_renderer_text_new();
	g_object_set_property(G_OBJECT(renderer), "editable", &value);
	g_signal_connect(renderer, "edited", (GCallback)prefsdlg_device_cell_edited, 
		(gpointer)&DEVICELIST_COL_NAME);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", DEVICELIST_COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(devicelist), col);

	/* Second column to display the device id */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Id"), renderer, 
		"text", DEVICELIST_COL_ID, NULL);
	g_object_set_property(G_OBJECT(renderer), "editable", &value);
	g_signal_connect(renderer, "edited", (GCallback)prefsdlg_device_cell_edited, 
		(gpointer)&DEVICELIST_COL_ID);
	gtk_tree_view_append_column(GTK_TREE_VIEW(devicelist), col);
			
	/* Third column to display the device node */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Node"), renderer, 
		"text", DEVICELIST_COL_NODE, NULL);
	g_object_set_property(G_OBJECT(renderer), "editable", &value);
	g_signal_connect(renderer, "edited", (GCallback)prefsdlg_device_cell_edited, 
		(gpointer)&DEVICELIST_COL_NODE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(devicelist), col);	
	
	/* Fourth column to display the mount point */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Mount point"), renderer, 
		"text", DEVICELIST_COL_MOUNT, NULL);
	g_object_set_property(G_OBJECT(renderer), "editable", &value);
	g_signal_connect(renderer, "edited", (GCallback)prefsdlg_device_cell_edited, 
		(gpointer)&DEVICELIST_COL_MOUNT);
	gtk_tree_view_append_column(GTK_TREE_VIEW(devicelist), col);
	
	/* Fifth column for writing cdr */
	renderer = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Write CD-R"), renderer, "active",
		DEVICELIST_WRITE_CDR, NULL);
	g_object_set_property(G_OBJECT(renderer), "activatable", &value);
	g_signal_connect(renderer, "toggled", (GCallback)prefsdlg_device_capability_edited, 
		(gpointer)&DEVICELIST_WRITE_CDR);
    gtk_tree_view_append_column(GTK_TREE_VIEW(devicelist), col);
	
	/* Sixth column for writing cdrw */
	renderer = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Write CD-RW"), renderer, "active",
		DEVICELIST_WRITE_CDRW, NULL);
	g_object_set_property(G_OBJECT(renderer), "activatable", &value);
	g_signal_connect(renderer, "toggled", (GCallback)prefsdlg_device_capability_edited, 
		(gpointer)&DEVICELIST_WRITE_CDRW);
    gtk_tree_view_append_column(GTK_TREE_VIEW(devicelist), col);
	
	/* 7th column for writing dvdr */
	renderer = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Write DVD-R"), renderer, "active",
		DEVICELIST_WRITE_DVDR, NULL);
	g_object_set_property(G_OBJECT(renderer), "activatable", &value);
	g_signal_connect(renderer, "toggled", (GCallback)prefsdlg_device_capability_edited, 
		(gpointer)&DEVICELIST_WRITE_DVDR);
    gtk_tree_view_append_column(GTK_TREE_VIEW(devicelist), col);
	
	/* 8th column for writing dvdrw */
	renderer = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Write DVD-RAM"), renderer, "active",
		DEVICELIST_WRITE_DVDRAM, NULL);
	g_object_set_property(G_OBJECT(renderer), "activatable", &value);
	g_signal_connect(renderer, "toggled", (GCallback)prefsdlg_device_capability_edited, 
		(gpointer)&DEVICELIST_WRITE_DVDRAM);
    gtk_tree_view_append_column(GTK_TREE_VIEW(devicelist), col);

	g_value_unset(&value);
}


gboolean
prefsdlg_foreach_device(GtkTreeModel *devicemodel,
					  GtkTreePath *path,
					  GtkTreeIter *iter,
					  gpointer userdata)
{
	GB_LOG_FUNC
	g_return_val_if_fail(devicemodel != NULL, TRUE);
	g_return_val_if_fail(iter != NULL, TRUE);
	
	gint* devicecount = (gint*)userdata; 
	++(*devicecount);
	
	gchar *icon = NULL, *name = NULL, *id = NULL, *node = NULL, *mount = NULL;
	gboolean writecdr = FALSE, writecdrw = FALSE, writedvdr = FALSE, writedvdram = FALSE;
	gtk_tree_model_get(devicemodel, iter, DEVICELIST_COL_ICON, &icon, 
		DEVICELIST_COL_NAME, &name, DEVICELIST_COL_ID, &id, 
		DEVICELIST_COL_NODE, &node, DEVICELIST_COL_MOUNT, &mount,
		DEVICELIST_WRITE_CDR, &writecdr, DEVICELIST_WRITE_CDRW, &writecdrw,
		DEVICELIST_WRITE_DVDR, &writedvdr, DEVICELIST_WRITE_DVDRAM, &writedvdram, -1);
	
	gint capabilities = 0;
	if(writecdr) capabilities |= DC_WRITE_CDR;
	if(writecdrw) capabilities |= DC_WRITE_CDRW;
	if(writedvdr) capabilities |= DC_WRITE_DVDR;
	if(writedvdram) capabilities |= DC_WRITE_DVDRAM;	
	
	if((name == NULL) || (id == NULL) || (node == NULL))
		g_critical(_("Invalid row in device list"));	
	else
		devices_write_device_to_gconf(*devicecount, name, id, node, mount, capabilities);
	
	g_free(icon);
	g_free(name);	
	g_free(id);	
	g_free(node);
	g_free(mount);
	
	return FALSE; /* do not stop walking the store, call us with next row */
}

	
void 
prefsdlg_on_ok(GtkButton* button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(prefsdlg_xml != NULL);
	
	GtkWidget* entry = glade_xml_get_widget(prefsdlg_xml, "tmpDirEntry");
	const gchar* tempdir = gtk_entry_get_text(GTK_ENTRY(entry));
	preferences_set_string(GB_TEMP_DIR, tempdir);
	gbcommon_mkdir(tempdir);	
	
	GtkWidget* checkCleanTmp = 	glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_cleantempdir);
	preferences_set_bool(GB_CLEANTEMPDIR, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkCleanTmp)));
	
	GtkWidget* checkShowHidden = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_showhidden);
	preferences_set_bool(GB_SHOWHIDDEN, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkShowHidden)));

	GtkWidget* checkShowHumanSize = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_showhumansize);
	preferences_set_bool(GB_SHOWHUMANSIZE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkShowHumanSize)));
	
	GtkWidget* checkAlwaysScan = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_alwaysscan);
	preferences_set_bool(GB_ALWAYS_SCAN, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkAlwaysScan)));
	
	GtkWidget* devicelist = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(devicelist != NULL);
	GtkTreeModel* devicemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(devicelist));
	g_return_if_fail(devicemodel != NULL);
	gint devicecount = 0;
	gtk_tree_model_foreach(devicemodel, prefsdlg_foreach_device, &devicecount);
}


void 
prefsdlg_populate_device_list()
{	
	GB_LOG_FUNC
	
	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget* devicelist = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(devicelist != NULL);
	GtkListStore* devicemodel = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(devicelist)));
	g_return_if_fail(devicemodel != NULL);

	GSList* devices = preferences_get_key_subkeys(GB_DEVICES_KEY);
	GSList* item = devices;	
	while(item)
	{
		gchar* devicekey = (gchar*)item->data;				
		gchar* devicenamekey = g_strconcat(devicekey, GB_DEVICE_NAME_LABEL, NULL);
		gchar* deviceidkey = g_strconcat(devicekey, GB_DEVICE_ID_LABEL, NULL);		
		gchar* devicenodekey = g_strconcat(devicekey, GB_DEVICE_NODE_LABEL, NULL);
		gchar* devicemountkey = g_strconcat(devicekey, GB_DEVICE_MOUNT_LABEL, NULL);
		gchar* devicecapabilitieskey = g_strconcat(devicekey, GB_DEVICE_CAPABILITIES_LABEL, NULL);
		
		gchar* devicename = preferences_get_string(devicenamekey);
		gchar* deviceid = preferences_get_string(deviceidkey);
		gchar* devicenode = preferences_get_string(devicenodekey);
		gchar* devicemount = preferences_get_string(devicemountkey);
		gint capabilities = preferences_get_int(devicecapabilitieskey);
		
		if((devicename != NULL) && (strlen(devicename) > 0))
		{		
			GB_DECLARE_STRUCT(GtkTreeIter, iter);
			gtk_list_store_append(devicemodel, &iter);		
			gtk_list_store_set(devicemodel, &iter, 
				DEVICELIST_COL_ICON, GNOME_STOCK_PIXMAP_CDROM,
				DEVICELIST_COL_NAME, devicename,
				DEVICELIST_COL_ID, deviceid,
				DEVICELIST_COL_NODE, devicenode, 
				DEVICELIST_COL_MOUNT, devicemount,
				DEVICELIST_WRITE_CDR, capabilities & DC_WRITE_CDR,
				DEVICELIST_WRITE_CDRW, capabilities & DC_WRITE_CDRW,
				DEVICELIST_WRITE_DVDR, capabilities & DC_WRITE_DVDR,
				DEVICELIST_WRITE_DVDRAM, capabilities & DC_WRITE_DVDRAM,
				-1);
		}
		
		g_free(deviceidkey);
		g_free(devicenamekey);		
		g_free(devicenodekey);
		g_free(devicekey);
		g_free(devicemountkey);
		
		g_free(deviceid);
		g_free(devicename);		
		g_free(devicenode);
		g_free(devicemount);
		item = item->next;
	}
	
	g_slist_free(devices);			
}


void
prefsdlg_clear_device_list()
{
	GB_LOG_FUNC

	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget* devicelist = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(devicelist != NULL);
	GtkListStore* devicemodel = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(devicelist)));
	g_return_if_fail(devicemodel != NULL);	
	gtk_list_store_clear(devicemodel);
}


void 
prefsdlg_on_scan(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(prefsdlg_xml != NULL);		
	gbcommon_start_busy_cursor1(prefsdlg_xml, widget_prefsdlg);
	
	prefsdlg_clear_device_list();
	devices_probe_busses();
	prefsdlg_populate_device_list();
	
	gbcommon_end_busy_cursor1(prefsdlg_xml, widget_prefsdlg);
}


void 
prefsdlg_on_add(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget* devicelist = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(devicelist != NULL);
	GtkListStore* devicemodel = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(devicelist)));
	g_return_if_fail(devicemodel != NULL);

	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	gtk_list_store_append(devicemodel, &iter);		
	gtk_list_store_set(devicemodel, &iter, 
		DEVICELIST_COL_ICON, GNOME_STOCK_PIXMAP_CDROM,
		DEVICELIST_COL_NAME, "New CD Burner",
		DEVICELIST_COL_ID, "1,0,0",
		DEVICELIST_COL_NODE, "/dev/cdrom", 
		DEVICELIST_COL_MOUNT, "/mnt/cdrom", -1);
}


gboolean
prefsdlg_on_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	GB_LOG_FUNC
	prefsdlg_on_ok(NULL, NULL);	
	return TRUE;
}


GtkWidget* 
prefsdlg_new(void)
{	
	GB_LOG_FUNC	
	prefsdlg_xml = glade_xml_new(glade_file, widget_prefsdlg, NULL);
	glade_xml_signal_autoconnect(prefsdlg_xml);		
	
	gchar* tempdir = preferences_get_string(GB_TEMP_DIR);
	GtkWidget* entry = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_tempdir);
	gtk_entry_set_text(GTK_ENTRY(entry), tempdir);
	g_free(tempdir);
	
	GtkWidget* checkCleanTmp = 	glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_cleantempdir);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkCleanTmp), 
		preferences_get_bool(GB_CLEANTEMPDIR));
	
	GtkWidget* checkShowHidden = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_showhidden);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkShowHidden), 
		preferences_get_bool(GB_SHOWHIDDEN));
	
	GtkWidget* checkShowHumanSizes = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_showhumansize);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkShowHumanSizes),
		preferences_get_bool(GB_SHOWHUMANSIZE));

	GtkWidget* checkAlwaysScan = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_alwaysscan);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkAlwaysScan), 
		preferences_get_bool(GB_ALWAYS_SCAN));
	
	prefsdlg_create_device_list();
	prefsdlg_populate_device_list();
	
	return glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg);
}


void 
prefsdlg_delete(GtkWidget* self)
{
	GB_LOG_FUNC
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_free(prefsdlg_xml);
	prefsdlg_xml = NULL;
}
