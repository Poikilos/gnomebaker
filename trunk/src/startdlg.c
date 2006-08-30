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
 * File: startdlg.c
 * Copyright: luke_biddell@yahoo.com
 * Created on: Wed Apr  7 23:08:35 2004
 */

#include "startdlg.h"
#include "preferences.h"
#include "devices.h"
#include "gbcommon.h"

static const guint xpad = 12;
static const guint ypad = 0;

#define TABLE_FILL  \
    GTK_EXPAND | GTK_FILL, GTK_FILL    \

#define TABLE_ATTACH_OPTIONS_1 			\
	TABLE_FILL, 0, ypad	\

#define TABLE_ATTACH_OPTIONS_2            \
    TABLE_FILL, xpad, ypad    \

#define TABLE_ATTACH_OPTIONS_3            \
    TABLE_FILL, xpad + xpad, ypad    \


static void
startdlg_create_iso_toggled(GtkToggleButton *toggle_button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(toggle_button != NULL);
    g_return_if_fail(user_data != NULL);

    StartDlg *start_dlg = (StartDlg*)user_data;
	gboolean state = gtk_toggle_button_get_active(toggle_button);
    gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->writer), !state);
    gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->write_speed), !state);
    gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->write_mode), !state);
	gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->dummy), !state);
	gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->eject), !state);
	gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->burn_free), !state);
    gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->on_the_fly), !state);
    gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->finalize), !state);
    gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->iso_file), state);
    gtk_widget_set_sensitive(GTK_WIDGET(start_dlg->browse), state);
}


static void
startdlg_on_ok_clicked(GtkButton  *button, gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(user_data != NULL);

    StartDlg *start_dlg = (StartDlg*)user_data;
    devices_save_optionmenu(start_dlg->reader, GB_READER);
    devices_save_optionmenu(start_dlg->writer, GB_WRITER);
    const gint index = gtk_option_menu_get_history(start_dlg->write_speed);
    if(index == 0)
        preferences_set_int(start_dlg->dvdmode ? GB_DVDWRITE_SPEED : GB_CDWRITE_SPEED, 0);
    else
    {
        gchar *text = gbcommon_get_option_menu_selection(start_dlg->write_speed);
        text[strlen(text) - 1] = '\0';
        const gint speed = atoi(text);
        preferences_set_int(start_dlg->dvdmode ? GB_DVDWRITE_SPEED : GB_CDWRITE_SPEED, speed);
        g_free(text);
    }
    preferences_set_bool(GB_BURNFREE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->burn_free)));
    preferences_set_bool(GB_FAST_BLANK, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->fast_erase)));
    gchar *text = gbcommon_get_option_menu_selection(start_dlg->write_mode);
    preferences_set_string(start_dlg->dvdmode ? GB_DVDWRITE_MODE : GB_WRITE_MODE, text);
    g_free(text);
    preferences_set_bool(GB_FINALIZE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->finalize)));
    preferences_set_bool(GB_FAST_FORMAT, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->fast_format)));
    preferences_set_bool(GB_FORCE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->force_format)));
    preferences_set_bool(GB_DUMMY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->dummy)));
    preferences_set_bool(GB_EJECT, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->eject)));
    preferences_set_bool(GB_JOLIET, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->joliet)));
    preferences_set_bool(GB_ROCKRIDGE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->rock_ridge)));
    preferences_set_bool(GB_CREATEISOONLY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->iso_only)));
    preferences_set_bool(GB_ONTHEFLY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start_dlg->on_the_fly)));
    preferences_set_string(GB_LAST_ISO, gtk_entry_get_text(start_dlg->iso_file));
}


static void
startdlg_on_browse_clicked(GtkButton  *button, gpointer user_data)
{
    GB_LOG_FUNC
    StartDlg *start_dlg = (StartDlg*)user_data;
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new( _("Please select an image file to save to."),
            GTK_WINDOW(start_dlg->dialog), GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

    const gchar *file = gtk_entry_get_text(start_dlg->iso_file);
    if(file != NULL && strlen(file) > 0)
    {
        gchar *base_name = g_path_get_basename(file);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_chooser), base_name);
        g_free(base_name);
        gchar *dir = g_path_get_dirname(file);
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser), dir);
        g_free(dir);
    }
    else
    {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_chooser), "gnomebaker.iso");
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser), g_get_home_dir());
    }

    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(file_chooser), FALSE);
    GtkFileFilter *image_filter = gtk_file_filter_new();
    gtk_file_filter_add_custom(image_filter, GTK_FILE_FILTER_MIME_TYPE, gbcommon_iso_file_filter, NULL, NULL);
    gtk_file_filter_set_name(image_filter,_("CD/DVD Image files"));
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), image_filter);

    if(gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT)
    {
        gchar *file_name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        gtk_entry_set_text(start_dlg->iso_file, file_name);
        g_free(file_name);
    }
    gtk_widget_destroy(file_chooser);
}


static GtkCheckButton*
startdlg_create_check_button(const gchar *name, const gchar *key)
{
	GB_LOG_FUNC
	g_return_val_if_fail(name != NULL, NULL);
	GtkWidget *check = gtk_check_button_new_with_label(name);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), preferences_get_bool(key));
	gtk_widget_show(check);
	return GTK_CHECK_BUTTON(check);
}


static GtkRadioButton*
startdlg_create_radio_button(const gchar *name, GtkRadioButton *group, gboolean active)
{
    GB_LOG_FUNC
    g_return_val_if_fail(name != NULL, NULL);
    GtkWidget *radio = gtk_radio_button_new_with_label_from_widget(group, name);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), active);
    gtk_widget_show(radio);
    return GTK_RADIO_BUTTON(radio);
}


static GtkWidget*
startdlg_create_label(const gchar *text)
{
    GtkWidget *label = gtk_label_new (text);
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    return label;
}


static GtkTable*
startdlg_create_table()
{
    GtkWidget *table = gtk_table_new (6, 4, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (table), xpad);
    gtk_table_set_row_spacings (GTK_TABLE(table), xpad);
    gtk_table_set_col_spacings (GTK_TABLE(table), xpad);
    return GTK_TABLE(table);
}


static guint
startdlg_add_device_section(GtkTable *table, StartDlg *start_dlg, gboolean show_reader, gboolean show_write_mode)
{
    guint row = 0;

    if(show_reader)
    {
        gtk_table_attach(table, startdlg_create_label(_("<b>Reader</b>")), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_1);
        ++row;
        gtk_table_attach (GTK_TABLE (table), GTK_WIDGET(start_dlg->reader), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
        ++row;
    }
    gtk_table_attach(table, startdlg_create_label(_("<b>Writer</b>")), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_1);
    ++row;
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET(start_dlg->writer), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, startdlg_create_label(_("Speed:")), 0, 1, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->write_speed), 1, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);

    GtkWidget *menu4 = gtk_menu_new ();
    const gchar *speeds[] = {_("Auto"), "1X", "2X", "4X", "6X", "8X", "10X", "12X", "16X", "24X", "32X", "40X", "48X", "52X", NULL};
    gint i = 0;
    for(; i < (start_dlg->dvdmode ? 9 : 14); ++i)
    {
        GtkWidget *menu_item = gtk_menu_item_new_with_label(speeds[i]);
        gtk_widget_show(menu_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu4), menu_item);
    }
    gtk_widget_show(menu4);
    gtk_option_menu_set_menu (start_dlg->write_speed, menu4);
    const int s = preferences_get_int(start_dlg->dvdmode ? GB_DVDWRITE_SPEED : GB_CDWRITE_SPEED);
    if(s == 0)
        gtk_option_menu_set_history(start_dlg->write_speed, s);
    else
    {
        gchar *default_speed = g_strdup_printf("%dX", s);
        gbcommon_set_option_menu_selection(start_dlg->write_speed, default_speed);
        g_free(default_speed);
    }

    if(show_write_mode)
    {
        gtk_table_attach(table, startdlg_create_label(_("Mode:")), 2, 3, row, row + 1, TABLE_ATTACH_OPTIONS_2);
        gtk_table_attach (GTK_TABLE (table), GTK_WIDGET(start_dlg->write_mode), 3, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);

        GtkWidget *menu5 = gtk_menu_new ();
        const gchar *cd_modes[] = {_("Auto"), "dao", "raw16", "raw96p", "raw96r", "tao", "sao", NULL};
        const gchar *dvd_modes[] = {_("Auto"), "dao", NULL};
        const gchar **modes = cd_modes;
        if(start_dlg->dvdmode) modes = dvd_modes;
        const gchar **mode = modes;
        while(*mode != NULL)
        {
            GtkWidget *menu_item = gtk_menu_item_new_with_label(*mode);
            gtk_widget_show(menu_item);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu5), menu_item);
            ++mode;
        }
        gtk_widget_show(menu5);
        gtk_option_menu_set_menu (start_dlg->write_mode, menu5);
        gchar *write_mode = preferences_get_string(start_dlg->dvdmode ? GB_DVDWRITE_MODE : GB_WRITE_MODE);
        gbcommon_set_option_menu_selection(start_dlg->write_mode, write_mode);
        g_free(write_mode);
    }
    ++row;
    gtk_table_attach(table, startdlg_create_label(_("<b>Options</b>")), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_1);
    return row;
}


static void
startdlg_add_action_area(StartDlg *start_dlg)
{
    /* button stuff */
    GtkWidget *dialog_action_area5 = start_dlg->dialog->action_area;
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area5), GTK_BUTTONBOX_END);

    GtkWidget *button28 = gtk_button_new_from_stock ("gtk-cancel");
    gtk_dialog_add_action_widget (start_dlg->dialog, button28, GTK_RESPONSE_CANCEL);
    GTK_WIDGET_SET_FLAGS (button28, GTK_CAN_DEFAULT);

    GtkWidget *button27 = gtk_button_new ();
    gtk_dialog_add_action_widget (start_dlg->dialog, button27, GTK_RESPONSE_OK);
    GTK_WIDGET_SET_FLAGS (button27, GTK_CAN_DEFAULT);

    GtkWidget *alignment11 = gtk_alignment_new (0.5, 0.5, 0, 0);
    gtk_container_add (GTK_CONTAINER (button27), alignment11);

    GtkWidget *hbox20 = gtk_hbox_new (FALSE, 2);
    gtk_container_add (GTK_CONTAINER (alignment11), hbox20);

    GtkWidget *image77 = gtk_image_new_from_stock ("gtk-ok", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start (GTK_BOX (hbox20), image77, FALSE, FALSE, 0);

    GtkWidget *label255 = gtk_label_new_with_mnemonic (_("_Start"));
    gtk_box_pack_start (GTK_BOX (hbox20), label255, FALSE, FALSE, 0);
    g_signal_connect ((gpointer) button27, "clicked", G_CALLBACK (startdlg_on_ok_clicked), start_dlg);
}


static void
startdlg_blank_cdrw_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, FALSE, FALSE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->fast_erase), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
}


static void
startdlg_create_audio_cd_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, FALSE, TRUE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->dummy), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->burn_free), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    /*gtk_table_attach(table, GTK_WIDGET(start_dlg->on_the_fly), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS);*/
    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
}


static void
startdlg_burn_cd_image_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, FALSE, TRUE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->dummy), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->burn_free), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
}


static void
startdlg_burn_dvd_image_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, FALSE, FALSE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
}


static GtkTable*
startdlg_create_information_tab(StartDlg *start_dlg)
{
    GtkTable *table2 = GTK_TABLE(startdlg_create_table());
    guint row = 0;
    gtk_table_attach(table2, startdlg_create_label(_("<b>Disk Information</b>")), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_1);
    ++row;
    gtk_table_attach(table2, startdlg_create_label(_("Volume ID:")), 0, 1, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table2, GTK_WIDGET(start_dlg->volume_id), 1, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table2, startdlg_create_label(_("Application ID:")), 0, 1, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table2, GTK_WIDGET(start_dlg->app_id), 1, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table2, startdlg_create_label(_("Preparer:")), 0, 1, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table2, GTK_WIDGET(start_dlg->preparer), 1, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table2, startdlg_create_label(_("Publisher:")), 0, 1, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table2, GTK_WIDGET(start_dlg->publisher), 1, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table2, startdlg_create_label(_("Copyright:")), 0, 1, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table2, GTK_WIDGET(start_dlg->copyright), 1, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table2, startdlg_create_label(_("Abstract:")), 0, 1, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table2, GTK_WIDGET(start_dlg->abstract), 1, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table2, startdlg_create_label(_("Bibliography:")), 0, 1, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table2, GTK_WIDGET(start_dlg->bibliography), 1, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    return table2;
}


static GtkTable*
startdlg_create_filesystem_tab(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = 0;
    gtk_table_attach(table, startdlg_create_label(_("<b>Filesystem</b>")), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_1);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->joliet), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->rock_ridge), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    return table;
}


static void
startdlg_create_data_disk_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, FALSE, TRUE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->burn_disk), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->dummy), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    ++row;
    if(!start_dlg->dvdmode)
    {
        gtk_table_attach(table, GTK_WIDGET(start_dlg->burn_free), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_3);
        gtk_table_attach(table, GTK_WIDGET(start_dlg->on_the_fly), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    }
    else
    {
        gtk_table_attach(table, GTK_WIDGET(start_dlg->finalize), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    }
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->iso_only), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    GtkWidget *hbox = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(start_dlg->iso_file), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(start_dlg->browse), FALSE, FALSE, 0);
    gtk_table_attach(table, hbox, 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_3);

    GtkNotebook *notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_append_page(notebook, GTK_WIDGET(table), gtk_label_new(_("Writing")));
    gtk_notebook_append_page(notebook, GTK_WIDGET(startdlg_create_filesystem_tab(start_dlg)), gtk_label_new(_("Filesystem")));
    gtk_notebook_append_page(notebook, GTK_WIDGET(startdlg_create_information_tab(start_dlg)), gtk_label_new(_("Disk Information")));
    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(notebook), FALSE, FALSE, xpad);
    g_signal_emit_by_name(start_dlg->iso_only, "toggled", start_dlg->iso_only, start_dlg);
}


static void
startdlg_append_data_disk_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, FALSE, TRUE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->dummy), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    if(!start_dlg->dvdmode)
    {
        gtk_table_attach(table, GTK_WIDGET(start_dlg->burn_free), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
        gtk_table_attach(table, GTK_WIDGET(start_dlg->on_the_fly), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    }
    else
    {
        gtk_table_attach(table, GTK_WIDGET(start_dlg->finalize), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    }

    GtkNotebook *notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_append_page(notebook, GTK_WIDGET(table), gtk_label_new(_("Writing")));
    gtk_notebook_append_page(notebook, GTK_WIDGET(startdlg_create_filesystem_tab(start_dlg)), gtk_label_new(_("Filesystem")));
    gtk_notebook_append_page(notebook, GTK_WIDGET(startdlg_create_information_tab(start_dlg)), gtk_label_new(_("Disk Information")));
    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(notebook), FALSE, FALSE, xpad);

    g_signal_emit_by_name(start_dlg->iso_only, "toggled", start_dlg->iso_only, start_dlg);
}


static void
startdlg_copy_audio_cd_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, TRUE, TRUE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->dummy), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->burn_free), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
}


static void
startdlg_copy_data_cd_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, TRUE, TRUE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->burn_disk), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->dummy), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->burn_free), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->iso_only), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);

    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    ++row;
    GtkWidget *hbox = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(start_dlg->iso_file), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(start_dlg->browse), FALSE, FALSE, 0);
    gtk_table_attach(table, hbox, 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    g_signal_emit_by_name(start_dlg->iso_only, "toggled", start_dlg->iso_only, start_dlg);
}


static void
startdlg_copy_cd_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, TRUE, FALSE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->dummy), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->on_the_fly), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
}


static void
startdlg_copy_dvd_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, TRUE, FALSE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->burn_disk), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->dummy), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->iso_only), 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);

    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    ++row;
    GtkWidget *hbox = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(start_dlg->iso_file), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(start_dlg->browse), FALSE, FALSE, 0);
    gtk_table_attach(table, hbox, 0, 4, row, row + 1, TABLE_ATTACH_OPTIONS_3);
    g_signal_emit_by_name(start_dlg->iso_only, "toggled", start_dlg->iso_only, start_dlg);
}


static void
startdlg_format_dvdrw_section(StartDlg *start_dlg)
{
    GtkTable *table = GTK_TABLE(startdlg_create_table());
    guint row = startdlg_add_device_section(table, start_dlg, FALSE, FALSE);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->eject), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_table_attach(table, GTK_WIDGET(start_dlg->force_format), 2, 4, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    ++row;
    gtk_table_attach(table, GTK_WIDGET(start_dlg->fast_format), 0, 2, row, row + 1, TABLE_ATTACH_OPTIONS_2);
    gtk_box_pack_end (GTK_BOX (start_dlg->dialog->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
}


StartDlg*
startdlg_new(const BurnType burn_type)
{
	GB_LOG_FUNC

    StartDlg *start_dlg = g_new0(StartDlg, 1);
    start_dlg->dialog = GTK_DIALOG(gtk_dialog_new ());
    gtk_window_set_default_size(GTK_WINDOW(start_dlg->dialog), 320, -1);
    gtk_window_set_title(GTK_WINDOW(start_dlg->dialog), _(BurnTypeText[burn_type]));
    gtk_window_set_modal (GTK_WINDOW (start_dlg->dialog), TRUE);

    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (start_dlg->dialog), TRUE);
    gtk_dialog_set_has_separator (start_dlg->dialog, FALSE);

    start_dlg->writer = GTK_OPTION_MENU(gtk_option_menu_new ());
    devices_populate_optionmenu(GTK_WIDGET(start_dlg->writer), GB_WRITER, TRUE);
    start_dlg->reader = GTK_OPTION_MENU(gtk_option_menu_new ());
    devices_populate_optionmenu(GTK_WIDGET(start_dlg->reader), GB_READER, FALSE);
    start_dlg->write_speed = GTK_OPTION_MENU(gtk_option_menu_new ());
    start_dlg->write_mode = GTK_OPTION_MENU(gtk_option_menu_new ());
    start_dlg->dummy = startdlg_create_check_button(_("Dummy write"), GB_DUMMY);
    start_dlg->eject = startdlg_create_check_button(_("Eject disk"), GB_EJECT);
    start_dlg->fast_erase = startdlg_create_check_button(_("Fast blank"), GB_FAST_BLANK);
    start_dlg->burn_free = startdlg_create_check_button(_("BurnFree"), GB_BURNFREE);
    start_dlg->burn_disk = startdlg_create_radio_button(_("Burn the disk"),
            NULL, !preferences_get_bool(GB_CREATEISOONLY));
    start_dlg->iso_only = startdlg_create_radio_button(_("Only create image"),
            GTK_RADIO_BUTTON(start_dlg->burn_disk), preferences_get_bool(GB_CREATEISOONLY));
    g_signal_connect(G_OBJECT(start_dlg->iso_only), "toggled", (GCallback)startdlg_create_iso_toggled, start_dlg);
    start_dlg->force_format = startdlg_create_check_button(_("Force format"), GB_FORCE);
    start_dlg->finalize = startdlg_create_check_button(_("Finalize"), GB_FINALIZE);
    start_dlg->fast_format = startdlg_create_check_button(_("Fast format"), GB_FAST_FORMAT);
    start_dlg->joliet = startdlg_create_check_button(_("Joliet"), GB_JOLIET);
    start_dlg->rock_ridge = startdlg_create_check_button(_("Rock Ridge"), GB_ROCKRIDGE);
    start_dlg->on_the_fly = startdlg_create_check_button(_("On the fly"), GB_ONTHEFLY);
    start_dlg->iso_file = GTK_ENTRY(gtk_entry_new());
    gchar *last_iso = preferences_get_string(GB_LAST_ISO);
    if(last_iso == NULL || strlen(last_iso) == 0)
    {
        g_free(last_iso);
        last_iso = preferences_get_default_iso();
    }
    gtk_entry_set_text(start_dlg->iso_file, last_iso);
    g_free(last_iso);
    start_dlg->browse = GTK_BUTTON(gtk_button_new_with_mnemonic(_("_Browse...")));
    start_dlg->volume_id = GTK_ENTRY(gtk_entry_new_with_max_length(32));
    gtk_entry_set_text(start_dlg->volume_id, _("GnomeBaker data disk"));
    start_dlg->app_id = GTK_ENTRY(gtk_entry_new_with_max_length(128));
    gtk_entry_set_text(start_dlg->app_id, "GnomeBaker");
    start_dlg->preparer = GTK_ENTRY(gtk_entry_new_with_max_length(128));
    gtk_entry_set_text(start_dlg->preparer, g_get_real_name ());
    start_dlg->publisher = GTK_ENTRY(gtk_entry_new_with_max_length(128));
    start_dlg->copyright = GTK_ENTRY(gtk_entry_new_with_max_length(37));
    start_dlg->abstract = GTK_ENTRY(gtk_entry_new_with_max_length(37));
    start_dlg->bibliography = GTK_ENTRY(gtk_entry_new_with_max_length(37));
    g_signal_connect ((gpointer) start_dlg->browse, "clicked", G_CALLBACK (startdlg_on_browse_clicked), start_dlg);

    /* FIXME - need to pass in the project */
    gchar *vol_id = NULL;/*datacd_compilation_get_volume_id();*/

	switch(burn_type)
	{
		case blank_cdrw:
			startdlg_blank_cdrw_section(start_dlg);
			break;
		case create_audio_cd:
			startdlg_create_audio_cd_section(start_dlg);
            break;
		case create_mixed_cd:
            /* not supported yet */
			break;
		case burn_cd_image:
			startdlg_burn_cd_image_section(start_dlg);
			break;
		case burn_dvd_image:
            start_dlg->dvdmode = TRUE;
			startdlg_burn_dvd_image_section(start_dlg);
			break;
		case create_video_cd:
            /* not supported yet */
			break;
		case create_data_cd:
			if(vol_id != NULL)  gtk_entry_set_text(start_dlg->volume_id, vol_id );
            startdlg_create_data_disk_section(start_dlg);
            break;
        case append_data_cd:
			if(vol_id != NULL)  gtk_entry_set_text(start_dlg->volume_id, vol_id );
            startdlg_append_data_disk_section(start_dlg);
            g_free(vol_id);
			break;
		case copy_audio_cd:
			startdlg_copy_audio_cd_section(start_dlg);
			break;
		case copy_data_cd:
            startdlg_copy_data_cd_section(start_dlg);
			break;
        case copy_cd:
            startdlg_copy_cd_section(start_dlg);
            break;
        case copy_dvd:
            startdlg_copy_dvd_section(start_dlg);
            break;
		case format_dvdrw:
            start_dlg->dvdmode = TRUE;
            startdlg_format_dvdrw_section(start_dlg);
			break;
		case create_data_dvd:
			if(vol_id != NULL)  gtk_entry_set_text(start_dlg->volume_id, vol_id );
            start_dlg->dvdmode = TRUE;
            startdlg_create_data_disk_section(start_dlg);
            break;
        case append_data_dvd:
			if(vol_id != NULL)  gtk_entry_set_text(start_dlg->volume_id, vol_id );
            start_dlg->dvdmode = TRUE;
            startdlg_append_data_disk_section(start_dlg);
			break;
		default:
			break;
	};

	g_free(vol_id);

    startdlg_add_action_area(start_dlg);

    gtk_widget_show_all(GTK_WIDGET(start_dlg->dialog));
    gbcommon_center_window_on_parent(GTK_WIDGET(start_dlg->dialog));
	return start_dlg;
}


void
startdlg_delete(StartDlg *self)
{
	GB_LOG_FUNC
    /* Manually delete all of the widgets here as not all get added to the container */
    gtk_widget_hide(GTK_WIDGET(self->dialog));

#define DESTROY_WIDGET(widget)                                          \
    if(GTK_IS_WIDGET(widget)) gtk_widget_destroy(GTK_WIDGET(widget));   \

    DESTROY_WIDGET(self->reader);
    DESTROY_WIDGET(self->writer);
    DESTROY_WIDGET(self->write_speed);
    DESTROY_WIDGET(self->write_mode);
    DESTROY_WIDGET(self->dummy);
    DESTROY_WIDGET(self->eject);
    DESTROY_WIDGET(self->fast_erase);
    DESTROY_WIDGET(self->burn_free);
    DESTROY_WIDGET(self->burn_disk);
    DESTROY_WIDGET(self->force_format);
    DESTROY_WIDGET(self->finalize);
    DESTROY_WIDGET(self->fast_format);
    DESTROY_WIDGET(self->joliet);
    DESTROY_WIDGET(self->rock_ridge);
    DESTROY_WIDGET(self->on_the_fly);
    DESTROY_WIDGET(self->iso_only);
    DESTROY_WIDGET(self->iso_file);
    DESTROY_WIDGET(self->browse);
    DESTROY_WIDGET(self->volume_id);
    DESTROY_WIDGET(self->app_id);
    DESTROY_WIDGET(self->preparer);
    DESTROY_WIDGET(self->publisher);
    DESTROY_WIDGET(self->copyright);
    DESTROY_WIDGET(self->abstract);
    DESTROY_WIDGET(self->bibliography);

    gtk_widget_destroy(GTK_WIDGET(self->dialog));

    g_free(self);
}



