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
 * File: audiocd.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Fri May  7 22:37:25 2004
 */

#ifndef _AUDIOCD_H_
#define _AUDIOCD_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glade/glade.h>
#include <gnome.h>


enum
{    
	AUDIOCD_COL_ICON = 0,
    AUDIOCD_COL_FILE,
	AUDIOCD_COL_DURATION,
	/*AUDIOCD_COL_SIZE,*/
	AUDIOCD_COL_ARTIST,
	AUDIOCD_COL_ALBUM,
	AUDIOCD_COL_TITLE,
    AUDIOCD_NUM_COLS
};


void audiocd_new();
void audiocd_clear();
void audiocd_remove();
void audiocd_add_selection(GtkSelectionData* selection);
void audiocd_import_session();
							  
							  
#endif	/*_AUDIOCD_H_*/
