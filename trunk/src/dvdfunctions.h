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
 * File: dvdfunctions.h
 * Created by: Christoffer Sørensen
 */

#ifndef _DVDFUNCTIONS_H_
#define _DVDFUNCTIONS_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "exec.h"

void dvdformat_add_args(ExecCmd * const dvdFormat);
void growisofs_add_args(ExecCmd * const growisofs,GtkTreeModel* datamodel);

#endif	/*_DVDFUNCTIONS_H_*/
