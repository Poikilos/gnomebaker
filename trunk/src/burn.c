/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * File: burn.c
 * Created by: luke <luke@dhcp-45-369>
 * Created on: Tue Jan 28 22:12:09 2003
 */

#include <gnome.h>
#include <signal.h>
#include <sys/types.h>
#include <glib/gprintf.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include "burn.h"
#include "preferences.h"
#include "exec.h"
#include "cdrecord.h"
#include "readcd.h"
#include "cdda2wav.h"
#include "gnomebaker.h"
#include "startdlg.h"
#include "progressdlg.h"
#include "mkisofs.h"
#include "gbcommon.h"
#include "audiocd.h"
#include "mpg123.h"
#include "oggdec.h"
#include "sox.h"

Exec *burnargs = NULL;


gboolean burn_start_process();
const gint burn_show_start_dlg(const BurnType burntype);
void burn_end_proc(void *ex, void *data);
gboolean 
burn_foreachaudiotrack_func(GtkTreeModel *model, GtkTreePath  *path,
                			GtkTreeIter  *iter, gpointer user_data);


/*
 * Initialises the burn functions and controls.
 */
gboolean
burn_init()
{
	GB_LOG_FUNC
	return TRUE;
}

/*
 *
 */
gboolean
burn_iso(const gchar * const file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, FALSE);
	gboolean ok = FALSE;

	if(burn_show_start_dlg(burn_cd_image) == GTK_RESPONSE_OK)
	{
		burnargs = exec_new(1);
		ExecCmd *e = &burnargs->cmds[0];

		cdrecord_add_iso_args(e, file);

		ok = burn_start_process();
	}

	return ok;
}


gboolean
burn_create_data_cd(GtkTreeModel* datamodel)
{
	GB_LOG_FUNC
	g_return_val_if_fail(datamodel != NULL, FALSE);
	gboolean ok = FALSE;

	if(burn_show_start_dlg(create_data_cd) == GTK_RESPONSE_OK)
	{	
		ok = TRUE;
		gchar* file = NULL;
		if(preferences_get_bool(GB_CREATEISOONLY))
		{
			GtkWidget *filesel = gtk_file_selection_new("Please select an iso file to save to...");	
			gtk_file_selection_set_select_multiple(GTK_FILE_SELECTION(filesel), FALSE);
			gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), "gnomebaker.iso");
		
			if((ok = (gtk_dialog_run(GTK_DIALOG(filesel)) == GTK_RESPONSE_OK)))
			{
				file = g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel)));		
				burnargs = exec_new(1);			
			}
			
			gtk_widget_destroy(filesel);
		}
		else
		{
			file = preferences_get_create_data_cd_image();
			burnargs = exec_new(2);
		}
			
		if(ok && mkisofs_add_args(&burnargs->cmds[0], datamodel, file))
		{
			if(!preferences_get_bool(GB_CREATEISOONLY))
				cdrecord_add_iso_args(&burnargs->cmds[1], file);
			ok = burn_start_process();
		}
		
		g_free(file);
	}

	return ok;
}


gboolean 
burn_create_audio_cd(GtkTreeModel* audiomodel)
{
	GB_LOG_FUNC
	g_return_val_if_fail(audiomodel != NULL, FALSE);
	gboolean ok = FALSE;

	if(burn_show_start_dlg(create_audio_cd) == GTK_RESPONSE_OK)
	{
		burnargs = exec_new(0);
		
		GList *audiofiles = g_list_alloc();
		
		gtk_tree_model_foreach(audiomodel, burn_foreachaudiotrack_func, &audiofiles);
		
		ExecCmd* cmd = exec_add_cmd(burnargs);		
		cdrecord_add_create_audio_cd_args(cmd, audiofiles);
		
		ok = burn_start_process();
		
		const GList *audiofile = audiofiles;
		while(audiofile != NULL)
		{
			g_free(audiofile->data);
			audiofile = audiofile->next;
		}		
		
		g_list_free(audiofiles);
	}

	return ok;
}


gboolean 
burn_foreachaudiotrack_func(GtkTreeModel *model, GtkTreePath  *path,
                			GtkTreeIter  *iter, gpointer user_data)
{
	GB_LOG_FUNC
	g_return_val_if_fail(user_data != NULL, FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(model != NULL, FALSE);
	g_return_val_if_fail(path != NULL, FALSE);

	GList** audiofiles = (GList**)user_data;
	gchar *file = NULL;
			
	gtk_tree_model_get (model, iter, AUDIOCD_COL_FILE, &file, -1);
	
	g_message( "%s", file);
	
	gchar* mime = gnome_vfs_get_mime_type(file);
	if(mime != NULL)
	{
		ExecCmd* cmd = exec_add_cmd(burnargs);
		gchar* convertedfile = NULL;
			
		/* Check that the file extension is one we support */			
		if(g_ascii_strcasecmp(mime, "audio/x-mp3") == 0)
			mpg123_add_mp3_args(cmd, file, &convertedfile);
		else if(g_ascii_strcasecmp(mime, "application/ogg") == 0)
			oggdec_add_args(cmd, file, &convertedfile);
		else if(g_ascii_strcasecmp(mime, "audio/x-wav") == 0)
			sox_add_wav_args(cmd, file, &convertedfile);
		
		g_message("burn - [%s]", convertedfile);
		*audiofiles = g_list_append(*audiofiles, convertedfile);
		g_free(mime);	
	}

	g_free(file);
	file = NULL;

	return FALSE;
}


/*
 * Creates the information required to copy an existing data cd.
 */
gboolean
burn_copy_data_cd()
{
	GB_LOG_FUNC
	gboolean ok = FALSE;

	if(burn_show_start_dlg(copy_data_cd) == GTK_RESPONSE_OK)
	{
		ok = TRUE;
		gchar* file = NULL;		
		if(preferences_get_bool(GB_CREATEISOONLY))
		{
			GtkWidget *filesel = gtk_file_selection_new("Please select an iso file to save to...");
			gtk_file_selection_set_select_multiple(GTK_FILE_SELECTION(filesel), FALSE);
			gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), "gnomebaker.iso");
		
			if((ok = (gtk_dialog_run(GTK_DIALOG(filesel)) == GTK_RESPONSE_OK)))
			{
				burnargs = exec_new(1);			
				file = g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel)));
			}
			
			gtk_widget_destroy(filesel);
		}
		else
		{
			burnargs = exec_new(2);			
			file = preferences_get_copy_data_cd_image();			
		}

		if(ok)
		{
			readcd_add_copy_args(&burnargs->cmds[0], file);
			if(!preferences_get_bool(GB_CREATEISOONLY))
				cdrecord_add_iso_args(&burnargs->cmds[1], file);
			ok = burn_start_process();
		}
		
		g_free(file);
	}

	return ok;
}

/*
 *  Creates the information required to copy an audio cd to a blank cd.
 */
gboolean
burn_copy_audio_cd()
{
	GB_LOG_FUNC
	gboolean ok = FALSE;

	if(burn_show_start_dlg(copy_audio_cd) == GTK_RESPONSE_OK)
	{
		burnargs = exec_new(2);

		cdda2wav_add_copy_args(&burnargs->cmds[0]);
		cdrecord_add_audio_args(&burnargs->cmds[1]);

		ok = burn_start_process();
	}

	return ok;
}


gboolean
burn_blank_cdrw()
{
	GB_LOG_FUNC
	gboolean ok = FALSE;
	
	if(burn_show_start_dlg(blank_cdrw) == GTK_RESPONSE_OK)
	{		
		burnargs = exec_new(1);
		cdrecord_add_blank_args(&burnargs->cmds[0]);

		ok = burn_start_process();
	}

	return ok;
}


/*
 *
 */
const gint
burn_show_start_dlg(const BurnType burntype)
{
	GB_LOG_FUNC
	if(burnargs != NULL)
		exec_delete(burnargs);
	burnargs = NULL;	

	GtkWidget *dlg = startdlg_new(burntype);	
	gint ret = gtk_dialog_run(GTK_DIALOG(dlg));	
	startdlg_delete(dlg);
		
	return ret;
}

/*
 * This function kicks off the whole writing process on a separate thread.
 */
gboolean
burn_start_process()
{
	GB_LOG_FUNC
	gboolean ok = TRUE;
	
	/* Wire up the function that gets called at the end of the burning process. 
	   This finalises the text in the progress dialog.*/
	burnargs->endProc = burn_end_proc;

	/*
	 * Create the dlg before we start the thread as callbacks from the
	 * thread may need to use the controls 
	 */
	GtkWidget *dlg = progressdlg_new(burnargs->cmdCount);
	
	if(exec_go(burnargs) == 0)
	{
		gtk_dialog_run(GTK_DIALOG(dlg));		
	}
	else
	{
		g_critical("Failed to start child process");
		ok = FALSE;
	}
	
	if(burnargs != NULL)
		exec_delete(burnargs);
	burnargs = NULL;

	progressdlg_delete(dlg);
	
	return ok;
}


gboolean
burn_end_process()
{
	GB_LOG_FUNC
	gboolean ok = TRUE;

	exec_cancel(burnargs);

	return ok;
}


void
burn_end_proc(void *ex, void *data)
{
	GB_LOG_FUNC	
	g_return_if_fail(ex != NULL);
	Exec* e = (Exec*)ex;	
	
	progressdlg_reset_fraction(1.0);
	
	ExecState outcome = COMPLETE;

	gint i = 0;
	for(; i < e->cmdCount; i++)
	{
		if(e->cmds[i].state == CANCELLED)
		{			
			/*progressdlg_set_text("Cancelled");*/
			outcome = CANCELLED;
			progressdlg_dismiss();
			break;
		}
		else if(e->cmds[i].state == COMPLETE)
		{
			progressdlg_set_text("Completed");
		}
		else if(e->cmds[i].state == FAILED)
		{
			progressdlg_set_text("Failed");
			outcome = FAILED;
			break;
		}
	}
	
	if(outcome != CANCELLED)
		progressdlg_enable_close(TRUE);
}
