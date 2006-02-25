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
 * File: splashdlg.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Fri Apr  9 21:07:18 2004
 */

#include "splashdlg.h"
#include "gbcommon.h"


/* Splash dialog glade widget names */
static const gchar *const widget_splashdlg = "splashWnd";
static const gchar *const widget_splashdlg_label = "splashLabel";


static GladeXML *splashdlg_xml = NULL;


GtkWidget* 
splashdlg_new(void)
{
	GB_LOG_FUNC	
	splashdlg_xml = glade_xml_new(glade_file, widget_splashdlg, NULL);
	glade_xml_signal_autoconnect(splashdlg_xml);		
	GtkWidget *dlg = glade_xml_get_widget(splashdlg_xml, widget_splashdlg);	
	
	gbcommon_start_busy_cursor(dlg);	
	
	return dlg;
}


void 
splashdlg_delete(GtkWidget *self)
{
	GB_LOG_FUNC
	gbcommon_end_busy_cursor(self);
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_object_unref(splashdlg_xml);
	splashdlg_xml = NULL;
}


void 
splashdlg_set_text(const gchar *text)
{
#if !defined(__linux__)    
	GB_LOG_FUNC	
	g_return_if_fail(splashdlg_xml != NULL);
	GB_TRACE("splashdlg_set_text - [%s]\n", text);
	GtkWidget *label = glade_xml_get_widget(splashdlg_xml, widget_splashdlg_label);
	gtk_label_set_text(GTK_LABEL(label), text);	
	g_main_context_iteration(NULL, TRUE);
#endif
}
