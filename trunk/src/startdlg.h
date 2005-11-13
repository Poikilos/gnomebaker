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
 * File: startdlg.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Wed Apr  7 23:08:35 2004
 */

#ifndef _STARTDLG_H_
#define _STARTDLG_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "burn.h"

typedef struct 
{
    GtkDialog* dialog;
    GtkOptionMenu* reader;
    GtkOptionMenu* writer;
    GtkSpinButton* write_speed;
    GtkOptionMenu* write_mode;
    GtkCheckButton* dummy;
    GtkCheckButton* eject;    
    GtkCheckButton* fast_erase;
    GtkCheckButton* burn_free;
    GtkRadioButton* burn_disk; 
    GtkCheckButton* force_format;  
    GtkCheckButton* finalize;
    GtkCheckButton* fast_format;
    GtkCheckButton* joliet;
    GtkCheckButton* rock_ridge;
    GtkCheckButton* on_the_fly;    
    GtkRadioButton* iso_only;  
    GtkEntry* iso_file;
    GtkButton* browse;
    GtkEntry* volume_id;
    GtkEntry* app_id;
    GtkEntry* preparer;
    GtkEntry* publisher;
    GtkEntry* copyright;
    GtkEntry* abstract;
    GtkEntry* bibliography;
    gboolean gdvdmode;
} StartDlg;



StartDlg* startdlg_new(const BurnType burntype);
void startdlg_delete(StartDlg* self);


#endif	/*_STARTDLG_H_*/

