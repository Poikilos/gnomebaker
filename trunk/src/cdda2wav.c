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
 * File: cdda2wav.h
 * Created by: Luke Biddell
 * Created on: Sun Jan 11 17:22:09 2004
 */

#include "cdda2wav.h"
#include "preferences.h"
#include "gnomebaker.h"
#include "progressdlg.h"
#include <glib/gprintf.h>
#include <ctype.h>
#include "gbcommon.h"
#include "devices.h"


gint totaltracks = -1;
gint totaltracksread = 0;


void
cdda2wav_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	totaltracks = -1;
	totaltracksread = 0;
	
	progressdlg_set_status(_("<b>Extracting audio tracks...</b>"));
	progressdlg_increment_exec_number();

	gint response = GTK_RESPONSE_NO;
	
	gchar* tmp = preferences_get_string(GB_TEMP_DIR);
	gchar* file = g_strdup_printf("%s/gbtrack_01.wav", tmp);	
	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		gdk_threads_enter();
		response = gnomebaker_show_msg_dlg(GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
			_("Audio tracks from a previous sesion already exist on disk, "
			"do you wish to use the existing tracks?"));
		gdk_flush();
		gdk_threads_leave();
	}
	
	g_free(file);
	
	if(response == GTK_RESPONSE_NO)
	{		
		gchar* cmd = g_strdup_printf("rm -fr %s/gbtrack*", tmp);
		system(cmd);
		g_free(cmd);
		
		gdk_threads_enter();
		response = gnomebaker_show_msg_dlg(GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
			_("Please insert the audio CD into the CD reader"));				
		gdk_flush();
		gdk_threads_leave();	
	}
	
	if(response == GTK_RESPONSE_CANCEL)
	{
		ExecCmd* e = (ExecCmd*)ex;
		e->state = CANCELLED;
	}
	else if(response == GTK_RESPONSE_YES)
	{
		ExecCmd* e = (ExecCmd*)ex;
		e->state = SKIP;
	}
	
	g_free(tmp);
}


void
cdda2wav_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	
	gchar* text = (gchar*)buffer;
	
	/*g_message( "cdda2wav_read_proc - read [%s]", text);*/
	
	if(totaltracks == -1)
	{
		const gchar* tracksstart = strstr(text, _("Tracks:"));		
		if(tracksstart != NULL)
		{
			gchar tmpbuf[4];
			memset(tmpbuf, 0x0, 4);
			g_strlcpy(tmpbuf, tracksstart + 7, 3);
			g_strstrip(tmpbuf);
			totaltracks = atoi(tmpbuf);			
			g_message(_("cdda2wav_read_proc - total tracks %d"), totaltracks);
		}
	}
	else if(totaltracks)
	{	
		const gchar* pcnt = strrchr(text, '%');
		if(pcnt != NULL)
		{			
			const gchar* tmp = pcnt;
			do
				tmp--;
			while(*tmp != '\r');	
			tmp++;
					
			const gint bufsize = pcnt - tmp + 2;
			gchar* tmpbuf = g_malloc(bufsize * sizeof(gchar));
			memset(tmpbuf, 0x0, bufsize * sizeof(gchar));
			g_strlcpy(tmpbuf, tmp, bufsize - 1);
						
			gfloat fraction = atof(tmpbuf)/100.0;
			
			g_free(tmpbuf);
						
			const gchar* hundred = NULL;
			if(fraction > 0.95 || fraction < 0.05)
			{
				hundred = strstr(text, "100%");
				if(hundred != NULL)
					fraction = 1.0;
			}
			
			/*g_message("cdda2wav_read_proc - track fraction %f", fraction);*/
			
			fraction *= (1.0/(gfloat)totaltracks);			
			fraction += ((gfloat)totaltracksread *(1.0/(gfloat)totaltracks));
								
			progressdlg_set_fraction(fraction);
						
			if(hundred != NULL)
				totaltracksread++;
		}
	}
		
	progressdlg_append_output(text);
}


/*
 * Populates the information required to extract audio tracks from an existing
 * audio cd
 */
void
cdda2wav_add_copy_args(ExecCmd * e)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
	exec_cmd_add_arg(e, "%s", "cdda2wav");
	exec_cmd_add_arg(e, "%s", "-x");
	exec_cmd_add_arg(e, "%s", "cddb=1");
	exec_cmd_add_arg(e, "%s", "-B");
	exec_cmd_add_arg(e, "%s", "-g");
	exec_cmd_add_arg(e, "%s", "-Q");
	exec_cmd_add_arg(e, "%s", "-paranoia");
	
	gchar* reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(e, "-D%s", reader);
	g_free(reader);
	
	exec_cmd_add_arg(e, "%s", "-Owav");
	
	gchar* tmp = preferences_get_string(GB_TEMP_DIR);
	exec_cmd_add_arg(e, "%s/gbtrack", tmp);
	g_free(tmp);

	e->preProc = cdda2wav_pre_proc;
	e->readProc = cdda2wav_read_proc;
}
