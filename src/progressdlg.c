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

/* The number of icons for indication of the status */
#define STATUS_ICONS_ARRAY_SIZE 14

/* Progress dialog glade widget names */
static const gchar *const widget_progdlg = "progDlg";
static const gchar *const widget_progdlg_progbar = "progressbar6";
static const gchar *const widget_progdlg_output = "textview1";
static const gchar *const widget_progdlg_output_scroll = "scrolledwindow17";
static const gchar *const widget_progdlg_toggle_output_label = "label297";
static const gchar *const widget_progdlg_process_title = "label295";
static const gchar *const widget_progdlg_process_description = "textview3";

static GladeXML *progdlg_xml = NULL;
static GtkProgressBar *progress_bar = NULL;
static GtkTextView *text_view = NULL;
static GtkTextBuffer *text_buffer = NULL;
static GtkWidget *text_view_scroll = NULL;
static GtkLabel *status_label = NULL;
static GtkWindow *parent_window = NULL;
static gchar *original_parent_window_title = NULL;
static GdkPixbuf *original_parent_icon = NULL;
static gint icon_index = 0;
static gint timer_tag = 0;
static gint number_of_execs = 0;
static gint current_exec = -1;
static GCallback close_function = NULL;
static GdkPixbuf **status_icons = NULL;
static gdouble approximation_interval = 0.0;
static gdouble approximation_fraction = 0.0;
static GTimer *timer = NULL;
static gboolean scroll_output = TRUE;


static void
progressdlg_set_icon(const gint index)
{
    GB_LOG_FUNC
    g_return_if_fail(index >= 0  && index <= STATUS_ICONS_ARRAY_SIZE - 1);

    /*GB_TRACE("progressdlg_set_icon - using icon [%d]\n", index);*/
    GdkPixbuf *icon = status_icons[index];
    gtk_window_set_icon(parent_window, icon);
    gtk_window_set_icon(progressdlg_get_window(), icon);
}


static gchar*
progressdlg_format_time(gint seconds, gboolean approximate)
{
    GB_LOG_FUNC
    
    const gint remaining_seconds = seconds % 60;
    const gint minutes = (seconds - remaining_seconds) / 60;
    if(!approximate)
    {
        if(minutes > 1)
            return g_strdup_printf(_("%d minutes %d seconds"), minutes, remaining_seconds);
        else if(minutes > 0)
            return g_strdup_printf(_("%d minute %d seconds"), minutes, remaining_seconds);
        else
            return g_strdup_printf(_("%d seconds"), seconds);
    } 
    else
    {
        if(minutes == 0)
            return g_strdup(_("1 minute"));
        else
            return g_strdup_printf(_("%d minutes"), minutes + 1);
    }
}


GtkWidget*
progressdlg_new(const Exec *exec, GtkWindow *parent, GCallback call_on_premature_close)
{
	GB_LOG_FUNC
    g_return_val_if_fail(exec != NULL, NULL);
    g_return_val_if_fail(parent != NULL, NULL);

    icon_index = 0;
    close_function = call_on_premature_close;
	number_of_execs = exec_count_operations(exec);
	current_exec = -1;
    timer_tag = 0;
    approximation_fraction = 0.0;
    approximation_interval = 0.0;
    scroll_output = preferences_get_bool(GB_SCROLL_OUTPUT);

    if(status_icons == NULL)
    {
      status_icons = (GdkPixbuf **) g_malloc(sizeof(GdkPixbuf*) * STATUS_ICONS_ARRAY_SIZE);
        gint i = 0;
        for(; i < STATUS_ICONS_ARRAY_SIZE; ++i)
        {
            gchar *file_name = g_strdup_printf(IMAGEDIR"/state%.2d.png", i);
            GB_TRACE("progressdlg_new - loading icon [%s]\n", file_name);
            GdkPixbuf *icon = gdk_pixbuf_new_from_file(file_name, NULL);
            status_icons[i] = icon;
            g_free(file_name);
        }
    }

	progdlg_xml = glade_xml_new(glade_file, widget_progdlg, NULL);
	glade_xml_signal_autoconnect(progdlg_xml);
    GtkWidget *widget = glade_xml_get_widget(progdlg_xml, widget_progdlg);

	progress_bar = GTK_PROGRESS_BAR(glade_xml_get_widget(progdlg_xml, widget_progdlg_progbar));
    gtk_progress_bar_set_text(progress_bar, " ");
	text_view = GTK_TEXT_VIEW(glade_xml_get_widget(progdlg_xml, widget_progdlg_output));
	text_buffer = gtk_text_view_get_buffer(text_view);
	text_view_scroll = glade_xml_get_widget(progdlg_xml, widget_progdlg_output_scroll);
    status_label = GTK_LABEL(glade_xml_get_widget(progdlg_xml, widget_progdlg_toggle_output_label));

    GtkWidget *process_title = glade_xml_get_widget(progdlg_xml, widget_progdlg_process_title);
    gchar *markup = g_markup_printf_escaped("<b><big>%s</big></b>", exec->process_title);
    gtk_label_set_text(GTK_LABEL(process_title), markup);
    gtk_label_set_use_markup(GTK_LABEL(process_title), TRUE);
    g_free(markup);

    GtkWidget *process_description = glade_xml_get_widget(progdlg_xml, widget_progdlg_process_description);
    GtkTextBuffer *buff = gtk_text_view_get_buffer(GTK_TEXT_VIEW(process_description));
    gtk_text_buffer_set_text(buff, exec->process_description, -1);

    parent_window = parent;
    original_parent_window_title = g_strdup(gtk_window_get_title(parent_window));
    original_parent_icon = gtk_window_get_icon(parent_window);
    gdk_pixbuf_ref(original_parent_icon);
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
progressdlg_delete(GtkWidget *self)
{
	GB_LOG_FUNC
    g_return_if_fail(self != NULL);
	gtk_widget_hide(self);
	gtk_widget_destroy(self);
	g_object_unref(progdlg_xml);
	progress_bar = NULL;
	text_view = NULL;
	text_buffer = NULL;
	text_view_scroll = NULL;
	progdlg_xml = NULL;
    status_label = NULL;
    gtk_window_set_title(parent_window, original_parent_window_title);
    g_free(original_parent_window_title);
    original_parent_window_title = NULL;
    g_timer_destroy(timer);
    gtk_window_set_icon(parent_window, original_parent_icon);
    gdk_pixbuf_unref(original_parent_icon);
    original_parent_icon = NULL;
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
	g_return_if_fail(progress_bar != NULL);

	/* work out the overall fraction using our knowledge of the
	   number of operations we are performing */
	fraction *= (1.0/(gfloat)number_of_execs);
	fraction += ((gfloat)current_exec *(1.0/(gfloat)number_of_execs));
	gchar *percnt = g_strdup_printf("%d%%",(gint)(fraction * 100));
	gtk_progress_bar_set_fraction(progress_bar, fraction);
	/*gtk_progress_bar_set_text(progress_bar, percnt);		*/
    gtk_window_set_title(parent_window, percnt);
    g_free(percnt);

    int i = 0;
    for(; i <= 11; ++i)
    {
        const gfloat lower_bound = (1.0/11.0) * (gfloat)i;
        const gfloat upper_bound = (1.0/11.0) * (gfloat)(i + 1);
        /*GB_TRACE("progressdlg_set_fraction - fraction is [%f] lower_bound is [%f] upper_bound is [%f]\n",
            fraction, lower_bound, upper_bound);*/
        if(fraction > lower_bound && fraction < upper_bound && i > icon_index)
        {
            icon_index = i;
            progressdlg_set_icon(i);
            break;
        }
    }

    if(fraction > 0.0)
    {
        gdouble time = (g_timer_elapsed(timer, NULL) / fraction) * (1.0 - fraction);
        gchar *formatted = progressdlg_format_time((gint)time + 1.0, TRUE);
        gchar *text = g_strdup_printf(_("Approximately %s remaining"), formatted);
        gtk_progress_bar_set_text(progress_bar, text);
        g_free(formatted);
        g_free(text);
    }
}


void
progressdlg_append_output(const gchar *output)
{
    /*GB_LOG_FUNC*/
	g_return_if_fail(text_view != NULL);
	g_return_if_fail(text_buffer != NULL);

	GtkTextIter text_iter;
    gtk_text_buffer_get_end_iter(text_buffer, &text_iter);
	gtk_text_buffer_insert(text_buffer, &text_iter, output, strlen(output));
	if(scroll_output)
	{
		gtk_text_buffer_get_end_iter(text_buffer, &text_iter);
		GtkTextMark *mark = gtk_text_buffer_create_mark(text_buffer, "end of buffer", &text_iter, TRUE);
		gtk_text_view_scroll_to_mark(text_view, mark, 0.1, FALSE, 0.0, 0.0);
		gtk_text_buffer_delete_mark(text_buffer, mark);
	}
}


void
progressdlg_on_close(GtkButton  *button, gpointer user_data)
{
	GB_LOG_FUNC
	if(close_function != NULL)
        close_function();
}


void
progressdlg_set_status(const gchar *status)
{
	GB_LOG_FUNC
    gchar *markup = g_markup_printf_escaped("<i>%s</i>", status);
	gtk_label_set_text(status_label, markup);
	gtk_label_set_use_markup(status_label, TRUE);
    g_free(markup);
}


void
progressdlg_on_output(GtkExpander *expander, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_if_fail(progdlg_xml != NULL);
    GtkWidget *label = gtk_expander_get_label_widget(expander);
    gtk_label_set_text(GTK_LABEL(label),
            gtk_expander_get_expanded(expander) ? _("Show output"): _("Hide output"));
}


gboolean
progressdlg_on_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	GB_LOG_FUNC
	progressdlg_on_close(NULL, user_data);
	return TRUE;
}


gboolean
progressdlg_pulse_ontimer(gpointer user_data)
{
	/*GB_LOG_FUNC*/
	g_return_val_if_fail(progress_bar != NULL, TRUE);
	gtk_progress_bar_pulse(progress_bar);
    /*++icon_index;
    if(icon_index > 11)
        icon_index = 0;
    progressdlg_set_icon(icon_index);*/
	return TRUE;
}


void
progressdlg_pulse_start()
{
	GB_LOG_FUNC
	g_return_if_fail(progress_bar != NULL);
    gtk_progress_bar_set_text(progress_bar, " ");
	gtk_progress_bar_set_pulse_step(progress_bar, 0.01);
	timer_tag = gtk_timeout_add(40, (GtkFunction)progressdlg_pulse_ontimer, NULL);
}


void
progressdlg_pulse_stop()
{
	GB_LOG_FUNC
	gtk_timeout_remove(timer_tag);
	timer_tag = 0;
}


void
progressdlg_increment_exec_number()
{
	GB_LOG_FUNC
	g_return_if_fail(text_view != NULL);
	g_return_if_fail(text_buffer != NULL);

	/* Force the fraction to be at the start of the current portion */
	if(++current_exec > 0)
		progressdlg_set_fraction(0.0);

	/* Clear out the text buffer so it only contains the current exec output */
	GtkTextIter end_iter, start_iter;
	gtk_text_buffer_get_end_iter(text_buffer, &end_iter);
	gtk_text_buffer_get_start_iter(text_buffer, &start_iter);
	gtk_text_buffer_delete(text_buffer, &start_iter, &end_iter);

/*    if(timer != NULL)
        g_timer_start(timer);*/
}


void
progressdlg_finish(GtkWidget *self, const Exec *ex)
{
    GB_LOG_FUNC
    g_return_if_fail(self != NULL);

    if(ex->outcome != CANCELLED)
    {
        gtk_progress_bar_set_fraction(progress_bar, 1.0);
        gchar *formatted = progressdlg_format_time((gint)g_timer_elapsed(timer, NULL), FALSE);
        gchar *text = g_strdup_printf(_("Elapsed time %s"), formatted);
        gtk_progress_bar_set_text(progress_bar, text);
        g_free(formatted);
        g_free(text);
        if(ex->outcome == COMPLETED)
        {
            const gchar *completed = _("Completed");
            progressdlg_set_status(completed);
            gtk_window_set_title(parent_window, completed);
            if(preferences_get_bool(GB_PLAY_SOUND))
                media_start_playing(MEDIADIR"/BurnOk.wav");
            progressdlg_set_icon(12);
            gblibnotify_notification(completed, ex->process_title);
        }
        else if(ex->outcome == FAILED)
        {
            const gchar *failed = _("Failed");
            progressdlg_set_status(failed);
            gtk_window_set_title(parent_window, failed);
            if(preferences_get_bool(GB_PLAY_SOUND))
               media_start_playing(MEDIADIR"/BurnFailed.wav");
            if(ex->err != NULL)
                progressdlg_append_output(ex->err->message);
            progressdlg_set_icon(13);
            gblibnotify_notification(failed, ex->process_title);
        }
        /* Scrub out he close_function callback as whatever exec was doing is finished */
        close_function = NULL;
        /* Now we wait for the user to close the dialog */
        gtk_dialog_run(GTK_DIALOG(self));
    }
}


static gboolean
progressdlg_approximation_ontimer(gpointer user_data)
{
    /*GB_LOG_FUNC*/
    approximation_fraction = approximation_fraction + approximation_interval;
    if(approximation_fraction > (1.0 - approximation_interval))
        approximation_fraction = 0.99;
    progressdlg_set_fraction(approximation_fraction);
    return TRUE;
}


void
progressdlg_start_approximation(gint seconds)
{
    GB_LOG_FUNC

    gint timeout = (seconds * 1000) / 100;
    approximation_interval = 1.0 / (((gdouble)seconds) * (1000/timeout));
    GB_TRACE("progressdlg_start_approximation - seconds [%d] timeout [%d] interval [%f]\n",
            seconds, timeout, approximation_interval);
    timer_tag = gtk_timeout_add(timeout, (GtkFunction)progressdlg_approximation_ontimer, NULL);
}


void
progressdlg_stop_approximation()
{
    GB_LOG_FUNC

    gtk_timeout_remove(timer_tag);
    timer_tag = 0;
    /* now wind the progress bar to 100% */
    const gdouble remaining = 1.0 - approximation_fraction;
    const gint iterations = 2000 / 40;
    const gdouble portion = remaining / (gdouble)iterations;
    gint i = 0;
    for(; i < iterations; ++i)
    {
        approximation_fraction += portion;
        progressdlg_set_fraction(approximation_fraction);
        while(gtk_events_pending())
            gtk_main_iteration();
        g_usleep(40000);
    }
    approximation_fraction = 0.0;
    approximation_interval = 0.0;
}

