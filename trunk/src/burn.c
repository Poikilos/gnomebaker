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


static StartDlg*
burn_show_start_dlg(const BurnType burntype)
{
	GB_LOG_FUNC
	if(burnargs != NULL)
		exec_delete(burnargs);
	burnargs = NULL;	

	StartDlg* dlg = startdlg_new(burntype);	
	if(gtk_dialog_run(dlg->dialog) != GTK_RESPONSE_OK)	
    {
	   startdlg_delete(dlg);
       dlg = NULL;
    }
    else
        gtk_widget_hide(GTK_WIDGET(dlg->dialog));
	return dlg;
}


static gboolean
burn_stop_process()
{
    GB_LOG_FUNC
    exec_stop(burnargs);
    return TRUE;
}


static void
burn_run_process()
{
	GB_LOG_FUNC
	/* Create the dlg before we start the thread as callbacks from the
	 * thread may need to use the controls.*/
	GtkWidget *dlg = progressdlg_new(burnargs, gnomebaker_get_window(), G_CALLBACK(burn_stop_process));
    exec_run(burnargs);
    progressdlg_finish(dlg, burnargs);
	progressdlg_delete(dlg);
}


static void
burn_cd_iso(const gchar* file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);

    StartDlg* dlg = burn_show_start_dlg(burn_cd_image);
	if(dlg != NULL)
	{        
		burnargs = exec_new(_("Burning CD image"), _("Please wait while the CD image you selected is burned to CD."));
        mkisofs_add_calc_iso_size_args(exec_cmd_new(burnargs), file);
		cdrecord_add_iso_args(exec_cmd_new(burnargs), file);
		burn_run_process();
        startdlg_delete(dlg);
	}
}


void
burn_dvd_iso(const gchar* file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);
    
    /* LB - I've had to remove this as gnome vfs doesn't return application/x-cd-image
     * if an iso doesn't have a .iso extension. So we just have to hope for the best :-( */    
    
    /* gchar* mime = gbcommon_get_mime_type(file);
    if(g_ascii_strcasecmp(mime, "application/x-cd-image") == 0)*/   
    {
        StartDlg* dlg = burn_show_start_dlg(burn_dvd_image);
        if(dlg != NULL)
        {
            burnargs = exec_new(_("Burning DVD image"), _("Please wait while the DVD image you selected is burned to DVD."));
            growisofs_add_iso_args(exec_cmd_new(burnargs),file);
            burn_run_process();
            startdlg_delete(dlg);
        }
    }
    /*else
    {
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE,
          _("The file you have selected is not a DVD image. Please select a DVD image to burn."));
    }
    g_free(mime);*/
}


void
burn_cue_or_toc(const gchar* file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);

    StartDlg* dlg = burn_show_start_dlg(burn_cd_image);
	if(dlg != NULL)
	{
		burnargs = exec_new(_("Burning CD image"), _("Please wait while the CD image you selected is burned to CD."));
		cdrdao_add_image_args(exec_cmd_new(burnargs), file);
		burn_run_process(FALSE);
        startdlg_delete(dlg);        
	}
}


void
burn_cd_image_file(const gchar* file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);
    
    /* LB - I've had to remove this as gnome vfs doesn't return application/x-cd-image
     * if an iso doesn't have a .iso extension. So we just have to hope for the best :-( */
     
	/*gchar* mime = gbcommon_get_mime_type(file);
	g_return_if_fail(mime != NULL);
	GB_TRACE("mime type is %s for %s", mime, file);
	 Check that the mime type is iso 
	if(g_ascii_strcasecmp(mime, "application/x-cd-image") == 0)
		burn_cd_iso(file);
	else if(gbcommon_str_has_suffix(file, ".cue") || gbcommon_str_has_suffix(file, ".toc"))
		burn_cue_or_toc(file);
	else
		gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE,
		  _("The file you have selected is not a cd image. Please select a cd image to burn."));    
	g_free(mime);	*/
    
    if(gbcommon_str_has_suffix(file, ".cue") || gbcommon_str_has_suffix(file, ".toc"))
        burn_cue_or_toc(file);
    else
        burn_cd_iso(file);
}


void
burn_create_data_cd(GtkTreeModel* datamodel)
{
	GB_LOG_FUNC
	g_return_if_fail(datamodel != NULL);
    
    StartDlg* dlg = burn_show_start_dlg(create_data_cd);
	if(dlg != NULL)
	{	                     
		if(preferences_get_bool(GB_CREATEISOONLY))
		{
            burnargs = exec_new(_("Creating data CD image"), _("Please wait while the data CD image is created."));
			mkisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg, FALSE);
            burn_run_process();
		}
        else if(preferences_get_bool(GB_ONTHEFLY))
        {
            burnargs = exec_new(_("Burning data CD"), _("Please wait while the data is burned directly to CD."));
            mkisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg, TRUE);
            ExecCmd* cmd = exec_cmd_new(burnargs);
            cmd->piped = TRUE;
            mkisofs_add_args(cmd, datamodel, dlg, FALSE);
            cdrecord_add_iso_args(exec_cmd_new(burnargs), NULL);
            burn_run_process();
        }
		else
		{
            burnargs = exec_new(_("Burning data CD"), _("Please wait while the data disk image is created and then burned to CD. Depending on the speed of your CD writer, this may take some time. "));
            mkisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg,  TRUE);			
            mkisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg, FALSE);
            gchar* file = preferences_get_create_data_cd_image();
            cdrecord_add_iso_args(exec_cmd_new(burnargs), file);            
            g_free(file);
            burn_run_process();
		}
        startdlg_delete(dlg);
	}
}


void
burn_append_data_cd(GtkTreeModel* datamodel)
{
    GB_LOG_FUNC
    g_return_if_fail(datamodel != NULL);
    
    StartDlg* dlg = burn_show_start_dlg(append_data_cd);
    if(dlg != NULL)
    {                        
        if(preferences_get_bool(GB_ONTHEFLY))
        {
            burnargs = exec_new(_("Appending to data CD"), _("Please wait while the additional data is burned directly to CD."));
            mkisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg, TRUE);
            ExecCmd* cmd = exec_cmd_new(burnargs);
            cmd->piped = TRUE;
            mkisofs_add_args(cmd, datamodel, dlg, FALSE);
            cdrecord_add_iso_args(exec_cmd_new(burnargs), NULL);
            burn_run_process();
        }
        else
        {
            burnargs = exec_new(_("Appending to data CD"), _("Please wait while the data disk image is created and then appended to the CD. Depending on the speed of your CD writer, this may take some time. "));
            mkisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg,  TRUE);
            mkisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg, FALSE);
            gchar* file = preferences_get_create_data_cd_image();            
            cdrecord_add_iso_args(exec_cmd_new(burnargs), file);
            g_free(file);      
            burn_run_process();
        }
        startdlg_delete(dlg);
    }
}


void
burn_create_audio_cd(GtkTreeModel* model)
{
    GB_LOG_FUNC
    g_return_if_fail(model != NULL);
    
    StartDlg* dlg = burn_show_start_dlg(create_audio_cd);
    if(dlg != NULL)
    {       
        burnargs = exec_new(_("Burning audio CD"), _("Please wait while the selected tracks are converted to CD audio and then burned to CD."));
        const gboolean onthefly = FALSE;/*preferences_get_bool(GB_ONTHEFLY);*/
        
        GtkTreeIter iter;
        if(gtk_tree_model_get_iter_first(model, &iter))
        {
            gchar* trackdir = preferences_get_convert_audio_track_dir();
            GList *audiofiles = NULL;
            gint i = 0, sectors = 0;            
            do
            {
                ExecCmd* cmd = exec_cmd_new(burnargs);
                cmd->piped = onthefly;
                MediaInfo* info = NULL;
                gtk_tree_model_get (model, &iter, AUDIOCD_COL_INFO, &info, -1);
                
                gchar* filename = g_strdup_printf("gbtrack_%.2d.wav", i + 1);
                gchar* convertedfile = g_build_filename(trackdir, filename, NULL);
                if(!onthefly)                                   
                    audiofiles = g_list_append(audiofiles, convertedfile);  
                else 
                    g_free(convertedfile);                    
                gstreamer_add_args(cmd, info->filename, convertedfile);
                
                gchar* inffilename = g_strdup_printf("gbtrack_%.2d.inf", i + 1);
                gchar* inffile = g_build_filename(trackdir, inffilename, NULL);
                media_info_create_inf_file(info, i + 1, inffile, &sectors);
                if(onthefly) 
                    audiofiles = g_list_append(audiofiles, inffile);  
                else 
                    g_free(inffile);                    
                    
                g_free(filename);
                g_free(inffilename);

                ++i;
            } while (gtk_tree_model_iter_next(model, &iter));
            
            ExecCmd* cmd = exec_cmd_new(burnargs);
            cdrecord_add_create_audio_cd_args(cmd, audiofiles);                
            cmd->workingdir = trackdir;
            
            for(; audiofiles != NULL; audiofiles = audiofiles->next)
                g_free(audiofiles->data);
            g_list_free(audiofiles);
            
            burn_run_process();
            startdlg_delete(dlg);
        }
    }
}


void
burn_copy_data_cd()
{
	GB_LOG_FUNC

    StartDlg* dlg = burn_show_start_dlg(copy_data_cd);
	if(dlg != NULL)
	{        
		if(preferences_get_bool(GB_CREATEISOONLY))
		{
            burnargs = exec_new(_("Extracting CD image"), _("Please wait while the data CD image is extracted."));
			readcd_add_copy_args(exec_cmd_new(burnargs), dlg);
            burn_run_process();
		}
		else
		{
            burnargs = exec_new(_("Copying data CD"), _("Please wait while the data CD image is extracted and then burned to CD."));
            readcd_add_copy_args(exec_cmd_new(burnargs), dlg);
            gchar* file = preferences_get_copy_data_cd_image();
            mkisofs_add_calc_iso_size_args(exec_cmd_new(burnargs), file);            
            cdrecord_add_iso_args(exec_cmd_new(burnargs), file);
            g_free(file);
            burn_run_process();
		}
        startdlg_delete(dlg);
	}
}


void
burn_copy_audio_cd()
{
	GB_LOG_FUNC
    StartDlg* dlg = burn_show_start_dlg(copy_audio_cd);
	if(dlg != NULL)
	{
		burnargs = exec_new(_("Copying audio CD"), _("Please wait while the audio CD tracks are extracted and then burned to CD."));
		cdda2wav_add_copy_args(exec_cmd_new(burnargs));
		cdrecord_add_audio_args(exec_cmd_new(burnargs));
		burn_run_process();
        startdlg_delete(dlg);
	}
}


void
burn_blank_cdrw()
{
	GB_LOG_FUNC
	
    StartDlg* dlg = burn_show_start_dlg(blank_cdrw);
	if(dlg != NULL)
	{		
		burnargs = exec_new(_("Blanking CD"), _("Please wait while the CD is blanked."));
		cdrecord_add_blank_args(exec_cmd_new(burnargs));
		burn_run_process();
        startdlg_delete(dlg);
	}
}


void
burn_format_dvdrw()
{
	GB_LOG_FUNC
	
    StartDlg* dlg = burn_show_start_dlg(format_dvdrw);
    if(dlg != NULL)
	{		
		burnargs = exec_new(_("Formatting re-writeable DVD"), _("Please wait while the DVD is formatted."));
		dvdformat_add_args(exec_cmd_new(burnargs));
		burn_run_process();
        startdlg_delete(dlg);
	}
}


void
burn_create_data_dvd(GtkTreeModel* datamodel)
{
	GB_LOG_FUNC
    g_return_if_fail(datamodel != NULL);
    
    StartDlg* dlg = burn_show_start_dlg(create_data_dvd);
    if(dlg != NULL)
	{	            
        if(preferences_get_bool(GB_CREATEISOONLY))
        {
            burnargs = exec_new(_("Creating data DVD image"), _("Please wait while the data DVD image is created."));
            mkisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg, FALSE);
            burn_run_process();
        }
        else
        {
            burnargs = exec_new(_("Burning data DVD"), _("Please wait while the data is burned directly to DVD."));
            growisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg);
            burn_run_process();
        }
        startdlg_delete(dlg);
	}
}


void
burn_append_data_dvd(GtkTreeModel* datamodel)
{
    GB_LOG_FUNC
    g_return_if_fail(datamodel != NULL);
    
    StartDlg* dlg = burn_show_start_dlg(append_data_dvd);
    if(dlg != NULL)
    {               
        burnargs = exec_new(_("Appending to data DVD"), _("Please wait while the data is appended directly to the DVD."));
        growisofs_add_args(exec_cmd_new(burnargs), datamodel, dlg);
        burn_run_process();
        startdlg_delete(dlg);
    }
}


gboolean
burn_init()
{
    GB_LOG_FUNC
#ifdef ENABLE_NLS
    gint i;
    for (i = 0; i < BurnTypeCount; ++i)
        BurnTypeText[i] = _(BurnTypeText[i]);
#endif
    return TRUE;
}
