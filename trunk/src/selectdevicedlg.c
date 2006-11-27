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
 * File: selectdevicedlg.h
 * Copyright: User luke biddell
 * Created on: Sun Jan 16 00:42:55 2005
 */


#include "selectdevicedlg.h"
#include "gbcommon.h"
#include "devices.h"
#include "preferences.h"


/* Select device dialog widget names */
static const gchar *const widget_select_writer = "cmbWriter";
static const gchar *const widget_select_device_dlg = "selectDeviceDlg";

static GladeXML *selectdevicedlgdlg_xml = NULL;


GtkWidget*
selectdevicedlg_new(void)
{
	GB_LOG_FUNC
	selectdevicedlgdlg_xml = glade_xml_new(glade_file, widget_select_device_dlg, NULL);
	glade_xml_signal_autoconnect(selectdevicedlgdlg_xml);

    GtkComboBox *combo_box = GTK_COMBO_BOX(glade_xml_get_widget(selectdevicedlgdlg_xml, widget_select_writer));
	devices_populate_combo_box(combo_box, GB_WRITER, TRUE);

    GtkWidget *dlg = glade_xml_get_widget(selectdevicedlgdlg_xml, widget_select_device_dlg);
    gbcommon_center_window_on_parent(dlg);
	return dlg;
}


void
selectdevicedlg_delete(GtkWidget *self)
{
	g_return_if_fail(NULL != self);
	gtk_widget_destroy(self);
	g_object_unref(selectdevicedlgdlg_xml);
	selectdevicedlgdlg_xml = NULL;
}


void
selectdevicedlg_on_ok_clicked(GtkButton *button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(selectdevicedlgdlg_xml != NULL);

	GtkComboBox *combo_box = GTK_COMBO_BOX(glade_xml_get_widget(selectdevicedlgdlg_xml, widget_select_writer));
    devices_save_combo_box(combo_box, GB_WRITER);
}
