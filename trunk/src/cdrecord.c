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

#include "cdrecord.h"
#include "preferences.h"
#include "gnomebaker.h"
#include "progressdlg.h"
#include <glib/gprintf.h>
#include "gbcommon.h"
#include "devices.h"


gint totaltrackstowrite = 1;
gint firsttrack = -1;


/*
 * We pass a pointer to this function to Exec which will call us when it has
 * read from it pipe. We get the data and stuff the text into our text entry
 * for the user to read.
 */
void
cdrecord_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	
	progressdlg_set_status(_("<b>Burning disk...</b>"));
	progressdlg_increment_exec_number();
	
	gdk_threads_enter();
	gint ret = gnomebaker_show_msg_dlg(GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
			  _("Please insert a blank CD into the CD writer"));
	gdk_flush();
	gdk_threads_leave();
	
	if(ret == GTK_RESPONSE_CANCEL)
	{
		ExecCmd* e = (ExecCmd*)ex;
		e->state = CANCELLED;
	}
}


void
cdrecord_blank_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	progressdlg_set_status(_("<b>Blanking disk...</b>"));
	progressdlg_set_text("");
	
	gdk_threads_enter();
	gint ret = gnomebaker_show_msg_dlg(GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
		_("Please insert the CD-RW into the CD writer"));
	gdk_flush();
	gdk_threads_leave();
	
	if(ret == GTK_RESPONSE_CANCEL)
	{
		ExecCmd* e = (ExecCmd*)ex;
		e->state = CANCELLED;
	}
	else
	{
		progressdlg_pulse_start();
	}
}


void
cdrecord_blank_post_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	progressdlg_pulse_stop();
}


void
cdrecord_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC

	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);

	/* 	This is a hack for the moment until I figure out what's 
		going on with the charset. */
	gchar *buf = (gchar*)buffer;
	const gint len = strlen(buf);		
	gint i = 0;
	for(; i < len; i++)
	{
		if(!g_ascii_isalnum(buf[i]) && !g_ascii_iscntrl(buf[i])
			&& !g_ascii_ispunct(buf[i]) && !g_ascii_isspace(buf[i]))
		{
			buf[i] = ' ';
		}
	}
	
	const gchar* track = strstr(buf, _("Track"));
	if(track != NULL)
	{
		gint currenttrack = 0;
		gfloat current = 0, total = 0; 
		if(sscanf(track, "%*s %d %*s %f %*s %f", &currenttrack, &current, &total) > 0)
		{
			if(current > 0.0)
			{
				if(firsttrack == -1)
					firsttrack = currenttrack;			

				/* Figure out how many tracks we have written so far and calc the fraction */
				gfloat totalfraction = 
					((gfloat)currenttrack - firsttrack) * (1.0 / totaltrackstowrite);
				
				/* now add on the fraction of the track we are currently writing */
				totalfraction += ((current / total) * (1.0 / totaltrackstowrite));
				
				/*g_message("^^^^^ current [%d] first [%d] current [%f] total [%f] fraction [%f]",
					currenttrack, firsttrack, current, total, totalfraction);*/
				
				progressdlg_set_fraction(totalfraction);
			}
		}		
	}
	
	const gchar* fixating = strstr(buf, _("Fixating"));
	if(fixating != NULL)
	{
		/* Order of these is important as set fraction also sets the text */
		progressdlg_set_fraction(1.0);		
		progressdlg_set_text(_("Fixating"));
	}
	
	progressdlg_append_output(buffer);
}


/*
 *  Populates the common information required to burn a cd
 */
void
cdrecord_add_common_args(ExecCmd * const cdBurn)
{
	GB_LOG_FUNC
	g_return_if_fail(cdBurn != NULL);	

	exec_cmd_add_arg(cdBurn, "%s", "cdrecord");
	
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cdBurn, "dev=%s", writer);
	g_free(writer);
	
	exec_cmd_add_arg(cdBurn, "%s", "gracetime=2");
	
	gchar* speed = g_strdup_printf("%d", preferences_get_int(GB_WRITE_SPEED));
	exec_cmd_add_arg(cdBurn, "speed=%s", speed);
	g_free(speed);
	
	exec_cmd_add_arg(cdBurn, "%s", "-v");	

	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cdBurn, "%s", "-eject");

	if(preferences_get_bool(GB_DUMMY))
		exec_cmd_add_arg(cdBurn, "%s", "-dummy");
	
	if(preferences_get_bool(GB_BURNFREE))
		exec_cmd_add_arg(cdBurn, "%s", "driveropts=burnfree");
	
	gchar* mode = preferences_get_string(GB_WRITE_MODE);
	if(g_ascii_strcasecmp(mode, "default") != 0)
		exec_cmd_add_arg(cdBurn, "-%s", mode);

	g_free(mode);
	
	totaltrackstowrite = 1;
	firsttrack = -1;
}


void
cdrecord_add_create_audio_cd_args(ExecCmd* e, const GList* audiofiles)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	g_return_if_fail(e != NULL);
	
	cdrecord_add_common_args(e);
	exec_cmd_add_arg(e, "%s", "-useinfo");
	exec_cmd_add_arg(e, "%s", "-pad");
	exec_cmd_add_arg(e, "%s", "-audio");	
	
	const GList *audiofile = audiofiles;
	while(audiofile != NULL)
	{
		if(audiofile->data)	
		{
			exec_cmd_add_arg(e, "%s", audiofile->data);
			g_message(_("cdrecord - adding create audio data [%s]"), (gchar*)audiofile->data);
			totaltrackstowrite++;
		}
		audiofile = audiofile->next;
	}		
	
	e->readProc = cdrecord_read_proc;
	e->preProc = cdrecord_pre_proc;
}


/*
 *  ISO
 */
void
cdrecord_add_iso_args(ExecCmd * const cdBurn, const gchar * const iso)
{
	GB_LOG_FUNC
	g_return_if_fail(cdBurn != NULL);
	g_return_if_fail(iso != NULL);	
	
	cdrecord_add_common_args(cdBurn);
	
	/*if(!prefs->multisession)*/
		exec_cmd_add_arg(cdBurn, "%s", "-multi");

	exec_cmd_add_arg(cdBurn, "%s", iso);
	cdBurn->readProc = cdrecord_read_proc;
	cdBurn->preProc = cdrecord_pre_proc;
}


void 
cdrecord_add_audio_args(ExecCmd * const cdBurn)
{
	GB_LOG_FUNC
	g_return_if_fail(cdBurn != NULL);
	cdrecord_add_common_args(cdBurn);
	exec_cmd_add_arg(cdBurn, "%s", "-useinfo");
	exec_cmd_add_arg(cdBurn, "%s", "-audio");
	exec_cmd_add_arg(cdBurn, "%s", "-pad");
	
	GError *err = NULL;
	
	gchar* tmp = preferences_get_string(GB_TEMP_DIR);
	GDir *dir = g_dir_open(tmp, 0, &err);

	if(dir != NULL)
	{
		totaltrackstowrite = 0;
		/* loop around reading the files in the directory */
		const gchar *name = g_dir_read_name(dir);	
		while(name != NULL)
		{
			if(g_str_has_suffix(name, ".wav"))
			{
				g_message( _("adding [%s]"), name);
				gchar* fullpath = g_build_filename(tmp, name, NULL);
				exec_cmd_add_arg(cdBurn, "%s", fullpath);
				totaltrackstowrite++;
				g_free(fullpath);
			}
			
			name = g_dir_read_name(dir);
		}
		g_dir_close(dir);
	}
	
	cdBurn->readProc = cdrecord_read_proc;
	cdBurn->preProc = cdrecord_pre_proc;
	
	g_free(tmp);
}


void 
cdrecord_add_blank_args(ExecCmd * const cdBurn)
{
	GB_LOG_FUNC
	g_return_if_fail(cdBurn != NULL);	
	
	cdBurn->readProc = cdrecord_read_proc;
	cdBurn->preProc = cdrecord_blank_pre_proc;
	cdBurn->postProc = cdrecord_blank_post_proc;

	/*exec_add_arg(cdBurn, "%s",
	 * "/home/luke/projects/cddummy/src/cddummy"); */
	exec_cmd_add_arg(cdBurn, "%s", "cdrecord");
	
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cdBurn, "dev=%s", writer);
	g_free(writer);

	gchar* speed = g_strdup_printf("%d", preferences_get_int(GB_WRITE_SPEED));
	exec_cmd_add_arg(cdBurn, "speed=%s", speed);
	g_free(speed);
	
	exec_cmd_add_arg(cdBurn, "%s", "-v");
	/*exec_cmd_add_arg(cdBurn, "%s", "-format");*/

	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cdBurn, "%s", "-eject");
	
	exec_cmd_add_arg(cdBurn, "%s", "gracetime=2");
	
	exec_cmd_add_arg(cdBurn, "%s", preferences_get_bool(GB_FAST_BLANK) 
		? "blank=fast" : "blank=all");
}
