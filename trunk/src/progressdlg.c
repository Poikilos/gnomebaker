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
#include "gblibnotify.h"

/* Progress dialog glade widget names */
static const gchar* const widget_progdlg = "progDlg";
static const gchar* const widget_progdlg_progbar = "progressbar6";
static const gchar* const widget_progdlg_output = "textview1";
static const gchar* const widget_progdlg_output_scroll = "scrolledwindow17";
static const gchar* const widget_progdlg_toggleoutputlabel = "label297";
static const gchar* const widget_progdlg_processtitle = "label295";
static const gchar* const widget_progdlg_processdescription = "label296";

static GladeXML* progdlg_xml = NULL;
static GtkProgressBar* progbar = NULL;
static GtkTextView* textview = NULL;
static GtkTextBuffer* textBuffer = NULL;
static GtkWidget* textviewScroll = NULL;
static GtkLabel* statuslabel = NULL;
static GtkWindow* parentwindow = NULL;
static gchar* originalparentwindowtitle = NULL;
static GdkPixbuf* originalparenticon = NULL;
static gint iconindex = 0;
static gint timertag = 0;
static gint numberofexecs = 0;
static gint currentexec = -1;
static GCallback closefunction = NULL;
static GHashTable* statusicons = NULL;
static gdouble approximationinterval = 0.0;
static gdouble approximationfraction = 0.0;
static GTimer* timer = NULL;
static gboolean scrolloutput = TRUE;


static void 
progressdlg_set_icon(const gint index) 
{   
    GB_LOG_FUNC
    g_return_if_fail(index >= 0  && index <= 13);
    
    /*GB_TRACE("progressdlg_set_icon - using icon [%d]\n", index);*/
    GdkPixbuf* icon = (GdkPixbuf*)g_hash_table_lookup(statusicons, (gpointer)index);
    gtk_window_set_icon(parentwindow, icon);
    gtk_window_set_icon(progressdlg_get_window(), icon);
}


static gchar* 
progressdlg_format_time(gint seconds)
{
    GB_LOG_FUNC   
    
    const gint remainingseconds = seconds % 60;
    const gint minutes = (seconds - remainingseconds) / 60;
    if(minutes > 1)
        return g_strdup_printf(_("%d minutes %d seconds"), minutes, remainingseconds);
    else if(minutes > 0)
        return g_strdup_printf(_("%d minute %d seconds"), minutes, remainingseconds);
    else 
        return g_strdup_printf(_("%d seconds"), seconds);
}


GtkWidget* 
progressdlg_new(const Exec* exec, GtkWindow* parent, GCallback callonprematureclose)
{		
	GB_LOG_FUNC
    g_return_val_if_fail(exec != NULL, NULL);
    g_return_val_if_fail(parent != NULL, NULL);
    
    iconindex = 0;
    closefunction = callonprematureclose;
	numberofexecs = exec_count_operations(exec);
	currentexec = -1;
    timertag = 0;
    approximationfraction = 0.0;
    approximationinterval = 0.0;
    scrolloutput = preferences_get_bool(GB_SCROLL_OUTPUT);
    
    if(statusicons == NULL) 
    {
        statusicons = g_hash_table_new(g_direct_hash, g_direct_equal);
        gint i = 0;     
        for(; i < 14; ++i)
        {
            gchar* filename = g_strdup_printf(IMAGEDIR"/state%.2d.png", i);
            GB_TRACE("progressdlg_new - loading icon [%s]\n", filename);
            GdkPixbuf* icon = gdk_pixbuf_new_from_file(filename, NULL);
            g_hash_table_insert(statusicons, (gpointer)i, icon);
            g_free(filename);    
        }
    }
    
	progdlg_xml = glade_xml_new(glade_file, widget_progdlg, NULL);
	glade_xml_signal_autoconnect(progdlg_xml);		
    GtkWidget* widget = glade_xml_get_widget(progdlg_xml, widget_progdlg); 
	
	progbar = GTK_PROGRESS_BAR(glade_xml_get_widget(progdlg_xml, widget_progdlg_progbar));	
    gtk_progress_bar_set_text(progbar, " ");
	textview = GTK_TEXT_VIEW(glade_xml_get_widget(progdlg_xml, widget_progdlg_output));
	textBuffer = gtk_text_view_get_buffer(textview);	
	textviewScroll = glade_xml_get_widget(progdlg_xml, widget_progdlg_output_scroll);
    statuslabel = GTK_LABEL(glade_xml_get_widget(progdlg_xml, widget_progdlg_toggleoutputlabel));
    
    GtkWidget* processtitle = glade_xml_get_widget(progdlg_xml, widget_progdlg_processtitle);      
    gchar* markup = g_markup_printf_escaped("<b><big>%s</big></b>", exec->processtitle);
    gtk_label_set_text(GTK_LABEL(processtitle), markup);
    gtk_label_set_use_markup(GTK_LABEL(processtitle), TRUE);
    g_free(markup);
    
    GtkWidget* processdesc = glade_xml_get_widget(progdlg_xml, widget_progdlg_processdescription);
    gtk_label_set_text(GTK_LABEL(processdesc), exec->processdescription);
    
    parentwindow = parent;
    originalparentwindowtitle = g_strdup(gtk_window_get_title(parentwindow));
    originalparenticon = gtk_window_get_icon(parentwindow);
    gdk_pixbuf_ref(originalparenticon);
	progressdlg_set_icon(0);
    /* Center the window on the main window and then pump the events so it's
     * visible quickly ready for feedback from the exec layer */
    gbcommon_center_window_on_parent(widget);
    while(gtk_events_pending())
        gtk_main_iteration();
        
    timer = g_timer_new();        
    
    return widget;
}


void 
progressdlg_delete(GtkWidget* self)
{
	GB_LOG_FUNC
    g_return_if_fail(self != NULL);        
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_object_unref(progdlg_xml);	
	progbar = NULL;
	textview = NULL;
	textBuffer = NULL;
	textviewScroll = NULL;
	progdlg_xml = NULL;
    statuslabel = NULL;
    gtk_window_set_title(parentwindow, originalparentwindowtitle);
    g_free(originalparentwindowtitle);
    originalparentwindowtitle = NULL;
    g_timer_destroy(timer);
    gtk_window_set_icon(parentwindow, originalparenticon);
    gdk_pixbuf_unref(originalparenticon);
    originalparenticon = NULL;
    timer = NULL;
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
	/*gtk_progress_bar_set_text(progbar, percnt);		*/
    gtk_window_set_title(parentwindow, percnt);
    g_free(percnt);
    
    int i = 0;
    for(; i <= 11; ++i)
    {
        const gfloat lowerbound = (1.0/11.0) * (gfloat)i;
        const gfloat upperbound = (1.0/11.0) * (gfloat)(i + 1);
        /*GB_TRACE("progressdlg_set_fraction - fraction is [%f] lowerbound is [%f] upperbound is [%f]\n", 
            fraction, lowerbound, upperbound);*/
        if(fraction > lowerbound && fraction < upperbound && i > iconindex)
        {
            iconindex = i;
            progressdlg_set_icon(i);
            break;
        }
    }
    
    if(fraction > 0.0)
    {
        gdouble time = (g_timer_elapsed(timer, NULL) / fraction) * (1.0 - fraction); 
        gchar* formatted = progressdlg_format_time((gint)time + 1.0);
        gchar* text = g_strdup_printf(_("%s remaining"), formatted);
        gtk_progress_bar_set_text(progbar, text); 
        g_free(formatted);
        g_free(text);
    }
}


void 
progressdlg_append_output(const gchar* output)
{
    /*GB_LOG_FUNC*/
	g_return_if_fail(textview != NULL);
	g_return_if_fail(textBuffer != NULL);
    
	GtkTextIter textIter;
    gtk_text_buffer_get_end_iter(textBuffer, &textIter);
	gtk_text_buffer_insert(textBuffer, &textIter, output, strlen(output));	
	if(scrolloutput)
	{
		gtk_text_buffer_get_end_iter(textBuffer, &textIter);
		GtkTextMark* mark = gtk_text_buffer_create_mark(textBuffer, "end of buffer", &textIter, TRUE);
		gtk_text_view_scroll_to_mark(textview, mark, 0.1, FALSE, 0.0, 0.0);
		gtk_text_buffer_delete_mark(textBuffer, mark);
	}
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
    gchar* markup = g_markup_printf_escaped("<i>%s</i>", status);	
	gtk_label_set_text(statuslabel, markup);
	gtk_label_set_use_markup(statuslabel, TRUE);	
    g_free(markup);
}


void 
progressdlg_on_output(GtkExpander* expander, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(progdlg_xml != NULL);
    GtkWidget* label = gtk_expander_get_label_widget(expander);
    gtk_label_set_text(GTK_LABEL(label), 
        gtk_expander_get_expanded(expander) ? _("Show output"): _("Hide output"));
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
    /*++iconindex;
    if(iconindex > 11) 
        iconindex = 0;
    progressdlg_set_icon(iconindex);*/
	return TRUE;
}


void 
progressdlg_pulse_start()
{
	GB_LOG_FUNC
	g_return_if_fail(progbar != NULL);
    gtk_progress_bar_set_text(progbar, " ");
	gtk_progress_bar_set_pulse_step(progbar, 0.01);		
	timertag = gtk_timeout_add(40, (GtkFunction)progressdlg_pulse_ontimer, NULL);
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
    
    if(timer != NULL)
        g_timer_start(timer);
}


void 
progressdlg_finish(GtkWidget* self, const Exec* ex)
{
    GB_LOG_FUNC
    g_return_if_fail(self != NULL);       
    
    if(ex->outcome != CANCELLED)
    {
        gtk_progress_bar_set_fraction(progbar, 1.0);
        gchar* formatted = progressdlg_format_time((gint)g_timer_elapsed(timer, NULL));
        gchar* text = g_strdup_printf(_("Elapsed time %s"), formatted);
        gtk_progress_bar_set_text(progbar, text); 
        g_free(formatted);
        g_free(text);
        if(ex->outcome == COMPLETED)
        {
            const gchar* completed = _("Completed");
            progressdlg_set_status(completed);
            gtk_window_set_title(parentwindow, completed);
            if(preferences_get_bool(GB_PLAY_SOUND))
                media_start_playing(MEDIADIR"/BurnOk.wav");
            progressdlg_set_icon(12);
            gblibnotify_notification(completed, _("Gnomebaker completed successfully!"));
        }
        else if(ex->outcome == FAILED) 
        {
            const gchar* failed = _("Failed");
            progressdlg_set_status(failed);
            gtk_window_set_title(parentwindow, failed);
            if(preferences_get_bool(GB_PLAY_SOUND))
               media_start_playing(MEDIADIR"/BurnFailed.wav");
            if(ex->err != NULL)
                progressdlg_append_output(ex->err->message);
            progressdlg_set_icon(13);
            gblibnotify_notification(failed, _("Gnomebaker <b>failed</b> to complete the action!"));
        }
        /* Scrub out he closefunction callback as whatever exec was doing is finished */
        closefunction = NULL;
        /* Now we wait for the user to close the dialog */
        gtk_dialog_run(GTK_DIALOG(self));
    }
}


static gboolean 
progressdlg_approximation_ontimer(gpointer userdata)
{
    /*GB_LOG_FUNC*/
    approximationfraction = approximationfraction + approximationinterval;
    if(approximationfraction > (1.0 - approximationinterval))
        approximationfraction = 0.99;                 
    progressdlg_set_fraction(approximationfraction);
    return TRUE;
}


void 
progressdlg_start_approximation(gint seconds)
{    
    GB_LOG_FUNC
    g_return_if_fail(progbar != NULL);
    
    gint timeout = (seconds * 1000) / 100;
    approximationinterval = 1.0 / (((gdouble)seconds) * (1000/timeout));        
    GB_TRACE("progressdlg_start_approximation - seconds [%d] timeout [%d] interval [%f]\n", 
            seconds, timeout, approximationinterval);
    timertag = gtk_timeout_add(timeout, (GtkFunction)progressdlg_approximation_ontimer, NULL);
}


void 
progressdlg_stop_approximation()
{    
    GB_LOG_FUNC
    g_return_if_fail(progbar != NULL);
    gtk_timeout_remove(timertag);
    timertag = 0;    
    
    /* now wind the progress bar to 100% */
    const gdouble remaining = 1.0 - approximationfraction;   
    const gint iterations = 2000 / 40;
    const gdouble portion = remaining / (gdouble)iterations;
    gint i = 0;
    for(; i < iterations; ++i)
    {
        approximationfraction += portion;
        progressdlg_set_fraction(approximationfraction);
        while(gtk_events_pending())
            gtk_main_iteration();
        g_usleep(40000);
    }
    approximationfraction = 0.0;
    approximationinterval = 0.0;
}

