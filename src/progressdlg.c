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
 * File: progressdlg.h
 * Created by: luke_biddell@yahoo.com
 * Created on: Tue Apr  6 22:51:22 2004
 */

#include "progressdlg.h"
#include "burn.h"
#include "gbcommon.h"
#include <glib/gprintf.h>


void progressdlg_on_stop(GtkButton* button, gpointer user_data);
void progressdlg_on_close(GtkButton* button, gpointer user_data);
void progressdlg_on_output(GtkButton* button, gpointer user_data);
gboolean progressdlg_on_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data);
gboolean progressdlg_pulse_ontimer(gpointer userdata);


GladeXML* progdlg_xml = NULL;
GtkProgressBar* progbar = NULL;
GtkTextView* textview = NULL;
GtkTextBuffer* textBuffer = NULL;
GtkWidget* textviewScroll = NULL;

gint x = 0;
gint y = 0;
gint timertag = 0;
gint numberofexecs = 0;
gint currentexec = -1;


GtkWidget* 
progressdlg_new(gint numberofoperations)
{		
	GB_LOG_FUNC
	numberofexecs = numberofoperations;
	currentexec = -1;
	progdlg_xml = glade_xml_new(glade_file, widget_progdlg, NULL);
	glade_xml_signal_autoconnect(progdlg_xml);		
	progressdlg_enable_close(FALSE);	
	
	progbar = GTK_PROGRESS_BAR(glade_xml_get_widget(progdlg_xml, widget_progdlg_progbar));	
	textview = GTK_TEXT_VIEW(glade_xml_get_widget(progdlg_xml, widget_progdlg_output));
	textBuffer = gtk_text_view_get_buffer(textview);	
	textviewScroll = glade_xml_get_widget(progdlg_xml, widget_progdlg_output_scroll);
	
	GtkWidget* widget = glade_xml_get_widget(progdlg_xml, widget_progdlg);		
	gtk_window_get_size(GTK_WINDOW(widget), &x, &y);
	return widget;
}


void 
progressdlg_delete(GtkWidget* self)
{
	GB_LOG_FUNC
	if(self != NULL && GTK_IS_DIALOG(self))
	{
		gtk_widget_hide(self);
		gtk_widget_destroy(self);
	}
	
	g_free(progdlg_xml);
	
	progbar = NULL;
	textview = NULL;
	textBuffer = NULL;
	textviewScroll = NULL;
	progdlg_xml = NULL;
}


void 
progressdlg_set_fraction(gfloat fraction)
{
	GB_LOG_FUNC
	g_return_if_fail(progbar != NULL);
	
	/* work out the overall fraction using our knowledge of the 
	   number of operations we are performing */
	fraction *= (1.0/(gfloat)numberofexecs);			
	fraction += ((gfloat)currentexec *(1.0/(gfloat)numberofexecs));
	
	gchar* percnt = g_strdup_printf("%d%%",(gint)(fraction * 100));
	
	gdk_threads_enter();
	gtk_progress_bar_set_fraction(progbar, fraction);
	gtk_progress_bar_set_text(progbar, percnt);	
	gdk_flush();
	gdk_threads_leave();		
	
	g_free(percnt);
}


void 
progressdlg_set_text(const gchar* text)
{
	GB_LOG_FUNC
	g_return_if_fail(progbar != NULL);
	
	gdk_threads_enter();
	gtk_progress_bar_set_text(progbar, text);	
	gdk_flush();
	gdk_threads_leave();
}


void 
progressdlg_append_output(const gchar* output)
{
	GB_LOG_FUNC
	g_return_if_fail(textview != NULL);
	g_return_if_fail(textBuffer != NULL);
	
	GtkTextIter textIter;

	gdk_threads_enter();		
	gtk_text_buffer_get_end_iter(textBuffer, &textIter);
	gtk_text_buffer_insert(textBuffer, &textIter, output, strlen(output));	
	gtk_text_iter_set_line(&textIter, gtk_text_buffer_get_line_count(textBuffer));
	gtk_text_view_scroll_to_iter(textview, &textIter, 0.0, TRUE, 0.0, 0.0);	
	gdk_flush();
	gdk_threads_leave();
}


void 
progressdlg_on_stop(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC
	burn_end_process();
}


void 
progressdlg_on_close(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC
	gtk_dialog_response(GTK_DIALOG(glade_xml_get_widget(progdlg_xml, widget_progdlg)), 
		GTK_RESPONSE_CLOSE);
}


void 
progressdlg_enable_close(gboolean enable)
{
	GB_LOG_FUNC
	g_return_if_fail(progdlg_xml != NULL);
	
	GtkWidget* closebutton = glade_xml_get_widget(progdlg_xml, widget_progdlg_close);
	g_return_if_fail(closebutton != NULL);
	
	GtkWidget* stopbutton = glade_xml_get_widget(progdlg_xml, widget_progdlg_stop);
	g_return_if_fail(stopbutton != NULL);
	
	/*gdk_threads_enter();*/
	gtk_widget_set_sensitive(closebutton, enable);
	gtk_widget_set_sensitive(stopbutton, !enable);
	/*gdk_threads_leave();*/	
}


void 
progressdlg_set_status(const gchar* status)
{
	GB_LOG_FUNC
	g_return_if_fail(progdlg_xml != NULL);
	
	GtkWidget* statuslabel = glade_xml_get_widget(progdlg_xml, "statusLabel");
	g_return_if_fail(statuslabel != NULL);
	
	gdk_threads_enter();
	gtk_label_set_text(GTK_LABEL(statuslabel), status);
	gtk_label_set_use_markup(GTK_LABEL(statuslabel), TRUE);
	gdk_flush();
	gdk_threads_leave();		
}


void 
progressdlg_on_output(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(progdlg_xml != NULL);
	g_return_if_fail(textview != NULL);
	g_return_if_fail(textviewScroll != NULL);
	
	static gboolean showing = TRUE;
	showing = !showing;

	if(!showing)
	{
		gtk_widget_show(GTK_WIDGET(textview));
		gtk_widget_show(textviewScroll);
	}
	else
	{
		gtk_widget_hide(GTK_WIDGET(textview));
		gtk_widget_hide(textviewScroll);		
		gtk_window_resize(GTK_WINDOW(
			glade_xml_get_widget(progdlg_xml, widget_progdlg)), x, y);
	}	
}


gboolean
progressdlg_on_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{	
	GB_LOG_FUNC
	progressdlg_on_stop(NULL, user_data);
	progressdlg_on_close(NULL, user_data);
	return TRUE;
}


void 
progressdlg_pulse_start()
{
	GB_LOG_FUNC
	g_return_if_fail(progbar != NULL);
	gtk_progress_bar_set_pulse_step(progbar, 0.01);		
	
	timertag = gtk_timeout_add(20, (GtkFunction)progressdlg_pulse_ontimer, NULL);	
}


void 
progressdlg_pulse_stop()
{
	GB_LOG_FUNC
	gtk_timeout_remove(timertag);
	timertag = 0;
}


gboolean 
progressdlg_pulse_ontimer(gpointer userdata)
{
	/*GB_LOG_FUNC*/
	g_return_val_if_fail(progbar != NULL, TRUE);
	
	gdk_threads_enter();
	gtk_progress_bar_pulse(progbar);	
	gdk_flush();
	gdk_threads_leave();	
	
	return TRUE;
}


void 
progressdlg_increment_exec_number()
{
	GB_LOG_FUNC
	g_return_if_fail(textview != NULL);
	g_return_if_fail(textBuffer != NULL);
	
	/* Force the fraction to be at the start of the current portion */
	if(++currentexec > 0)
		progressdlg_set_fraction(0.0);
	
	/* Clear out the text buffer so it only contains the current exec output */
	GtkTextIter endIter, startIter;

	gdk_threads_enter();			
	gtk_text_buffer_get_end_iter(textBuffer, &endIter);		
	gtk_text_buffer_get_start_iter(textBuffer, &startIter);	
	gtk_text_buffer_delete(textBuffer, &startIter, &endIter);	
	gdk_flush();
	gdk_threads_leave();
}


void 
progressdlg_reset_fraction(gfloat fraction)
{
	GB_LOG_FUNC
	g_return_if_fail(progbar != NULL);
	
	gdk_threads_enter();
	gtk_progress_bar_set_fraction(progbar, fraction);
	gdk_flush();
	gdk_threads_leave();	
}


void
progressdlg_dismiss()
{
	GB_LOG_FUNC
	g_return_if_fail(progdlg_xml != NULL);	
	GtkWidget* widget = glade_xml_get_widget(progdlg_xml, widget_progdlg);	
	g_return_if_fail(widget != NULL);	
	gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_CLOSE);
}
