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
 * File: execfunctions.h
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun Jan 11 17:22:09 2004
 */

#ifndef _EXECFUNCTIONS_H_
#define _EXECFUNCTIONS_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "exec.h"

void cdda2wav_add_copy_args (ExecCmd * e);
void cdrecord_add_iso_args (ExecCmd * const cdBurn, const gchar * const iso);
void cdrecord_add_audio_args (ExecCmd * const cdBurn);
void cdrecord_add_blank_args (ExecCmd * const cdBurn);
void cdrecord_add_create_audio_cd_args(ExecCmd* e, const GList* audiofiles);
void dvdformat_add_args(ExecCmd * const dvdFormat);
void growisofs_add_args(ExecCmd * const growisofs,GtkTreeModel* datamodel);
void growisofs_add_iso_args(ExecCmd * const growisofs,const gchar *iso);
gboolean mkisofs_add_args(ExecCmd* e, GtkTreeModel* datamodel, const gchar* iso);
void readcd_add_copy_args (ExecCmd * e, const gchar* iso);
void cdrdao_add_bin_args(ExecCmd* cmd, const gchar* const bin);
void generic_read_proc(void *ex, void *buffer);
void media_convert_add_args(ExecCmd * cmd, GSList* _elements);
void media_next_element();

#endif	/*_EXECFUNCTIONS_H_*/
