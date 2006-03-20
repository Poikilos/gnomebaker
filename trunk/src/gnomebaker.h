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
 * File: gnomebaker.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Tue Apr  6 23:28:51 2004
 */

#ifndef _GNOMEBAKER_H_
#define _GNOMEBAKER_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glade/glade.h>
#include <gnome.h>
#include "project.h"


GtkWidget *gnomebaker_new();
void gnomebaker_delete(GtkWidget *self);
gint gnomebaker_show_msg_dlg(GtkWindow *parent, GtkMessageType type, GtkButtonsType buttons,
		  			GtkButtonsType additional, const gchar  *message);
GladeXML *gnomebaker_getxml();
void gnomebaker_show_busy_cursor(gboolean is_busy);
void gnomebaker_on_add_dir(gpointer widget, gpointer user_data);
void gnomebaker_on_add_files(gpointer widget, gpointer user_data);
void gnomebaker_update_status(const gchar *status);
void gnomebaker_enable_widget(const gchar *widget_name, gboolean enabled);
void gnomebaker_on_help(gpointer widget, gpointer user_data);
GtkWindow *gnomebaker_get_window();
void gnomebaker_on_close_project(gpointer widget, Project *project);

#endif	/*_GNOMEBAKER_H_*/
