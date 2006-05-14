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
#include "devices.h"
#include "media.h"


static Exec *burn_args = NULL;


static StartDlg*
burn_show_start_dlg(const BurnType burn_type)
{
	GB_LOG_FUNC
	if(burn_args != NULL)
		exec_delete(burn_args);
	burn_args = NULL;	

	StartDlg *dlg = startdlg_new(burn_type);	
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
    exec_stop(burn_args);
    return TRUE;
}


static void
burn_run_process()
{
	GB_LOG_FUNC
	/* Create the dlg before we start the thread as callbacks from the
	 * thread may need to use the controls.*/
	GtkWidget *dlg = progressdlg_new(burn_args, gnomebaker_get_window(), G_CALLBACK(burn_stop_process));
    exec_run(burn_args);
    progressdlg_finish(dlg, burn_args);
	progressdlg_delete(dlg);
}


static void
burn_cd_iso(const gchar *file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);

    StartDlg *dlg = burn_show_start_dlg(burn_cd_image);
	if(dlg != NULL)
	{        
		burn_args = exec_new(_("Burning CD image"), _("Please wait while the CD image you selected is burned to CD."));
        mkisofs_add_calc_iso_size_args(exec_cmd_new(burn_args), file);
		cdrecord_add_iso_args(exec_cmd_new(burn_args), file);
		burn_run_process();
        startdlg_delete(dlg);
	}
}


void
burn_dvd_iso(const gchar *file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);
    
      /* GnomeVFS fails to correctly identify a CD image if the extension is no .iso, so we'll offer the user to burn it anyway 
        When gnomevfs is fixed, we'll go back to the previous code.
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE,
        _("The file you have selected is not a DVD image. Please select a DVD image to burn."));*/
    gchar *mime = gbcommon_get_mime_type(file);
    if((g_ascii_strcasecmp(mime, "application/x-cd-image") == 0) || 
            (gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE, 
            _("The file you have selected seems not to be a DVD image. Do you really want to write it to DVD?")) == GTK_RESPONSE_YES))
    {
        StartDlg *dlg = burn_show_start_dlg(burn_dvd_image);
        if(dlg != NULL)
        {
            burn_args = exec_new(_("Burning DVD image"), _("Please wait while the DVD image you selected is burned to DVD."));
            growisofs_add_iso_args(exec_cmd_new(burn_args),file);
            burn_run_process();
            startdlg_delete(dlg);
        }
    }
    g_free(mime);
}


void
burn_cue_or_toc(const gchar *file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);

    StartDlg *dlg = burn_show_start_dlg(burn_cd_image);
	if(dlg != NULL)
	{
		burn_args = exec_new(_("Burning CD image"), _("Please wait while the CD image you selected is burned to CD."));
		cdrdao_add_image_args(exec_cmd_new(burn_args), file);
		burn_run_process(FALSE);
        startdlg_delete(dlg);        
	}
}


void
burn_cd_image_file(const gchar *file)
{
	GB_LOG_FUNC
	g_return_if_fail(file != NULL);
    
    /* GnomeVFS fails to correctly identify a CD image if the extension is no .iso, so we'll offer the user to burn it anyway 
        When gnomevfs is fixed, we'll go back to the previous code.
        gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_BUTTONS_NONE,
        _("The file you have selected is not a cd image. Please select a cd image to burn."));*/
    gchar *mime = gbcommon_get_mime_type(file);            
    if(g_ascii_strcasecmp(mime, "application/x-cd-image") == 0)
        burn_cd_iso(file);
    else if(gbcommon_str_has_suffix(file, ".cue") || gbcommon_str_has_suffix(file, ".toc"))
        burn_cue_or_toc(file);    
    else if(gnomebaker_show_msg_dlg(NULL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE, 
            _("The file you have selected seems not to be a CD image. Do you really want to write it to CD?")) == GTK_RESPONSE_YES) 
        burn_cd_iso(file);
    g_free(mime);
}


void
burn_create_data_cd(const gchar *arguments_file)
{
	GB_LOG_FUNC
    g_return_if_fail(arguments_file != NULL);
    
    StartDlg *dlg = burn_show_start_dlg(create_data_cd);
	if(dlg != NULL)
	{	                     
		if(preferences_get_bool(GB_CREATEISOONLY))
		{
            burn_args = exec_new(_("Creating data CD image"), _("Please wait while the data CD image is created."));
			mkisofs_add_args(exec_cmd_new(burn_args), dlg, arguments_file, NULL, FALSE);
            burn_run_process();
		}
        else if(preferences_get_bool(GB_ONTHEFLY))
        {
            burn_args = exec_new(_("Burning data CD"), _("Please wait while the data is burned directly to CD."));
            mkisofs_add_args(exec_cmd_new(burn_args), dlg, arguments_file, NULL, TRUE);
            ExecCmd *cmd = exec_cmd_new(burn_args);
            cmd->piped = TRUE;
            mkisofs_add_args(cmd, dlg, arguments_file, NULL, FALSE);
            cdrecord_add_iso_args(exec_cmd_new(burn_args), NULL);
            burn_run_process();
        }
		else
		{
            burn_args = exec_new(_("Burning data CD"), _("Please wait while the data disk image is created and then burned to CD. Depending on the speed of your CD writer, this may take some time. "));
            mkisofs_add_args(exec_cmd_new(burn_args), dlg,  arguments_file, NULL, TRUE);			
            mkisofs_add_args(exec_cmd_new(burn_args), dlg, arguments_file, NULL, FALSE);
            gchar *file = preferences_get_create_data_cd_image();
            cdrecord_add_iso_args(exec_cmd_new(burn_args), file);            
            g_free(file);
            burn_run_process();
		}
        startdlg_delete(dlg);
	}
}


void
burn_append_data_cd(const gchar *arguments_file, const gchar *msinfo)
{
    GB_LOG_FUNC
    g_return_if_fail(arguments_file != NULL);
    g_return_if_fail(msinfo != NULL);    
    
    StartDlg *dlg = burn_show_start_dlg(append_data_cd);
    if(dlg != NULL)
    {                        
        if(preferences_get_bool(GB_ONTHEFLY))
        {
            burn_args = exec_new(_("Appending to data CD"), _("Please wait while the additional data is burned directly to CD."));
            mkisofs_add_args(exec_cmd_new(burn_args), dlg, arguments_file, msinfo, TRUE);
            ExecCmd *cmd = exec_cmd_new(burn_args);
            cmd->piped = TRUE;
            mkisofs_add_args(cmd, dlg, arguments_file, msinfo, FALSE);
            cdrecord_add_iso_args(exec_cmd_new(burn_args), NULL);
            burn_run_process();
        }
        else
        {
            burn_args = exec_new(_("Appending to data CD"), _("Please wait while the data disk image is created and then appended to the CD. Depending on the speed of your CD writer, this may take some time. "));
            mkisofs_add_args(exec_cmd_new(burn_args), dlg, arguments_file, msinfo, TRUE);
            mkisofs_add_args(exec_cmd_new(burn_args), dlg, arguments_file, msinfo, FALSE);
            gchar *file = preferences_get_create_data_cd_image();            
            cdrecord_add_iso_args(exec_cmd_new(burn_args), file);
            g_free(file);      
            burn_run_process();
        }
        startdlg_delete(dlg);
    }
}


void
burn_create_audio_cd(GSList *media_infos)
{
    GB_LOG_FUNC
    g_return_if_fail(media_infos != NULL);
    
    StartDlg *dlg = burn_show_start_dlg(create_audio_cd);
    if(dlg != NULL)
    {       
        burn_args = exec_new(_("Burning audio CD"), _("Please wait while the selected tracks are converted to CD audio and then burned to CD."));
        const gboolean on_the_fly = FALSE;/*preferences_get_bool(GB_ONTHEFLY);*/
        
        gchar *track_dir = preferences_get_convert_audio_track_dir();
        GList *audio_files = NULL;
        gint i = 0, sectors = 0;            
        GSList *media_info = media_infos; 
        for(; media_info != NULL; media_info = media_info->next)
        {
            MediaInfo *info = (MediaInfo*)media_info->data;     
            ExecCmd *cmd = exec_cmd_new(burn_args);
            cmd->piped = on_the_fly;

            gchar *file_name = g_strdup_printf("gbtrack_%.2d.wav", i + 1);
            gchar *converted_file = g_build_filename(track_dir, file_name, NULL);
            if(!on_the_fly)                                   
                audio_files = g_list_append(audio_files, converted_file);  
            else 
                g_free(converted_file);                    
            gstreamer_add_args(cmd, info->file_name, converted_file);
            
            gchar *inf_file_name = g_strdup_printf("gbtrack_%.2d.inf", i + 1);
            gchar *inf_file = g_build_filename(track_dir, inf_file_name, NULL);
            media_info_create_inf_file(info, i + 1, inf_file, &sectors);
            if(on_the_fly) 
                audio_files = g_list_append(audio_files, inf_file);  
            else 
                g_free(inf_file);                    
                
            g_free(file_name);
            g_free(inf_file_name);

            ++i;
        }
        
        ExecCmd *cmd = exec_cmd_new(burn_args);
        cdrecord_add_create_audio_cd_args(cmd, audio_files);                
        cmd->working_dir = track_dir;
        
        for(; audio_files != NULL; audio_files = audio_files->next)
            g_free(audio_files->data);
        g_list_free(audio_files);
        
        burn_run_process();
        startdlg_delete(dlg);
    }
}


void
burn_copy_data_cd()
{
	GB_LOG_FUNC

    StartDlg *dlg = burn_show_start_dlg(copy_data_cd);
	if(dlg != NULL)
	{        
		if(preferences_get_bool(GB_CREATEISOONLY))
		{
            burn_args = exec_new(_("Extracting CD image"), _("Please wait while the data CD image is extracted."));
			readcd_add_copy_args(exec_cmd_new(burn_args), dlg);
            burn_run_process();
		}
		else
		{
            burn_args = exec_new(_("Copying data CD"), _("Please wait while the data CD image is extracted and then burned to CD."));
            readcd_add_copy_args(exec_cmd_new(burn_args), dlg);
            gchar *file = preferences_get_copy_data_cd_image();
            mkisofs_add_calc_iso_size_args(exec_cmd_new(burn_args), file);            
            cdrecord_add_iso_args(exec_cmd_new(burn_args), file);
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
    StartDlg *dlg = burn_show_start_dlg(copy_audio_cd);
	if(dlg != NULL)
	{
		burn_args = exec_new(_("Copying audio CD"), _("Please wait while the audio CD tracks are extracted and then burned to CD."));
		cdda2wav_add_copy_args(exec_cmd_new(burn_args));
		cdrecord_add_audio_args(exec_cmd_new(burn_args));
		burn_run_process();
        startdlg_delete(dlg);
	}
}


void
burn_copy_cd()
{
    GB_LOG_FUNC
    StartDlg *dlg = burn_show_start_dlg(copy_cd);
    if(dlg != NULL)
    {
        burn_args = exec_new(_("Copying CD"), _("Please wait while the CD is copied"));
        cdrdao_add_copy_args(exec_cmd_new(burn_args));
        burn_run_process();
        startdlg_delete(dlg);
    }
}


void
burn_blank_cdrw()
{
	GB_LOG_FUNC
	
    StartDlg *dlg = burn_show_start_dlg(blank_cdrw);
	if(dlg != NULL)
	{		
		burn_args = exec_new(_("Blanking CD"), _("Please wait while the CD is blanked."));
		cdrecord_add_blank_args(exec_cmd_new(burn_args));
		burn_run_process();
        startdlg_delete(dlg);
	}
}


void
burn_format_dvdrw()
{
	GB_LOG_FUNC
	
    StartDlg *dlg = burn_show_start_dlg(format_dvdrw);
    if(dlg != NULL)
	{		
		burn_args = exec_new(_("Formatting re-writeable DVD"), _("Please wait while the DVD is formatted."));
		dvdformat_add_args(exec_cmd_new(burn_args));
		burn_run_process();
        startdlg_delete(dlg);
	}
    /*burn_test();*/
}


void
burn_create_data_dvd(const gchar *arguments_file)
{
	GB_LOG_FUNC
    g_return_if_fail(arguments_file != NULL);
    
    StartDlg *dlg = burn_show_start_dlg(create_data_dvd);
    if(dlg != NULL)
	{	            
        if(preferences_get_bool(GB_CREATEISOONLY))
        {
            burn_args = exec_new(_("Creating data DVD image"), _("Please wait while the data DVD image is created."));
            mkisofs_add_args(exec_cmd_new(burn_args), dlg, arguments_file, NULL, FALSE);
            burn_run_process();
        }
        else
        {
            burn_args = exec_new(_("Burning data DVD"), _("Please wait while the data is burned directly to DVD."));
            growisofs_add_args(exec_cmd_new(burn_args), dlg, arguments_file, NULL);
            burn_run_process();
        }
        startdlg_delete(dlg);
	}
}


void
burn_append_data_dvd(const gchar *arguments_file, const gchar *msinfo)
{
    GB_LOG_FUNC
    g_return_if_fail(arguments_file != NULL);
    g_return_if_fail(msinfo != NULL);
    
    StartDlg *dlg = burn_show_start_dlg(append_data_dvd);
    if(dlg != NULL)
    {   
        burn_args = exec_new(_("Appending to data DVD"), _("Please wait while the data is appended directly to the DVD."));
        growisofs_add_args(exec_cmd_new(burn_args), dlg, arguments_file, msinfo);
        burn_run_process();
        startdlg_delete(dlg);
    }
}


gboolean
burn_init()
{
    GB_LOG_FUNC
    gint i;
    for (i = 0; i < BurnTypeCount; ++i)
        BurnTypeText[i] = _(BurnTypeText[i]);
    return TRUE;
}


/* TEST CODE */

static gint total_time = 10;

static void
test_pre_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    progressdlg_set_status(_("Verifying the disk content"));
    progressdlg_increment_exec_number();
    progressdlg_start_approximation(total_time);
}


static void
test_lib_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    static gint interval = 50000;
    gint counter = 0;
    
    while(counter < ((total_time - 9) * (G_USEC_PER_SEC / interval)))
    {
        while(gtk_events_pending())
            gtk_main_iteration();
        g_usleep(interval);
        ++counter;
    }
}


static void
test_post_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    progressdlg_stop_approximation();
}


void 
burn_test()
{
    GB_LOG_FUNC
    burn_args = exec_new(_("Appending to data DVD"), _("Please wait while the data is appended directly to the DVD."));
    ExecCmd *cmd = exec_cmd_new(burn_args);
    cmd->pre_proc = test_pre_proc;
    cmd->lib_proc = test_lib_proc;
    cmd->post_proc = test_post_proc;
    burn_run_process();
}


