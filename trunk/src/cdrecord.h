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
 * File: cdrecord.h
 * Created by: luke_biddell@yahoo.com
 * Created on: Sun Jan 11 15:31:08 2004
 */

#ifndef _CDRECORD_H_
#define _CDRECORD_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "exec.h"

void cdrecord_add_iso_args (ExecCmd * const cdBurn, const gchar * const iso);
void cdrecord_add_audio_args (ExecCmd * const cdBurn);
void cdrecord_add_blank_args (ExecCmd * const cdBurn);
void cdrecord_add_create_audio_cd_args(ExecCmd* e, const GList* audiofiles);

#endif	/*_CDRECORD_H_*/
