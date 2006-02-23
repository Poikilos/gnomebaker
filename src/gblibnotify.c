/*
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
 /*
 * File: gblibnotify.c
 * Copyright: Milen Dzhumerov <gamehack@1nsp1r3d.co.uk> Richard Hughes <richard@hughsie.com>
 */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gnome.h>
#include "gblibnotify.h"
#include "gbcommon.h"

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#include <libnotify/notification.h>
#endif

static const gint timeout_seconds = 5;
static const gint MILISECONDS_IN_SECOND = 1000;

#ifdef HAVE_LIBNOTIFY    
#if (LIBNOTIFY_VERSION_MINOR >= 3)
static NotifyNotification *global_notify = NULL;
#elif (LIBNOTIFY_VERSION_MINOR == 2)
static NotifyHandle *global_notify = NULL;
#endif
#endif


/** Gets the position to "point" to (i.e. bottom middle of the icon)
 *
 *  @param	widget		the GtkWidget
 *  @param	x		X co-ordinate return
 *  @param	y		Y co-ordinate return
 *  @return			Success, return FALSE when no icon present
 */
static gboolean
glibnotify_get_widget_position(GtkWidget *widget, gint *x, gint *y)
{
    GB_LOG_FUNC
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(x != NULL, FALSE);
    g_return_val_if_fail(y != NULL, FALSE);

	gdk_window_get_origin (GDK_WINDOW (widget->window), x, y);

	*x += widget->allocation.x;
	*y += widget->allocation.y;
	*x += widget->allocation.width / 2;
	*y += widget->allocation.height;

	GB_TRACE("glibnotify_get_widget_position - widget position x=[%i], y=[%i]\n", *x, *y);
	return TRUE;
}

/** Convenience function to call libnotify
 *
 *  @param	subject		The subject text, e.g. "CD Burned"
 *  @param	content		The content text, e.g. "It finished successfully etc"
 *  @return			Success, if a notification is displayed.
 */
gboolean
gblibnotify_notification(const gchar *subject, const gchar *content)
{
    GB_LOG_FUNC
    g_return_val_if_fail(subject != NULL, FALSE);
    g_return_val_if_fail(content != NULL, FALSE);
#ifdef HAVE_LIBNOTIFY    
#if (LIBNOTIFY_VERSION_MINOR >= 3)
	gint x, y;
	global_notify = notify_notification_new (subject, content, "", NULL);
	/* not sure if we have to free the pixbuf since it could be used internally in libnotify
	   have to investigate further also need supply the full path of the filename 
       with the auto* magic stuff*/
    GdkPixbuf *icon_pixbuf = gdk_pixbuf_new_from_file(IMAGEDIR"/gnomebaker-48.png", NULL);
	notify_notification_set_icon_from_pixbuf (global_notify, icon_pixbuf);
    notify_notification_set_timeout (global_notify, timeout_seconds * MILISECONDS_IN_SECOND);

	/*if (point) 
    {
		glibnotify_get_widget_position (point, &x, &y);
		notify_notification_set_hint_int32 (global_notify, "x", x);
		notify_notification_set_hint_int32 (global_notify, "y", y);
	}*/

	if (!notify_notification_show(global_notify, NULL)) 
    {
		GB_TRACE("gb_libnotify_notification - failed to send notification [%s]\n", content);
		return FALSE;
	}
#elif (LIBNOTIFY_VERSION_MINOR == 2)
    NotifyHints *hints = NULL;
    gint x, y;

    /*if (point) 
    {
        glibnotify_get_widget_position (point, &x, &y);
        hints = notify_hints_new();
        notify_hints_set_int (hints, "x", x);
        notify_hints_set_int (hints, "y", y);
    }*/

    /* the path for the icon, this really should use the auto* stuff and not being hardcoded */
    NotifyIcon *icon = gdk_pixbuf_new_from_file(IMAGEDIR"/gnomebaker-48.png", NULL);
    global_notify = notify_send_notification (global_notify, /* replaces all */
               NULL,
               NOTIFY_URGENCY_NORMAL,
               subject, content,
               icon, /* icon */
               TRUE, timeout_seconds,
               hints, /* hints */
               NULL, /* no user data */
               0);   /* no actions */
    notify_icon_destroy(icon);
    if(!global_notify) 
    {
        GB_TRACE("gb_libnotify_notification - failed to send notification [%s]\n", content);
        return FALSE;
    }
#endif
#endif    
	return TRUE;
}

/** Clear the libnotify message
 *
 *  @return			If we removed the message.
 */
gboolean
gblibnotify_clear (void)
{    
    GB_LOG_FUNC
#ifdef HAVE_LIBNOTIFY    
#if (LIBNOTIFY_VERSION_MINOR >= 3)    
    if (global_notify)
        notify_notification_close (global_notify, NULL);
#elif (LIBNOTIFY_VERSION_MINOR == 2)	
    if (global_notify)
        notify_close (global_notify);
#endif
#endif
	return TRUE;
}

/** Initialiser for libnotify
 *
 *  @param	nice_name	The nice_name, e.g. "Gnomebaker"
 *  @return			If we initialised correctly.
 *
 *  @note	This function must be called before any calls to
 *		gb_libnotify_notification are made.
 */
gboolean
gblibnotify_init(const gchar *nice_name)
{
    GB_LOG_FUNC
    g_return_val_if_fail(nice_name != NULL, FALSE);
    gboolean ret = TRUE;
#ifdef HAVE_LIBNOTIFY        
    global_notify = NULL;
#if (LIBNOTIFY_VERSION_MINOR >= 3)        
	ret = notify_init (nice_name);
#elif (LIBNOTIFY_VERSION_MINOR == 2)    
    ret = notify_glib_init (nice_name, NULL);
#endif
    if(!ret)    
        GB_TRACE("gblibnotify_init - Failed to initialise libnotify");
#endif
	return ret;
}



