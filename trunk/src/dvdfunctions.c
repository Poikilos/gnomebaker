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
 * File: dvdfunctions.c
 * Created by: Christoffer Sørensen christoffer@curo.dk
 */

#include "dvdfunctions.h"
#include "preferences.h"
#include "gnomebaker.h"
#include "progressdlg.h"
#include <glib/gprintf.h>
#include "gbcommon.h"
#include "datacd.h"
#include "exec.h"
#include "devices.h"


/*
 * We pass a pointer to this function to Exec which will call us when it has
 * read from it pipe. We get the data and stuff the text into our text entry
 * for the user to read.
 */
void
dvdformat_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC	
	g_return_if_fail(ex != NULL);
	
	progressdlg_set_status(_("<b>Formatting DVD...</b>"));
	progressdlg_increment_exec_number();
	
	gdk_threads_enter();
	gint ret = gnomebaker_show_msg_dlg(GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
			  _("Please insert a rewritable DVD into the DVD writer"));
	gdk_threads_leave();
	
	if(ret == GTK_RESPONSE_CANCEL)
	{
		ExecCmd* e = (ExecCmd*)ex;
		e->state = CANCELLED;
	}
}


void
dvdformat_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);
		
	/* 	This is a hack for the moment until I figure out what's 
		going on with the charset. */
	gchar *buf = (gchar*)buffer;
	
	/*g_message("read_proc: %s",buf);*/
	
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
		
	const gchar* format = strstr(buf, "formatting");
	if(format != NULL)
	{
		gint curpercent = 0;
		if(sscanf(format, "%*s %d", &curpercent) > 0)
		{
			if(curpercent > 0)
				progressdlg_set_fraction(curpercent);
			else
				g_message(_("Failed to get percent in dvdformat_read_proc"));
		}		
	}
	
	progressdlg_append_output(buf);
}


void 
dvdformat_add_args(ExecCmd * const dvdFormat)
{
	GB_LOG_FUNC
	g_return_if_fail(dvdFormat != NULL);
	
	dvdFormat->readProc = dvdformat_read_proc;
	dvdFormat->preProc = dvdformat_pre_proc;
	
	exec_cmd_add_arg(dvdFormat, "%s", "dvd+rw-format");
	
	gchar* writer = devices_get_device_config(GB_WRITER,GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(dvdFormat, "%s", writer);
	g_free(writer);
		
	if(preferences_get_bool(GB_FORCE))
	{
		if(!preferences_get_bool(GB_FAST_BLANK))
			exec_cmd_add_arg(dvdFormat, "%s","-force=full");
		else
			exec_cmd_add_arg(dvdFormat, "%s","-force");
	}
	else
	{
		if(!preferences_get_bool(GB_FAST_BLANK))
			exec_cmd_add_arg(dvdFormat, "%s", "-format=full");
	}	
}


void 
growisofs_pre_proc(void *ex,void *buffer)
{		
	GB_LOG_FUNC	
	g_return_if_fail(ex != NULL);
	
	progressdlg_set_status(_("<b>Burning DVD...</b>"));
	progressdlg_increment_exec_number();
	
	gdk_threads_enter();
	gint ret = gnomebaker_show_msg_dlg(GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
			  _("Please insert a writable DVD into the DVD writer"));
	gdk_flush();
	gdk_threads_leave();
	
	if(ret == GTK_RESPONSE_CANCEL)
	{
		ExecCmd* e = (ExecCmd*)ex;
		e->state = CANCELLED;
	}
}


void 
growisofs_post_proc(void *ex,void *buffer)
{
	GB_LOG_FUNC
	/* if(prefs->eject)
	{
		gchar command[32] = "eject ";
		strcat(command, prefs->writeBlockDevice);
		GString* buf = exec_run_cmd(command);
		if(buffer != NULL)
		{
			Something is probably wrong 
			ExecCmd* e = (ExecCmd*)ex;
			e->state = FAILED;
			progressdlg_append_output(buf->str);
			
		}
	}
			*/	
}


void
growisofs_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	/*
	WARNING: /dev/hdc already carries isofs!
About to execute 'mkisofs -R -J -gui A Song is born.ogg Connected.ogg Daybreak.ogg Dearest.ogg Endlerss Sorrow (Gone With The Wind).ogg Evolution.ogg M.ogg Naturally.ogg Never Ever.ogg No More Words.ogg Opening Run.ogg Still Alone.ogg Taskinlude.ogg Unite.ogg | builtin_dd of=/dev/hdc obs=32k seek=0'
INFO:ingUTF-8 character encoding detected by locale settings.
        Assuming UTF-8 encoded filenames on source filesystem,
        use -input-charset to override.
/dev/hdc: "Current Write Speed" is 4.0x1385KBps.
  1.76% done, estimate finish Sun Dec 26 20:39:08 2004
  3.47% done, estimate finish Sun Dec 26 20:38:40 2004
  5.17% done, estimate finish Sun Dec 26 20:38:50 2004
	...
 98.01% done, estimate finish Sun Dec 26 20:38:40 2004
Total translation table size: 0
Total rockridge attributes bytes: 1386
Total directory bytes: 0
Path table size(bytes): 10
Max brk space used 21000
29084 extents written (56 MB)
builtin_dd: 29088*2KB out @ average 1.5x1385KBps
/dev/hdc: flushing cache
/dev/hdc: stopping de-icing
/dev/hdc: writing lead-out

	*/
	
	
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
	
	const gchar* progressstr = strstr(buf, "done");
	if(progressstr != NULL)
	{
		gint progress = 0;
		if(sscanf(buf,"%d.%*d",&progress) >0)
		{
			g_message(_("growisofs: progress: %d"),progress);
			gfloat fraction = (gfloat)progress / 100.0;
			g_message(_("growisofs: fraction: %f"),fraction);
			
			progressdlg_set_fraction(fraction);
		}
	}
	
	const gchar* leadout = strstr(buf,"writing lead-out");
	if(leadout != NULL)	
		progressdlg_set_fraction(1.0);
		
	progressdlg_append_output(buf);
}


gboolean
growisofs_foreach_func(GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      user_data)
{
	GB_LOG_FUNC
	gchar *icon, *file, *filepath;
	glong size;
		
	gtk_tree_model_get (model, iter, DATACD_COL_ICON, &icon, DATACD_COL_FILE, &file,
		DATACD_COL_SIZE, &size, DATACD_COL_PATH, &filepath, -1);
	
	/* Only add files that are not part of an existing session */
	if(g_ascii_strcasecmp(icon, DATACD_EXISTING_SESSION_ICON) != 0)
	{	
		g_message( "%s %ld %s", file, size, filepath);
		
		gchar* buffer = g_strdup_printf("%s", filepath);		
		exec_cmd_add_arg((ExecCmd*)user_data, "%s", buffer);
		g_free(buffer);
	}	
	
	g_free(icon);	
	g_free(file);	
	g_free(filepath);
	
	return FALSE; /* do not stop walking the store, call us with next row */
}


void 
growisofs_add_args(ExecCmd * const growisofs,GtkTreeModel* datamodel)
{
	GB_LOG_FUNC
	g_return_if_fail(growisofs != NULL);
	
	growisofs->readProc = growisofs_read_proc;
	growisofs->preProc = growisofs_pre_proc;
	//growisofs->postProc = growisofs_post_proc;
	exec_cmd_add_arg(growisofs, "%s", "growisofs");
	
	/* merge new session with existing one */
	gchar* writer = devices_get_device_config(GB_WRITER,GB_DEVICE_ID_LABEL);
	gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datamodel), DATACD_EXISTING_SESSION);
	if(msinfo != NULL)
		exec_cmd_add_arg(growisofs, "-M %s", writer);
	else
		exec_cmd_add_arg(growisofs, "-Z %s", writer);
	g_free(msinfo);
	g_free(writer);
	
	gchar* speed = g_strdup_printf("%d", preferences_get_int(GB_WRITE_SPEED));
	exec_cmd_add_arg(growisofs,"-speed=%s",speed);
	g_free(speed);
	
	/* TODO: Rock Ridge and Joliet should go to preferences! */
	exec_cmd_add_arg(growisofs, "%s", "-R"); /* Rock Ridge */
	exec_cmd_add_arg(growisofs, "%s", "-J"); /* Joliet */
	
	/* -gui makes the output more verbose, so we can 
	    interpret it easier */
	exec_cmd_add_arg(growisofs,"%s","-gui");

	/* stop the reloading of the disc */
	exec_cmd_add_arg(growisofs,"%s","-use-the-force-luke=notray");
	
	/* TODO: Overburn support */
	/* preferences_get_int(GB_OVERBURN)
	if(prefs->overburn)
		exec_cmd_add_arg(growisofs, "%s", "-overburn"); */
	/* TODO: where do we get temporary vars from ? */
	/*if(prefs->finalize)
		exec_cmd_add_arg(growisofs, "%s", "-dvd-compat");*/
	/* -dvd-compat closes the session on DVD+RW's also */	
		
	gtk_tree_model_foreach(datamodel, growisofs_foreach_func, growisofs);
}
