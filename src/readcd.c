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
 * File: readcd.h
 * Created by: luke_biddell@yahoo.com
 * Created on: Sun Jan 11 17:22:38 2004
 */

#include "readcd.h"
#include "preferences.h"
#include "gnomebaker.h"
#include "progressdlg.h"
#include <glib/gprintf.h>
#include "gbcommon.h"
#include "devices.h"

void readcd_pre_proc(void *ex, void *buffer);
void readcd_read_proc(void *ex, void *buffer);
void readcd_post_proc(void *ex, void *buffer);

gint totalguchars = -1;

/*
 *  Populates the information required to make an iso from an existing data cd
 */
void
readcd_add_copy_args(ExecCmd * e, const gchar* iso)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	g_return_if_fail(iso != NULL);	

	exec_cmd_add_arg(e, "%s", "readcd");
	
	gchar* reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(e, "dev=%s", reader);	
	g_free(reader);
	
	exec_cmd_add_arg(e, "f=%s", iso);	
	exec_cmd_add_arg(e, "%s", "-notrunc");
	exec_cmd_add_arg(e, "%s", "-clone");
	/*exec_cmd_add_arg(e, "%s", "-silent");*/

	e->preProc = readcd_pre_proc;	
	e->readProc = readcd_read_proc;
	e->postProc = readcd_post_proc;
}


void
readcd_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	totalguchars = -1;
	
	progressdlg_set_status("<b>Reading CD image...</b>");	
	progressdlg_increment_exec_number();
	
	gint response = GTK_RESPONSE_NO;
	gchar* file = preferences_get_copy_data_cd_image();	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		gdk_threads_enter();
		response = gnomebaker_show_msg_dlg(GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
			"A CD image from a previous session already exists on disk, "
			"do you wish to use the existing image?");		
		gdk_flush();
		gdk_threads_leave();
	}
	
	g_free(file);
	
	if(response == GTK_RESPONSE_NO)
	{
		gdk_threads_enter();
		response = gnomebaker_show_msg_dlg(GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
				  "Please insert the source CD into the CD reader");
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
}


void
readcd_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);
		
	gchar* text = (gchar*)buffer;
	
	if(totalguchars == -1)		
	{
		gchar* end = strstr(text, "end:");	
		if(end != NULL)
		{			
			if(sscanf(end, "%*s %d", &totalguchars) > 0)
				g_message("readcd size is %d", totalguchars);
		}
		progressdlg_append_output(text);
	}
	else if(totalguchars)
	{
		const gchar* start = strstr(text, "addr:");
		if(start != NULL)
		{
			gint readguchars = 0;
			if(sscanf(start, "%*s %d", &readguchars) > 0)
			{
				/*g_message( "read %d, total %d", readguchars, totalguchars);*/
				const gfloat fraction = (gfloat)readguchars/(gfloat)totalguchars;
				progressdlg_set_fraction(fraction);
			}		
		}
		else
		{
			progressdlg_append_output(text);
		}
	}	
}


void
readcd_post_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(ex != NULL);
	ExecCmd* e =(ExecCmd*)ex;
	if(e->exitCode == 0)
	{
		progressdlg_set_fraction(1.0);
		progressdlg_set_text("100%");
	}
}
