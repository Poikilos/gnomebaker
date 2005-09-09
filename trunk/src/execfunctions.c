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
#include "gst/gst.h"
#include "media.h"


static gint cdrecord_totaltrackstowrite = 1;
static gint cdrecord_firsttrack = -1;
static guint64 cdrecord_totaldiskbytes = 0;

static gint cdda2wav_totaltracks = -1;
static gint cdda2wav_totaltracksread = 0;

static gint readcd_totalguchars = -1;
static gint cdrdao_cdminutes = -1;


/*******************************************************************************
 * CDRECORD
 ******************************************************************************/

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
	
	progressdlg_set_status(_("<b>Burning disk...</b>"));
	progressdlg_increment_exec_number();
	if(!devices_query_cdstatus(GB_WRITER))
	{
		gdk_threads_enter();
		gint ret = gnomebaker_show_msg_dlg(progressdlg_get_window(),GTK_MESSAGE_INFO, 
            GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE, _("Please insert the CD into the CD writer"));
		/*gdk_flush();*/
		gdk_threads_leave();
		
		if(ret == GTK_RESPONSE_CANCEL)
			exec_cmd_set_state((ExecCmd*)ex, CANCELLED, FALSE);
	}
    
    /* If we have the total disk bytes set then let cdrecord know about it */
    if(cdrecord_totaldiskbytes > 0)
    {
        gchar* total = g_strdup_printf("%llus", cdrecord_totaldiskbytes);    
        exec_cmd_update_arg((ExecCmd*)ex, "tsize=", total);
        g_free(total);
    }
        
    gdk_threads_enter();
    devices_mount_device(GB_WRITER, NULL);
    gdk_threads_leave();
}


static void
cdrecord_blank_pre_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	progressdlg_set_status(_("<b>Blanking disk...</b>"));
	progressdlg_set_text("");
	gint ret = GTK_RESPONSE_OK;
	if(!devices_query_cdstatus(GB_WRITER))
	{
		gdk_threads_enter();
		ret = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_INFO, 
            GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE, _("Please insert the CD-RW into the CD writer"));
		/*gdk_flush();*/
		gdk_threads_leave();
    }
    
    if(ret == GTK_RESPONSE_CANCEL)
        exec_cmd_set_state((ExecCmd*)ex, CANCELLED, FALSE);
    else
        progressdlg_pulse_start();
    gdk_threads_enter();
    devices_mount_device(GB_WRITER, NULL);
    gdk_threads_leave();
}


static void
cdrecord_blank_post_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	progressdlg_pulse_stop();
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
            
            progressdlg_set_fraction(totalfraction);
        }
	}
	
	const gchar* fixating = strstr(buf, "Fixating");
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
static void
cdrecord_add_common_args(ExecCmd* cdBurn)
{
	GB_LOG_FUNC
	g_return_if_fail(cdBurn != NULL);	
    cdrecord_totaldiskbytes = 0;

	exec_cmd_add_arg(cdBurn, "%s", "cdrecord");
	
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cdBurn, "dev=%s", writer);
	g_free(writer);
	
	exec_cmd_add_arg(cdBurn, "%s", "gracetime=2");
	
	gchar* speed = g_strdup_printf("%d", preferences_get_int(GB_CDWRITE_SPEED));
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
	if(g_ascii_strcasecmp(mode, _("default")) != 0)
		exec_cmd_add_arg(cdBurn, "-%s", mode);

	g_free(mode);
	
	cdrecord_totaltrackstowrite = 1;
	cdrecord_firsttrack = -1;
}


void
cdrecord_add_create_audio_cd_args(ExecCmd* e, const GList* audiofiles)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
	cdrecord_add_common_args(e);
	exec_cmd_add_arg(e, "%s", "-audio");	
    exec_cmd_add_arg(e, "%s", "-pad");
    exec_cmd_add_arg(e, "%s", "-useinfo");
    exec_cmd_add_arg(e, "%s", "-text");
    
	/* if we are on the fly this will be a list of inf files, otherwise
     * it's the list of wavs to burn */
    const GList *audiofile = audiofiles;
    for (; audiofile != NULL ; audiofile = audiofile->next)
    {
        exec_cmd_add_arg(e, "%s", audiofile->data);
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
		exec_cmd_add_arg(cdBurn, "%s", "-multi");
        
    exec_cmd_add_arg(cdBurn, "tsize=%ss", "-1");
    exec_cmd_add_arg(cdBurn, "%s", "-pad");

    if(iso != NULL)
	    exec_cmd_add_arg(cdBurn, "%s", iso);
	else 
	    exec_cmd_add_arg(cdBurn, "%s", "-"); /* no filename so we're on the fly */
	
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
                exec_cmd_add_arg(ex, "%s", fullpath);
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
	exec_cmd_add_arg(cdBurn, "%s", "-useinfo");
	exec_cmd_add_arg(cdBurn, "%s", "-audio");
	exec_cmd_add_arg(cdBurn, "%s", "-pad");
	  
	cdBurn->readProc = cdrecord_read_proc;
	cdBurn->preProc = cdrecord_copy_audio_cd_pre_proc;	
}


void 
cdrecord_add_blank_args(ExecCmd* cdBurn)
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

	gchar* speed = g_strdup_printf("%d", preferences_get_int(GB_CDWRITE_SPEED));
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


/*******************************************************************************
 * CDDA2WAV
 ******************************************************************************/


static void
cdda2wav_pre_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	cdda2wav_totaltracks = -1;
	cdda2wav_totaltracksread = 0;
	
	progressdlg_set_status(_("<b>Extracting audio tracks...</b>"));
	progressdlg_increment_exec_number();

	gint response = GTK_RESPONSE_NO;
	
	gchar* tmp = preferences_get_string(GB_TEMP_DIR);
	gchar* file = g_strdup_printf("%s/gbtrack_01.wav", tmp);	
	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		gdk_threads_enter();
		response = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
			_("Audio tracks from a previous sesion already exist on disk, "
			"do you wish to use the existing tracks?"));
		/*gdk_flush();*/
		gdk_threads_leave();
	}
	
	g_free(file);
	
	if(response == GTK_RESPONSE_NO)
	{		
		gchar* cmd = g_strdup_printf("rm -fr %s/gbtrack*", tmp);
		system(cmd);
		g_free(cmd);
	    if(!devices_query_cdstatus(GB_READER))	
        {
            gdk_threads_enter();
            response = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
                _("Please insert the audio CD into the CD reader"));				
            /*gdk_flush();*/
            gdk_threads_leave();	
        }
	}
	
	if(response == GTK_RESPONSE_CANCEL)
		exec_cmd_set_state((ExecCmd*)ex, CANCELLED, FALSE);
	else if(response == GTK_RESPONSE_YES)
		exec_cmd_set_state((ExecCmd*)ex, SKIP, FALSE);
        
    gdk_threads_enter();
	devices_mount_device(GB_READER, NULL);
    gdk_threads_leave();
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
		{
			gchar tmpbuf[4];
			gbcommon_memset(tmpbuf, 4);
			g_strlcpy(tmpbuf, tracksstart + 7, 3);
			g_strstrip(tmpbuf);
			cdda2wav_totaltracks = atoi(tmpbuf);			
			GB_TRACE("cdda2wav_read_proc - total tracks %d", cdda2wav_totaltracks);
		}
	}
	else if(cdda2wav_totaltracks)
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
			gbcommon_memset(tmpbuf, bufsize * sizeof(gchar));
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
			
			/*GB_TRACE("cdda2wav_read_proc - track fraction %f", fraction);*/
			
			fraction *= (1.0/(gfloat)cdda2wav_totaltracks);			
			fraction += ((gfloat)cdda2wav_totaltracksread *(1.0/(gfloat)cdda2wav_totaltracks));
								
			progressdlg_set_fraction(fraction);
						
			if(hundred != NULL)
				cdda2wav_totaltracksread++;
		}
	}
		
	progressdlg_append_output(text);
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
	
	exec_cmd_add_arg(e, "%s", "cdda2wav");
	exec_cmd_add_arg(e, "%s", "-x");
	exec_cmd_add_arg(e, "%s", "cddb=1");
    exec_cmd_add_arg(e, "%s", "speed=52");
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
    
    progressdlg_set_status(_("<b>Calculating data disk image size...</b>"));
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
	
	progressdlg_set_status(_("<b>Creating data disk image...</b>"));
	progressdlg_increment_exec_number();
	
	gchar* file = preferences_get_create_data_cd_image();	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		gdk_threads_enter();
		gint ret = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_OK,
			_("A data CD image from a previous session already exists on disk, "
			"do you wish to use the existing image?"));
		/*gdk_flush();*/
		gdk_threads_leave();
		
		if(ret  == GTK_RESPONSE_YES)		
			exec_cmd_set_state((ExecCmd*)ex, SKIP, FALSE);
		else if(ret == GTK_RESPONSE_CANCEL)
			exec_cmd_set_state((ExecCmd*)ex, CANCELLED, FALSE);
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
		const gchar* ptr = percent;
		while((!g_ascii_isspace(*ptr)) && (ptr > (gchar*)buffer))
			--ptr;
		
		ptr++;
		
		gchar* pct = g_malloc((percent - ptr + 1) * sizeof(gchar));
		gbcommon_memset(pct, (percent - ptr + 1) * sizeof(gchar));
		strncpy(pct, ptr, percent - ptr);
		progressdlg_set_fraction(atof(pct)/100.0);
		g_free(pct);
	}
	
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
    guint64 size = 0;
		
	gtk_tree_model_get (model, iter, DATACD_COL_FILE, &file, DATACD_COL_SIZE, &size, 
		DATACD_COL_PATH, &filepath, DATACD_COL_SESSION, &existingsession, -1);
	
	/* Only add files that are not part of an existing session */
	if(!existingsession)
	{
		gchar* buffer = g_strdup_printf("%s=%s", file, filepath);		
		exec_cmd_add_arg((ExecCmd*)user_data, "%s", buffer);	
		g_free(buffer);
	}

	g_free(file);	
	g_free(filepath);
	
	return FALSE; /* do not stop walking the store, call us with next row */
}


gboolean
mkisofs_add_args(ExecCmd* e, GtkTreeModel* datamodel, const gchar* iso, const gboolean calculatesize)
{
	GB_LOG_FUNC
	g_return_val_if_fail(e != NULL, FALSE);
	g_return_val_if_fail(datamodel != NULL, FALSE);
	cdrecord_totaldiskbytes = 0;
	
	/* If this is a another session on an existing cd we don't show the 
	   iso details dialog */	
	gint ret = GTK_RESPONSE_OK;
	gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datamodel), DATACD_EXISTING_SESSION);
	
	gchar* volume = NULL;
	gchar* createdby = NULL;
	if(msinfo == NULL && !calculatesize)
	{
		GladeXML* dialog = glade_xml_new(glade_file, widget_isofsdlg, NULL);	
		GtkEntry* created = GTK_ENTRY(glade_xml_get_widget(dialog, widget_isofsdlg_createdby));
		gtk_entry_set_text(created, g_get_real_name());
		GtkWidget* dlg = glade_xml_get_widget(dialog, widget_isofsdlg);
        gbcommon_center_window_on_parent(dlg);
		ret = gtk_dialog_run(GTK_DIALOG(dlg));
		volume = g_strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(dialog, widget_isofsdlg_volume))));
		createdby = g_strdup(gtk_entry_get_text(created));
		gtk_widget_hide(dlg);
		gtk_widget_destroy(dlg);
	}
	
	if(ret == GTK_RESPONSE_OK)
	{
		exec_cmd_add_arg(e, "%s", "mkisofs");		
		if(volume != NULL && createdby != NULL)
		{
			exec_cmd_add_arg(e, "%s",  "-V");			
			exec_cmd_add_arg(e, "%s", volume);
			exec_cmd_add_arg(e, "%s", "-p");
			exec_cmd_add_arg(e, "%s", createdby);
		}
        
        exec_cmd_add_arg(e, "%s", "-iso-level");
        exec_cmd_add_arg(e, "%s", "3");
        exec_cmd_add_arg(e, "%s", "-l"); /* allow 31 character iso9660 filenames */
		
        if(preferences_get_bool(GB_ROCKRIDGE))		
        {
		    exec_cmd_add_arg(e, "%s", "-R");
            exec_cmd_add_arg(e, "%s", "-hide-rr-moved");
        }
		
        if(preferences_get_bool(GB_JOLIET))
        {
		    exec_cmd_add_arg(e, "%s", "-J");
            exec_cmd_add_arg(e, "%s", "-joliet-long");
        }
        
        /*exec_cmd_add_arg(e, "%s", "-f"); don't follow links */
		/*exec_cmd_add_arg(e, "%s", "-hfs");*/		
				
		if(msinfo != NULL)
		{
			exec_cmd_add_arg(e, "%s", "-C");
			exec_cmd_add_arg(e, "%s", msinfo);
			
			gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
			exec_cmd_add_arg(e, "%s", "-M");
			exec_cmd_add_arg(e, "%s", writer);
			g_free(writer);			
			
			/* This is a cludge so that we don't ask the user if they want to use the existing iso */
			gchar* createdataiso = preferences_get_create_data_cd_image();
			gchar* cmd = g_strdup_printf("rm -fr %s", createdataiso);
			system(cmd);			
			g_free(cmd);
			g_free(createdataiso);
		}		
		
        if(iso != NULL) /* no filename means we're on the fly */
        {            
            exec_cmd_add_arg(e, "%s", "-gui");	
            exec_cmd_add_arg(e, "%s", "-o");
            exec_cmd_add_arg(e, "%s", iso);
        }
        
        if(calculatesize)
        {
            exec_cmd_add_arg(e, "%s", "--print-size");      
            e->preProc = mkisofs_calc_size_pre_proc;
            e->readProc = mkisofs_calc_size_read_proc;
            e->postProc = mkisofs_calc_size_post_proc;
        }
        else
        {
            e->preProc = mkisofs_pre_proc;
            e->readProc = mkisofs_read_proc;
        }
        
		exec_cmd_add_arg(e, "%s", "-graft-points");
		gtk_tree_model_foreach(datamodel, mkisofs_foreach_func, e);			
	}
	
	/* We don't own the msinfo gchar datacd does 
	g_free(msinfo);*/
	g_free(volume);
	g_free(createdby);
	
	return (ret == GTK_RESPONSE_OK);
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
	
	progressdlg_set_status(_("<b>Formatting DVD...</b>"));
	progressdlg_increment_exec_number();
	gint ret = GTK_RESPONSE_OK;
	if(!devices_query_cdstatus(GB_WRITER))
	{
		gdk_threads_enter();
		ret = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
				  _("Please insert a rewritable DVD into the DVD writer"));
		gdk_threads_leave();
    }
    if(ret == GTK_RESPONSE_CANCEL)
        exec_cmd_set_state((ExecCmd*)ex, CANCELLED, FALSE);
    gdk_threads_enter();    
    devices_mount_device(GB_WRITER, NULL);
    gdk_threads_leave();
}


static void
dvdformat_read_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC	
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);		
    
    /*  * formatting 24.5% */     
    gfloat percent = 0.0;
    sscanf((gchar*)buffer, "%*s %*s %f%%", &percent);
    progressdlg_set_fraction(percent/100.0);    
	progressdlg_append_output((gchar*)buffer);
}


static void
dvdformat_post_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
	if(preferences_get_bool(GB_EJECT))
	{
		devices_eject_cd(GB_WRITER);
	}
}


void 
dvdformat_add_args(ExecCmd* dvdFormat)
{
	GB_LOG_FUNC
	g_return_if_fail(dvdFormat != NULL);
	
	dvdFormat->readProc = dvdformat_read_proc;
	dvdFormat->preProc = dvdformat_pre_proc;
	dvdFormat->postProc = dvdformat_post_proc;
	
	exec_cmd_add_arg(dvdFormat, "%s", "dvd+rw-format");
	
	gchar* writer = devices_get_device_config(GB_WRITER,GB_DEVICE_NODE_LABEL);
	exec_cmd_add_arg(dvdFormat, "%s", writer);
	g_free(writer);
	
	/* make the output gui friendly */
	exec_cmd_add_arg(dvdFormat, "%s", "-gui");
		
	if(preferences_get_bool(GB_FORCE))
	{
		if(!preferences_get_bool(GB_FAST_FORMAT))
			exec_cmd_add_arg(dvdFormat, "%s","-force=full");
		else
			exec_cmd_add_arg(dvdFormat, "%s","-force");
	}
	else if(!preferences_get_bool(GB_FAST_FORMAT))
	{
		exec_cmd_add_arg(dvdFormat, "%s", "-blank=full");
	}
    else
    {
        exec_cmd_add_arg(dvdFormat, "%s", "-blank");
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
	
	progressdlg_set_status(_("<b>Burning DVD...</b>"));
	progressdlg_increment_exec_number();
	
	if(!devices_query_cdstatus(GB_WRITER))
	{	
		gdk_threads_enter();
		gint ret = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
				  _("Please insert a writable DVD into the DVD writer"));
		/*gdk_flush();*/
		gdk_threads_leave();
		
		if(ret == GTK_RESPONSE_CANCEL)
			exec_cmd_set_state((ExecCmd*)ex, CANCELLED, FALSE);
	}
    gdk_threads_enter();
    devices_mount_device(GB_WRITER, NULL);
    gdk_threads_leave();
}


static void 
growisofs_post_proc(void* ex,void* buffer)
{
	GB_LOG_FUNC
	if(preferences_get_bool(GB_EJECT))
	{
		devices_eject_cd(GB_WRITER);
	}
}


static void
growisofs_read_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);
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

	gchar *buf = (gchar*)buffer;
	const gchar* progressstr = strstr(buf, "done");
	if(progressstr != NULL)
	{
		gint progress = 0;
		if(sscanf(buf,"%d.%*d",&progress) >0)
		{
			GB_TRACE("growisofs: progress: %d", progress);
			gfloat fraction = (gfloat)progress / 100.0;
			GB_TRACE("growisofs: fraction: %f", fraction);
			
			progressdlg_set_fraction(fraction);
		}
	}
	
	const gchar* leadout = strstr(buf,"writing lead-out");
	if(leadout != NULL)	
	{
		progressdlg_set_fraction(1.0);
		progressdlg_set_text(_("Writing lead-out"));
	}		
	progressdlg_append_output(buf);
}

static void
growisofs_read_iso_proc(void* ex, void* buffer)
{
	GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);
/*
WARNING: /dev/hdc already carries isofs!
About to execute 'builtin_dd if=/home2/cs/SL-9.3-LiveDVD-i386-1.iso of=/dev/hdc obs=32k seek=0'
/dev/hdc: "Current Write Speed" is 4.0x1385KBps.
  14876672/1480370176 ( 1.0%) @1.6x, remaining 9:51
  22380544/1480370176 ( 1.5%) @1.6x, remaining 10:51
  29753344/1480370176 ( 2.0%) @1.6x, remaining 10:33
/dev/hdc: flushing cache
/dev/hdc: writing lead-out

	*/
	
	gchar *buf = (gchar*)buffer;
	const gchar* progressstr = strstr(buf, "remaining");
	if(progressstr != NULL)
	{
		gint progress = 0;
		if(sscanf(buf,"%*d/%*d ( %d.%*d%%)",&progress) >0)
		{
			GB_TRACE("growisofs: progress: %d", progress);
			gfloat fraction = (gfloat)progress / 100.0;
			GB_TRACE("growisofs: fraction: %f", fraction);			
			progressdlg_set_fraction(fraction);
		}
	}
	
	const gchar* leadout = strstr(buf,"writing lead-out");
	if(leadout != NULL)	
	{
		progressdlg_set_fraction(1.0);
		progressdlg_set_text(_("Writing lead-out"));
	}		
	progressdlg_append_output(buf);
}


gboolean
growisofs_add_args(ExecCmd* e, GtkTreeModel* datamodel)
{
	GB_LOG_FUNC
	g_return_val_if_fail(e != NULL, FALSE);
	        
    /* If this is a another session on an existing cd we don't show the 
	   iso details dialog */	
	gint ret = GTK_RESPONSE_OK;
	gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datamodel), DATACD_EXISTING_SESSION);   
	gchar* volume = NULL;
	gchar* createdby = NULL;
	if(msinfo == NULL)
	{
		GladeXML* dialog = glade_xml_new(glade_file, widget_isofsdlg, NULL);	
		GtkEntry* created = GTK_ENTRY(glade_xml_get_widget(dialog, widget_isofsdlg_createdby));
		gtk_entry_set_text(created, g_get_real_name());
		GtkWidget* dlg = glade_xml_get_widget(dialog, widget_isofsdlg);
        gbcommon_center_window_on_parent(dlg);
		ret = gtk_dialog_run(GTK_DIALOG(dlg));
		volume = g_strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(dialog, widget_isofsdlg_volume))));
		createdby = g_strdup(gtk_entry_get_text(created));
		gtk_widget_hide(dlg);
		gtk_widget_destroy(dlg);
	}
	
	if(ret == GTK_RESPONSE_OK)
	{
		exec_cmd_add_arg(e, "%s", "growisofs");		
		if(volume != NULL && createdby != NULL)
		{
			exec_cmd_add_arg(e, "%s",  "-V");			
			exec_cmd_add_arg(e, "%s", volume);
			exec_cmd_add_arg(e, "%s", "-p");
			exec_cmd_add_arg(e, "%s", createdby);
		}
        
        /* http://www.troubleshooters.com/linux/coasterless_dvd.htm#_Gotchas 
            states that dao with dvd compat is the best way of burning dvds */
        exec_cmd_add_arg(e, "%s", "-use-the-force-luke=dao");
        /*exec_cmd_add_arg(e, "%s", "-dvd-compat");*/
                
        exec_cmd_add_arg(e, "%s", "-iso-level");
        exec_cmd_add_arg(e, "%s", "3");
        exec_cmd_add_arg(e, "%s", "-l"); /* allow 31 character iso9660 filenames */
		
        if(preferences_get_bool(GB_ROCKRIDGE))		
        {
		    exec_cmd_add_arg(e, "%s", "-R");
            exec_cmd_add_arg(e, "%s", "-hide-rr-moved");
        }
		
        if(preferences_get_bool(GB_JOLIET))
        {
		    exec_cmd_add_arg(e, "%s", "-J");
            exec_cmd_add_arg(e, "%s", "-joliet-long");
        }
        
        /*exec_cmd_add_arg(e, "%s", "-f"); don't follow links */
		/*exec_cmd_add_arg(e, "%s", "-hfs");*/		
				
        /* merge new session with existing one */	
        gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datamodel), DATACD_EXISTING_SESSION);
        if(msinfo != NULL)
            exec_cmd_add_arg(e, "%s", "-M");
        else
            exec_cmd_add_arg(e, "%s", "-Z");

        gchar* writer = devices_get_device_config(GB_WRITER,GB_DEVICE_NODE_LABEL);
        exec_cmd_add_arg(e, "%s", writer);
        g_free(writer);
        
        gchar* speed = g_strdup_printf("%d", preferences_get_int(GB_DVDWRITE_SPEED));
        exec_cmd_add_arg(e,"-speed=%s", speed);
        g_free(speed);
        
        /* stop the reloading of the disc */
        exec_cmd_add_arg(e, "%s", "-use-the-force-luke=notray");
        
        /* force overwriting existing filesystem */
        exec_cmd_add_arg(e, "%s", "-use-the-force-luke=tty");
	
        /* TODO: Overburn support */
        /* preferences_get_int(GB_OVERBURN)
        if(prefs->overburn)
            exec_cmd_add_arg(growisofs, "%s", "-overburn"); */
        
        if(preferences_get_bool(GB_FINALIZE))
            exec_cmd_add_arg(e, "%s", "-dvd-compat");

        exec_cmd_add_arg(e, "%s", "-gui");	
        exec_cmd_add_arg(e, "%s", "-graft-points");
		gtk_tree_model_foreach(datamodel, mkisofs_foreach_func, e);	
				
		e->readProc = growisofs_read_proc;
        e->preProc = growisofs_pre_proc;
        e->postProc = growisofs_post_proc;
	}
	
	/* We don't own the msinfo gchar datacd does 
	g_free(msinfo);*/
	g_free(volume);
	g_free(createdby);
	
	return (ret == GTK_RESPONSE_OK);
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
	exec_cmd_add_arg(growisofs, "%s", "growisofs");
	exec_cmd_add_arg(growisofs,"%s","-dvd-compat");
	
	gchar* speed = g_strdup_printf("%d", preferences_get_int(GB_DVDWRITE_SPEED));
	exec_cmd_add_arg(growisofs,"-speed=%s",speed);
	g_free(speed);
	
	/* -gui makes the output more verbose, so we can 
	    interpret it easier */
    /*	exec_cmd_add_arg(growisofs,"%s","-gui"); */
	/* -gui does not work with iso's, argh! */

	/* stop the reloading of the disc */
	exec_cmd_add_arg(growisofs,"%s","-use-the-force-luke=notray");
	
	/* force overwriting existing filesystem */
    exec_cmd_add_arg(growisofs,"%s","-use-the-force-luke=tty");
	
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_NODE_LABEL);
	exec_cmd_add_arg(growisofs, "%s", "-Z");
	
	gchar* buffer = g_strdup_printf("%s=%s",writer, iso);		
	exec_cmd_add_arg(growisofs, "%s", buffer);
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
	
	progressdlg_set_status(_("<b>Reading CD image...</b>"));	
	progressdlg_increment_exec_number();
	
	gint response = GTK_RESPONSE_NO;
	gchar* file = preferences_get_copy_data_cd_image();	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		gdk_threads_enter();
		response = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
			"A CD image from a previous session already exists on disk, "
			"do you wish to use the existing image?");		
		/*gdk_flush();*/
		gdk_threads_leave();
	}
	
	g_free(file);
	
	if(response == GTK_RESPONSE_NO)
	{
        if(!devices_query_cdstatus(GB_READER)) 
        {
            gdk_threads_enter();
            response = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
                      "Please insert the source CD into the CD reader");
            /*gdk_flush();*/
            gdk_threads_leave();
        }
	}
	
	if(response == GTK_RESPONSE_CANCEL)
		exec_cmd_set_state((ExecCmd*)ex, CANCELLED, FALSE);
	else if(response == GTK_RESPONSE_YES)
		exec_cmd_set_state((ExecCmd*)ex, SKIP, FALSE);
    gdk_threads_enter();
    devices_mount_device(GB_READER, NULL);
    gdk_threads_leave();
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
			{
				/*GB_TRACE( "read %d, total %d", readguchars, readcd_totalguchars);*/
				const gfloat fraction = (gfloat)readguchars/(gfloat)readcd_totalguchars;
				progressdlg_set_fraction(fraction);
			}		
		}
		else
		{
			progressdlg_append_output(text);
		}
	}	
}


static void
readcd_post_proc(void* ex, void* buffer)
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


/*
 *  Populates the information required to make an iso from an existing data cd
 */
void
readcd_add_copy_args(ExecCmd* e, const gchar* iso)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	g_return_if_fail(iso != NULL);	

	exec_cmd_add_arg(e, "%s", "readcd");
	
	gchar* reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(e, "dev=%s", reader);	
	g_free(reader);
	
	exec_cmd_add_arg(e, "f=%s", iso);	
	/*exec_cmd_add_arg(e, "%s", "-notrunc");
	exec_cmd_add_arg(e, "%s", "-clone");
	exec_cmd_add_arg(e, "%s", "-silent");*/

	e->preProc = readcd_pre_proc;	
	e->readProc = readcd_read_proc;
	//e->postProc = readcd_post_proc;
}

/*******************************************************************************
 * CDRDAO
 ******************************************************************************/

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
    progressdlg_append_output(output);
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
	
	exec_cmd_add_arg(cmd, "%s", "cdrdao");
	exec_cmd_add_arg(cmd, "%s", "write");
	
	gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cmd, "%s", "--device");
	exec_cmd_add_arg(cmd, "%s", writer);
	g_free(writer);	
	
	gchar* speed = g_strdup_printf("%d", preferences_get_int(GB_CDWRITE_SPEED));
	exec_cmd_add_arg(cmd, "%s", "--speed");
	exec_cmd_add_arg(cmd, "%s", speed);
	g_free(speed);
    
    exec_cmd_add_arg(cmd, "%s", "--buffers");
    exec_cmd_add_arg(cmd, "%s", "64");
    exec_cmd_add_arg(cmd, "%s", "-n"); /* turn off the 10 second pause */
	
	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cmd, "%s", "--eject");

	if(preferences_get_bool(GB_DUMMY))
		exec_cmd_add_arg(cmd, "%s", "--simulate");
    
    exec_cmd_add_arg(cmd, "%s", toc_or_cue);
}


/*******************************************************************************
 * GSTREAMER
 ******************************************************************************/
static void
gstreamer_pipeline_eos(GstElement* gstelement, gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(user_data != NULL);
    exec_cmd_set_state((ExecCmd*)user_data, COMPLETE, TRUE);
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
    exec_cmd_set_state((ExecCmd*)user_data, FAILED, TRUE);
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
    
    gst_bin_add_many (GST_BIN (mip->pipeline), mip->converter,mip->scale,mip->encoder,mip->dest, NULL);
    

  /* This function synchronizes a bins state on all of its
   * contained children. */
    gst_bin_sync_children_state (GST_BIN (mip->pipeline));
}
 
 
static void
gstreamer_pre_proc(void* ex, void* buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    ExecCmd* cmd = (ExecCmd*)ex;
    
    gchar* filename = g_path_get_basename(cmd->argv[0]);
    gchar* text = g_strdup_printf(_("<b>Converting [%.32s] to cd audio...</b>"), filename);
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
    
    GB_TRACE("Converting [%s] to [%s]", cmd->argv[0], cmd->argv[1]);
    
    MediaPipeline* gstdata = g_new0(MediaPipeline, 1);
    
    /* create a new pipeline to hold the elements */
    gstdata->pipeline = gst_thread_new ("gnomebaker-convert-to-wav-pipeline");
    gstdata->source = gst_element_factory_make("filesrc","file-source");
    g_object_set (G_OBJECT (gstdata->source), "location", cmd->argv[0], NULL);
        
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
        g_object_set (G_OBJECT (gstdata->dest), "location", cmd->argv[1], NULL);
    }
    
    gst_bin_add_many (GST_BIN (gstdata->pipeline), gstdata->source, gstdata->decoder, NULL);
    
    gst_element_link(gstdata->converter, gstdata->scale);
    gst_element_link(gstdata->encoder, gstdata->dest);
    gst_element_link(gstdata->source, gstdata->decoder);
    
    g_signal_connect (gstdata->pipeline, "error", G_CALLBACK(gstreamer_pipeline_error), cmd);
    g_signal_connect (gstdata->pipeline, "eos", G_CALLBACK (gstreamer_pipeline_eos), cmd);
    
    gst_element_set_state (gstdata->pipeline, GST_STATE_PLAYING);
    while(!exec_cmd_wait_for_signal(cmd, 1))
    {
        /* If we're not writing to someone's pipe we update the progress bar */
        if(pipe == NULL)
        {
            GstFormat fmt = GST_FORMAT_BYTES;
            gint64 pos = 0, total = 0;
            if(gst_element_query (gstdata->dest, GST_QUERY_POSITION, &fmt, &pos) && 
                gst_element_query (gstdata->dest, GST_QUERY_TOTAL, &fmt, &total))
            {
                progressdlg_set_fraction((gfloat)pos/(gfloat)total); 
            }
        }
    }
    
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
    
    exec_cmd_add_arg(cmd, "%s", from);
    exec_cmd_add_arg(cmd, "%s", to);
    
    cmd->libProc = gstreamer_lib_proc;
    cmd->preProc = gstreamer_pre_proc;   
}

