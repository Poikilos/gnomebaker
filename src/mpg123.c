/*
 * File: mpg123.h
 * Created by: User <Email>
 * Created on: Fri Oct 22 22:19:29 2004
 */

#include "mpg123.h"
#include "preferences.h"
#include "gbcommon.h"
#include "progressdlg.h"


void mpg123_pre_proc(void *ex, void *buffer);
void mpg123_read_proc(void *ex, void *buffer);



void 
mpg123_add_mp3_args(ExecCmd* cmd, gchar* file, gchar** convertedfile)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);	
	g_return_if_fail(file != NULL);	
	
	/*exec_cmd_add_arg(cmd, "%s", "lame");
	exec_cmd_add_arg(cmd, "%s", "--decode");
	exec_cmd_add_arg(cmd, "%s", "--resample 44.1");
	exec_cmd_add_arg(cmd, "%s", "-ms");
	exec_cmd_add_arg(cmd, "%s", "--brief");	
	exec_cmd_add_arg(cmd, "%s", file);*/
	
	//mpg123 -v --rate 44100 --stereo -w honda.wav HondaServiceCall.mp3
	
	exec_cmd_add_arg(cmd, "%s", "mpg123");
	exec_cmd_add_arg(cmd, "%s", "-v");
	exec_cmd_add_arg(cmd, "%s", "--resync");
	exec_cmd_add_arg(cmd, "%s", "-r44100");
	exec_cmd_add_arg(cmd, "%s", "--stereo");
	
	
	gchar* trackdir = preferences_get_convert_audio_track_dir();
	
	gchar* filename = g_path_get_basename(file);
	if(filename != NULL)
	{
		*convertedfile = g_build_filename(trackdir, filename, NULL);
		gchar* suffix = strstr(*convertedfile, ".mp3");
		if(suffix != NULL)
			strncpy(suffix, ".wav", 4);
				
		exec_cmd_add_arg(cmd, "-w%s", *convertedfile);
		
		g_message("Converted file is [%s]", *convertedfile);
		g_free(filename);
		
		exec_cmd_add_arg(cmd, "%s", file);
	}
	
	g_free(trackdir);		
	
	cmd->preProc = mpg123_pre_proc;
	cmd->readProc = mpg123_read_proc;
}


void
mpg123_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	progressdlg_set_status("<b>Converting mp3 to cd audio...</b>");
	progressdlg_increment_exec_number();
}


void
mpg123_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);
/*	
	const gchar* frame = strstr(buffer, "Frame#");
	if(frame != NULL)
	{
		guint current, total;
		if(sscanf(frame, "%*s\t%d/%d", &current, &total) > 0)
		{
			g_message("track [%d] [%d]", current, total);		
			progressdlg_set_fraction((gfloat)current/(gfloat)total);
		}
	}
	else
	{
		progressdlg_append_output(buffer);
	}
*/
	const gchar* frame = strstr(buffer, "Frame#");
	if(frame != NULL)
	{
		guint current, remaining;		
		if(sscanf(frame, "%*s\t%d [%d]", &current, &remaining) > 0)
			progressdlg_set_fraction((gfloat)current/(gfloat)(current + remaining));
	}
	else
	{
		progressdlg_append_output(buffer);
	}	
}
