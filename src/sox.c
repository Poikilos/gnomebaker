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
 * File: sox.h
 * Created by: User <Email>
 * Created on: Sat Nov 20 16:15:44 2004
 */

#include "sox.h"
#include "preferences.h"
#include "gbcommon.h"
#include "progressdlg.h"


void
sox_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	progressdlg_set_status("<b>Converting wav to cd audio...</b>");
	progressdlg_increment_exec_number();
}


void
sox_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);
	
	progressdlg_append_output(buffer);
}


void 
sox_add_wav_args(ExecCmd* cmd, gchar* file, gchar** convertedfile)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);	
	g_return_if_fail(file != NULL);	
	
	exec_cmd_add_arg(cmd, "%s", "sox");
	exec_cmd_add_arg(cmd, "%s", "-V");	
	exec_cmd_add_arg(cmd, "%s", file);
	exec_cmd_add_arg(cmd, "%s", "-r 44100");
	exec_cmd_add_arg(cmd, "%s", "-c 2");
	exec_cmd_add_arg(cmd, "%s", "-w");
	
	gchar* trackdir = preferences_get_convert_audio_track_dir();
	
	gchar* filename = g_path_get_basename(file);
	if(filename != NULL)
	{
		*convertedfile = g_build_filename(trackdir, filename, NULL);
		
		exec_cmd_add_arg(cmd, "%s", *convertedfile);
		
		g_message("Converted file is [%s]", *convertedfile);
		g_free(filename);
	}
	
	g_free(trackdir);		
	
	cmd->preProc = sox_pre_proc;
	cmd->readProc = sox_read_proc;
}
