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
 * File: filebrowser.h
 * Created by: luke_biddell@yahoo.com
 * Created on: Sun Mar 21 20:02:51 2004
 */

#ifndef _FILEBROWSER_H_
#define _FILEBROWSER_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

void 
filebrowser_refresh (void);
void
filebrowser_setup_tree_and_list (GtkTreeView * dirtree, GtkTreeView * filelist);
void
filebrowser_on_drag_data_get (GtkWidget * widget,
			      GdkDragContext * context,
			      GtkSelectionData * selection_data,
			      guint info, guint time, gpointer data);

#endif	/*_FILEBROWSER_H_*/
