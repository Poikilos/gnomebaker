/***************************************************************************
 *            startdlgdvd.h
 *
 *  Sun Dec 26 13:32:03 2004
 *  Copyright  2004  Christoffer Sørensen
 *  Email christoffer@curo.dk
 ****************************************************************************/

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
 
#ifndef _STARTDLGDVD_H_
#define _STARTDLGDVD_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <glade/glade.h>
#include "burn.h"


GtkWidget* startdlgdvd_new(const BurnType burntype);
void startdlgdvd_delete(GtkWidget* self);


#endif	/*_STARTDLGDVD_H_*/
