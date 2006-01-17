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
 * File: execfunctions.c
 * Copyright: luke_biddell@yahoo.com
 * Created on: Sun Jan 11 17:22:09 2004
 */

#include "execfunctions.h"
#include <glib/gprintf.h>
#include <ctype.h>
#include "preferences.h"
#include "gnomebaker.h"
#include "progressdlg.h"
#include "gbcommon.h"
#include "devices.h"
#include "datacd.h"
#include <gst/gst.h>
#include "media.h"


static gint cdrecord_totaltrackstowrite = 1;
static gint cdrecord_firsttrack = -1;
static guint64 cdrecord_totaldiskbytes = 0;

static gint cdda2wav_totaltracks = -1;
static gint cdda2wav_totaltracksread = 0;

static gint readcd_totalguchars = -1;
/*static gint cdrdao_cdminutes = -1;*/


static void
execfunctions_find_line_set_status(const gchar* buffer, const gchar* text, const gchar delimiter)
{
    GB_LOG_FUNC    
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(text != NULL);
    
    const gchar* start = strstr(buffer, text);
    if(start != NULL)
    {
        const gchar* ptr = start;
        while(*ptr != delimiter)
            ++ptr;
        gchar* message = g_strndup(start, (ptr - start) / sizeof(gchar));
        g_strstrip(message);
        progressdlg_set_status(message);
        g_free(message);
    }    
}


static void
execfunctions_prompt_for_disk_post_proc(void* ex, void* buffer)
{
    GB_LOG_FUNC   
    if(((ExecCmd*)ex)->exitCode == 0 && devices_reader_is_also_writer())
    {
        devices_eject_disk(GB_WRITER);
        devices_prompt_for_disk(progressdlg_get_window(), GB_WRITER);
    }
}


/*******************************************************************************
 * CDRECORD
 ******************************************************************************/

static void
cdrecord_blank_pre_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	progressdlg_set_status(_("Preparing to blank disk"));
    if(devices_prompt_for_disk(progressdlg_get_window(), GB_WRITER) == GTK_RESPONSE_CANCEL)
        exec_cmd_set_state((ExecCmd*)ex, CANCELLED);
    else
        progressdlg_pulse_start();
    devices_unmount_device(GB_WRITER);
}


static void
cdrecord_blank_post_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	progressdlg_pulse_stop();
}


static void
cdrecord_blank_read_proc(void* ex, void* buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);

    gchar *buf = (gchar*)buffer;
    execfunctions_find_line_set_status(buf, "Last chance", '.');
    execfunctions_find_line_set_status(buf, "Performing OPC", '.');
    execfunctions_find_line_set_status(buf, "Blanking", '\r');
    progressdlg_append_output(buffer);
}


/*
 * We pass a pointer to this function to Exec which will call us when it has
 * read from it pipe. We get the data and stuff the text into our text entry
 * for the user to read.
 */
static void
cdrecord_pre_proc(void* ex, void* buffer)
{   
    GB_LOG_FUNC 
    g_return_if_fail(ex != NULL);
    
    progressdlg_set_status(_("Preparing to burn disk"));
    progressdlg_increment_exec_number();
    
    if(devices_prompt_for_disk(progressdlg_get_window(), GB_WRITER) == GTK_RESPONSE_CANCEL)
        exec_cmd_set_state((ExecCmd*)ex, CANCELLED);
    else 
    {
        /* If we have the total disk bytes set then let cdrecord know about it */
        if(cdrecord_totaldiskbytes > 0)
            exec_cmd_update_arg((ExecCmd*)ex, "tsize=", "tsize=%ds", cdrecord_totaldiskbytes);
        devices_unmount_device(GB_WRITER);
    }
}


static void
cdrecord_read_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);

	gchar *buf = (gchar*)buffer;
	const gchar* track = strstr(buf, "Track");
	if(track != NULL)
	{
        gint currenttrack = 0;
        gfloat current = 0, total = 0; 
        
        const gchar* of = strstr(buf, " of ");
        if(of != NULL) /* regular burn i.e not on the fly */
        {
            /*  Track 01:    3 of   19 MB written (fifo 100%) [buf  98%]   4.5x. */        
            sscanf(track, "%*s %d %*s %f %*s %f", &currenttrack, &current, &total);
        }
        else /* on the fly */
        {            
            /* Track 01:   12 MB written (fifo 100%) [buf  96%]   4.0x. */
            sscanf(track, "%*s %d %*s %f", &currenttrack, &current);
            total = cdrecord_totaldiskbytes / 1024 / 1024;
        }
        
        if(current > 0.0)
        {
            if(cdrecord_firsttrack == -1)
                cdrecord_firsttrack = currenttrack;			
    
            /* Figure out how many tracks we have written so far and calc the fraction */
            gfloat totalfraction = 
                ((gfloat)currenttrack - cdrecord_firsttrack) * (1.0 / cdrecord_totaltrackstowrite);
            
            /* now add on the fraction of the track we are currently writing */
            totalfraction += ((current / total) * (1.0 / cdrecord_totaltrackstowrite));
            
            /*GB_TRACE("^^^^^ current [%d] first [%d] current [%f] total [%f] fraction [%f]",
                currenttrack, cdrecord_firsttrack, current, total, totalfraction);*/
            
            gchar* status = g_strdup_printf(_("Writing track %d"), currenttrack);
            progressdlg_set_status(status);
            g_free(status);
            progressdlg_set_fraction(totalfraction);
        }
	}
    else 
    {
        progressdlg_append_output(buffer);        
        execfunctions_find_line_set_status(buf, "Performing OPC", '.');
        execfunctions_find_line_set_status(buf, "Last chance", '.');
        execfunctions_find_line_set_status(buf, "Writing Leadout", '.');
        execfunctions_find_line_set_status(buf, "Fixating", '.');	
    }
}


/*
 *  Populates the common information required to burn a cd
 */
static void
cdrecord_add_common_args(ExecCmd* cdBurn)
{
	GB_LOG_FUNC
	g_return_if_fail(cdBurn != NULL);	
    
    cdrecord_totaldiskbytes = 0;
    cdrecord_totaltrackstowrite = 1;
    cdrecord_firsttrack = -1;

	exec_cmd_add_arg(cdBurn, "cdrecord");
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cdBurn, "dev=%s", writer);
	g_free(writer);

	if(preferences_get_bool(GB_CDRECORD_FORCE))
	   exec_cmd_add_arg(cdBurn, "-force");
	
	exec_cmd_add_arg(cdBurn, "gracetime=5");
	exec_cmd_add_arg(cdBurn, "speed=%d", preferences_get_int(GB_CDWRITE_SPEED));
	exec_cmd_add_arg(cdBurn, "-v");	

	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cdBurn, "-eject");

	if(preferences_get_bool(GB_DUMMY))
		exec_cmd_add_arg(cdBurn, "-dummy");
	
	if(preferences_get_bool(GB_BURNFREE))
		exec_cmd_add_arg(cdBurn, "driveropts=burnfree");
	
	gchar* mode = preferences_get_string(GB_WRITE_MODE);
	if(g_ascii_strcasecmp(mode, _("default")) != 0)
		exec_cmd_add_arg(cdBurn, "-%s", mode);
	g_free(mode);	
}


void
cdrecord_add_create_audio_cd_args(ExecCmd* e, const GList* audiofiles)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
	cdrecord_add_common_args(e);
	exec_cmd_add_arg(e, "-audio");	
    exec_cmd_add_arg(e, "-pad");
    exec_cmd_add_arg(e, "-useinfo");
    exec_cmd_add_arg(e, "-text");
    
	/* if we are on the fly this will be a list of inf files, otherwise
     * it's the list of wavs to burn */
    const GList *audiofile = audiofiles;
    for (; audiofile != NULL ; audiofile = audiofile->next)
    {
        exec_cmd_add_arg(e, audiofile->data);
        GB_TRACE("cdrecord - adding create audio data [%s]", (gchar*)audiofile->data);
        cdrecord_totaltrackstowrite++;
    }		
	e->readProc = cdrecord_read_proc;
	e->preProc = cdrecord_pre_proc;
}


/*
 *  ISO
 */
void
cdrecord_add_iso_args(ExecCmd* cdBurn, const gchar* iso)
{
	GB_LOG_FUNC
	g_return_if_fail(cdBurn != NULL);
	
	cdrecord_add_common_args(cdBurn);
	
	/*if(!prefs->multisession)*/
		exec_cmd_add_arg(cdBurn, "-multi");
        
    exec_cmd_add_arg(cdBurn, "tsize=%ds", -1);
    exec_cmd_add_arg(cdBurn, "-pad");

    if(iso != NULL)
	    exec_cmd_add_arg(cdBurn, iso);
	else 
	    exec_cmd_add_arg(cdBurn, "-"); /* no filename so we're on the fly */
	
	cdBurn->readProc = cdrecord_read_proc;
	cdBurn->preProc = cdrecord_pre_proc;
}


static void
cdrecord_copy_audio_cd_pre_proc(void* ex, void* buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    
    cdrecord_pre_proc(ex, buffer);
    
    GError *err = NULL; 
    gchar* tmp = preferences_get_string(GB_TEMP_DIR);
    GDir *dir = g_dir_open(tmp, 0, &err);
    if(dir != NULL)
    {
        cdrecord_totaltrackstowrite = 0;
        /* loop around reading the files in the directory */
        const gchar *name = g_dir_read_name(dir); 
        while(name != NULL)
        {
            if(g_str_has_suffix(name, ".wav"))
            {
                GB_TRACE("adding [%s]", name);
                gchar* fullpath = g_build_filename(tmp, name, NULL);
                exec_cmd_add_arg(ex, fullpath);
                cdrecord_totaltrackstowrite++;
                g_free(fullpath);
            }
            
            name = g_dir_read_name(dir);
        }
        g_dir_close(dir);
    }
    g_free(tmp);    
}


void 
cdrecord_add_audio_args(ExecCmd* cdBurn)
{
	GB_LOG_FUNC
	g_return_if_fail(cdBurn != NULL);
	cdrecord_add_common_args(cdBurn);
	exec_cmd_add_arg(cdBurn, "-useinfo");
	exec_cmd_add_arg(cdBurn, "-audio");
	exec_cmd_add_arg(cdBurn, "-pad");
	  
	cdBurn->readProc = cdrecord_read_proc;
	cdBurn->preProc = cdrecord_copy_audio_cd_pre_proc;	
}


void 
cdrecord_add_blank_args(ExecCmd* cdBurn)
{
	GB_LOG_FUNC
	g_return_if_fail(cdBurn != NULL);	   
	
	cdBurn->readProc = cdrecord_blank_read_proc;
	cdBurn->preProc = cdrecord_blank_pre_proc;
	cdBurn->postProc = cdrecord_blank_post_proc;

	exec_cmd_add_arg(cdBurn, "cdrecord");
	
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cdBurn, "dev=%s", writer);
	g_free(writer);

	exec_cmd_add_arg(cdBurn, "speed=%d", preferences_get_int(GB_CDWRITE_SPEED));
	exec_cmd_add_arg(cdBurn, "-v");
	/*exec_cmd_add_arg(cdBurn, "-format");*/
    
    if(preferences_get_bool(GB_CDRECORD_FORCE))
       exec_cmd_add_arg(cdBurn, "-force");

	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cdBurn, "-eject");
	
	exec_cmd_add_arg(cdBurn, "gracetime=5");
	exec_cmd_add_arg(cdBurn, "blank=%s", preferences_get_bool(GB_FAST_BLANK) ? "fast" : "all");
}


/*******************************************************************************
 * CDDA2WAV
 ******************************************************************************/


static void
cdda2wav_pre_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	cdda2wav_totaltracks = -1;
	cdda2wav_totaltracksread = 0;
	
	progressdlg_set_status(_("Preparing to extract audio tracks"));
	progressdlg_increment_exec_number();

	gint response = GTK_RESPONSE_NO;
	
	gchar* tmp = preferences_get_string(GB_TEMP_DIR);
	gchar* file = g_strdup_printf("%s/gbtrack_01.wav", tmp);	
	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		response = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
			_("Audio tracks from a previous sesion already exist on disk, "
			"do you wish to use the existing tracks?"));
	}
	
	g_free(file);
	
	if(response == GTK_RESPONSE_NO)
	{		
		gchar* cmd = g_strdup_printf("rm -fr %s/gbtrack*", tmp);
		system(cmd);
		g_free(cmd);
	    response = devices_prompt_for_disk(progressdlg_get_window(), GB_READER);
	}
	
	if(response == GTK_RESPONSE_CANCEL)
		exec_cmd_set_state((ExecCmd*)ex, CANCELLED);
	else if(response == GTK_RESPONSE_YES)
		exec_cmd_set_state((ExecCmd*)ex, SKIPPED);
        
	devices_unmount_device(GB_READER);
	g_free(tmp);
}


static void
cdda2wav_read_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	
	gchar* text = (gchar*)buffer;
	if(cdda2wav_totaltracks == -1)
	{
		const gchar* tracksstart = strstr(text, "Tracks:");		
		if(tracksstart != NULL)
            sscanf(tracksstart, "Tracks:%d", &cdda2wav_totaltracks);
        progressdlg_append_output(text);
	}
	else if(cdda2wav_totaltracks)
	{	
		const gchar* pcnt = strchr(text, '%');
		if(pcnt != NULL)
		{			
			do
				pcnt--;
            while(!g_ascii_isspace(*pcnt));
            gfloat fraction = 0.0;
            sscanf(++pcnt, "%f%%", &fraction);
            fraction /= 100.0;
            fraction *= (1.0/(gfloat)cdda2wav_totaltracks);			
			fraction += ((gfloat)cdda2wav_totaltracksread *(1.0/(gfloat)cdda2wav_totaltracks));
			progressdlg_set_fraction(fraction);

            const gchar* hundred = strstr(text, "rderr,");
			if(hundred != NULL)
				cdda2wav_totaltracksread++;
    
            gchar* status = g_strdup_printf(_("Extracting audio track %d"), cdda2wav_totaltracksread + 1);
            progressdlg_set_status(status);
            g_free(status);                
		}
        else
            progressdlg_append_output(text);
	}			
}


/*
 * Populates the information required to extract audio tracks from an existing
 * audio cd
 */
void
cdda2wav_add_copy_args(ExecCmd* e)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
	exec_cmd_add_arg(e, "cdda2wav");
	exec_cmd_add_arg(e, "-x");
	exec_cmd_add_arg(e, "cddb=1");
    exec_cmd_add_arg(e, "speed=52");
	exec_cmd_add_arg(e, "-B");
	exec_cmd_add_arg(e, "-g");
	exec_cmd_add_arg(e, "-Q");
	exec_cmd_add_arg(e, "-paranoia");
	
	gchar* reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(e, "-D%s", reader);
	g_free(reader);
	
	exec_cmd_add_arg(e, "-Owav");
	
	gchar* tmp = preferences_get_string(GB_TEMP_DIR);
	exec_cmd_add_arg(e, "%s/gbtrack", tmp);
	g_free(tmp);

	e->preProc = cdda2wav_pre_proc;
	e->readProc = cdda2wav_read_proc;
    e->postProc = execfunctions_prompt_for_disk_post_proc;
}


/*******************************************************************************
 * MKISOFS
 ******************************************************************************/


/*
	mkisofs -o gb.iso -R -J -hfs *

 mkisofs -V "Jamie Michelle Wedding" -p "Luke Biddell" -A "GnomeBaker" -o gb.iso -R -J -hfs *

 also 
 -L (include dot files) -X (mixed case filenames) -s (multiple dots in name)
	 
shell> NEXT_TRACK=`cdrecord -msinfo dev=0,6,0`
shell> echo $NEXT_TRACK
shell> mkisofs -R -o cd_image2 -C $NEXT_TRACK -M /dev/scd5 private_collection/
*/


static void
mkisofs_calc_size_pre_proc(void* ex, void* buffer)
{   
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    
    progressdlg_set_status(_("Calculating data disk image size"));
    progressdlg_increment_exec_number();
    progressdlg_pulse_start();
}

    
static void
mkisofs_calc_size_read_proc(void* ex, void* buffer)
{
    GB_LOG_FUNC 
    g_return_if_fail(ex != NULL);
    g_return_if_fail(buffer != NULL);

    const gchar* written = strstr(buffer, "written =");
    if(written != NULL)
    {
        if(sscanf(written, "written = %llu\n", &cdrecord_totaldiskbytes) == 1)
            GB_TRACE("mkisofs_calc_size_read_proc - size is [%llu]", cdrecord_totaldiskbytes);
    }
    progressdlg_append_output(buffer);
}


static void
mkisofs_calc_size_post_proc(void* ex, void* buffer)
{   
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    progressdlg_pulse_stop();
}


static void
mkisofs_pre_proc(void* ex, void* buffer)
{	
	GB_LOG_FUNC
	g_return_if_fail(ex != NULL);
	
	progressdlg_set_status(_("Creating data disk image"));
	progressdlg_increment_exec_number();
	
	gchar* file = preferences_get_create_data_cd_image();	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		gint ret = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_OK,
			_("A data CD image from a previous session already exists on disk, "
			"do you wish to use the existing image?"));
		
		if(ret  == GTK_RESPONSE_YES)		
			exec_cmd_set_state((ExecCmd*)ex, SKIPPED);
		else if(ret == GTK_RESPONSE_CANCEL)
			exec_cmd_set_state((ExecCmd*)ex, CANCELLED);
	}
	
	g_free(file);
}


static void
mkisofs_read_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC	
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);
    
	const gchar* percent = strrchr(buffer, '%');
	if(percent != NULL)
	{
		while((!g_ascii_isspace(*percent)) && (percent > (gchar*)buffer))
			--percent;		
        gfloat progress = 0.0;
        if(sscanf(++percent, "%f%%", &progress) == 1)
		  progressdlg_set_fraction(progress/100.0);
	}
    else 
	   progressdlg_append_output(buffer);
}


static gboolean
mkisofs_foreach_func(GtkTreeModel* model,
                GtkTreePath* path,
                GtkTreeIter* iter,
                gpointer user_data)
{
	GB_LOG_FUNC
	gchar *file = NULL, *filepath = NULL;
	gboolean existingsession = FALSE;
		
	gtk_tree_model_get (model, iter, DATACD_COL_FILE, &file,
		DATACD_COL_PATH, &filepath, DATACD_COL_SESSION, &existingsession, -1);
	
	/* Only add files that are not part of an existing session */
	if(!existingsession)
		exec_cmd_add_arg((ExecCmd*)user_data, "%s=%s", file, filepath); 

	g_free(file);	
	g_free(filepath);
	
	return FALSE; /* do not stop walking the store, call us with next row */
}


static void
mkisofs_add_common_args(ExecCmd* e, GtkTreeModel* datamodel, StartDlg* start_dlg)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);   
    g_return_if_fail(datamodel != NULL);
    g_return_if_fail(start_dlg != NULL);
    
    const gchar* text = gtk_entry_get_text(start_dlg->volume_id);
    if(text != NULL && strlen(text) > 0)
    {
        exec_cmd_add_arg(e, "-V");
        exec_cmd_add_arg(e, text);
    }
    text = gtk_entry_get_text(start_dlg->app_id);
    if(text != NULL && strlen(text) > 0)
    {
        exec_cmd_add_arg(e, "-A");
        exec_cmd_add_arg(e, text);
    }
    text = gtk_entry_get_text(start_dlg->preparer);
    if(text != NULL && strlen(text) > 0)
    {
        exec_cmd_add_arg(e, "-p");
        exec_cmd_add_arg(e, text);
    }
    text = gtk_entry_get_text(start_dlg->publisher);
    if(text != NULL && strlen(text) > 0)
    {
        exec_cmd_add_arg(e, "-publisher");
        exec_cmd_add_arg(e, text);
    }
    text = gtk_entry_get_text(start_dlg->copyright);
    if(text != NULL && strlen(text) > 0)
    {
        exec_cmd_add_arg(e, "-copyright");
        exec_cmd_add_arg(e, text);
    }
    text = gtk_entry_get_text(start_dlg->abstract);
    if(text != NULL && strlen(text) > 0)
    {
        exec_cmd_add_arg(e, "-abstract");
        exec_cmd_add_arg(e, text);
    }
    text = gtk_entry_get_text(start_dlg->bibliography);
    if(text != NULL && strlen(text) > 0)
    {
        exec_cmd_add_arg(e, "-bilio");
        exec_cmd_add_arg(e, text);
    }
        
    exec_cmd_add_arg(e, "-iso-level");
    exec_cmd_add_arg(e, "3");
    exec_cmd_add_arg(e, "-l"); /* allow 31 character iso9660 filenames */
    
    if(preferences_get_bool(GB_ROCKRIDGE))      
    {
        exec_cmd_add_arg(e, "-R");
        exec_cmd_add_arg(e, "-hide-rr-moved");
    }
    
    if(preferences_get_bool(GB_JOLIET))
    {
        exec_cmd_add_arg(e, "-J");
        exec_cmd_add_arg(e, "-joliet-long");
    }    
    
    /*exec_cmd_add_arg(e, "-f"); don't follow links */
    /*exec_cmd_add_arg(e, "-hfs");*/        
    
    exec_cmd_add_arg(e, "-graft-points");
    gtk_tree_model_foreach(datamodel, mkisofs_foreach_func, e);         
} 


void
mkisofs_add_args(ExecCmd* e, GtkTreeModel* datamodel, StartDlg* start_dlg, const gboolean calculatesize)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	g_return_if_fail(datamodel != NULL);
    g_return_if_fail(start_dlg != NULL);
	cdrecord_totaldiskbytes = 0;
    
    exec_cmd_add_arg(e, "mkisofs");     
	
	/* If this is a another session on an existing cd we don't show the 
	   iso details dialog */	
	gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datamodel), DATACD_EXISTING_SESSION);
	if(msinfo != NULL)
	{
		exec_cmd_add_arg(e, "-C");
		exec_cmd_add_arg(e, msinfo);
		
		gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
		exec_cmd_add_arg(e, "-M");
		exec_cmd_add_arg(e, writer);
		g_free(writer);			
		
		/* This is a cludge so that we don't ask the user if they want to use the existing iso */
		gchar* createdataiso = preferences_get_create_data_cd_image();
		gchar* cmd = g_strdup_printf("rm -fr %s", createdataiso);
		system(cmd);			
		g_free(cmd);
		g_free(createdataiso);
	}	
    
    if(preferences_get_bool(GB_CREATEISOONLY))
    {
        exec_cmd_add_arg(e, "-gui");    
        exec_cmd_add_arg(e, "-o");                    
        exec_cmd_add_arg(e, gtk_entry_get_text(start_dlg->iso_file));
    }
    else if(!preferences_get_bool(GB_ONTHEFLY))
    {            
        exec_cmd_add_arg(e, "-gui");    
        exec_cmd_add_arg(e, "-o");            
        gchar* file = preferences_get_create_data_cd_image();            
        exec_cmd_add_arg(e, file);
        g_free(file);
    }
    
    if(calculatesize)
    {
        exec_cmd_add_arg(e, "--print-size");      
        e->preProc = mkisofs_calc_size_pre_proc;
        e->readProc = mkisofs_calc_size_read_proc;
        e->postProc = mkisofs_calc_size_post_proc;
    }
    else
    {
        e->preProc = mkisofs_pre_proc;
        e->readProc = mkisofs_read_proc;
    }
    
    mkisofs_add_common_args(e, datamodel, start_dlg);
	
	/* We don't own the msinfo gchar datacd does 
	g_free(msinfo);*/
}


void 
mkisofs_add_calc_iso_size_args(ExecCmd* e, const gchar* iso)
{
    g_return_if_fail(e != NULL);
    g_return_if_fail(iso != NULL);    
    
    exec_cmd_add_arg(e, "mkisofs");       
    exec_cmd_add_arg(e, "--print-size");
    exec_cmd_add_arg(e, iso);
    e->preProc = mkisofs_calc_size_pre_proc;
    e->readProc = mkisofs_calc_size_read_proc;
    e->postProc = mkisofs_calc_size_post_proc;
}


/*******************************************************************************
 * DVD+RW TOOLS
 ******************************************************************************/

/*
 * We pass a pointer to this function to Exec which will call us when it has
 * read from it pipe. We get the data and stuff the text into our text entry
 * for the user to read.
 */
static void
dvdformat_pre_proc(void* ex, void* buffer)
{	
	GB_LOG_FUNC	
	g_return_if_fail(ex != NULL);
	
	progressdlg_set_status(_("Formatting DVD"));
	progressdlg_increment_exec_number();
    if(devices_prompt_for_disk(progressdlg_get_window(), GB_WRITER) == GTK_RESPONSE_CANCEL)
        exec_cmd_set_state((ExecCmd*)ex, CANCELLED);
    devices_unmount_device(GB_WRITER);
}


static void
dvdformat_read_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC	
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);		
    
    /*  * formatting 24.5% */     
    gfloat percent = 0.0;
    if(sscanf((gchar*)buffer, "%*s %*s %f%%", &percent) == 1)
        progressdlg_set_fraction(percent/100.0);
    else    
        progressdlg_append_output((gchar*)buffer);
}


static void
dvdformat_post_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	if(preferences_get_bool(GB_EJECT))
		devices_eject_disk(GB_WRITER);
}


void 
dvdformat_add_args(ExecCmd* dvdFormat)
{
	GB_LOG_FUNC
	g_return_if_fail(dvdFormat != NULL);
	
	dvdFormat->readProc = dvdformat_read_proc;
	dvdFormat->preProc = dvdformat_pre_proc;
	dvdFormat->postProc = dvdformat_post_proc;
	
	exec_cmd_add_arg(dvdFormat, "dvd+rw-format");
	
	gchar* writer = devices_get_device_config(GB_WRITER,GB_DEVICE_NODE_LABEL);
	exec_cmd_add_arg(dvdFormat, writer);
	g_free(writer);
	
	/* make the output gui friendly */
	exec_cmd_add_arg(dvdFormat, "-gui");
		
	if(preferences_get_bool(GB_FORCE))
	{
		if(!preferences_get_bool(GB_FAST_FORMAT))
			exec_cmd_add_arg(dvdFormat, "-force=full");
		else
			exec_cmd_add_arg(dvdFormat, "-force");
	}
	else if(!preferences_get_bool(GB_FAST_FORMAT))
	{
		exec_cmd_add_arg(dvdFormat, "-blank=full");
	}
    else
    {
        exec_cmd_add_arg(dvdFormat, "-blank");
    }        
}


/*******************************************************************************
 * GROWISOFS
 ******************************************************************************/


static void 
growisofs_pre_proc(void* ex,void* buffer)
{		
	GB_LOG_FUNC	
	g_return_if_fail(ex != NULL);
	
	progressdlg_set_status(_("Preparing to burn DVD"));
	progressdlg_increment_exec_number();
	if(devices_prompt_for_disk(progressdlg_get_window(), GB_WRITER) == GTK_RESPONSE_CANCEL)
		exec_cmd_set_state((ExecCmd*)ex, CANCELLED);
    devices_unmount_device(GB_WRITER);
}


static void 
growisofs_post_proc(void* ex,void* buffer)
{
	GB_LOG_FUNC
	if(preferences_get_bool(GB_EJECT))
		devices_eject_disk(GB_WRITER);
}


static void
growisofs_read_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);
    
	gchar *buf = (gchar*)buffer;
	const gchar* progressstr = strstr(buf, "done, estimate");
	if(progressstr != NULL)
	{
		gint progress = 0;
		if(sscanf(buf, "%d.%*d", &progress) > 0)
			progressdlg_set_fraction((gfloat)progress / 100.0);
        progressdlg_set_status(_("Writing DVD"));
	}
    else 
    {
        progressdlg_append_output(buf);        
    	execfunctions_find_line_set_status(buf, "restarting DVD", '.');
        execfunctions_find_line_set_status(buf, "writing lead-out", '\r');
    }
}


static void
growisofs_read_iso_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);
	
	gchar *buf = (gchar*)buffer;
	const gchar* progressstr = strstr(buf, "remaining");
	if(progressstr != NULL)
	{
		gint progress = 0;
		if(sscanf(buf,"%*d/%*d ( %d.%*d%%)",&progress) >0)
			progressdlg_set_fraction((gfloat)progress / 100.0);
        progressdlg_set_status(_("Writing DVD"));
	}
    else 
    {
    	execfunctions_find_line_set_status(buf, "restarting DVD", '.');
        execfunctions_find_line_set_status(buf, "writing lead-out", '\r');
    	progressdlg_append_output(buf);
    }
}


void
growisofs_add_args(ExecCmd* e, GtkTreeModel* datamodel, StartDlg* start_dlg)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	    
    exec_cmd_add_arg(e, "growisofs");           
    
    /* merge new session with existing one */	
    gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datamodel), DATACD_EXISTING_SESSION);
    if(msinfo != NULL)
        exec_cmd_add_arg(e, "-M");
    else
        exec_cmd_add_arg(e, "-Z");

    gchar* writer = devices_get_device_config(GB_WRITER,GB_DEVICE_NODE_LABEL);
    exec_cmd_add_arg(e, writer);
    g_free(writer);
    
    /* TODO: Overburn support
    if(preferences_get_int(GB_OVERBURN))
        exec_cmd_add_arg(growisofs, "-overburn"); */                
    
    exec_cmd_add_arg(e,"-speed=%d", preferences_get_int(GB_DVDWRITE_SPEED));
    
    /*http://fy.chalmers.se/~appro/linux/DVD+RW/tools/growisofs.c
        for the use-the-force options */        
    /* stop the reloading of the disc */
    exec_cmd_add_arg(e, "-use-the-force-luke=notray");
    
    /* force overwriting existing filesystem */
    exec_cmd_add_arg(e, "-use-the-force-luke=tty");
            
    /* http://www.troubleshooters.com/linux/coasterless_dvd.htm#_Gotchas 
        states that dao with dvd compat is the best way of burning dvds */
    gchar* mode = preferences_get_string(GB_DVDWRITE_MODE);
    if(g_ascii_strcasecmp(mode, _("default")) != 0)
        exec_cmd_add_arg(e, "-use-the-force-luke=dao");
    g_free(mode);    
    
    if(preferences_get_bool(GB_DUMMY))
        exec_cmd_add_arg(e, "-use-the-force-luke=dummy");
    
    if(preferences_get_bool(GB_FINALIZE))
        exec_cmd_add_arg(e, "-dvd-compat");

    exec_cmd_add_arg(e, "-gui");	
    
    mkisofs_add_common_args(e, datamodel, start_dlg);
			
	e->readProc = growisofs_read_proc;
    e->preProc = growisofs_pre_proc;
    e->postProc = growisofs_post_proc;
	
	/* We don't own the msinfo gchar datacd does 
	g_free(msinfo);*/
}

void 
growisofs_add_iso_args(ExecCmd* growisofs, const gchar *iso)
{
	GB_LOG_FUNC
	g_return_if_fail(growisofs != NULL);
	g_return_if_fail(iso != NULL);
	
	growisofs->readProc = growisofs_read_iso_proc;
	growisofs->preProc = growisofs_pre_proc;
	growisofs->postProc = growisofs_post_proc;
	exec_cmd_add_arg(growisofs, "growisofs");
	exec_cmd_add_arg(growisofs, "-dvd-compat");
	
	exec_cmd_add_arg(growisofs, "-speed=%d", preferences_get_int(GB_DVDWRITE_SPEED));
	
	/* -gui makes the output more verbose, so we can 
	    interpret it easier */
    /*	exec_cmd_add_arg(growisofs, "-gui"); */
	/* -gui does not work with iso's, argh! */

	/* stop the reloading of the disc */
	exec_cmd_add_arg(growisofs, "-use-the-force-luke=notray");
	
	/* force overwriting existing filesystem */
    exec_cmd_add_arg(growisofs, "-use-the-force-luke=tty");
	
    exec_cmd_add_arg(growisofs, "-Z");
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_NODE_LABEL);
	exec_cmd_add_arg(growisofs, "%s=%s", writer, iso);      
	g_free(writer);		
}


/*******************************************************************************
 * READCD
 ******************************************************************************/


static void
readcd_pre_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	readcd_totalguchars = -1;
	
	progressdlg_set_status(_("Reading CD image"));	
	progressdlg_increment_exec_number();
	
	gint response = GTK_RESPONSE_NO;
	gchar* file = preferences_get_copy_data_cd_image();	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		response = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
			_("A CD image from a previous session already exists on disk, "
			"do you wish to use the existing image?"));		
	}
	
	g_free(file);
	
	if(response == GTK_RESPONSE_NO)
        response = devices_prompt_for_disk(progressdlg_get_window(), GB_READER);
        
    if(response == GTK_RESPONSE_CANCEL)
		exec_cmd_set_state((ExecCmd*)ex, CANCELLED);
	else if(response == GTK_RESPONSE_YES)
		exec_cmd_set_state((ExecCmd*)ex, SKIPPED);
    devices_unmount_device(GB_READER);
}


static void
readcd_read_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);
		
	gchar* text = (gchar*)buffer;
	if(readcd_totalguchars == -1)		
	{
		gchar* end = strstr(text, "end:");	
		if(end != NULL)
		{			
			if(sscanf(end, "%*s %d", &readcd_totalguchars) > 0)
				GB_TRACE("readcd size is %d", readcd_totalguchars);
		}
		progressdlg_append_output(text);
	}
	else if(readcd_totalguchars)
	{
		const gchar* start = strstr(text, "addr:");
		if(start != NULL)
		{
			gint readguchars = 0;
			if(sscanf(start, "%*s %d", &readguchars) > 0)
				progressdlg_set_fraction((gfloat)readguchars/(gfloat)readcd_totalguchars);
		}
		else
			progressdlg_append_output(text);
	}	
}


/*
 *  Populates the information required to make an iso from an existing data cd
 */
void
readcd_add_copy_args(ExecCmd* e, StartDlg* start_dlg)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	g_return_if_fail(start_dlg != NULL);	

	exec_cmd_add_arg(e, "readcd");
	
	gchar* reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(e, "dev=%s", reader);	
	g_free(reader);
	
    if(preferences_get_bool(GB_CREATEISOONLY))
    {
	   exec_cmd_add_arg(e, "f=%s", gtk_entry_get_text(start_dlg->iso_file));	
    }
    else
    {
       gchar* file = preferences_get_copy_data_cd_image();
       exec_cmd_add_arg(e, "f=%s", file);    
       g_free(file);   
    }
	/*exec_cmd_add_arg(e, "-notrunc");
	exec_cmd_add_arg(e, "-clone");
	exec_cmd_add_arg(e, "-silent");*/

	e->preProc = readcd_pre_proc;	
	e->readProc = readcd_read_proc;
    e->postProc = execfunctions_prompt_for_disk_post_proc;
}

/*******************************************************************************
 * CDRDAO
 ******************************************************************************/
/*
static void
cdrdao_extract_read_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);	
    
	const gchar* output = (gchar*)buffer;		
	const gchar* length = strstr(output, ", length");
	if(length != NULL)
	{
		gint len = 0;
		if(sscanf(length, ", length %d:", &len) == 1)
			cdrdao_cdminutes = len;
	}

	const gchar* newline = strstr(output, "\n");
	if(newline != NULL)
	{
		gint currentminutes = 0;
		if(sscanf(output, "\n%d:", &currentminutes) == 1)
			progressdlg_set_fraction((gfloat)currentminutes/(gfloat)cdrdao_cdminutes);
	}
	progressdlg_append_output(output);
}
*/
/* cdrdao copy --source-device 1,0,0 --source-driver generic-mmc --device
0,6,0 --speed 4 --on-the-fly --buffers 64 

sudo cdrdao write --device $CDR_DEVICE video.cue

Wrote 536 of 536 MB (Buffers 100%  96%)

*/
static void
cdrdao_write_image_read_proc(void* ex, void* buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);   
    
    const gchar* output = (gchar*)buffer;       
    const gchar* wrote = strstr(output, "Wrote");
    if(wrote != NULL)
    {
        gint current = 0, total = 0;
        if(sscanf(wrote, "Wrote %d of %d", &current, &total) == 2)
            progressdlg_set_fraction((gfloat)current/(gfloat)total);
    }
    else 
    {
        progressdlg_append_output(output);        
        execfunctions_find_line_set_status(output, "Writing track", '(');
    }
}


void
cdrdao_add_image_args(ExecCmd* cmd, const gchar* toc_or_cue)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);
	g_return_if_fail(toc_or_cue != NULL);	
    
    cmd->workingdir = g_path_get_dirname(toc_or_cue);
	
	cmd->preProc = cdrecord_pre_proc;	
	cmd->readProc = cdrdao_write_image_read_proc;
	
	exec_cmd_add_arg(cmd, "cdrdao");
	exec_cmd_add_arg(cmd, "write");
	
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cmd, "--device");
	exec_cmd_add_arg(cmd, writer);
	g_free(writer);	
	
	exec_cmd_add_arg(cmd, "--speed");
	exec_cmd_add_arg(cmd, "%d", preferences_get_int(GB_CDWRITE_SPEED));
    exec_cmd_add_arg(cmd, "--buffers");
    exec_cmd_add_arg(cmd, "64");
    exec_cmd_add_arg(cmd, "-n"); /* turn off the 10 second pause */
	
	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cmd, "--eject");

	if(preferences_get_bool(GB_DUMMY))
		exec_cmd_add_arg(cmd, "--simulate");
    
    exec_cmd_add_arg(cmd, toc_or_cue);
}


/*******************************************************************************
 * GSTREAMER
 ******************************************************************************/
static guint gstreamertimer = 0;

static gboolean
gstreamer_progress_timer(gpointer data)
{
    GB_LOG_FUNC   
    g_return_val_if_fail(data != NULL, FALSE);
    
    GstFormat fmt = GST_FORMAT_BYTES;
    gint64 pos = 0, total = 0;
    if(gst_element_query (GST_ELEMENT(data), GST_QUERY_POSITION, &fmt, &pos) && 
        gst_element_query (GST_ELEMENT(data), GST_QUERY_TOTAL, &fmt, &total))
    {
        progressdlg_set_fraction((gfloat)pos/(gfloat)total); 
    }
    return TRUE;
}
 
 
static void
gstreamer_pipeline_eos(GstElement* gstelement, gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(user_data != NULL);
    exec_cmd_set_state((ExecCmd*)user_data, COMPLETED);
}


static void
gstreamer_pipeline_error(GstElement* gstelement,
                        GstElement* element,
                        GError* error,
                        gchar* message,
                        gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(user_data != NULL);
    if(error != NULL)
        progressdlg_append_output(error->message);
    progressdlg_append_output(message);
    exec_cmd_set_state((ExecCmd*)user_data, FAILED);
}
 

static void
gstreamer_new_decoded_pad(GstElement* element,
                           GstPad* pad,
                           gboolean last,
                           MediaPipeline*  mip)
{
    GB_LOG_FUNC
    g_return_if_fail(mip != NULL);
    g_return_if_fail(element != NULL);
    g_return_if_fail(pad != NULL);
    
    GstCaps *caps = gst_pad_get_caps (pad);
    GstStructure *str = gst_caps_get_structure (caps, 0);
    if (!g_strrstr (gst_structure_get_name (str), "audio"))
        return;
    
    GstPad* decodepad = gst_element_get_pad (mip->converter, "src"); 
    gst_pad_link (pad, decodepad);
    gst_element_link(mip->decoder,mip->converter);
    
    gst_bin_add_many(GST_BIN(mip->pipeline), mip->converter, mip->scale, mip->encoder, mip->dest, NULL);

    /* This function synchronizes a bins state on all of its contained children. */
    gst_bin_sync_children_state(GST_BIN(mip->pipeline));
}
 
 
static void
gstreamer_pre_proc(void* ex, void* buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    ExecCmd* cmd = (ExecCmd*)ex;
    
    gchar* filename = g_path_get_basename((gchar*)g_ptr_array_index(cmd->args, 0));
    gchar* text = g_strdup_printf(_("Converting %s to cd audio"), filename);
    progressdlg_set_status(text);
    g_free(filename);
    g_free(text);
    progressdlg_increment_exec_number();
}


static void 
gstreamer_lib_proc(void* ex, void* data)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);  
    ExecCmd* cmd = (ExecCmd*)ex;
    gint* pipe = (gint*)data;
    
    GB_TRACE("Converting [%s] to [%s]", (gchar*)g_ptr_array_index(cmd->args, 0), 
        (gchar*)g_ptr_array_index(cmd->args, 1));
    
    MediaPipeline* gstdata = g_new0(MediaPipeline, 1);
    
    /* create a new pipeline to hold the elements */
    gstdata->pipeline = gst_pipeline_new ("gnomebaker-convert-to-wav-pipeline");
    gstdata->source = gst_element_factory_make("filesrc","file-source");
    g_object_set (G_OBJECT (gstdata->source), "location", g_ptr_array_index(cmd->args, 0), NULL);
        
    /* decoder */
    gstdata->decoder = gst_element_factory_make ("decodebin", "decoder");
    g_signal_connect (gstdata->decoder, "new-decoded-pad", G_CALLBACK (gstreamer_new_decoded_pad), gstdata);
    
    /* create an audio converter */
    gstdata->converter = gst_element_factory_make ("audioconvert", "converter");
    
    /* audioscale resamples audio */
    gstdata->scale = gst_element_factory_make("audioscale","scale");
    
    GstCaps *filtercaps = gst_caps_new_simple ("audio/x-raw-int",
                                          "channels", G_TYPE_INT, 2,
                                          "rate",     G_TYPE_INT, 44100,
                                          "width",    G_TYPE_INT, 16,
                                          "depth",    G_TYPE_INT, 16,
                                          NULL);
    /* and an wav encoder */
    gstdata->encoder = gst_element_factory_make ("wavenc", "encoder");
    gst_element_link_filtered (gstdata->scale,gstdata->encoder,filtercaps);
    gst_caps_free (filtercaps); 
    
    /* finally the output filesink */   
    if(pipe != NULL)
    {   
         gstdata->dest = gst_element_factory_make("fdsink","file-out");
         g_object_set (G_OBJECT (gstdata->dest), "fd", pipe[1], NULL);
    }
    else
    {
        gstdata->dest = gst_element_factory_make("filesink","file-out");
        g_object_set (G_OBJECT (gstdata->dest), "location", g_ptr_array_index(cmd->args, 1), NULL);
    }
    
    gst_bin_add_many (GST_BIN (gstdata->pipeline), gstdata->source, gstdata->decoder, NULL);
    
    gst_element_link(gstdata->converter, gstdata->scale);
    gst_element_link(gstdata->encoder, gstdata->dest);
    gst_element_link(gstdata->source, gstdata->decoder);
    
    g_signal_connect (gstdata->pipeline, "error", G_CALLBACK(gstreamer_pipeline_error), cmd);
    g_signal_connect (gstdata->pipeline, "eos", G_CALLBACK (gstreamer_pipeline_eos), cmd);
    
    /* If we're not writing to someone's pipe we update the progress bar */        
    if(pipe == NULL)
        gstreamertimer = g_timeout_add(1000, gstreamer_progress_timer, gstdata->dest);
    
    gst_element_set_state (gstdata->pipeline, GST_STATE_PLAYING);
    while(gst_bin_iterate(GST_BIN(gstdata->pipeline)) && (exec_cmd_get_state(cmd) == RUNNING))
    {                
        /* Keep the GUI responsive by pumping all of the events if we're not writing
         * to a pipe (as someone else will be pumping the event loop) */
        while(pipe == NULL && gtk_events_pending())
            gtk_main_iteration();
    }
    
    if(pipe == NULL)
        g_source_remove(gstreamertimer);
    gst_element_set_state (gstdata->pipeline, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (gstdata->pipeline));
    g_free(gstdata);
}


void 
gstreamer_add_args(ExecCmd* cmd, const gchar* from, const gchar* to)
{
    GB_LOG_FUNC
    g_return_if_fail(cmd != NULL);  
    g_return_if_fail(from != NULL);        
    g_return_if_fail(to != NULL);
    
    exec_cmd_add_arg(cmd, from);
    exec_cmd_add_arg(cmd, to);
    
    cmd->libProc = gstreamer_lib_proc;
    cmd->preProc = gstreamer_pre_proc;   
}


/*******************************************************************************
 * GSTREAMER
 ******************************************************************************/
static void
md5sum_pre_proc(void* ex, void* buffer)
{
    GB_LOG_FUNC
    progressdlg_set_status(_("Verifying the disk content"));
    progressdlg_increment_exec_number();
    progressdlg_pulse_start();
}


static void
md5sum_post_proc(void* ex, void* buffer)
{
    GB_LOG_FUNC
    progressdlg_pulse_stop();
    if(preferences_get_bool(GB_EJECT))
        devices_eject_disk(GB_WRITER);
}


void 
md5sum_add_args(ExecCmd* cmd, const gchar* md5)
{
    GB_LOG_FUNC
    g_return_if_fail(cmd != NULL);  
    g_return_if_fail(md5 != NULL);
    
    exec_cmd_add_arg(cmd, "md5sum");
    gchar* writer = devices_get_device_config(GB_WRITER,GB_DEVICE_NODE_LABEL);
    exec_cmd_add_arg(cmd, writer);
    g_free(writer);
    exec_cmd_add_arg(cmd, md5);
    
    cmd->preProc = md5sum_pre_proc;
    cmd->postProc = md5sum_post_proc;
}


