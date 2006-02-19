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
 * File: datacd.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun May  9 15:16:08 2004
 */

#ifndef _DATACD_H_
#define _DATACD_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glade/glade.h>
#include <gnome.h>


void datacd_new();
void datacd_clear();
void datacd_remove(GtkTreeView *widget);
void datacd_add_selection(GtkSelectionData* selection);
void datacd_import_session();
void datacd_open_project();
void datacd_save_project();
gchar* datacd_compilation_get_volume_id();

#endif	/*_DATACD_H_*/
