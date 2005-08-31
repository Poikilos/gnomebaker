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
 * Copyright: luke_biddell@yahoo.com
 * Created on: Tue Jan 28 22:12:09 2003
 */

#include "burn.h"
#include <gnome.h>
#include <signal.h>
#include <sys/types.h>
#include <glib/gprintf.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include "preferences.h"
#include "exec.h"
#include "execfunctions.h"
#include "gnomebaker.h"
#include "startdlg.h"
#include "progressdlg.h"
#include "gbcommon.h"
#include "audiocd.h"
#include "devices.h"
#include "media.h"


static Exec *burnargs = NULL;


/*
 *
 */
static gint
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


static void
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
        const ExecState state = exec_cmd_get_state(&e->cmds[i]);
		if(state == CANCELLED)
		{			
			/*progressdlg_set_text("Cancelled");*/
			outcome = CANCELLED;
			progressdlg_dismiss();
			break;
		}
		else if(state == COMPLETE)
		{
			progressdlg_set_text(_("Completed"));
		}
		else if(state == FAILED)
		{
			progressdlg_set_text(_("Failed"));
			outcome = FAILED;
			break;
		}
	}
    
    if(e->err != NULL)
        progressdlg_append_output(e->err->message);
	
	if(outcome != CANCELLED)
		progressdlg_enable_close(TRUE);
}



/*
 * This function kicks off the whole writing process on a separate thread.
 */
static gboolean
burn_start_process()
{
	GB_LOG_FUNC
	gboolean ok = TRUE;		  
	
	/* Wire up the function that gets called at the end of the burning process. 
	   This finalises the text in the progress dialog.*/
	burnargs->endProc = burn_end_proc;

	/*
	 * Create the dlg before we start the thread as callbacks from the
	 * thread may need to use the controls. If we're on the fly then 
     * there is only ever _1_ command to tell the progress bar about.
	 */
	GtkWidget *dlg = progressdlg_new(burnargs->onthefly ? 1 : burnargs->cmdCount);
    gtk_widget_show(dlg);
    gtk_main_iteration();
    
    GThread* thread = exec_go(burnargs);
	if(thread != NULL)
	{
		gtk_dialog_run(GTK_DIALOG(dlg));
        g_thread_join(thread);
	}
	else
	{
		g_critical("Failed to start child process");
		ok = FALSE;
	}
	
	progressdlg_delete(dlg);	
	return ok;
}



/*
 *
 */
gboolean
burn_iso(const gchar* file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, FALSE);
	gboolean ok = FALSE;

	if(burn_show_start_dlg(burn_cd_image) == GTK_RESPONSE_OK)
	{
		burnargs = exec_new(1);
		cdrecord_add_iso_args(&burnargs->cmds[0], file);
		ok = burn_start_process();
	}

	return ok;
}

gboolean
burn_dvd_iso(const gchar* file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, FALSE);
	gboolean ok = FALSE;

	if(burn_show_start_dlg(burn_dvd_image) == GTK_RESPONSE_OK)
	{
		burnargs = exec_new(1);
		growisofs_add_iso_args(&burnargs->cmds[0],file);
		ok = burn_start_process();
	}

	return ok;
}



gboolean
burn_cue_or_toc(const gchar* file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, FALSE);
	gboolean ok = FALSE;

	if(burn_show_start_dlg(burn_cd_image) == GTK_RESPONSE_OK)
	{
		burnargs = exec_new(1);
		cdrdao_add_image_args(&burnargs->cmds[0], file);
		ok = burn_start_process(FALSE);
	}

	return ok;
}


gboolean 
burn_cd_image_file(const gchar* file)
{
	GB_LOG_FUNC
	g_return_val_if_fail(file != NULL, FALSE);
	gchar* mime = gbcommon_get_mime_type(file);
	g_return_val_if_fail(mime != NULL, FALSE);
	GB_TRACE("mime type is %s for %s", mime, file);
	gboolean ret = FALSE;
	
	/* Check that the mime type is iso */
	if(g_ascii_strcasecmp(mime, "application/x-cd-image") == 0)
		ret = burn_iso(file);
	else if(g_str_has_suffix(file, ".cue") || g_str_has_suffix(file, ".toc"))
		ret = burn_cue_or_toc(file);
	else
		gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE,
		  _("The file you have selected is not a cd image. Please select a cd image to burn."));
	
	g_free(mime);	
	return ret;
}


gboolean
burn_create_data_cd(GtkTreeModel* datamodel)
{
	GB_LOG_FUNC
	g_return_val_if_fail(datamodel != NULL, FALSE);
    
	gboolean ok = FALSE;	
	if((ok = (burn_show_start_dlg(create_data_cd) == GTK_RESPONSE_OK)))
	{	
		if(preferences_get_bool(GB_CREATEISOONLY))
		{
			GtkWidget *filesel = gtk_file_chooser_dialog_new(
				_("Please select an iso file to save to..."), NULL, GTK_FILE_CHOOSER_ACTION_SAVE, 
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);			
			gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filesel), FALSE);
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filesel), "gnomebaker.iso");
		
			if((ok = (gtk_dialog_run(GTK_DIALOG(filesel)) == GTK_RESPONSE_OK)))
			{
				gchar* file = g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel)));		
				burnargs = exec_new(1);			
                ok = mkisofs_add_args(&burnargs->cmds[0], datamodel, file);
                g_free(file);
			}
			
			gtk_widget_destroy(filesel);            
            if(ok)
                ok = burn_start_process();
		}
        else if(preferences_get_bool(GB_ONTHEFLY))
        {
            burnargs = exec_new(2);
            burnargs->onthefly = TRUE;
            ok = mkisofs_add_args(&burnargs->cmds[0], datamodel, NULL);			
            if(ok)                 
            {
                cdrecord_add_iso_args(&burnargs->cmds[1], NULL);
                ok = burn_start_process();
            }
        }
		else
		{
            burnargs = exec_new(2);
			gchar* file = preferences_get_create_data_cd_image();            
            ok = mkisofs_add_args(&burnargs->cmds[0], datamodel, file);			
            if(ok)
            {
                cdrecord_add_iso_args(&burnargs->cmds[1], file);            
                ok = burn_start_process();
            }
            g_free(file);                
		}
	}
	return ok;
}


gboolean 
burn_create_audio_cd(GtkTreeModel* model)
{
    GB_LOG_FUNC
    g_return_val_if_fail(model != NULL, FALSE);
    gboolean ok = FALSE;
    
    if(burn_show_start_dlg(create_audio_cd) == GTK_RESPONSE_OK)
    {       
        const gint rows = gtk_tree_model_iter_n_children(model, NULL);
        burnargs = exec_new(rows + 1);
        burnargs->onthefly = preferences_get_bool(GB_ONTHEFLY);
        
        GtkTreeIter iter;
        if(gtk_tree_model_get_iter_first(model, &iter))
        {
            gchar* trackdir = preferences_get_convert_audio_track_dir();
            GList *audiofiles = NULL;
            gint i = 0, sectors = 0;            
            do
            {
                MediaInfo* info = NULL;
                gtk_tree_model_get (model, &iter, AUDIOCD_COL_INFO, &info, -1);
                
                gchar* filename = g_strdup_printf("gbtrack_%.2d.wav", i + 1);
                gchar* convertedfile = g_build_filename(trackdir, filename, NULL);
                if(!burnargs->onthefly)                                   
                    audiofiles = g_list_append(audiofiles, convertedfile);  
                else 
                    g_free(convertedfile);                    
                gstreamer_add_args(&burnargs->cmds[i], info->filename, convertedfile);
                
                gchar* inffilename = g_strdup_printf("gbtrack_%.2d.inf", i + 1);
                gchar* inffile = g_build_filename(trackdir, inffilename, NULL);
                media_info_create_inf_file(info, i + 1, inffile, &sectors);
                if(burnargs->onthefly) 
                    audiofiles = g_list_append(audiofiles, inffile);  
                else 
                    g_free(inffile);                    
                    
                g_free(filename);
                g_free(inffilename);

                ++i;
            } while (gtk_tree_model_iter_next(model, &iter));
            
            cdrecord_add_create_audio_cd_args(&burnargs->cmds[rows], audiofiles);
                
            burnargs->cmds[rows].workingdir = trackdir;
            
            for(; audiofiles != NULL; audiofiles = audiofiles->next)
                g_free(audiofiles->data);
            g_list_free(audiofiles);
            
            ok = burn_start_process();
        }
    }

    return ok;
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
			GtkWidget *filesel = gtk_file_chooser_dialog_new(
				_("Please select an iso file to save to..."), NULL, GTK_FILE_CHOOSER_ACTION_SAVE, 
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);			
			gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filesel), FALSE);
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filesel), "gnomebaker.iso");
		
			if((ok = (gtk_dialog_run(GTK_DIALOG(filesel)) == GTK_RESPONSE_OK)))
			{
				burnargs = exec_new(1);			
				file = g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel)));
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


gboolean
burn_format_dvdrw()
{
	GB_LOG_FUNC
	gboolean ok = FALSE;
	
	if(burn_show_start_dlg(format_dvdrw) == GTK_RESPONSE_OK)
	{		
		burnargs = exec_new(1);
		dvdformat_add_args(&burnargs->cmds[0]);
		ok = burn_start_process();
	}

	return ok;
}


gboolean
burn_create_data_dvd(GtkTreeModel* datamodel)
{
	GB_LOG_FUNC
	gboolean ok = FALSE;

	if(burn_show_start_dlg(create_data_dvd) == GTK_RESPONSE_OK)
	{	
		burnargs = exec_new(1);
		if(growisofs_add_args(&burnargs->cmds[0],datamodel))
		    ok = burn_start_process();
	}

	return ok;
}


gboolean
burn_end_process()
{
    GB_LOG_FUNC
    exec_stop(burnargs);
    return TRUE;
}


/*
 * Initialises the burn functions and controls.
 */
gboolean
burn_init()
{
    GB_LOG_FUNC
    return TRUE;
}
