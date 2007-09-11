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


/* Prefs dialog glade widget names */
static const gchar *const widget_prefsdlg = "prefsDlg";
static const gchar *const widget_prefsdlg_temp_dir = "tmpDirEntry";
static const gchar *const widget_prefsdlg_clean_temp_dir = "checkCleanTmp";
static const gchar *const widget_prefsdlg_ask_on_quit = "checkAskOnQuit";
static const gchar *const widget_prefsdlg_play_sound = "checkPlaySound";
static const gchar *const widget_prefsdlg_always_scan = "checkAlwaysScan";
static const gchar *const widget_prefsdlg_devicelist = "treeview12";
static const gchar *const widget_prefsdlg_scroll_output = "checkScrollOutput";
static const gchar *const widget_prefsdlg_cdrecord_force = "checkCDRecordForce";
static const gchar *const widget_prefsdlg_backend = "cb_select_backend";

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


static GladeXML *prefsdlg_xml = NULL;


static void
prefsdlg_device_cell_edited(GtkCellRendererText *cell,
							gchar *path_string,
							gchar *new_text,
							gpointer user_data)
{
	GB_LOG_FUNC

	g_return_if_fail(cell != NULL);
	g_return_if_fail(path_string != NULL);
	g_return_if_fail(new_text != NULL);
	g_return_if_fail(user_data != NULL);

	gint *column_num = (gint*)user_data;

	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget *device_list = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(device_list != NULL);
	GtkListStore *device_model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(device_list)));
	g_return_if_fail(device_model != NULL);

	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(device_model), &iter, path_string))
	{
		GValue val = {0};
		g_value_init(&val, G_TYPE_STRING);
		g_value_set_string(&val, new_text);
		gtk_list_store_set_value(device_model, &iter, *column_num, &val);
		g_value_unset(&val);
	}
}


static void
prefsdlg_device_capability_edited(GtkCellRendererToggle *cell,
                                   gchar *path, gpointer user_data)
{
	GB_LOG_FUNC

	g_return_if_fail(cell != NULL);
	g_return_if_fail(path != NULL);
	g_return_if_fail(user_data != NULL);

	gint *column_num = (gint*)user_data;

	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget *device_list = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(device_list != NULL);
	GtkListStore *device_model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(device_list)));
	g_return_if_fail(device_model != NULL);

	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(device_model), &iter, path))
	{
		GValue val = {0};
		g_value_init(&val, G_TYPE_BOOLEAN);
		g_value_set_boolean(&val, !gtk_cell_renderer_toggle_get_active(cell));
		gtk_list_store_set_value(device_model, &iter, *column_num, &val);
		g_value_unset(&val);
	}
}


static void
prefsdlg_create_device_list()
{
	GB_LOG_FUNC

	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget *device_list = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);

	GtkListStore *store = gtk_list_store_new(DEVICELIST_NUM_COLS,
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
            G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
    gtk_tree_view_set_model(GTK_TREE_VIEW(device_list), GTK_TREE_MODEL(store));
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
    gtk_tree_view_append_column(GTK_TREE_VIEW(device_list), col);

	/* Second column to display the device id */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Id"), renderer,
            "text", DEVICELIST_COL_ID, NULL);
	g_object_set_property(G_OBJECT(renderer), "editable", &value);
	g_signal_connect(renderer, "edited", (GCallback)prefsdlg_device_cell_edited,
            (gpointer)&DEVICELIST_COL_ID);
	gtk_tree_view_append_column(GTK_TREE_VIEW(device_list), col);

	/* Third column to display the device node */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Node"), renderer,
            "text", DEVICELIST_COL_NODE, NULL);
	g_object_set_property(G_OBJECT(renderer), "editable", &value);
	g_signal_connect(renderer, "edited", (GCallback)prefsdlg_device_cell_edited,
            (gpointer)&DEVICELIST_COL_NODE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(device_list), col);

	/* Fourth column to display the mount point */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Mount point"), renderer,
            "text", DEVICELIST_COL_MOUNT, NULL);
	g_object_set_property(G_OBJECT(renderer), "editable", &value);
	g_signal_connect(renderer, "edited", (GCallback)prefsdlg_device_cell_edited,
            (gpointer)&DEVICELIST_COL_MOUNT);
	gtk_tree_view_append_column(GTK_TREE_VIEW(device_list), col);

	/* Fifth column for writing cdr */
	renderer = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Write CD-R"), renderer, "active",
            DEVICELIST_WRITE_CDR, NULL);
	g_object_set_property(G_OBJECT(renderer), "activatable", &value);
	g_signal_connect(renderer, "toggled", (GCallback)prefsdlg_device_capability_edited,
            (gpointer)&DEVICELIST_WRITE_CDR);
    gtk_tree_view_append_column(GTK_TREE_VIEW(device_list), col);

	/* Sixth column for writing cdrw */
	renderer = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Write CD-RW"), renderer, "active",
            DEVICELIST_WRITE_CDRW, NULL);
	g_object_set_property(G_OBJECT(renderer), "activatable", &value);
	g_signal_connect(renderer, "toggled", (GCallback)prefsdlg_device_capability_edited,
            (gpointer)&DEVICELIST_WRITE_CDRW);
    gtk_tree_view_append_column(GTK_TREE_VIEW(device_list), col);

	/* 7th column for writing dvdr */
	renderer = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Write DVD-R"), renderer, "active",
            DEVICELIST_WRITE_DVDR, NULL);
	g_object_set_property(G_OBJECT(renderer), "activatable", &value);
	g_signal_connect(renderer, "toggled", (GCallback)prefsdlg_device_capability_edited,
            (gpointer)&DEVICELIST_WRITE_DVDR);
    gtk_tree_view_append_column(GTK_TREE_VIEW(device_list), col);

	/* 8th column for writing dvdrw */
	renderer = gtk_cell_renderer_toggle_new();
	col = gtk_tree_view_column_new_with_attributes(_("Write DVD-RAM"), renderer, "active",
            DEVICELIST_WRITE_DVDRAM, NULL);
	g_object_set_property(G_OBJECT(renderer), "activatable", &value);
	g_signal_connect(renderer, "toggled", (GCallback)prefsdlg_device_capability_edited,
            (gpointer)&DEVICELIST_WRITE_DVDRAM);
    gtk_tree_view_append_column(GTK_TREE_VIEW(device_list), col);

	g_value_unset(&value);
}


static gboolean
prefsdlg_foreach_device(GtkTreeModel *device_model,
					  GtkTreePath *path,
					  GtkTreeIter *iter,
					  gpointer userdata)
{
	GB_LOG_FUNC
	g_return_val_if_fail(device_model != NULL, TRUE);
	g_return_val_if_fail(iter != NULL, TRUE);

	gint *device_count = (gint*)userdata;
	++(*device_count);

	gchar *icon = NULL, *name = NULL, *id = NULL, *node = NULL, *mount = NULL;
	gboolean write_cdr = FALSE, write_cdrw = FALSE, write_dvdr = FALSE, write_dvdram = FALSE;
	gtk_tree_model_get(device_model, iter, DEVICELIST_COL_ICON, &icon,
            DEVICELIST_COL_NAME, &name, DEVICELIST_COL_ID, &id,
            DEVICELIST_COL_NODE, &node, DEVICELIST_COL_MOUNT, &mount,
            DEVICELIST_WRITE_CDR, &write_cdr, DEVICELIST_WRITE_CDRW, &write_cdrw,
            DEVICELIST_WRITE_DVDR, &write_dvdr, DEVICELIST_WRITE_DVDRAM, &write_dvdram, -1);

	gint capabilities = 0;
	if(write_cdr) capabilities |= DC_WRITE_CDR;
	if(write_cdrw) capabilities |= DC_WRITE_CDRW;
	if(write_dvdr) capabilities |= DC_WRITE_DVDR;
	if(write_dvdram) capabilities |= DC_WRITE_DVDRAM;

	if((name == NULL) || (id == NULL) || (node == NULL))
		g_warning("prefsdlg_foreach_device - Invalid row in device list");
	else
		devices_write_device_to_gconf(*device_count, name, id, node, mount, capabilities);

	g_free(icon);
	g_free(name);
	g_free(id);
	g_free(node);
	g_free(mount);

	return FALSE; /* do not stop walking the store, call us with next row */
}


void /* libglade callback */
prefsdlg_on_ok(GtkButton *button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(prefsdlg_xml != NULL);

	GtkWidget *entry = glade_xml_get_widget(prefsdlg_xml, "tmpDirEntry");
	const gchar *temp_dir = gtk_entry_get_text(GTK_ENTRY(entry));
	preferences_set_string(GB_TEMP_DIR, temp_dir);
	gbcommon_mkdir(temp_dir);

	// this is for the cdrecord additional arguments
	GtkWidget *cdrecord_force = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_cdrecord_force);
	preferences_set_bool(GB_CDRECORD_FORCE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cdrecord_force)));

	GtkWidget *clean_temp = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_clean_temp_dir);
	preferences_set_bool(GB_CLEANTEMPDIR, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(clean_temp)));

	GtkWidget *always_scan = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_always_scan);
	preferences_set_bool(GB_ALWAYS_SCAN, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(always_scan)));

	GtkWidget *play_sound = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_play_sound);
	preferences_set_bool(GB_PLAY_SOUND, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_sound)));

    GtkWidget *ask_on_quit = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_ask_on_quit);

	GtkWidget *scroll_output = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_scroll_output);
	preferences_set_bool(GB_SCROLL_OUTPUT, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(scroll_output)));

	GtkWidget *device_list = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(device_list != NULL);
	GtkTreeModel *device_model = gtk_tree_view_get_model(GTK_TREE_VIEW(device_list));
	g_return_if_fail(device_model != NULL);
	gint device_count = 0;
	gtk_tree_model_foreach(device_model, prefsdlg_foreach_device, &device_count);
}

static void
prefsdlg_populate_backend_list()
{
	GB_LOG_FUNC
	
	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget *backend_list = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_backend);
	g_return_if_fail(backend_list != NULL);

	/* create model */	
	GtkListStore *model  = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);    
    gtk_combo_box_set_model(GTK_COMBO_BOX(backend_list), GTK_TREE_MODEL(model));

    GtkTreeIter iter;

	/* append data */
	if (backend_is_backend_supported(BACKEND_CDRECORD))
	{
	   	gtk_list_store_append(model, &iter);
	    gtk_list_store_set(model, &iter, 0, "cdrecord", 1, BACKEND_CDRECORD, -1);  	
	} 
	
	if (backend_is_backend_supported(BACKEND_WODIM)) 
	{
	    gtk_list_store_append(model, &iter);
	    gtk_list_store_set(model, &iter, 0, "wodim", 1, BACKEND_WODIM, -1);
	}
    
    /* create renderer */
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (backend_list), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (backend_list), renderer, "text", 0, NULL);
    
    enum backend b = preferences_get_int(GB_BACKEND);

    /*
     Current Backend selection 
     */
    
    /* Get first iter */
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter);
    
    do
    { 
    	GValue back = {0};
    	gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 1, &back);
    	if (g_value_get_int(&back) == b)
    		gtk_combo_box_set_active_iter(GTK_COMBO_BOX(backend_list), &iter);
    	g_value_unset(&back);
    } 
    while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));

}

static void
prefsdlg_populate_device_list()
{
	GB_LOG_FUNC

	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget *device_list = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(device_list != NULL);
	GtkListStore *device_model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(device_list)));
	g_return_if_fail(device_model != NULL);

	GSList *devices = preferences_get_key_subkeys(GB_DEVICES_KEY);
	GSList *item = devices;
	for(; item != NULL; item = item->next)
	{
		gchar *device_key = (gchar*)item->data;
		gchar *device_name_key = g_strconcat(device_key, GB_DEVICE_NAME_LABEL, NULL);
		gchar *device_id_key = g_strconcat(device_key, GB_DEVICE_ID_LABEL, NULL);
		gchar *device_node_key = g_strconcat(device_key, GB_DEVICE_NODE_LABEL, NULL);
		gchar *device_mount_key = g_strconcat(device_key, GB_DEVICE_MOUNT_LABEL, NULL);
		gchar *devicecapabilitieskey = g_strconcat(device_key, GB_DEVICE_CAPABILITIES_LABEL, NULL);
		gchar *device_name = preferences_get_string(device_name_key);
		gchar *device_id = preferences_get_string(device_id_key);
		gchar *device_node = preferences_get_string(device_node_key);
		gchar *device_mount = preferences_get_string(device_mount_key);
		gint capabilities = preferences_get_int(devicecapabilitieskey);

		if((device_name != NULL) && (strlen(device_name) > 0))
		{
			GB_DECLARE_STRUCT(GtkTreeIter, iter);
			gtk_list_store_append(device_model, &iter);
			gtk_list_store_set(device_model, &iter,
    				DEVICELIST_COL_ICON, GNOME_STOCK_PIXMAP_CDROM,
    				DEVICELIST_COL_NAME, device_name,
    				DEVICELIST_COL_ID, device_id,
    				DEVICELIST_COL_NODE, device_node,
    				DEVICELIST_COL_MOUNT, device_mount,
    				DEVICELIST_WRITE_CDR, capabilities & DC_WRITE_CDR,
    				DEVICELIST_WRITE_CDRW, capabilities & DC_WRITE_CDRW,
    				DEVICELIST_WRITE_DVDR, capabilities & DC_WRITE_DVDR,
    				DEVICELIST_WRITE_DVDRAM, capabilities & DC_WRITE_DVDRAM, -1);
		}

		g_free(device_id_key);
		g_free(device_name_key);
		g_free(device_node_key);
		g_free(device_key);
		g_free(device_mount_key);
		g_free(device_id);
		g_free(device_name);
		g_free(device_node);
		g_free(device_mount);
	}

	g_slist_free(devices);
}

static void
prefsdlg_clear_device_list()
{
	GB_LOG_FUNC

	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget *device_list = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(device_list != NULL);
	GtkListStore *device_model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(device_list)));
	g_return_if_fail(device_model != NULL);
	gtk_list_store_clear(device_model);
}

void /* libglade callback */
prefsdlg_on_backend_changed(GtkComboBox *cb, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model  = gtk_combo_box_get_model(cb);
	GValue b = {0};
	
	gtk_combo_box_get_active_iter(cb, &iter);
	
	gtk_tree_model_get_value(model, &iter, 1, &b);
	
	enum backend selected_backend = g_value_get_int(&b);
	
	preferences_set_int(GB_BACKEND, selected_backend);
}

void /* libglade callback */
prefsdlg_on_scan(GtkButton  *button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(prefsdlg_xml != NULL);
	gbcommon_start_busy_cursor1(prefsdlg_xml, widget_prefsdlg);

	prefsdlg_clear_device_list();
	devices_probe_busses();
	prefsdlg_populate_device_list();

	gbcommon_end_busy_cursor1(prefsdlg_xml, widget_prefsdlg);
}


void /* libglade callback */
prefsdlg_on_add(GtkButton  *button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(prefsdlg_xml != NULL);
	GtkWidget *device_list = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_devicelist);
	g_return_if_fail(device_list != NULL);
	GtkListStore *device_model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(device_list)));
	g_return_if_fail(device_model != NULL);

	GB_DECLARE_STRUCT(GtkTreeIter, iter);
	gtk_list_store_append(device_model, &iter);
	gtk_list_store_set(device_model, &iter,
    		DEVICELIST_COL_ICON, GNOME_STOCK_PIXMAP_CDROM,
    		DEVICELIST_COL_NAME, "New CD Burner",
    		DEVICELIST_COL_ID, "1,0,0",
    		DEVICELIST_COL_NODE, "/dev/cdrom",
    		DEVICELIST_COL_MOUNT, "/mnt/cdrom", -1);
}


gboolean /* libglade callback */
prefsdlg_on_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
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

	gchar *temp_dir = preferences_get_string(GB_TEMP_DIR);
	GtkWidget *entry = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_temp_dir);
	gtk_entry_set_text(GTK_ENTRY(entry), temp_dir);
	g_free(temp_dir);

	// cdrecord additional arguments
	GtkWidget *cdrecord_force = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_cdrecord_force);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cdrecord_force),
            preferences_get_bool(GB_CDRECORD_FORCE));

	GtkWidget *clean_temp = 	glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_clean_temp_dir);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(clean_temp),
            preferences_get_bool(GB_CLEANTEMPDIR));

	GtkWidget *scroll_output = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_scroll_output);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scroll_output),
            preferences_get_bool(GB_SCROLL_OUTPUT));

	GtkWidget *always_scan = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_always_scan);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(always_scan),
            preferences_get_bool(GB_ALWAYS_SCAN));

    GtkWidget *play_sound = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg_play_sound);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_sound),
            preferences_get_bool(GB_PLAY_SOUND));

	prefsdlg_create_device_list();
	prefsdlg_populate_device_list();
	prefsdlg_populate_backend_list();
	
	GtkWidget *dlg = glade_xml_get_widget(prefsdlg_xml, widget_prefsdlg);
    gbcommon_center_window_on_parent(dlg);
	return dlg;
}


void
prefsdlg_delete(GtkWidget *self)
{
	GB_LOG_FUNC
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_object_unref(prefsdlg_xml);
	prefsdlg_xml = NULL;
}
