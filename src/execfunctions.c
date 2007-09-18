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
#include <gst/gst.h>
#include "media.h"


static gint cdrecord_total_tracks_to_write = 1;
static gint cdrecord_first_track = -1;
static guint64 cdrecord_total_disk_bytes = 0;

static gint cdda2wav_total_tracks = -1;
static gint cdda2wav_total_tracks_read = 0;

static gint readcd_total_guchars = -1;
/*static gint cdrdao_cdminutes = -1;*/


static void
execfunctions_find_line_set_status(const gchar *buffer, const gchar *text, const gchar delimiter)
{
    GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(text != NULL);

    const gchar *start = strstr(buffer, text);
    if(start != NULL)
    {
        const gchar *ptr = start;
        while(*ptr != delimiter)
            ++ptr;
        gchar *message = g_strndup(start, (ptr - start) / sizeof(gchar));
		g_strstrip(message);
        progressdlg_set_status(message);
        g_free(message);
    }
}


static void
execfunctions_prompt_for_disk_post_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    if(((ExecCmd*)ex)->exit_code == 0 && devices_reader_is_also_writer())
    {
        devices_eject_disk(GB_WRITER);
        devices_prompt_for_disk(progressdlg_get_window(), GB_WRITER);
    }
}


/*******************************************************************************
 * CDRECORD
 ******************************************************************************/

static void
cdrecord_blank_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	progressdlg_set_status(_("Preparing to blank disk"));
    if(devices_prompt_for_disk(progressdlg_get_window(), GB_WRITER) == GTK_RESPONSE_CANCEL)
        exec_cmd_set_state((ExecCmd*)ex, CANCELLED);
    else
    {
        /* This time approximation is my first attempt at figuring out how long blanking a
         * cd really takes. It's not very scientific and I will at some point try to figure it
         * out more accurately.*/
        gint speed = preferences_get_int(GB_CDWRITE_SPEED);
        if(speed == 0) speed = 1;
        gint approximation = 0;
        if(preferences_get_bool(GB_FAST_BLANK))
            approximation = (speed * -5) + 95;
        else
            approximation = ((60/speed) * (60/speed)) * 15;

        GB_TRACE("cdrecord_blank_pre_proc - approximation is [%d]\n", approximation);
        progressdlg_start_approximation(approximation);
        progressdlg_increment_exec_number();
    }
    devices_unmount_device(GB_WRITER);
}


static void
cdrecord_blank_post_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	progressdlg_stop_approximation();
}


static void
cdrecord_blank_read_proc(void *ex, void *buffer)
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
cdrecord_pre_proc(void *ex, void *buffer)
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
        if(cdrecord_total_disk_bytes > 0)
            exec_cmd_update_arg((ExecCmd*)ex, "tsize=", "tsize=%ds", cdrecord_total_disk_bytes);
        devices_unmount_device(GB_WRITER);
    }
}


static void
cdrecord_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);

	gchar *buf = (gchar*)buffer;
	const gchar *track = strstr(buf, "Track");
	if(track != NULL)
	{
        gint current_track = 0;
        gfloat current = 0, total = 0;

        const gchar *of = strstr(buf, " of ");
        if(of != NULL) /* regular burn i.e not on the fly */
        {
            /*  Track 01:    3 of   19 MB written (fifo 100%) [buf  98%]   4.5x. */
            sscanf(track, "%*s %d %*s %f %*s %f", &current_track, &current, &total);
        }
        else /* on the fly */
        {
            /* Track 01:   12 MB written (fifo 100%) [buf  96%]   4.0x. */
            sscanf(track, "%*s %d %*s %f", &current_track, &current);
            total = cdrecord_total_disk_bytes / 1024 / 1024;
        }

        if(current > 0.0)
        {
            if(cdrecord_first_track == -1)
                cdrecord_first_track = current_track;

            /* Figure out how many tracks we have written so far and calc the fraction */
            gfloat total_fraction =  ((gfloat)current_track - cdrecord_first_track)
                    * (1.0 / cdrecord_total_tracks_to_write);

            /* now add on the fraction of the track we are currently writing */
            total_fraction += ((current / total) * (1.0 / cdrecord_total_tracks_to_write));

            /*GB_TRACE("^^^^^ current [%d] first [%d] current [%f] total [%f] fraction [%f]",
                current_track, cdrecord_first_track, current, total, total_fraction);*/

            gchar *status = g_strdup_printf(_("Writing track %d"), current_track);
            progressdlg_set_status(status);
            g_free(status);
            progressdlg_set_fraction(total_fraction);
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


static void
cdrecord_copy_audio_cd_pre_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);

    cdrecord_pre_proc(ex, buffer);

    GError *err = NULL;
    gchar *tmp = preferences_get_string(GB_TEMP_DIR);
    GDir *dir = g_dir_open(tmp, 0, &err);
    GList *files_list = NULL;
    GList *p = NULL;

    if(dir != NULL)
    {
        cdrecord_total_tracks_to_write = 0;
        /* loop around reading the files in the directory */
        const gchar *name = g_dir_read_name(dir);
        while(name != NULL)
        {
            if(g_str_has_suffix(name, ".wav"))
            {
                GB_TRACE("cdrecord_copy_audio_cd_pre_proc - found [%s]\n", name);
                gchar *full_path = g_build_filename(tmp, name, NULL);
                files_list = g_list_insert_sorted(files_list, full_path, (GCompareFunc)strcmp);
                cdrecord_total_tracks_to_write++;
            }

            name = g_dir_read_name(dir);
        }
        g_dir_close(dir);

        for (p = files_list; p != NULL; p = g_list_next(p)) {
            GB_TRACE("cdrecord_copy_audio_cd_pre_proc - adding [%s]\n", p->data);
            exec_cmd_add_arg(ex, p->data);
            g_free(p->data);
            p->data = NULL;
        }
        g_list_free(files_list);
    }
    g_free(tmp);
}


/*
 *  Populates the common information required to burn a cd
 */
static void
cdrecord_add_common_args(ExecCmd *cmd)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);

    cdrecord_total_disk_bytes = 0;
    cdrecord_total_tracks_to_write = 1;
    cdrecord_first_track = -1;

	gchar *writer;
	
	switch(preferences_get_int(GB_BACKEND)) 
	{
		case BACKEND_CDRECORD:
			exec_cmd_add_arg(cmd, "cdrecord");
			writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
			break;
		case BACKEND_WODIM:
			exec_cmd_add_arg(cmd, "wodim");
			writer = devices_get_device_config(GB_WRITER, GB_DEVICE_NODE_LABEL);
			break;			
	}
		
	exec_cmd_add_arg(cmd, "dev=%s", writer);
	g_free(writer);

	if(preferences_get_bool(GB_CDRECORD_FORCE))
	   exec_cmd_add_arg(cmd, "-force");

	exec_cmd_add_arg(cmd, "gracetime=5");

    const gint speed = preferences_get_int(GB_CDWRITE_SPEED);
    if(speed > 0)
	   exec_cmd_add_arg(cmd, "speed=%d", speed);

    exec_cmd_add_arg(cmd, "-v");

	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cmd, "-eject");

	if(preferences_get_bool(GB_DUMMY))
		exec_cmd_add_arg(cmd, "-dummy");

	if(preferences_get_bool(GB_BURNFREE))
		exec_cmd_add_arg(cmd, "driveropts=burnfree");

	gchar *mode = preferences_get_string(GB_WRITE_MODE);
	if(g_ascii_strcasecmp(mode, _("Auto")) != 0)
		exec_cmd_add_arg(cmd, "-%s", mode);
	g_free(mode);
}


void
cdrecord_add_create_audio_cd_args(ExecCmd *cmd, const GList *audio_files)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);

	cdrecord_add_common_args(cmd);
	exec_cmd_add_arg(cmd, "-audio");
    exec_cmd_add_arg(cmd, "-pad");
    exec_cmd_add_arg(cmd, "-useinfo");
    exec_cmd_add_arg(cmd, "-text");

	/* if we are on the fly this will be a list of inf files, otherwise
     * it's the list of wavs to burn */
    const GList *audio_file = audio_files;
    for (; audio_file != NULL ; audio_file = audio_file->next)
    {
        exec_cmd_add_arg(cmd, audio_file->data);
        GB_TRACE("cdrecord_add_create_audio_cd_args - adding [%s]\n", (gchar*)audio_file->data);
        cdrecord_total_tracks_to_write++;
    }
	cmd->read_proc = cdrecord_read_proc;
	cmd->pre_proc = cdrecord_pre_proc;
}


void
cdrecord_add_iso_args(ExecCmd *cmd, const gchar *iso)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);

	cdrecord_add_common_args(cmd);

    exec_cmd_add_arg(cmd, "-overburn");

	/*if(!prefs->multisession)*/
		exec_cmd_add_arg(cmd, "-multi");

    exec_cmd_add_arg(cmd, "tsize=%ds", -1);
    exec_cmd_add_arg(cmd, "-pad");

    if(iso != NULL)
	    exec_cmd_add_arg(cmd, iso);
	else
	    exec_cmd_add_arg(cmd, "-"); /* no file_name so we're on the fly */

	cmd->read_proc = cdrecord_read_proc;
	cmd->pre_proc = cdrecord_pre_proc;
}


void
cdrecord_add_audio_args(ExecCmd *cmd)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);
	cdrecord_add_common_args(cmd);
	exec_cmd_add_arg(cmd, "-useinfo");
	exec_cmd_add_arg(cmd, "-audio");
	exec_cmd_add_arg(cmd, "-pad");

	cmd->read_proc = cdrecord_read_proc;
	cmd->pre_proc = cdrecord_copy_audio_cd_pre_proc;
}


void
cdrecord_add_blank_args(ExecCmd *cmd)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);

	cmd->read_proc = cdrecord_blank_read_proc;
	cmd->pre_proc = cdrecord_blank_pre_proc;
	cmd->post_proc = cdrecord_blank_post_proc;

	gchar *writer;

	switch(preferences_get_int(GB_BACKEND)) 
	{
		case BACKEND_CDRECORD:
			exec_cmd_add_arg(cmd, "cdrecord");
			writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
			break;
		case BACKEND_WODIM:
			exec_cmd_add_arg(cmd, "wodim");
			writer = devices_get_device_config(GB_WRITER, GB_DEVICE_NODE_LABEL);
			break;			
	}
		
	exec_cmd_add_arg(cmd, "dev=%s", writer);
	g_free(writer);

    const gint speed = preferences_get_int(GB_CDWRITE_SPEED);
    if(speed > 0)
       exec_cmd_add_arg(cmd, "speed=%d", speed);

	exec_cmd_add_arg(cmd, "-v");
	/*exec_cmd_add_arg(cmd, "-format");*/

    if(preferences_get_bool(GB_CDRECORD_FORCE))
       exec_cmd_add_arg(cmd, "-force");

	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cmd, "-eject");

	exec_cmd_add_arg(cmd, "gracetime=5");
	exec_cmd_add_arg(cmd, "blank=%s", preferences_get_bool(GB_FAST_BLANK) ? "fast" : "all");
}


/*******************************************************************************
 * CDDA2WAV
 ******************************************************************************/


static void
cdda2wav_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	cdda2wav_total_tracks = -1;
	cdda2wav_total_tracks_read = 0;

	progressdlg_set_status(_("Preparing to extract audio tracks"));
	progressdlg_increment_exec_number();

	gint response = GTK_RESPONSE_NO;

	gchar *tmp = preferences_get_string(GB_TEMP_DIR);
	gchar *file = g_strdup_printf("%s/gbtrack_01.wav", tmp);

	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		response = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
                _("Audio tracks from a previous sesion already exist on disk, "
			    "do you wish to use the existing tracks?"));
	}

	g_free(file);

	if(response == GTK_RESPONSE_NO)
	{
		gchar *cmd = g_strdup_printf("rm -fr %s/gbtrack*", tmp);
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
cdda2wav_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);

	gchar *text = (gchar*)buffer;
	if(cdda2wav_total_tracks == -1)
	{
		const gchar *tracks_start = strstr(text, "Tracks:");
		if(tracks_start != NULL)
            sscanf(tracks_start, "Tracks:%d", &cdda2wav_total_tracks);
        progressdlg_append_output(text);
	}
	else if(cdda2wav_total_tracks)
	{
		const gchar *pcnt = strchr(text, '%');
		if(pcnt != NULL)
		{
			do
				pcnt--;
            while(!g_ascii_isspace(*pcnt));
            gfloat fraction = 0.0;
            sscanf(++pcnt, "%f%%", &fraction);
            fraction /= 100.0;
            fraction *= (1.0/(gfloat)cdda2wav_total_tracks);
			fraction += ((gfloat)cdda2wav_total_tracks_read *(1.0/(gfloat)cdda2wav_total_tracks));
			progressdlg_set_fraction(fraction);

            const gchar *hundred = strstr(text, "rderr,");
			if(hundred != NULL)
				cdda2wav_total_tracks_read++;

            gchar *status = g_strdup_printf(_("Extracting audio track %d"), cdda2wav_total_tracks_read + 1);
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
cdda2wav_add_copy_args(ExecCmd *e)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
	gchar *reader = NULL;
	
	switch(preferences_get_int(GB_BACKEND)) 
	{
		case BACKEND_CDRECORD:
			exec_cmd_add_arg(e, "cdda2wav");
			reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
			break;
		case BACKEND_WODIM:
			exec_cmd_add_arg(e, "icedax");
			reader = devices_get_device_config(GB_READER, GB_DEVICE_NODE_LABEL);
			break;			
	}

	exec_cmd_add_arg(e, "-x");
	exec_cmd_add_arg(e, "cddb=1");
    exec_cmd_add_arg(e, "speed=52");
	exec_cmd_add_arg(e, "-B");
	exec_cmd_add_arg(e, "-g");
	exec_cmd_add_arg(e, "-Q");
	exec_cmd_add_arg(e, "-paranoia");

	exec_cmd_add_arg(e, "-D%s", reader);
	g_free(reader);

	exec_cmd_add_arg(e, "-Owav");

	gchar *tmp = preferences_get_string(GB_TEMP_DIR);
	exec_cmd_add_arg(e, "%s/gbtrack", tmp);
	g_free(tmp);

	e->pre_proc = cdda2wav_pre_proc;
	e->read_proc = cdda2wav_read_proc;
    e->post_proc = execfunctions_prompt_for_disk_post_proc;
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
mkisofs_calc_size_pre_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);

    progressdlg_set_status(_("Calculating data disk image size"));
    progressdlg_increment_exec_number();
    progressdlg_start_approximation(20);
}


static void
mkisofs_calc_size_read_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    g_return_if_fail(buffer != NULL);

    const gchar *written = strstr(buffer, "written =");
    if(written != NULL)
    {
        if(sscanf(written, "written = %llu\n", &cdrecord_total_disk_bytes) == 1)
            GB_TRACE("mkisofs_calc_size_read_proc - size is [%llu]\n", cdrecord_total_disk_bytes);
    }
    progressdlg_append_output(buffer);
}


static void
mkisofs_calc_size_post_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    progressdlg_stop_approximation();
}


static void
mkisofs_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(ex != NULL);

	progressdlg_set_status(_("Creating data disk image"));
	progressdlg_increment_exec_number();

	gchar *file = preferences_get_create_data_cd_image();
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
mkisofs_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);

	const gchar *percent = strrchr(buffer, '%');
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


/*static gboolean
mkisofs_foreach_func(GtkTreeModel *model,
                GtkTreePath *path,
                GtkTreeIter *iter,
                gpointer user_data)
{
	GB_LOG_FUNC
	gchar *file = NULL, *filepath = NULL;
	gboolean existingsession = FALSE;

	gtk_tree_model_get (model, iter, DATACD_COL_FILE, &file,
		DATACD_COL_PATH, &filepath, DATACD_COL_SESSION, &existingsession, -1);

	/Only add files that are not part of an existing session/
	if(!existingsession)
		exec_cmd_add_arg((ExecCmd*)user_data, "%s=%s", file, filepath);

	g_free(file);
	g_free(filepath);

	return FALSE; /do not stop walking the store, call us with next row/
}
*/



static void
mkisofs_add_common_args(ExecCmd *e, StartDlg *start_dlg, const gchar *arguments_file)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    g_return_if_fail(start_dlg != NULL);

    const gchar *text = gtk_entry_get_text(start_dlg->volume_id);
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
        exec_cmd_add_arg(e, "-r");
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

    /*this is not valid now since we have to build recursively the paths
     * We will use a GList*/
    /*
    gtk_tree_model_foreach(datamodel, mkisofs_foreach_func, e);
    */
        /*--path-list FILE */
    exec_cmd_add_arg(e, "--path-list");
    exec_cmd_add_arg(e, "%s", arguments_file);
}


void
mkisofs_add_args(ExecCmd *e, StartDlg *start_dlg, const gchar *arguments_file, const gchar *msinfo, const gboolean calculate_size)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
    g_return_if_fail(start_dlg != NULL);
	cdrecord_total_disk_bytes = 0;
	
	gchar* writer = NULL;
	
	switch(preferences_get_int(GB_BACKEND)) 
	{
		case BACKEND_CDRECORD:
			exec_cmd_add_arg(e, "mkisofs");
			writer = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
			break;
		case BACKEND_WODIM:
			exec_cmd_add_arg(e, "genisoimage");
			writer = devices_get_device_config(GB_READER, GB_DEVICE_NODE_LABEL);
			break;			
	}

	/* If this is a another session on an existing cd we don't show the
	   iso details dialog */
	if(msinfo != NULL)
	{
		exec_cmd_add_arg(e, "-C");
		exec_cmd_add_arg(e, msinfo);

		gchar *writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
		exec_cmd_add_arg(e, "-M");
		exec_cmd_add_arg(e, writer);
		g_free(writer);

		/* This is a cludge so that we don't ask the user if they want to use the existing iso */
		gchar *create_data_iso = preferences_get_create_data_cd_image();
		gchar *cmd = g_strdup_printf("rm -fr %s", create_data_iso);
		system(cmd);
		g_free(cmd);
		g_free(create_data_iso);
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
        gchar *file = preferences_get_create_data_cd_image();
        exec_cmd_add_arg(e, file);
        g_free(file);
    }

    if(calculate_size)
    {
        exec_cmd_add_arg(e, "--print-size");
        e->pre_proc = mkisofs_calc_size_pre_proc;
        e->read_proc = mkisofs_calc_size_read_proc;
        e->post_proc = mkisofs_calc_size_post_proc;
    }
    else
    {
        e->pre_proc = mkisofs_pre_proc;
        e->read_proc = mkisofs_read_proc;
    }

    mkisofs_add_common_args(e, start_dlg, arguments_file);

	/* We don't own the msinfo gchar datacd does
	g_free(msinfo);*/
}


void
mkisofs_add_calc_iso_size_args(ExecCmd *e, const gchar *iso)
{
    g_return_if_fail(e != NULL);
    g_return_if_fail(iso != NULL);


	switch(preferences_get_int(GB_BACKEND)) 
	{
		case BACKEND_CDRECORD:
			exec_cmd_add_arg(e, "mkisofs");			
			break;
		case BACKEND_WODIM:
			exec_cmd_add_arg(e, "genisoimage");
			break;			
	}

    exec_cmd_add_arg(e, "--print-size");
    exec_cmd_add_arg(e, iso);
    e->pre_proc = mkisofs_calc_size_pre_proc;
    e->read_proc = mkisofs_calc_size_read_proc;
    e->post_proc = mkisofs_calc_size_post_proc;
}



/*******************************************************************************
 * WODIM
 ******************************************************************************/

static void
wodim_blank_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	progressdlg_set_status(_("Preparing to blank disk"));
    if(devices_prompt_for_disk(progressdlg_get_window(), GB_WRITER) == GTK_RESPONSE_CANCEL)
        exec_cmd_set_state((ExecCmd*)ex, CANCELLED);
    else
    {
        /* This time approximation is my first attempt at figuring out how long blanking a
         * cd really takes. It's not very scientific and I will at some point try to figure it
         * out more accurately.*/
        gint speed = preferences_get_int(GB_CDWRITE_SPEED);
        if(speed == 0) speed = 1;
        gint approximation = 0;
        if(preferences_get_bool(GB_FAST_BLANK))
            approximation = (speed * -5) + 95;
        else
            approximation = ((60/speed) * (60/speed)) * 15;

        GB_TRACE("wodim_blank_pre_proc - approximation is [%d]\n", approximation);
        progressdlg_start_approximation(approximation);
        progressdlg_increment_exec_number();
    }
    devices_unmount_device(GB_WRITER);
}


static void
wodim_blank_post_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	progressdlg_stop_approximation();
}


static void
wodim_blank_read_proc(void *ex, void *buffer)
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
wodim_pre_proc(void *ex, void *buffer)
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
        if(cdrecord_total_disk_bytes > 0)
            exec_cmd_update_arg((ExecCmd*)ex, "tsize=", "tsize=%ds", cdrecord_total_disk_bytes);
        devices_unmount_device(GB_WRITER);
    }
}


static void
wodim_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);

	gchar *buf = (gchar*)buffer;
	const gchar *track = strstr(buf, "Track");
	if(track != NULL)
	{
        gint current_track = 0;
        gfloat current = 0, total = 0;

        const gchar *of = strstr(buf, " of ");
        if(of != NULL) /* regular burn i.e not on the fly */
        {
            /*  Track 01:    3 of   19 MB written (fifo 100%) [buf  98%]   4.5x. */
            sscanf(track, "%*s %d %*s %f %*s %f", &current_track, &current, &total);
        }
        else /* on the fly */
        {
            /* Track 01:   12 MB written (fifo 100%) [buf  96%]   4.0x. */
            sscanf(track, "%*s %d %*s %f", &current_track, &current);
            total = cdrecord_total_disk_bytes / 1024 / 1024;
        }

        if(current > 0.0)
        {
            if(cdrecord_first_track == -1)
                cdrecord_first_track = current_track;

            /* Figure out how many tracks we have written so far and calc the fraction */
            gfloat total_fraction =  ((gfloat)current_track - cdrecord_first_track)
                    * (1.0 / cdrecord_total_tracks_to_write);

            /* now add on the fraction of the track we are currently writing */
            total_fraction += ((current / total) * (1.0 / cdrecord_total_tracks_to_write));

            /*GB_TRACE("^^^^^ current [%d] first [%d] current [%f] total [%f] fraction [%f]",
                current_track, cdrecord_first_track, current, total, total_fraction);*/

            gchar *status = g_strdup_printf(_("Writing track %d"), current_track);
            progressdlg_set_status(status);
            g_free(status);
            progressdlg_set_fraction(total_fraction);
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


static void
wodim_copy_audio_cd_pre_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);

    wodim_pre_proc(ex, buffer);

    GError *err = NULL;
    gchar *tmp = preferences_get_string(GB_TEMP_DIR);
    GDir *dir = g_dir_open(tmp, 0, &err);
    GList *files_list = NULL;
    GList *p = NULL;

    if(dir != NULL)
    {
        cdrecord_total_tracks_to_write = 0;
        /* loop around reading the files in the directory */
        const gchar *name = g_dir_read_name(dir);
        while(name != NULL)
        {
            if(g_str_has_suffix(name, ".wav"))
            {
                GB_TRACE("wodim_copy_audio_cd_pre_proc - found [%s]\n", name);
                gchar *full_path = g_build_filename(tmp, name, NULL);
                files_list = g_list_insert_sorted(files_list, full_path, (GCompareFunc)strcmp);
                cdrecord_total_tracks_to_write++;
            }

            name = g_dir_read_name(dir);
        }
        g_dir_close(dir);

        for (p = files_list; p != NULL; p = g_list_next(p)) {
            GB_TRACE("wodim_copy_audio_cd_pre_proc - adding [%s]\n", p->data);
            exec_cmd_add_arg(ex, p->data);
            g_free(p->data);
            p->data = NULL;
        }
        g_list_free(files_list);
    }
    g_free(tmp);
}


/*
 *  Populates the common information required to burn a cd
 */
static void
wodim_add_common_args(ExecCmd *cmd)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);

    cdrecord_total_disk_bytes = 0;
    cdrecord_total_tracks_to_write = 1;
    cdrecord_first_track = -1;

	exec_cmd_add_arg(cmd, "wodim");
	gchar *writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cmd, "dev=%s", writer);
	g_free(writer);

	if(preferences_get_bool(GB_CDRECORD_FORCE))
	   exec_cmd_add_arg(cmd, "-force");

	exec_cmd_add_arg(cmd, "gracetime=5");

    const gint speed = preferences_get_int(GB_CDWRITE_SPEED);
    if(speed > 0)
	   exec_cmd_add_arg(cmd, "speed=%d", speed);

    exec_cmd_add_arg(cmd, "-v");

	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cmd, "-eject");

	if(preferences_get_bool(GB_DUMMY))
		exec_cmd_add_arg(cmd, "-dummy");

	if(preferences_get_bool(GB_BURNFREE))
		exec_cmd_add_arg(cmd, "driveropts=burnfree");

	gchar *mode = preferences_get_string(GB_WRITE_MODE);
	if(g_ascii_strcasecmp(mode, _("Auto")) != 0)
		exec_cmd_add_arg(cmd, "-%s", mode);
	g_free(mode);
}


void
wodim_add_create_audio_cd_args(ExecCmd *cmd, const GList *audio_files)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);

	wodim_add_common_args(cmd);
	exec_cmd_add_arg(cmd, "-audio");
    exec_cmd_add_arg(cmd, "-pad");
    exec_cmd_add_arg(cmd, "-useinfo");
    exec_cmd_add_arg(cmd, "-text");

	/* if we are on the fly this will be a list of inf files, otherwise
     * it's the list of wavs to burn */
    const GList *audio_file = audio_files;
    for (; audio_file != NULL ; audio_file = audio_file->next)
    {
        exec_cmd_add_arg(cmd, audio_file->data);
        GB_TRACE("wodim_add_create_audio_cd_args - adding [%s]\n", (gchar*)audio_file->data);
        cdrecord_total_tracks_to_write++;
    }
	cmd->read_proc = wodim_read_proc;
	cmd->pre_proc = wodim_pre_proc;
}


void
wodim_add_iso_args(ExecCmd *cmd, const gchar *iso)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);

	wodim_add_common_args(cmd);

    exec_cmd_add_arg(cmd, "-overburn");

	/*if(!prefs->multisession)*/
		exec_cmd_add_arg(cmd, "-multi");

    exec_cmd_add_arg(cmd, "tsize=%ds", -1);
    exec_cmd_add_arg(cmd, "-pad");

    if(iso != NULL)
	    exec_cmd_add_arg(cmd, iso);
	else
	    exec_cmd_add_arg(cmd, "-"); /* no file_name so we're on the fly */

	cmd->read_proc = wodim_read_proc;
	cmd->pre_proc = wodim_pre_proc;
}


void
wodim_add_audio_args(ExecCmd *cmd)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);
	wodim_add_common_args(cmd);
	exec_cmd_add_arg(cmd, "-useinfo");
	exec_cmd_add_arg(cmd, "-audio");
	exec_cmd_add_arg(cmd, "-pad");

	cmd->read_proc = wodim_read_proc;
	cmd->pre_proc = wodim_copy_audio_cd_pre_proc;
}


void
wodim_add_blank_args(ExecCmd *cmd)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);

	cmd->read_proc = wodim_blank_read_proc;
	cmd->pre_proc = wodim_blank_pre_proc;
	cmd->post_proc = wodim_blank_post_proc;

	exec_cmd_add_arg(cmd, "wodim");

	gchar *writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cmd, "dev=%s", writer);
	g_free(writer);

    const gint speed = preferences_get_int(GB_CDWRITE_SPEED);
    if(speed > 0)
       exec_cmd_add_arg(cmd, "speed=%d", speed);

	exec_cmd_add_arg(cmd, "-v");
	/*exec_cmd_add_arg(cmd, "-format");*/

    if(preferences_get_bool(GB_CDRECORD_FORCE))
       exec_cmd_add_arg(cmd, "-force");

	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cmd, "-eject");

	exec_cmd_add_arg(cmd, "gracetime=5");
	exec_cmd_add_arg(cmd, "blank=%s", preferences_get_bool(GB_FAST_BLANK) ? "fast" : "all");
}


/*******************************************************************************
 * ICEDAX
 ******************************************************************************/


static void
icedax_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	cdda2wav_total_tracks = -1;
	cdda2wav_total_tracks_read = 0;

	progressdlg_set_status(_("Preparing to extract audio tracks"));
	progressdlg_increment_exec_number();

	gint response = GTK_RESPONSE_NO;

	gchar *tmp = preferences_get_string(GB_TEMP_DIR);
	gchar *file = g_strdup_printf("%s/gbtrack_01.wav", tmp);

	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		response = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
                _("Audio tracks from a previous sesion already exist on disk, "
			    "do you wish to use the existing tracks?"));
	}

	g_free(file);

	if(response == GTK_RESPONSE_NO)
	{
		gchar *cmd = g_strdup_printf("rm -fr %s/gbtrack*", tmp);
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
icedax_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);

	gchar *text = (gchar*)buffer;
	if(cdda2wav_total_tracks == -1)
	{
		const gchar *tracks_start = strstr(text, "Tracks:");
		if(tracks_start != NULL)
            sscanf(tracks_start, "Tracks:%d", &cdda2wav_total_tracks);
        progressdlg_append_output(text);
	}
	else if(cdda2wav_total_tracks)
	{
		const gchar *pcnt = strchr(text, '%');
		if(pcnt != NULL)
		{
			do
				pcnt--;
            while(!g_ascii_isspace(*pcnt));
            gfloat fraction = 0.0;
            sscanf(++pcnt, "%f%%", &fraction);
            fraction /= 100.0;
            fraction *= (1.0/(gfloat)cdda2wav_total_tracks);
			fraction += ((gfloat)cdda2wav_total_tracks_read *(1.0/(gfloat)cdda2wav_total_tracks));
			progressdlg_set_fraction(fraction);

            const gchar *hundred = strstr(text, "rderr,");
			if(hundred != NULL)
				cdda2wav_total_tracks_read++;

            gchar *status = g_strdup_printf(_("Extracting audio track %d"), cdda2wav_total_tracks_read + 1);
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
icedax_add_copy_args(ExecCmd *e)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);

	exec_cmd_add_arg(e, "icedax");
	exec_cmd_add_arg(e, "-x");
	exec_cmd_add_arg(e, "cddb=1");
    exec_cmd_add_arg(e, "speed=52");
	exec_cmd_add_arg(e, "-B");
	exec_cmd_add_arg(e, "-g");
	exec_cmd_add_arg(e, "-Q");
	exec_cmd_add_arg(e, "-paranoia");

	gchar *reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(e, "-D%s", reader);
	g_free(reader);

	exec_cmd_add_arg(e, "-Owav");

	gchar *tmp = preferences_get_string(GB_TEMP_DIR);
	exec_cmd_add_arg(e, "%s/gbtrack", tmp);
	g_free(tmp);

	e->pre_proc = icedax_pre_proc;
	e->read_proc = icedax_read_proc;
    e->post_proc = execfunctions_prompt_for_disk_post_proc;
}


/*******************************************************************************
 * GENISOIMAGE
 ******************************************************************************/


/*
	genisoimage -o gb.iso -R -J -hfs *

 genisoimage -V "Jamie Michelle Wedding" -p "Luke Biddell" -A "GnomeBaker" -o gb.iso -R -J -hfs *

 also
 -L (include dot files) -X (mixed case filenames) -s (multiple dots in name)

shell> NEXT_TRACK=`wodim -msinfo dev=0,6,0`
shell> echo $NEXT_TRACK
shell> genisoimage -R -o cd_image2 -C $NEXT_TRACK -M /dev/scd5 private_collection/
*/


static void
genisoimage_calc_size_pre_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);

    progressdlg_set_status(_("Calculating data disk image size"));
    progressdlg_increment_exec_number();
    progressdlg_start_approximation(20);
}


static void
genisoimage_calc_size_read_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    g_return_if_fail(buffer != NULL);

    const gchar *written = strstr(buffer, "written =");
    if(written != NULL)
    {
        if(sscanf(written, "written = %llu\n", &cdrecord_total_disk_bytes) == 1)
            GB_TRACE("genisoimage_calc_size_read_proc - size is [%llu]\n", cdrecord_total_disk_bytes);
    }
    progressdlg_append_output(buffer);
}


static void
genisoimage_calc_size_post_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    progressdlg_stop_approximation();
}


static void
genisoimage_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(ex != NULL);

	progressdlg_set_status(_("Creating data disk image"));
	progressdlg_increment_exec_number();

	gchar *file = preferences_get_create_data_cd_image();
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
genisoimage_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);

	const gchar *percent = strrchr(buffer, '%');
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


/*static gboolean
mkisofs_foreach_func(GtkTreeModel *model,
                GtkTreePath *path,
                GtkTreeIter *iter,
                gpointer user_data)
{
	GB_LOG_FUNC
	gchar *file = NULL, *filepath = NULL;
	gboolean existingsession = FALSE;

	gtk_tree_model_get (model, iter, DATACD_COL_FILE, &file,
		DATACD_COL_PATH, &filepath, DATACD_COL_SESSION, &existingsession, -1);

	/Only add files that are not part of an existing session/
	if(!existingsession)
		exec_cmd_add_arg((ExecCmd*)user_data, "%s=%s", file, filepath);

	g_free(file);
	g_free(filepath);

	return FALSE; /do not stop walking the store, call us with next row/
}
*/



static void
genisoimage_add_common_args(ExecCmd *e, StartDlg *start_dlg, const gchar *arguments_file)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    g_return_if_fail(start_dlg != NULL);

    const gchar *text = gtk_entry_get_text(start_dlg->volume_id);
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
        exec_cmd_add_arg(e, "-r");
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

    /*this is not valid now since we have to build recursively the paths
     * We will use a GList*/
    /*
    gtk_tree_model_foreach(datamodel, mkisofs_foreach_func, e);
    */
        /*--path-list FILE */
    exec_cmd_add_arg(e, "--path-list");
    exec_cmd_add_arg(e, "%s", arguments_file);
}


void
genisoimage_add_args(ExecCmd *e, StartDlg *start_dlg, const gchar *arguments_file, const gchar *msinfo, const gboolean calculate_size)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
    g_return_if_fail(start_dlg != NULL);
	cdrecord_total_disk_bytes = 0;

    exec_cmd_add_arg(e, "mkisofs");

	/* If this is a another session on an existing cd we don't show the
	   iso details dialog */
	if(msinfo != NULL)
	{
		exec_cmd_add_arg(e, "-C");
		exec_cmd_add_arg(e, msinfo);

		gchar *writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
		exec_cmd_add_arg(e, "-M");
		exec_cmd_add_arg(e, writer);
		g_free(writer);

		/* This is a cludge so that we don't ask the user if they want to use the existing iso */
		gchar *create_data_iso = preferences_get_create_data_cd_image();
		gchar *cmd = g_strdup_printf("rm -fr %s", create_data_iso);
		system(cmd);
		g_free(cmd);
		g_free(create_data_iso);
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
        gchar *file = preferences_get_create_data_cd_image();
        exec_cmd_add_arg(e, file);
        g_free(file);
    }

    if(calculate_size)
    {
        exec_cmd_add_arg(e, "--print-size");
        e->pre_proc = genisoimage_calc_size_pre_proc;
        e->read_proc = genisoimage_calc_size_read_proc;
        e->post_proc = genisoimage_calc_size_post_proc;
    }
    else
    {
        e->pre_proc = genisoimage_pre_proc;
        e->read_proc = genisoimage_read_proc;
    }

    genisoimage_add_common_args(e, start_dlg, arguments_file);

	/* We don't own the msinfo gchar datacd does
	g_free(msinfo);*/
}


void
genisoimage_add_calc_iso_size_args(ExecCmd *e, const gchar *iso)
{
    g_return_if_fail(e != NULL);
    g_return_if_fail(iso != NULL);

    exec_cmd_add_arg(e, "genisoimage");
    exec_cmd_add_arg(e, "--print-size");
    exec_cmd_add_arg(e, iso);
    e->pre_proc = genisoimage_calc_size_pre_proc;
    e->read_proc = genisoimage_calc_size_read_proc;
    e->post_proc = genisoimage_calc_size_post_proc;
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
dvdformat_pre_proc(void *ex, void *buffer)
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
dvdformat_read_proc(void *ex, void *buffer)
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
dvdformat_post_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	if(preferences_get_bool(GB_EJECT))
		devices_eject_disk(GB_WRITER);
}


void
dvdformat_add_args(ExecCmd *cmd)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);

	cmd->read_proc = dvdformat_read_proc;
	cmd->pre_proc = dvdformat_pre_proc;
	cmd->post_proc = dvdformat_post_proc;

	exec_cmd_add_arg(cmd, "dvd+rw-format");

	gchar *writer = devices_get_device_config(GB_WRITER,GB_DEVICE_NODE_LABEL);
	exec_cmd_add_arg(cmd, writer);
	g_free(writer);

	/* make the output gui friendly */
	exec_cmd_add_arg(cmd, "-gui");

	if(preferences_get_bool(GB_FORCE))
	{
		if(!preferences_get_bool(GB_FAST_FORMAT))
			exec_cmd_add_arg(cmd, "-force=full");
		else
			exec_cmd_add_arg(cmd, "-force");
	}
	else if(!preferences_get_bool(GB_FAST_FORMAT))
	{
		exec_cmd_add_arg(cmd, "-blank=full");
	}
    else
    {
        exec_cmd_add_arg(cmd, "-blank");
    }
}


/*******************************************************************************
 * GROWISOFS
 ******************************************************************************/


static void
growisofs_pre_proc(void *ex, void *buffer)
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
growisofs_post_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	if(preferences_get_bool(GB_EJECT))
		devices_eject_disk(GB_WRITER);
}


static void
growisofs_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);

	gchar *buf = (gchar*)buffer;
	const gchar *progress_str = strstr(buf, "done, estimate");
	if(progress_str != NULL)
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
growisofs_read_iso_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);

	gchar *buf = (gchar*)buffer;
	const gchar *progress_str = strstr(buf, "remaining");
	if(progress_str != NULL)
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
growisofs_add_args(ExecCmd *e, StartDlg *start_dlg, const gchar *arguments_file, const gchar *msinfo)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);

    exec_cmd_add_arg(e, "growisofs");

    /* merge new session with existing one */
    if(msinfo != NULL)
        exec_cmd_add_arg(e, "-M");
    else
        exec_cmd_add_arg(e, "-Z");

    gchar *writer = devices_get_device_config(GB_WRITER,GB_DEVICE_NODE_LABEL);
    exec_cmd_add_arg(e, writer);
    g_free(writer);

    /* TODO: Overburn support
    if(preferences_get_int(GB_OVERBURN))
        exec_cmd_add_arg(growisofs, "-overburn"); */
    const gint speed = preferences_get_int(GB_DVDWRITE_SPEED);
    if(speed > 0)
       exec_cmd_add_arg(e, "-speed=%d", speed);

    /*http://fy.chalmers.se/~appro/linux/DVD+RW/tools/growisofs.c
        for the use-the-force options */
    /* stop the reloading of the disc */
    exec_cmd_add_arg(e, "-use-the-force-luke=notray");

    /* force overwriting existing filesystem */
    exec_cmd_add_arg(e, "-use-the-force-luke=tty");

    /* http://www.troubleshooters.com/linux/coasterless_dvd.htm#_Gotchas
        states that dao with dvd compat is the best way of burning dvds */
    gchar *mode = preferences_get_string(GB_DVDWRITE_MODE);
    if(g_ascii_strcasecmp(mode, _("Auto")) != 0)
        exec_cmd_add_arg(e, "-use-the-force-luke=dao");
    g_free(mode);

    if(preferences_get_bool(GB_DUMMY))
        exec_cmd_add_arg(e, "-use-the-force-luke=dummy");

    if(preferences_get_bool(GB_FINALIZE))
        exec_cmd_add_arg(e, "-dvd-compat");

    exec_cmd_add_arg(e, "-gui");

    mkisofs_add_common_args(e, start_dlg, arguments_file);

	e->read_proc = growisofs_read_proc;
    e->pre_proc = growisofs_pre_proc;
    e->post_proc = growisofs_post_proc;

	/* We don't own the msinfo gchar datacd does
	g_free(msinfo);*/
}


void
growisofs_add_iso_args(ExecCmd *cmd, const gchar *iso)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);
	g_return_if_fail(iso != NULL);

	cmd->read_proc = growisofs_read_iso_proc;
	cmd->pre_proc = growisofs_pre_proc;
	cmd->post_proc = growisofs_post_proc;
	exec_cmd_add_arg(cmd, "growisofs");
	exec_cmd_add_arg(cmd, "-dvd-compat");

    const gint speed = preferences_get_int(GB_DVDWRITE_SPEED);
    if(speed > 0)
       exec_cmd_add_arg(cmd, "-speed=%d", speed);

	/* -gui makes the output more verbose, so we can interpret it easier */
    /*	exec_cmd_add_arg(growisofs, "-gui"); */
	/* -gui does not work with iso's, argh! */

	/* stop the reloading of the disc */
	exec_cmd_add_arg(cmd, "-use-the-force-luke=notray");

	/* force overwriting existing filesystem */
    exec_cmd_add_arg(cmd, "-use-the-force-luke=tty");

    exec_cmd_add_arg(cmd, "-Z");
	gchar *writer = devices_get_device_config(GB_WRITER, GB_DEVICE_NODE_LABEL);
	exec_cmd_add_arg(cmd, "%s=%s", writer, iso);
	g_free(writer);
}


/*******************************************************************************
 * READCD
 ******************************************************************************/


static void
readcd_pre_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	readcd_total_guchars = -1;

	progressdlg_set_status(_("Reading CD image"));
	progressdlg_increment_exec_number();

	gint response = GTK_RESPONSE_NO;
	gchar *file = preferences_get_copy_data_cd_image();
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
readcd_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);

	gchar *text = (gchar*)buffer;
	if(readcd_total_guchars == -1)
	{
		gchar *end = strstr(text, "end:");
		if(end != NULL)
		{
			if(sscanf(end, "%*s %d", &readcd_total_guchars) > 0)
				GB_TRACE("readcd_read_proc - readcd size is [%d]\n", readcd_total_guchars);
		}
		progressdlg_append_output(text);
	}
	else if(readcd_total_guchars)
	{
		const gchar *start = strstr(text, "addr:");
		if(start != NULL)
		{
			gint read_guchars = 0;
			if(sscanf(start, "%*s %d", &read_guchars) > 0)
				progressdlg_set_fraction((gfloat)read_guchars/(gfloat)readcd_total_guchars);
		}
		else
			progressdlg_append_output(text);
	}
}


/*
 *  Populates the information required to make an iso from an existing data cd
 */
void
readcd_add_copy_args(ExecCmd *e, const gchar *iso)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	g_return_if_fail(iso != NULL);

	gchar *reader = NULL;

	switch(preferences_get_int(GB_BACKEND)) 
	{
		case BACKEND_CDRECORD:
			exec_cmd_add_arg(e, "readcd");
			reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
			break;
		case BACKEND_WODIM:
			exec_cmd_add_arg(e, "readom");
			reader = devices_get_device_config(GB_READER, GB_DEVICE_NODE_LABEL);
			break;			
	}

	exec_cmd_add_arg(e, "dev=%s", reader);
	g_free(reader);

    exec_cmd_add_arg(e, "f=%s", iso);

    if(!preferences_get_bool(GB_CREATEISOONLY))
        e->post_proc = execfunctions_prompt_for_disk_post_proc;

	/*exec_cmd_add_arg(e, "-notrunc");
	exec_cmd_add_arg(e, "-clone");
	exec_cmd_add_arg(e, "-silent");*/

	e->pre_proc = readcd_pre_proc;
	e->read_proc = readcd_read_proc;
}

/*******************************************************************************
 * CDRDAO
 ******************************************************************************/
/*
static void
cdrdao_extract_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	g_return_if_fail(ex != NULL);

	const gchar *output = (gchar*)buffer;
	const gchar *length = strstr(output, ", length");
	if(length != NULL)
	{
		gint len = 0;
		if(sscanf(length, ", length %d:", &len) == 1)
			cdrdao_cdminutes = len;
	}

	const gchar *new_line = strstr(output, "\n");
	if(new_line != NULL)
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
cdrdao_write_image_read_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);

    const gchar *output = (gchar*)buffer;
    const gchar *wrote = strstr(output, "Wrote");
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
cdrdao_add_image_args(ExecCmd *cmd, const gchar *toc_or_cue)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);
	g_return_if_fail(toc_or_cue != NULL);

    cmd->working_dir = g_path_get_dirname(toc_or_cue);

	cmd->pre_proc = cdrecord_pre_proc;
	cmd->read_proc = cdrdao_write_image_read_proc;

	exec_cmd_add_arg(cmd, "cdrdao");
	exec_cmd_add_arg(cmd, "write");

	gchar *writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
	exec_cmd_add_arg(cmd, "--device");
	exec_cmd_add_arg(cmd, writer);
	g_free(writer);

    const gint speed = preferences_get_int(GB_CDWRITE_SPEED);
    if(speed > 0)
    {
    	exec_cmd_add_arg(cmd, "--speed");
    	exec_cmd_add_arg(cmd, "%d", speed);
    }
    exec_cmd_add_arg(cmd, "--buffers");
    exec_cmd_add_arg(cmd, "64");
    exec_cmd_add_arg(cmd, "-n"); /* turn off the 10 second pause */

	if(preferences_get_bool(GB_EJECT))
		exec_cmd_add_arg(cmd, "--eject");

	if(preferences_get_bool(GB_DUMMY))
		exec_cmd_add_arg(cmd, "--simulate");

    exec_cmd_add_arg(cmd, toc_or_cue);
}


static void
cdrdao_copy_cd_read_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);

    const gchar *output = (gchar*)buffer;
    const gchar *wrote = strstr(output, "Wrote");
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
cdrdao_add_copy_args(ExecCmd *cmd)
{
    GB_LOG_FUNC
    g_return_if_fail(cmd != NULL);

    cmd->working_dir = preferences_get_string(GB_TEMP_DIR);
    cmd->read_proc = cdrdao_copy_cd_read_proc;

    exec_cmd_add_arg(cmd, "cdrdao");
    exec_cmd_add_arg(cmd, "copy");
    exec_cmd_add_arg(cmd, "--read-raw");

    gchar *reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
    exec_cmd_add_arg(cmd, "--source-device");
    exec_cmd_add_arg(cmd, reader);
    g_free(reader);

    gchar *writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
    exec_cmd_add_arg(cmd, "--device");
    exec_cmd_add_arg(cmd, writer);
    g_free(writer);

    if(preferences_get_bool(GB_ONTHEFLY))
        exec_cmd_add_arg(cmd, "--on-the-fly");

    const gint speed = preferences_get_int(GB_CDWRITE_SPEED);
    if(speed > 0)
    {
        exec_cmd_add_arg(cmd, "--speed");
        exec_cmd_add_arg(cmd, "%d", speed);
    }
    exec_cmd_add_arg(cmd, "--buffers");
    exec_cmd_add_arg(cmd, "64");
    exec_cmd_add_arg(cmd, "-n"); /* turn off the 10 second pause */

    if(preferences_get_bool(GB_EJECT))
        exec_cmd_add_arg(cmd, "--eject");

    if(preferences_get_bool(GB_DUMMY))
        exec_cmd_add_arg(cmd, "--simulate");

    /*if(preferences_get_int(GB_OVERBURN))*/
        exec_cmd_add_arg(cmd, "-overburn");
}


/*******************************************************************************
 * GSTREAMER
 ******************************************************************************/
static guint gstreamer_timer = 0;

static gboolean
gstreamer_progress_timer(gpointer data)
{
    GB_LOG_FUNC
    g_return_val_if_fail(data != NULL, FALSE);
    MediaPipeline *media_pipeline = (MediaPipeline*)data;

#ifdef GST_010
    GstFormat fmt = GST_FORMAT_PERCENT;
    gint64 pos = 0;
    if(gst_element_query_position(media_pipeline->source, &fmt, &pos))
        progressdlg_set_fraction((gfloat)(pos/GST_FORMAT_PERCENT_SCALE)/100.0);
#else
    GstFormat fmt = GST_FORMAT_BYTES;
    gint64 pos = 0, total = 0;
    if(gst_element_query(media_pipeline->dest, GST_QUERY_POSITION, &fmt, &pos) &&
            gst_element_query(media_pipeline->dest, GST_QUERY_TOTAL, &fmt, &total))
    {
        progressdlg_set_fraction((gfloat)pos/(gfloat)total);
    }
#endif
    return TRUE;
}

#ifndef GST_010

static void
gstreamer_pipeline_eos(GstElement *gstelement, gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(user_data != NULL);
    exec_cmd_set_state((ExecCmd*)user_data, COMPLETED);
}


static void
gstreamer_pipeline_error(GstElement *gstelement,
                        GstElement *element,
                        GError *error,
                        gchar *message,
                        gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(user_data != NULL);
    if(error != NULL)
        progressdlg_append_output(error->message);
    progressdlg_append_output(message);
    exec_cmd_set_state((ExecCmd*)user_data, FAILED);
}


#endif


static void
gstreamer_new_decoded_pad(GstElement *element,
                           GstPad *pad,
                           gboolean last,
                           MediaPipeline *media_pipeline)
{
    GB_LOG_FUNC
    g_return_if_fail(element != NULL);
    g_return_if_fail(pad != NULL);
    g_return_if_fail(media_pipeline != NULL);

    GstCaps *caps = gst_pad_get_caps(pad);
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *mimetype = gst_structure_get_name(structure);
    if(g_str_has_prefix(mimetype, "audio/x-raw"))
    {
        GstPad *decode_pad = gst_element_get_pad(media_pipeline->converter, "src");
        gst_pad_link(decode_pad, pad);
        gst_element_link(media_pipeline->decoder, media_pipeline->converter);
#ifdef GST_010
        gst_object_unref(decode_pad);
#else
        gst_bin_add_many(GST_BIN(media_pipeline->pipeline), media_pipeline->converter,
            media_pipeline->scale, media_pipeline->endian_converter, media_pipeline->encoder, media_pipeline->dest, NULL);

        /* This function synchronizes a bins state on all of its contained children. */
        gst_bin_sync_children_state(GST_BIN(media_pipeline->pipeline));
#endif
    }
#ifdef GST_010
    gst_caps_unref(caps);
#endif
}


#ifdef GST_010

static gboolean
gstreamer_bus_callback(GstBus *bus, GstMessage *message, ExecCmd *cmd)
{
    GB_LOG_FUNC
    g_return_val_if_fail(bus != NULL, FALSE);
    g_return_val_if_fail(message != NULL, FALSE);
    g_return_val_if_fail(cmd != NULL, FALSE);

    switch (GST_MESSAGE_TYPE (message))
    {
    case GST_MESSAGE_ERROR:
    {
        gchar *debug = NULL;
        GError *error = NULL;
        gst_message_parse_error(message, &error, &debug);
        g_warning("gstreamer_bus_callback - Error [%s] Debug [%s]\n", error->message, debug);
        if(error != NULL)
            progressdlg_append_output(error->message);
        progressdlg_append_output(debug);
        g_free(debug);
        exec_cmd_set_state(cmd, FAILED);
        break;
    }
    case GST_MESSAGE_EOS:
        exec_cmd_set_state(cmd, COMPLETED);
        break;
    default:
        break;
    }
    return TRUE;
}

#endif

static void
gstreamer_pre_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    ExecCmd *cmd = (ExecCmd*)ex;

    gchar *file_name = g_path_get_basename((gchar*)g_ptr_array_index(cmd->args, 0));
    gchar *text = g_strdup_printf(_("Converting %s to cd audio"), file_name);
    progressdlg_set_status(text);
    g_free(file_name);
    g_free(text);
    progressdlg_increment_exec_number();
}


static void
gstreamer_lib_proc(void *ex, void *data)
{
    GB_LOG_FUNC
    g_return_if_fail(ex != NULL);
    ExecCmd *cmd = (ExecCmd*)ex;
    gint *pipe = (gint*)data;

    GB_TRACE("gstreamer_lib_proc - converting [%s] to [%s]\n", (gchar*)g_ptr_array_index(cmd->args, 0),
            (gchar*)g_ptr_array_index(cmd->args, 1));

#ifdef GST_010
    MediaPipeline *media_pipeline = g_new0(MediaPipeline, 1);

    /* create a new pipeline to hold the elements */
    media_pipeline->pipeline = gst_pipeline_new("gnomebaker-convert-to-wav-pipeline");
    g_assert(media_pipeline->pipeline);
    gst_bus_add_watch (gst_pipeline_get_bus (GST_PIPELINE (media_pipeline->pipeline)), (GstBusFunc)gstreamer_bus_callback, cmd);

    /* start with the source */
    media_pipeline->source = gst_element_factory_make("filesrc", "file-source");
    g_assert(media_pipeline->source);
    g_object_set(G_OBJECT(media_pipeline->source), "location", g_ptr_array_index(cmd->args, 0), NULL);

    /* use a decode bin so we don't have to particularly care what the input format is */
    media_pipeline->decoder = gst_element_factory_make("decodebin", "decoder");
    g_assert(media_pipeline->decoder);
    g_signal_connect(media_pipeline->decoder, "new-decoded-pad", G_CALLBACK (gstreamer_new_decoded_pad), media_pipeline);

    /* create an audio converter */
    media_pipeline->converter = gst_element_factory_make("audioconvert", "converter");
    g_assert(media_pipeline->converter);

    /* audioscale resamples audio */
    media_pipeline->scale = gst_element_factory_make("audioresample", "scale");
    g_assert(media_pipeline->scale);

    /* Another audio converter. It is needed on big endian machines, otherwise
       the audioscale can't be connected to the wavenc due to incompatible endianness. */
    media_pipeline->endian_converter = gst_element_factory_make ("audioconvert", "endian-converter");
    g_assert(media_pipeline->endian_converter);

    /* and an wav encoder */
    media_pipeline->encoder = gst_element_factory_make("wavenc", "encoder");
    g_assert(media_pipeline->encoder);

    /* finally the output filesink or file descriptor dink */
    if(pipe != NULL)
    {
         media_pipeline->dest = gst_element_factory_make("fdsink", "file-out");
         g_assert(media_pipeline->dest);
         g_object_set(G_OBJECT(media_pipeline->dest), "fd", pipe[1], NULL);
    }
    else
    {
        media_pipeline->dest = gst_element_factory_make("filesink","file-out");
        g_assert(media_pipeline->dest);
        g_object_set(G_OBJECT(media_pipeline->dest), "location", g_ptr_array_index(cmd->args, 1), NULL);
    }

    /* add all our elements to the bin */
    gst_bin_add_many(GST_BIN(media_pipeline->pipeline), media_pipeline->source, media_pipeline->decoder,
            media_pipeline->converter, media_pipeline->scale, media_pipeline->endian_converter,
            media_pipeline->encoder, media_pipeline->dest, NULL);

    /* Now link all of our elements up */
    gst_element_link(media_pipeline->source, media_pipeline->decoder);

    /* don't make a link here between the decoder and the convertor
     * as we will get that in new-decoded-pad signal */

    gst_element_link_many(media_pipeline->converter, media_pipeline->scale, media_pipeline->endian_converter, NULL);
    GstCaps *filter_caps = gst_caps_new_simple("audio/x-raw-int",
            "channels", G_TYPE_INT, 2, "rate", G_TYPE_INT, 44100,
            "width", G_TYPE_INT, 16, "depth", G_TYPE_INT, 16, NULL);
    g_assert(filter_caps);
    gst_element_link_filtered(media_pipeline->endian_converter, media_pipeline->encoder, filter_caps);
    gst_caps_unref(filter_caps);
    gst_element_link(media_pipeline->encoder, media_pipeline->dest);

    /* If we're not writing to someone's pipe we update the progress bar in a timeout */
    if(pipe == NULL)
        gstreamer_timer = g_timeout_add(1000, gstreamer_progress_timer, media_pipeline);

    gst_element_set_state(media_pipeline->pipeline, GST_STATE_PLAYING);
    while(exec_cmd_get_state(cmd) == RUNNING)
    {
        /* Keep the GUI responsive by pumping all of the events if we're not writing
         * to a pipe (as someone else will be pumping the event loop) */
        while(pipe == NULL && gtk_events_pending())
            gtk_main_iteration();
    }

    if(pipe == NULL)
        g_source_remove(gstreamer_timer);
    gst_element_set_state(media_pipeline->pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(media_pipeline->pipeline));
    g_free(media_pipeline);
#else

    MediaPipeline *media_pipeline = g_new0(MediaPipeline, 1);

    /* create a new pipeline to hold the elements */
    media_pipeline->pipeline = gst_pipeline_new("gnomebaker-convert-to-wav-pipeline");
    media_pipeline->source = gst_element_factory_make("filesrc", "file-source");
    g_object_set(G_OBJECT(media_pipeline->source), "location", g_ptr_array_index(cmd->args, 0), NULL);

    /* decoder */
    media_pipeline->decoder = gst_element_factory_make("decodebin", "decoder");
    g_signal_connect(media_pipeline->decoder, "new-decoded-pad", G_CALLBACK (gstreamer_new_decoded_pad), media_pipeline);

    /* create an audio converter */
    media_pipeline->converter = gst_element_factory_make("audioconvert", "converter");

    /* audioscale resamples audio */
    media_pipeline->scale = gst_element_factory_make("audioscale", "scale");

    /* Another audio converter. It is needed on big endian machines, otherwise
       the audioscale can't be connected to the wavenc due to incompatible endianness. */
    media_pipeline->endian_converter = gst_element_factory_make ("audioconvert", "endianconverter");
    gst_element_link(media_pipeline->scale, media_pipeline->endian_converter);

    GstCaps *filter_caps = gst_caps_new_simple("audio/x-raw-int",
            "channels", G_TYPE_INT, 2, "rate", G_TYPE_INT, 44100,
            "width", G_TYPE_INT, 16, "depth", G_TYPE_INT, 16, NULL);
    /* and an wav encoder */
    media_pipeline->encoder = gst_element_factory_make("wavenc", "encoder");
    gst_element_link_filtered(media_pipeline->endian_converter, media_pipeline->encoder, filter_caps);
    gst_caps_free(filter_caps);

    /* finally the output filesink */
    if(pipe != NULL)
    {
         media_pipeline->dest = gst_element_factory_make("fdsink", "file-out");
         g_object_set(G_OBJECT(media_pipeline->dest), "fd", pipe[1], NULL);
    }
    else
    {
        media_pipeline->dest = gst_element_factory_make("filesink","file-out");
        g_object_set(G_OBJECT(media_pipeline->dest), "location", g_ptr_array_index(cmd->args, 1), NULL);
    }

    gst_bin_add_many(GST_BIN(media_pipeline->pipeline), media_pipeline->source, media_pipeline->decoder, NULL);

    gst_element_link(media_pipeline->converter, media_pipeline->scale);
    gst_element_link(media_pipeline->encoder, media_pipeline->dest);
    gst_element_link(media_pipeline->source, media_pipeline->decoder);

    g_signal_connect(media_pipeline->pipeline, "error", G_CALLBACK(gstreamer_pipeline_error), cmd);
    g_signal_connect(media_pipeline->pipeline, "eos", G_CALLBACK(gstreamer_pipeline_eos), cmd);

    /* If we're not writing to someone's pipe we update the progress bar */
    if(pipe == NULL)
        gstreamer_timer = g_timeout_add(1000, gstreamer_progress_timer, media_pipeline->dest);

    gst_element_set_state(media_pipeline->pipeline, GST_STATE_PLAYING);
    while(gst_bin_iterate(GST_BIN(media_pipeline->pipeline)) && (exec_cmd_get_state(cmd) == RUNNING))
    {
        /* Keep the GUI responsive by pumping all of the events if we're not writing
         * to a pipe (as someone else will be pumping the event loop) */
        while(pipe == NULL && gtk_events_pending())
            gtk_main_iteration();
    }

    if(pipe == NULL)
        g_source_remove(gstreamer_timer);
    gst_element_set_state(media_pipeline->pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(media_pipeline->pipeline));
    g_free(media_pipeline);

#endif
}


void
gstreamer_add_args(ExecCmd *cmd, const gchar *from, const gchar *to)
{
    GB_LOG_FUNC
    g_return_if_fail(cmd != NULL);
    g_return_if_fail(from != NULL);
    g_return_if_fail(to != NULL);

    exec_cmd_add_arg(cmd, from);
    exec_cmd_add_arg(cmd, to);

    cmd->lib_proc = gstreamer_lib_proc;
    cmd->pre_proc = gstreamer_pre_proc;
}


/*******************************************************************************
 * MD5 checking
 ******************************************************************************/
static void
md5sum_pre_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    progressdlg_set_status(_("Verifying the disk content"));
    progressdlg_increment_exec_number();
    progressdlg_pulse_start();
}


static void
md5sum_post_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    progressdlg_pulse_stop();
    if(preferences_get_bool(GB_EJECT))
        devices_eject_disk(GB_WRITER);
}


void
md5sum_add_args(ExecCmd *cmd, const gchar *md5)
{
    GB_LOG_FUNC
    g_return_if_fail(cmd != NULL);
    g_return_if_fail(md5 != NULL);

    exec_cmd_add_arg(cmd, "md5sum");
    gchar *writer = devices_get_device_config(GB_WRITER,GB_DEVICE_NODE_LABEL);
    exec_cmd_add_arg(cmd, writer);
    g_free(writer);
    exec_cmd_add_arg(cmd, md5);

    cmd->pre_proc = md5sum_pre_proc;
    cmd->post_proc = md5sum_post_proc;
}


/*******************************************************************************
 * dd
 ******************************************************************************/
static void
dd_pre_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC

    progressdlg_set_status(_("Reading DVD image"));
    progressdlg_increment_exec_number();

    gint response = GTK_RESPONSE_NO;
    gchar *file = preferences_get_copy_dvd_image();
    if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
    {
        response = gnomebaker_show_msg_dlg(progressdlg_get_window(), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
                _("A DVD image from a previous session already exists on disk, "
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
dd_read_proc(void *ex, void *buffer)
{
    GB_LOG_FUNC
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(ex != NULL);

    gchar *text = (gchar*)buffer;
    progressdlg_append_output(text);
}


/*
 *  Populates the information required to make an iso from an existing data DVD
 */
void
dd_add_copy_args(ExecCmd *e, const gchar *iso)
{
    GB_LOG_FUNC
    g_return_if_fail(e != NULL);
    g_return_if_fail(iso != NULL);

    exec_cmd_add_arg(e, "dd");
    exec_cmd_add_arg(e, "bs=4096");

    gchar *reader = devices_get_device_config(GB_READER, GB_DEVICE_ID_LABEL);
    exec_cmd_add_arg(e, "if=%s", reader);
    g_free(reader);

    exec_cmd_add_arg(e, "of=%s", iso);

    if(!preferences_get_bool(GB_CREATEISOONLY))
       e->post_proc = execfunctions_prompt_for_disk_post_proc;

    e->pre_proc = dd_pre_proc;
    e->read_proc = dd_read_proc;
}


