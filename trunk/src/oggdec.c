/*
 * File: ogg.h
 * Created by: User <Email>
 * Created on: Sat Aug 14 23:24:43 2004
 */

#include "oggdec.h"
#include "gbcommon.h"
#include "progressdlg.h"
#include "preferences.h"


void
oggdec_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	progressdlg_set_status("<b>Converting ogg to cd audio...</b>");
	progressdlg_increment_exec_number();
}


void
oggdec_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);
	
/*	
	const gchar* frame = strstr(buffer, "Time:");
	if(frame != NULL)
	{
		guint currentsecs, currentmins, totalmins, totalsecs;
		if(sscanf(frame, "Time: %d:%d.%*d [%*d:%*d.%*d] of %d:%d.%*d", 
				&currentmins, &currentsecs, &totalmins, &totalsecs) > 0)
		{
			g_message("track [%d] [%d] [%d] [%d]", currentmins, currentsecs, totalmins, totalsecs);		
			progressdlg_set_fraction(
				(gfloat)((currentmins * 60) + currentsecs) / 
				(gfloat)((totalmins * 60) + totalsecs));
		}
	}
*/
	const gchar* frame = strstr(buffer, "[");
	if(frame != NULL)
	{
		gfloat percentage;
		if(sscanf(frame, "[%f%%]", &percentage) > 0)
			progressdlg_set_fraction(percentage/100.0);
	}
	else
	{
		progressdlg_append_output(buffer);
	}
}


void 
oggdec_add_args(ExecCmd* cmd, gchar* file, gchar** convertedfile)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);	
	g_return_if_fail(file != NULL);	

/*	
	exec_cmd_add_arg(cmd, "%s", "oggdec");
	exec_cmd_add_arg(cmd, "%s", file);
	exec_cmd_add_arg(cmd, "%s", "-dwav");	
	
	gchar* trackdir = preferences_get_convert_audio_track_dir();
	
	gchar* filename = strrchr(file, '/');
	if(filename != NULL)
	{
		++filename;
		*convertedfile = g_build_filename(trackdir, filename, NULL);
		gchar* suffix = strstr(*convertedfile, ".ogg");
		if(suffix != NULL)
			strncpy(suffix, ".wav", 4);
		
		exec_cmd_add_arg(cmd, "-f%s", *convertedfile);
		
		g_message("Converted file is [%s]", *convertedfile);
	}
	
	g_free(trackdir);		
*/	

	exec_cmd_add_arg(cmd, "%s", "oggdec");
	exec_cmd_add_arg(cmd, "%s", "-b16");
	exec_cmd_add_arg(cmd, "%s", file);
	
	gchar* trackdir = preferences_get_convert_audio_track_dir();
	
	gchar* filename = g_path_get_basename(file);
	if(filename != NULL)
	{
		*convertedfile = g_build_filename(trackdir, filename, NULL);
		gchar* suffix = strstr(*convertedfile, ".ogg");
		if(suffix != NULL)
			strncpy(suffix, ".wav", 4);
		
		exec_cmd_add_arg(cmd, "-o%s", *convertedfile);
		
		g_message("Converted file is [%s]", *convertedfile);
		g_free(filename);
	}
	
	g_free(trackdir);		
	cmd->preProc = oggdec_pre_proc;
	cmd->readProc = oggdec_read_proc;	
}
