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
 * Created by: luke_biddell@yahoo.com
 * Created on: Wed Apr  7 23:08:35 2004
 */

#include "startdlg.h"
#include "preferences.h"
#include "devices.h"
#include "gnomebaker.h"
#include "gbcommon.h"


GladeXML* startdlg_xml = NULL;


void 
startdlg_populate_device_combos()
{
	GB_LOG_FUNC
	g_return_if_fail(startdlg_xml != NULL);	
	
	GtkWidget *optmenReadDev = glade_xml_get_widget(startdlg_xml, widget_startdlg_reader);
	devices_populate_optionmenu(optmenReadDev, GB_READER);	
	
	GtkWidget *optmenWriteDev = glade_xml_get_widget(startdlg_xml, widget_startdlg_writer);
	devices_populate_optionmenu(optmenWriteDev, GB_WRITER);	
}


GtkWidget* 
startdlg_new(const BurnType burntype)
{	
	GB_LOG_FUNC
	startdlg_xml = glade_xml_new(glade_file, widget_startdlg, NULL);
	glade_xml_signal_autoconnect(startdlg_xml);
	
	GtkWidget* dlg = glade_xml_get_widget(startdlg_xml, widget_startdlg);
	gtk_window_set_title(GTK_WINDOW(dlg), BurnTypeText[burntype]);
	
	startdlg_populate_device_combos();
		
	GtkWidget* spinSpeed = glade_xml_get_widget(startdlg_xml, widget_startdlg_speed);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinSpeed), preferences_get_int(GB_WRITE_SPEED));

	GtkWidget* checkDummy = glade_xml_get_widget(startdlg_xml, widget_startdlg_dummy);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkDummy), preferences_get_bool(GB_DUMMY));

	GtkWidget* checkMultisession = glade_xml_get_widget(startdlg_xml,  widget_startdlg_multisession);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkMultisession), preferences_get_bool(GB_MULTI_SESSION));

	GtkWidget* checkEject = glade_xml_get_widget(startdlg_xml, widget_startdlg_eject);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkEject), preferences_get_bool(GB_EJECT));
	
	GtkWidget* checkFastErase = glade_xml_get_widget(startdlg_xml, widget_startdlg_fasterase);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkFastErase), preferences_get_bool(GB_FAST_BLANK));
	
	GtkWidget* checkOverburn = glade_xml_get_widget(startdlg_xml, widget_startdlg_overburn);	
	/*gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkOverburn), appPrefs->overburn);*/
	
	GtkWidget* checkBurnFree = glade_xml_get_widget(startdlg_xml, widget_startdlg_burnfree);	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkBurnFree), preferences_get_bool(GB_BURNFREE));
	
	GtkWidget* checkISOOnly = glade_xml_get_widget(startdlg_xml, widget_startdlg_isoonly);	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkISOOnly), preferences_get_bool(GB_CREATEISOONLY));	
	
	GtkWidget *optmenWriteMode = glade_xml_get_widget(startdlg_xml, widget_startdlg_writemode);
	
	gchar* mode = preferences_get_string(GB_WRITE_MODE);
	gbcommon_set_option_menu_selection(GTK_OPTION_MENU(optmenWriteMode), mode);	
	g_free(mode);
		
	switch(burntype)
	{		
		case blank_cdrw:		
			gtk_widget_hide(checkDummy);
			gtk_widget_hide(checkMultisession);
			gtk_widget_hide(checkOverburn);
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_reader));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_modelabel));
			gtk_widget_hide(optmenWriteMode);
			gtk_widget_hide(checkBurnFree);
			gtk_widget_hide(checkISOOnly);
			gtk_widget_show(checkFastErase);
			break;				
		case create_audio_cd:
		case create_mixed_cd:
		case burn_cd_image:			
			gtk_widget_hide(checkISOOnly);
		case create_video_cd:			
		case create_data_cd:		
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_reader));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
			break;
		case copy_audio_cd:
			gtk_widget_hide(checkISOOnly);
		case copy_data_cd:		
		default:
			break;
	};
	
	return dlg;
}


void 
startdlg_delete(GtkWidget* self)
{	
	GB_LOG_FUNC
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_free(startdlg_xml);
	startdlg_xml = NULL;
}


void 
startdlg_on_ok_clicked(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC	
	g_return_if_fail(startdlg_xml != NULL);	
	
	GtkWidget* optmenReadDev = glade_xml_get_widget(startdlg_xml, widget_startdlg_reader);
	devices_save_optionmenu(GTK_OPTION_MENU(optmenReadDev), GB_READER);

	GtkWidget* optmenWriteDev = glade_xml_get_widget(startdlg_xml, widget_startdlg_writer);
	devices_save_optionmenu(GTK_OPTION_MENU(optmenWriteDev), GB_WRITER);

	preferences_set_int(GB_WRITE_SPEED, gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(glade_xml_get_widget(startdlg_xml, widget_startdlg_speed))));

	preferences_set_bool(GB_DUMMY, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlg_xml, widget_startdlg_dummy))));
	
	preferences_set_bool(GB_EJECT, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlg_xml, widget_startdlg_eject))));
	
	preferences_set_bool(GB_MULTI_SESSION, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlg_xml, widget_startdlg_multisession))));
	
	preferences_set_bool(GB_FAST_BLANK, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlg_xml, widget_startdlg_fasterase))));
	
	preferences_set_bool(GB_BURNFREE, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlg_xml, widget_startdlg_burnfree))));
	
	preferences_set_bool(GB_CREATEISOONLY, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(glade_xml_get_widget(startdlg_xml, widget_startdlg_isoonly))));
		
	GtkWidget* optmenWriteMode = glade_xml_get_widget(startdlg_xml, widget_startdlg_writemode);
	gchar* text = gbcommon_get_option_menu_selection(GTK_OPTION_MENU(optmenWriteMode));
	preferences_set_string(GB_WRITE_MODE, text);
	g_free(text);
}


void 
startdlg_on_scan(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(startdlg_xml != NULL);		
	gbcommon_start_busy_cursor1(startdlg_xml, widget_startdlg);
	
	if(devices_probe_busses())
		startdlg_populate_device_combos();
	
	gbcommon_end_busy_cursor1(startdlg_xml, widget_startdlg);
}
