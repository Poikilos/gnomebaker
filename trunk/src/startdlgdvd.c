/***************************************************************************
 *            startdlgdvd.c
 *
 *  Sun Dec 26 13:31:28 2004
 *  Copyright  2004  Christoffer Sørensen
 *  Email christoffer@curo.dk
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
 
 
#include "startdlgdvd.h"
#include "preferences.h"
#include "devices.h"
#include "gnomebaker.h"
#include "gbcommon.h"

void startdlgdvd_on_ok_clicked (GtkButton * button, gpointer user_data);
void startdlgdvd_on_scan (GtkButton * button, gpointer user_data);
void startdlgdvd_populate_device_combos();


GladeXML* startdlgdvd_xml = NULL;


GtkWidget* 
startdlgdvd_new(const BurnType burntype)
{	
	GB_LOG_FUNC
	startdlgdvd_xml = glade_xml_new(glade_file, widget_startdlgdvd, NULL);
	glade_xml_signal_autoconnect(startdlgdvd_xml);
	
	GtkWidget* dlg = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd);
	gtk_window_set_title(GTK_WINDOW(dlg), BurnTypeText[burntype]);
	
	startdlgdvd_populate_device_combos();
		
	GtkWidget* spinSpeed = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_speed);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinSpeed), preferences_get_int(GB_WRITE_SPEED));

	GtkWidget* checkDummyDvd = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_dummy);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkDummyDvd), preferences_get_bool(GB_DUMMY));

	GtkWidget* checkMultisession = glade_xml_get_widget(startdlgdvd_xml,  widget_startdlgdvd_multisession);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkMultisession), preferences_get_bool(GB_MULTI_SESSION));

	GtkWidget* checkEject = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_eject);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkEject), preferences_get_bool(GB_EJECT));
	
	GtkWidget* checkFastErase = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_fasterase);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkFastErase), preferences_get_bool(GB_FAST_BLANK));
	
	GtkWidget* checkOverburn = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_overburn);	
	/*gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkOverburn), appPrefs->overburn);*/
	
	GtkWidget* checkBurnFree = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_burnfree);	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkBurnFree), preferences_get_bool(GB_BURNFREE));
	
	GtkWidget* checkISOOnly = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_isoonly);	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkISOOnly), preferences_get_bool(GB_CREATEISOONLY));	
	

	GtkWidget* checkForce = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_force);	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkForce), preferences_get_bool(GB_FORCE));	

	GtkWidget* checkFinalize = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_finalize);	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkFinalize), preferences_get_bool(GB_FINALIZE));	


	GtkWidget *optmenWriteModeDvd = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_writemode);
	GList* items = GTK_MENU_SHELL(gtk_option_menu_get_menu(
		GTK_OPTION_MENU(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_reader))))->children;
	gchar* mode = preferences_get_string(GB_WRITE_MODE);
	gint index = 0;
	while(items)
	{
		if (GTK_BIN (items->data)->child)
		{
			GtkWidget *child = GTK_BIN (items->data)->child;				
			if (GTK_IS_LABEL (child))
			{
				gchar *text = NULL;			
				gtk_label_get (GTK_LABEL (child), &text);
				if(g_ascii_strcasecmp(text, mode) == 0)
					gtk_option_menu_set_history(GTK_OPTION_MENU(optmenWriteModeDvd), index);	
			}
		}
		items = items->next;
		++index;
	}
	
	g_free(mode);

	/* TODO: when widgets are hidden, all other widgets
			should be reordered so the layout is nicer */ 	
	switch(burntype)
	{		
		case format_dvdrw:
			/* Luke: should we hide or gray out controls ? */
			gtk_widget_hide(checkDummyDvd);
			gtk_widget_hide(checkMultisession);
			gtk_widget_hide(checkOverburn);
			gtk_widget_hide(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_reader));
			gtk_widget_hide(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_readlabel));
			gtk_widget_hide(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_modelabel));
			gtk_widget_hide(optmenWriteModeDvd);
			gtk_widget_hide(checkBurnFree);
			gtk_widget_hide(checkISOOnly);
			gtk_widget_set_sensitive(spinSpeed,FALSE);
			gtk_widget_hide(checkEject);
			gtk_widget_show(checkFastErase);
			gtk_widget_show(checkForce);		
			gtk_widget_hide(checkFinalize);
			break;
		case create_data_dvd:
			gtk_widget_hide(checkISOOnly); /*for now, will be added later */
			gtk_widget_hide(checkDummyDvd); /* growisofs does not support it */
			gtk_widget_hide(checkBurnFree); /* does growisofs support this ? */
			gtk_widget_hide(checkMultisession);
			gtk_widget_hide(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_reader));
			gtk_widget_set_sensitive(optmenWriteModeDvd,FALSE);
			gtk_widget_hide(checkForce);
			gtk_widget_show(checkFinalize);
			gtk_widget_hide(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_readlabel));
			break;
		default:
			break;
	};
	
	return dlg;
}


void 
startdlgdvd_delete(GtkWidget* self)
{	
	GB_LOG_FUNC
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_free(startdlgdvd_xml);
}


void 
startdlgdvd_on_ok_clicked(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC	
	g_return_if_fail(startdlgdvd_xml != NULL);	
	
	GtkWidget* optmenReaderDvd = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_reader);
	gint index = gtk_option_menu_get_history(GTK_OPTION_MENU(optmenReaderDvd));
	gchar* device = g_strdup_printf(GB_DEVICE_FORMAT, index + 1);
	preferences_set_string(GB_READER, device);
	g_free(device);
	
	GtkWidget* optmenWriterDvd = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_writer);
	index = gtk_option_menu_get_history(GTK_OPTION_MENU(optmenWriterDvd));
	device = g_strdup_printf(GB_DEVICE_FORMAT, index + 1);
	preferences_set_string(GB_WRITER, device);
	g_free(device);

	preferences_set_int(GB_WRITE_SPEED, gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_speed))));

	preferences_set_bool(GB_DUMMY, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_dummy))));
	
	preferences_set_bool(GB_EJECT, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_eject))));
	
	preferences_set_bool(GB_MULTI_SESSION, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_multisession))));
	
	preferences_set_bool(GB_FAST_BLANK, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_fasterase))));
	
	preferences_set_bool(GB_BURNFREE, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_burnfree))));
	
	preferences_set_bool(GB_CREATEISOONLY, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_isoonly))));
		
	preferences_set_bool(GB_FORCE, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_force))));

	preferences_set_bool(GB_FINALIZE, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_finalize))));

	GtkWidget* optmenWriteMode = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_writemode);
	GtkWidget* mode = GTK_BIN(optmenWriteMode)->child;
	if(mode != NULL && GTK_IS_LABEL(mode))
	{
		gchar *text = NULL;
		gtk_label_get(GTK_LABEL(mode), &text);
		preferences_set_string(GB_WRITE_MODE, text);
		/*g_free(text);*/
	}
}


void 
startdlgdvd_on_scan(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(startdlgdvd_xml != NULL);		
	gbcommon_start_busy_cursor1(startdlgdvd_xml, widget_startdlgdvd);
	
	if(devices_probe_busses())
		startdlgdvd_populate_device_combos();
	
	gbcommon_end_busy_cursor1(startdlgdvd_xml, widget_startdlgdvd);
}

void 
startdlgdvd_populate_device_combos()
{
	GB_LOG_FUNC
	g_return_if_fail(startdlgdvd_xml != NULL);	
	
	GtkWidget *optmenReaderDvd = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_reader);
	gchar* reader = preferences_get_string(GB_READER);
	devices_populate_optionmenu(optmenReaderDvd, reader);	
	g_free(reader);
	
	GtkWidget *optmenWriterDvd = glade_xml_get_widget(startdlgdvd_xml, widget_startdlgdvd_writer);
	gchar* writer = preferences_get_string(GB_WRITER);
	g_message("Writer is: %s",writer);
	devices_populate_optionmenu(optmenWriterDvd, writer);	
	g_free(writer);
}
