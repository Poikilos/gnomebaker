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

#ifndef _PROGRESSDLG_H_
#define _PROGRESSDLG_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <glade/glade.h>
#include "exec.h"

GtkWidget *progressdlg_new(const Exec *exec, GtkWindow *parent, GCallback call_on_premature_close);
void progressdlg_delete(GtkWidget *self);
void progressdlg_set_fraction(gfloat fraction);
void progressdlg_append_output(const gchar *output);
void progressdlg_set_status(const gchar *status);
void progressdlg_pulse_start();
void progressdlg_pulse_stop();
void progressdlg_increment_exec_number();
void progressdlg_finish(GtkWidget *self, const Exec *ex);
GtkWindow *progressdlg_get_window();
void progressdlg_start_approximation(gint seconds);
void progressdlg_stop_approximation();
void progressdlg_increment_total_exec_number();

#endif	/*_PROGRESSDLG_H_*/
