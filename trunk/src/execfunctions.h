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
#include "startdlg.h"

void cdda2wav_add_copy_args(ExecCmd *e);
void cdrecord_add_iso_args(ExecCmd *cmd, const gchar *iso);
void cdrecord_add_audio_args(ExecCmd *cmd);
void cdrecord_add_blank_args(ExecCmd *cmd);
void cdrecord_add_create_audio_cd_args(ExecCmd *e, const GList *audio_files);
void dvdformat_add_args(ExecCmd *cmd);
void growisofs_add_args(ExecCmd *e, StartDlg *start_dlg, const gchar *arguments_file, const gchar *msinfo);
void growisofs_add_iso_args(ExecCmd *cmd, const gchar *iso);
void mkisofs_add_args(ExecCmd *e, StartDlg *start_dlg, const gchar *arguments_file, const gchar *msinfo, const gboolean calculate_size);
void mkisofs_add_calc_iso_size_args(ExecCmd *e, const gchar *iso);
void readcd_add_copy_args(ExecCmd *e, const gchar *iso);
void cdrdao_add_copy_args(ExecCmd *cmd);
void cdrdao_add_image_args(ExecCmd *cmd, const gchar *toc_or_cue);
void gstreamer_add_args(ExecCmd *cmd, const gchar *from, const gchar *to);
void md5sum_add_args(ExecCmd *cmd, const gchar *md5);
void dd_add_copy_args(ExecCmd *e, const gchar *iso);

#endif	/*_EXECFUNCTIONS_H_*/
