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
 * Copyright: luke_biddell@yahoo.com
 * Created on: Tue Apr  6 22:51:22 2004
 */

#include "progressdlg.h"
#include "gbcommon.h"
#include <glib/gprintf.h>
#include "preferences.h"
#include "media.h"

/* Progress dialog glade widget names */
static const gchar* const widget_progdlg = "progDlg";
static const gchar* const widget_progdlg_progbar = "progressbar6";
static const gchar* const widget_progdlg_output = "textview1";
static const gchar* const widget_progdlg_output_scroll = "scrolledwindow17";
static const gchar* const widget_progdlg_toggleoutputlabel = "label249";
static const gchar* const widget_progdlg_processtitle = "label295";
static const gchar* const widget_progdlg_processdescription = "label296";

static GladeXML* progdlg_xml = NULL;
static GtkProgressBar* progbar = NULL;
static GtkTextView* textview = NULL;
static GtkTextBuffer* textBuffer = NULL;
static GtkWidget* textviewScroll = NULL;

static gint x = 0;
static gint y = 0;
static gint timertag = 0;
static gint numberofexecs = 0;
static gint currentexec = -1;
static GCallback closefunction = NULL;


GtkWidget* 
progressdlg_new(const Exec* exec, GCallback callonprematureclose)
{		
	GB_LOG_FUNC
    closefunction = callonprematureclose;
	numberofexecs = exec_count_operations(exec);
	currentexec = -1;
	progdlg_xml = glade_xml_new(glade_file, widget_progdlg, NULL);
	glade_xml_signal_autoconnect(progdlg_xml);		
	
	progbar = GTK_PROGRESS_BAR(glade_xml_get_widget(progdlg_xml, widget_progdlg_progbar));	
	textview = GTK_TEXT_VIEW(glade_xml_get_widget(progdlg_xml, widget_progdlg_output));
	textBuffer = gtk_text_view_get_buffer(textview);	
	textviewScroll = glade_xml_get_widget(progdlg_xml, widget_progdlg_output_scroll);
    
    GtkWidget* processtitle = glade_xml_get_widget(progdlg_xml, widget_progdlg_processtitle);      
    gchar* markup = g_strdup_printf("<b><big>%s</big></b>", exec->processtitle);
    gtk_label_set_text(GTK_LABEL(processtitle), markup);
    gtk_label_set_use_markup(GTK_LABEL(processtitle), TRUE);
    g_free(markup);
    
    GtkWidget* processdesc = glade_xml_get_widget(progdlg_xml, widget_progdlg_processdescription);
    gtk_label_set_text(GTK_LABEL(processdesc), exec->processdescription);
	
	GtkWidget* widget = glade_xml_get_widget(progdlg_xml, widget_progdlg);		
	gtk_window_get_size(GTK_WINDOW(widget), &x, &y);
    /* parent this window with the main window */
    gbcommon_center_window_on_parent(widget);
    gtk_main_iteration();
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


GtkWindow* 
progressdlg_get_window()
{
    GB_LOG_FUNC
    g_return_val_if_fail(progdlg_xml != NULL, NULL);
    return GTK_WINDOW(glade_xml_get_widget(progdlg_xml, widget_progdlg));
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
	gtk_progress_bar_set_fraction(progbar, fraction);
	gtk_progress_bar_set_text(progbar, percnt);		
	g_free(percnt);
}


void 
progressdlg_append_output(const gchar* output)
{
	GB_LOG_FUNC
	g_return_if_fail(textview != NULL);
	g_return_if_fail(textBuffer != NULL);
	
	GtkTextIter textIter;
	gtk_text_buffer_get_end_iter(textBuffer, &textIter);
	gtk_text_buffer_insert(textBuffer, &textIter, output, strlen(output));	
	gtk_text_iter_set_line(&textIter, gtk_text_buffer_get_line_count(textBuffer));
	gtk_text_view_scroll_to_iter(textview, &textIter, 0.0, TRUE, 0.0, 0.0);		
}


void 
progressdlg_on_close(GtkButton * button, gpointer user_data)
{
	GB_LOG_FUNC
	if(closefunction != NULL)
        closefunction();
}


void 
progressdlg_set_status(const gchar* status)
{
	GB_LOG_FUNC
	g_return_if_fail(progdlg_xml != NULL);
	
	GtkWidget* statuslabel = glade_xml_get_widget(progdlg_xml, "label297");
	g_return_if_fail(statuslabel != NULL);
    gchar* markup = g_strdup_printf("<i>%s</i>", status);	
	gtk_label_set_text(GTK_LABEL(statuslabel), markup);
	gtk_label_set_use_markup(GTK_LABEL(statuslabel), TRUE);	
    g_free(markup);
}


void 
progressdlg_on_output(GtkExpander* expander, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(progdlg_xml != NULL);
		
    if(gtk_expander_get_expanded(expander))
        gtk_window_resize(GTK_WINDOW(glade_xml_get_widget(progdlg_xml, widget_progdlg)), 1, 1);
//    else 
//        gtk_window_get_size(GTK_WINDOW(glade_xml_get_widget(progdlg_xml, widget_progdlg)), &x, &y);
    GB_TRACE("Window size is %d %d", x, y);
}


gboolean
progressdlg_on_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{	
	GB_LOG_FUNC
	progressdlg_on_close(NULL, user_data);
	return TRUE;
}


gboolean 
progressdlg_pulse_ontimer(gpointer userdata)
{
	/*GB_LOG_FUNC*/
	g_return_val_if_fail(progbar != NULL, TRUE);
	gtk_progress_bar_pulse(progbar);		
	return TRUE;
}


void 
progressdlg_pulse_start()
{
	GB_LOG_FUNC
	g_return_if_fail(progbar != NULL);
    gtk_progress_bar_set_text(progbar, "");
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
	gtk_text_buffer_get_end_iter(textBuffer, &endIter);		
	gtk_text_buffer_get_start_iter(textBuffer, &startIter);	
	gtk_text_buffer_delete(textBuffer, &startIter, &endIter);		
}


void 
progressdlg_finish(GtkWidget* self, const Exec* ex)
{
    GB_LOG_FUNC
    g_return_if_fail(self != NULL);
    
    if(ex->outcome != CANCELLED)
    {
        gtk_progress_bar_set_fraction(progbar, 1.0);
        gtk_progress_bar_set_text(progbar, " ");
        if(ex->outcome == COMPLETED)
        {
            progressdlg_set_status(_("Completed."));
            if(preferences_get_bool(GB_PLAY_SOUND))
                media_start_playing(PACKAGE_MEDIA_DIR"/BurnOk.wav");        
        }
        else if(ex->outcome == FAILED) 
        {
            progressdlg_set_status(_("Failed."));
            if(preferences_get_bool(GB_PLAY_SOUND))
               media_start_playing(PACKAGE_MEDIA_DIR"/BurnFailed.wav");        
            if(ex->err != NULL)
                progressdlg_append_output(ex->err->message);
        }
        /* Scrub out he closefunction callback as whatever exec was doing is finished */
        closefunction = NULL;
        /* Now we wait for the user to close the dialog */
        gtk_dialog_run(GTK_DIALOG(self));
    }
}



