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


/* Start dialog glade widget names */
static const gchar* const widget_startdlg = "startDlg";
static const gchar* const widget_startdlg_reader = "optmenReader";
static const gchar* const widget_startdlg_writer = "optmenWriter";
static const gchar* const widget_startdlg_speed = "spinSpeed";
static const gchar* const widget_startdlg_readlabel = "labelReader";
static const gchar* const widget_startdlg_writemode = "optionmenu7";
static const gchar* const widget_startdlg_modelabel = "label265";

static GladeXML* startdlg_xml = NULL;

static GtkWidget* checkDummy = NULL;
static GtkWidget* checkEject = NULL;
static GtkWidget* checkFastErase = NULL;
static GtkWidget* checkBurnFree = NULL;
static GtkWidget* radioBurnDisk = NULL;	
static GtkWidget* radioISOOnly = NULL;	
static GtkWidget* checkForce = NULL;	
static GtkWidget* checkFinalize = NULL;
static GtkWidget* checkFastFormat = NULL;
static GtkWidget* checkJoliet = NULL;
static GtkWidget* checkRockRidge = NULL;
static GtkWidget* checkOnTheFly = NULL;
static GtkWidget* labelFileSystem = NULL;

static const guint xpad = 12;
static const guint ypad = 0;
static gboolean gdvdmode = FALSE;

#define TABLE_ATTACH_OPTIONS_1 			\
	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, ypad	\
    
#define TABLE_ATTACH_OPTIONS_2            \
    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, xpad, ypad    \

#define TABLE_ATTACH_OPTIONS_3            \
    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, xpad + xpad, ypad    \


static void 
startdlg_create_iso_toggled(GtkToggleButton* togglebutton, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(togglebutton != NULL);
	gboolean state = gtk_toggle_button_get_active(togglebutton);
	gtk_widget_set_sensitive(checkDummy, !state);
	gtk_widget_set_sensitive(checkEject, !state);
	gtk_widget_set_sensitive(checkBurnFree, !state);
    gtk_widget_set_sensitive(checkOnTheFly, !state);
    gtk_widget_set_sensitive(checkFinalize, !state);
}


static void 
startdlg_populate_device_combos()
{
	GB_LOG_FUNC
	g_return_if_fail(startdlg_xml != NULL);	
	
	GtkWidget *optmenReadDev = glade_xml_get_widget(startdlg_xml, widget_startdlg_reader);
	devices_populate_optionmenu(optmenReadDev, GB_READER);	
	
	GtkWidget *optmenWriteDev = glade_xml_get_widget(startdlg_xml, widget_startdlg_writer);
	devices_populate_optionmenu(optmenWriteDev, GB_WRITER);	
}


static GtkWidget* 
startdlg_create_check_button(const gchar* name, const gchar* key)
{
	GB_LOG_FUNC
	g_return_val_if_fail(name != NULL, NULL);
	GtkWidget* check = gtk_check_button_new_with_label(name);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), preferences_get_bool(key));
	gtk_widget_show(check);
	return check;	
}


static GtkWidget* 
startdlg_create_radio_button(const gchar* name, GtkRadioButton* group, gboolean active)
{
    GB_LOG_FUNC
    g_return_val_if_fail(name != NULL, NULL);
    GtkWidget* radio = gtk_radio_button_new_with_label_from_widget(group, name);        
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), active);
    gtk_widget_show(radio);
    return radio;   
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
		
	checkDummy = startdlg_create_check_button(_("Dummy write"), GB_DUMMY);
	checkEject = startdlg_create_check_button(_("Eject disk"), GB_EJECT);	
	checkFastErase = startdlg_create_check_button(_("Fast blank"), GB_FAST_BLANK);		
	checkBurnFree = startdlg_create_check_button(_("BurnFree"), GB_BURNFREE);	
    radioBurnDisk = startdlg_create_radio_button(_("Burn the disk"), 
        NULL, !preferences_get_bool(GB_CREATEISOONLY)); 
	radioISOOnly = startdlg_create_radio_button(_("Create ISO only"), 
        GTK_RADIO_BUTTON(radioBurnDisk), preferences_get_bool(GB_CREATEISOONLY));	
	g_signal_connect(G_OBJECT(radioISOOnly), "toggled", (GCallback)startdlg_create_iso_toggled, NULL);
	checkForce = startdlg_create_check_button(_("Force"), GB_FORCE);		
	checkFinalize = startdlg_create_check_button(_("Finalize"), GB_FINALIZE);	
	checkFastFormat = startdlg_create_check_button(_("Fast"), GB_FAST_FORMAT);
    checkJoliet = startdlg_create_check_button(_("Joliet"), GB_JOLIET);
    checkRockRidge = startdlg_create_check_button(_("Rock Ridge"), GB_ROCKRIDGE);
    checkOnTheFly = startdlg_create_check_button(_("On The Fly"), GB_ONTHEFLY);
	
	GtkWidget *optmenWriteMode = glade_xml_get_widget(startdlg_xml, widget_startdlg_writemode);	
	GtkWidget *optmenReader = glade_xml_get_widget(startdlg_xml,widget_startdlg_reader);
	
	gchar* mode = preferences_get_string(GB_WRITE_MODE);
	gbcommon_set_option_menu_selection(GTK_OPTION_MENU(optmenWriteMode), mode);	
	g_free(mode);
    
    labelFileSystem = gtk_label_new(_("<b>File system</b>"));
    gtk_label_set_use_markup(GTK_LABEL(labelFileSystem), TRUE);
    gtk_widget_show(labelFileSystem);
    gtk_misc_set_alignment(GTK_MISC(labelFileSystem), 0, 0);
	
	GtkTable* table = GTK_TABLE(glade_xml_get_widget(startdlg_xml, "table3"));
		
	switch(burntype)
	{		
		case blank_cdrw:		
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_reader));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_modelabel));
			gtk_widget_hide(optmenWriteMode);
			gtk_table_attach(table, checkEject, 0, 2, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkFastErase, 2, 4, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gdvdmode = FALSE;
			break;				
		case create_audio_cd:
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_reader));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
			gtk_table_attach(table, checkEject, 0, 2, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkDummy, 2, 4, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkBurnFree, 0, 2, 7, 8, TABLE_ATTACH_OPTIONS_2);
            /*gtk_table_attach(table, checkOnTheFly, 2, 4, 7, 8, TABLE_ATTACH_OPTIONS);*/
		case create_mixed_cd:
			gdvdmode = FALSE;
			break;
		case burn_cd_image:			
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_reader));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
			gtk_table_attach(table, checkEject, 0, 2, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkDummy, 2, 4, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkBurnFree, 0, 2, 7, 8, TABLE_ATTACH_OPTIONS_2);
			gdvdmode = FALSE;
			break;
		case burn_dvd_image:			
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_reader));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_modelabel));
			gtk_widget_hide(optmenWriteMode);
			gtk_table_attach(table, checkEject, 0, 2, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gdvdmode = TRUE;
			break;
		case create_video_cd:			
			gdvdmode = FALSE;
			break;
		case create_data_cd:
            gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_reader));
            gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
            gtk_table_attach(table, radioBurnDisk, 0, 4, 6, 7, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, checkEject, 0, 2, 7, 8, TABLE_ATTACH_OPTIONS_3);
            gtk_table_attach(table, checkDummy, 2, 4, 7, 8, TABLE_ATTACH_OPTIONS_3);        
            gtk_table_attach(table, checkBurnFree, 0, 2, 8, 9, TABLE_ATTACH_OPTIONS_3);
            gtk_table_attach(table, checkOnTheFly, 2, 4, 8, 9, TABLE_ATTACH_OPTIONS_3);
            gtk_table_attach(table, radioISOOnly, 0, 4, 9, 10, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, labelFileSystem, 0, 4, 10, 11, TABLE_ATTACH_OPTIONS_1);            
            gtk_table_attach(table, checkJoliet, 0, 2, 11, 12, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, checkRockRidge, 2, 4, 11, 12, TABLE_ATTACH_OPTIONS_2);
            g_signal_emit_by_name(radioISOOnly, "toggled", radioISOOnly, NULL);
            gdvdmode = FALSE;
            break;		
        case append_data_cd:        
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_reader));
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
			gtk_table_attach(table, checkEject, 0, 2, 7, 8, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkDummy, 2, 4, 7, 8, TABLE_ATTACH_OPTIONS_2);		
			gtk_table_attach(table, checkBurnFree, 0, 2, 8, 9, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, checkOnTheFly, 2, 4, 8, 9, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, labelFileSystem, 0, 4, 9, 10, TABLE_ATTACH_OPTIONS_1);            
            gtk_table_attach(table, checkJoliet, 0, 2, 10, 11, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkRockRidge, 2, 4, 10, 11, TABLE_ATTACH_OPTIONS_2);
			g_signal_emit_by_name(radioISOOnly, "toggled", radioISOOnly, NULL);
			gdvdmode = FALSE;
			break;
		case copy_audio_cd:
			gtk_table_attach(table, checkEject, 0, 2, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkDummy, 2, 4, 6, 7, TABLE_ATTACH_OPTIONS_2);	
			gtk_table_attach(table, checkBurnFree, 0, 2, 7, 8, TABLE_ATTACH_OPTIONS_2);		
			gdvdmode = FALSE;
			break;
		case copy_data_cd:		
            gtk_table_attach(table, radioBurnDisk, 0, 4, 6, 7, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, checkEject, 0, 2, 7, 8, TABLE_ATTACH_OPTIONS_3);
            gtk_table_attach(table, checkDummy, 2, 4, 7, 8, TABLE_ATTACH_OPTIONS_3);        
            gtk_table_attach(table, checkBurnFree, 0, 2, 8, 9, TABLE_ATTACH_OPTIONS_3);
            gtk_table_attach(table, radioISOOnly, 0, 4, 9, 10, TABLE_ATTACH_OPTIONS_2);
			g_signal_emit_by_name(radioISOOnly, "toggled", radioISOOnly, NULL);
			gdvdmode = FALSE;
			break;
		case format_dvdrw:
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_modelabel));
			gtk_widget_hide(optmenWriteMode);
			gtk_widget_hide(optmenReader);
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
			gtk_table_attach(table, checkEject, 0, 2, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkForce, 2, 4, 6, 7, TABLE_ATTACH_OPTIONS_2);
			gtk_table_attach(table, checkFastFormat, 0, 2, 7, 8, TABLE_ATTACH_OPTIONS_2);
			gdvdmode = TRUE;
			break;
		case create_data_dvd:
            gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_modelabel));
            gtk_widget_hide(optmenWriteMode);
            gtk_widget_hide(optmenReader);
            gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
            gtk_table_attach(table, radioBurnDisk, 0, 4, 6, 7, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, checkEject, 0, 2, 7, 8, TABLE_ATTACH_OPTIONS_3);
            gtk_table_attach(table, checkDummy, 2, 4, 7, 8, TABLE_ATTACH_OPTIONS_3);        
            gtk_table_attach(table, checkFinalize, 0, 2, 8, 9, TABLE_ATTACH_OPTIONS_3);
            gtk_table_attach(table, radioISOOnly, 0, 4, 9, 10, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, labelFileSystem, 0, 4, 10, 11, TABLE_ATTACH_OPTIONS_1);            
            gtk_table_attach(table, checkJoliet, 0, 2, 11, 12, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, checkRockRidge, 2, 4, 11, 12, TABLE_ATTACH_OPTIONS_2);
            g_signal_emit_by_name(radioISOOnly, "toggled", radioISOOnly, NULL);
            gdvdmode = TRUE;
            break;
        case append_data_dvd:
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_modelabel));
			gtk_widget_hide(optmenWriteMode);
			gtk_widget_hide(optmenReader);
			gtk_widget_hide(glade_xml_get_widget(startdlg_xml, widget_startdlg_readlabel));
            gtk_table_attach(table, checkEject, 0, 2, 7, 8, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, checkDummy, 2, 4, 7, 8, TABLE_ATTACH_OPTIONS_2);        
            gtk_table_attach(table, checkFinalize, 0, 2, 8, 9, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, labelFileSystem, 0, 4, 9, 10, TABLE_ATTACH_OPTIONS_1);            
            gtk_table_attach(table, checkJoliet, 0, 2, 10, 11, TABLE_ATTACH_OPTIONS_2);
            gtk_table_attach(table, checkRockRidge, 2, 4, 10, 11, TABLE_ATTACH_OPTIONS_2);
            g_signal_emit_by_name(radioISOOnly, "toggled", radioISOOnly, NULL);
            gdvdmode = TRUE;
			break;
		default:
			break;
	};

	GtkWidget* spinSpeed = glade_xml_get_widget(startdlg_xml, widget_startdlg_speed);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinSpeed), 
        preferences_get_int(gdvdmode ? GB_DVDWRITE_SPEED : GB_CDWRITE_SPEED));
    
    gbcommon_center_window_on_parent(dlg);		
	return dlg;
}


void 
startdlg_delete(GtkWidget* self)
{	
	GB_LOG_FUNC
	
	gtk_widget_destroy(checkDummy); checkDummy = NULL;
	gtk_widget_destroy(checkEject); checkEject = NULL;
	gtk_widget_destroy(checkFastErase); checkFastErase = NULL;
	gtk_widget_destroy(checkBurnFree); checkBurnFree = NULL;
    gtk_widget_destroy(radioBurnDisk); radioBurnDisk = NULL;
	gtk_widget_destroy(radioISOOnly); radioISOOnly = NULL;
	gtk_widget_destroy(checkForce); checkForce = NULL;
	gtk_widget_destroy(checkFinalize); checkFinalize = NULL;
	gtk_widget_destroy(checkFastFormat); checkFastFormat = NULL;
    gtk_widget_destroy(checkJoliet); checkJoliet = NULL;
	gtk_widget_destroy(checkRockRidge); checkRockRidge = NULL;
    gtk_widget_destroy(checkOnTheFly); checkOnTheFly = NULL;
    gtk_widget_destroy(labelFileSystem); labelFileSystem = NULL;
	
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_object_unref(startdlg_xml);
	startdlg_xml = NULL;
}


void /* libglade callback */
startdlg_on_ok_clicked(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC	
	g_return_if_fail(startdlg_xml != NULL);	
	
	GtkWidget* optmenReadDev = glade_xml_get_widget(startdlg_xml, widget_startdlg_reader);
	devices_save_optionmenu(GTK_OPTION_MENU(optmenReadDev), GB_READER);
	GtkWidget* optmenWriteDev = glade_xml_get_widget(startdlg_xml, widget_startdlg_writer);
	devices_save_optionmenu(GTK_OPTION_MENU(optmenWriteDev), GB_WRITER);
    preferences_set_int(gdvdmode ? GB_DVDWRITE_SPEED : GB_CDWRITE_SPEED, gtk_spin_button_get_value(
            GTK_SPIN_BUTTON(glade_xml_get_widget(startdlg_xml, widget_startdlg_speed))));
    preferences_set_bool(GB_BURNFREE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkBurnFree)));          
    preferences_set_bool(GB_FAST_BLANK, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkFastErase)));
    GtkWidget* optmenWriteMode = glade_xml_get_widget(startdlg_xml, widget_startdlg_writemode);
    gchar* text = gbcommon_get_option_menu_selection(GTK_OPTION_MENU(optmenWriteMode));
    preferences_set_string(GB_WRITE_MODE, text);
    g_free(text);
    preferences_set_bool(GB_FINALIZE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkFinalize)));
    preferences_set_bool(GB_FAST_FORMAT, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkFastFormat)));
    preferences_set_bool(GB_FORCE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkForce)));    
    preferences_set_bool(GB_DUMMY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkDummy)));    
	preferences_set_bool(GB_EJECT, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkEject)));	
	preferences_set_bool(GB_JOLIET, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkJoliet)));
    preferences_set_bool(GB_ROCKRIDGE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkRockRidge)));
    preferences_set_bool(GB_CREATEISOONLY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radioISOOnly)));  
    preferences_set_bool(GB_ONTHEFLY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkOnTheFly)));	
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
