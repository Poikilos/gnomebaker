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
 * Created by: luke_biddell@yahoo.com
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



gint cdrecord_totaltrackstowrite = 1;
gint cdrecord_firsttrack = -1;

gint cdda2wav_totaltracks = -1;
gint cdda2wav_totaltracksread = 0;

gint readcd_totalguchars = -1;


/*******************************************************************************
 * CDRECORD
 ******************************************************************************/

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
			  _("Please insert the CD into the CD writer"));
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
				if(cdrecord_firsttrack == -1)
					cdrecord_firsttrack = currenttrack;			

				/* Figure out how many tracks we have written so far and calc the fraction */
				gfloat totalfraction = 
					((gfloat)currenttrack - cdrecord_firsttrack) * (1.0 / cdrecord_totaltrackstowrite);
				
				/* now add on the fraction of the track we are currently writing */
				totalfraction += ((current / total) * (1.0 / cdrecord_totaltrackstowrite));
				
				/*g_message("^^^^^ current [%d] first [%d] current [%f] total [%f] fraction [%f]",
					currenttrack, cdrecord_firsttrack, current, total, totalfraction);*/
				
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
	
	cdrecord_totaltrackstowrite = 1;
	cdrecord_firsttrack = -1;
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
			cdrecord_totaltrackstowrite++;
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
		cdrecord_totaltrackstowrite = 0;
		/* loop around reading the files in the directory */
		const gchar *name = g_dir_read_name(dir);	
		while(name != NULL)
		{
			if(g_str_has_suffix(name, ".wav"))
			{
				g_message( _("adding [%s]"), name);
				gchar* fullpath = g_build_filename(tmp, name, NULL);
				exec_cmd_add_arg(cdBurn, "%s", fullpath);
				cdrecord_totaltrackstowrite++;
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


/*******************************************************************************
 * CDDA2WAV
 ******************************************************************************/


void
cdda2wav_pre_proc(void *ex, void *buffer)
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
		response = gnomebaker_show_msg_dlg(GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_NONE,
			_("Audio tracks from a previous sesion already exist on disk, "
			"do you wish to use the existing tracks?"));
		gdk_flush();
		gdk_threads_leave();
	}
	
	g_free(file);
	
	if(response == GTK_RESPONSE_NO)
	{		
		gchar* cmd = g_strdup_printf("rm -fr %s/gbtrack*", tmp);
		system(cmd);
		g_free(cmd);
		
		gdk_threads_enter();
		response = gnomebaker_show_msg_dlg(GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_NONE,
			_("Please insert the audio CD into the CD reader"));				
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
	
	g_free(tmp);
}


void
cdda2wav_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	g_return_if_fail(buffer != NULL);
	
	gchar* text = (gchar*)buffer;
	
	/*g_message( "cdda2wav_read_proc - read [%s]", text);*/
	
	if(cdda2wav_totaltracks == -1)
	{
		const gchar* tracksstart = strstr(text, _("Tracks:"));		
		if(tracksstart != NULL)
		{
			gchar tmpbuf[4];
			memset(tmpbuf, 0x0, 4);
			g_strlcpy(tmpbuf, tracksstart + 7, 3);
			g_strstrip(tmpbuf);
			cdda2wav_totaltracks = atoi(tmpbuf);			
			g_message(_("cdda2wav_read_proc - total tracks %d"), cdda2wav_totaltracks);
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
			memset(tmpbuf, 0x0, bufsize * sizeof(gchar));
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
			
			/*g_message("cdda2wav_read_proc - track fraction %f", fraction);*/
			
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
cdda2wav_add_copy_args(ExecCmd * e)
{
	GB_LOG_FUNC
	g_return_if_fail(e != NULL);
	
	exec_cmd_add_arg(e, "%s", "cdda2wav");
	exec_cmd_add_arg(e, "%s", "-x");
	exec_cmd_add_arg(e, "%s", "cddb=1");
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


void
mkisofs_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	
	progressdlg_set_status(_("<b>Creating data disk image...</b>"));
	progressdlg_increment_exec_number();
	
	gchar* file = preferences_get_create_data_cd_image();	
	if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
	{
		gdk_threads_enter();
		gint ret = gnomebaker_show_msg_dlg(GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_BUTTONS_OK,
			_("A data CD image from a previous session already exists on disk, "
			"do you wish to use the existing image?"));
		gdk_flush();
		gdk_threads_leave();
		
		ExecCmd* e = (ExecCmd*)ex;
		
		if(ret  == GTK_RESPONSE_YES)		
			e->state = SKIP;
		else if(ret == GTK_RESPONSE_CANCEL)
			e->state = CANCELLED;
	}
	
	g_free(file);
}


void
mkisofs_read_proc(void *ex, void *buffer)
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
		memset(pct, 0x0, (percent - ptr + 1) * sizeof(gchar));
		strncpy(pct, ptr, percent - ptr);
		
		progressdlg_set_fraction(atof(pct)/100.0);
		g_free(pct);
	}
	
	progressdlg_append_output(buffer);
}


gboolean
mkisofs_foreach_func(GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      user_data)
{
	GB_LOG_FUNC
	gchar *file = NULL, *filepath = NULL;
	gboolean existingsession = FALSE;
		
	gtk_tree_model_get (model, iter, DATACD_COL_FILE, &file,
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
mkisofs_add_args(ExecCmd* e, GtkTreeModel* datamodel, const gchar* iso)
{
	GB_LOG_FUNC
	g_return_val_if_fail(e != NULL, FALSE);
	g_return_val_if_fail(datamodel != NULL, FALSE);
	g_return_val_if_fail(iso != NULL, FALSE);
	
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
			exec_cmd_add_arg(e, "-V \"%s\"", volume);			
			exec_cmd_add_arg(e, "-p \"%s\"", createdby);
		}
				
		exec_cmd_add_arg(e, "%s", "-r");
		/*exec_cmd_add_arg(e, "%s", "-f"); don't follow links */
		exec_cmd_add_arg(e, "%s", "-J");
		/*exec_cmd_add_arg(e, "%s", "-hfs");*/
		exec_cmd_add_arg(e, "%s", "-gui");
		exec_cmd_add_arg(e, "%s", "-joliet-long");
				
		if(msinfo != NULL)
		{
			exec_cmd_add_arg(e, "-C %s", msinfo);
			
			gchar* writer = devices_get_device_config(GB_WRITER, GB_DEVICE_ID_LABEL);
			exec_cmd_add_arg(e, "-M%s", writer);
			g_free(writer);			
			
			/* This is a cludge so that we don't ask the user if they want to use the existing iso */
			gchar* createdataiso = preferences_get_create_data_cd_image();
			gchar* cmd = g_strdup_printf("rm -fr %s", createdataiso);
			system(cmd);			
			g_free(cmd);
			g_free(createdataiso);
		}		
		
		exec_cmd_add_arg(e, "-o%s", iso);
		exec_cmd_add_arg(e, "%s", "-graft-points");
		gtk_tree_model_foreach(datamodel, mkisofs_foreach_func, e);	
				
		e->preProc = mkisofs_pre_proc;
		e->readProc = mkisofs_read_proc;
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
		if(!preferences_get_bool(GB_FAST_FORMAT))
			exec_cmd_add_arg(dvdFormat, "%s","-force=full");
		else
			exec_cmd_add_arg(dvdFormat, "%s","-force");
	}
	else
	{
		if(!preferences_get_bool(GB_FAST_FORMAT))
			exec_cmd_add_arg(dvdFormat, "%s", "-format=full");
	}	
}


/*******************************************************************************
 * GROWISOFS
 ******************************************************************************/


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

/*
gboolean
growisofs_foreach_func(GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      user_data)
{
	GB_LOG_FUNC
	gchar *file = NULL, *filepath = NULL;
	gboolean existingsession = FALSE;
		
	gtk_tree_model_get (model, iter, DATACD_COL_FILE, &file,
		DATACD_COL_PATH, &filepath, DATACD_COL_SESSION, &existingsession, -1);
	
	if(!existingsession)
	{
		gchar* buffer = g_strdup_printf("%s", filepath);		
		exec_cmd_add_arg((ExecCmd*)user_data, "%s", buffer);
		g_free(buffer);
	}	
	
	g_free(file);	
	g_free(filepath);
	
	return FALSE;
}
*/

void 
growisofs_add_args(ExecCmd * const growisofs,GtkTreeModel* datamodel)
{
	GB_LOG_FUNC
	g_return_if_fail(growisofs != NULL);
	
	growisofs->readProc = growisofs_read_proc;
	growisofs->preProc = growisofs_pre_proc;
	/*growisofs->postProc = growisofs_post_proc;*/
	exec_cmd_add_arg(growisofs, "%s", "growisofs");
	
	/* merge new session with existing one */
	gchar* writer = devices_get_device_config(GB_WRITER,GB_DEVICE_ID_LABEL);
	gchar* msinfo = (gchar*)g_object_get_data(G_OBJECT(datamodel), DATACD_EXISTING_SESSION);
	if(msinfo != NULL)
		exec_cmd_add_arg(growisofs, "-M%s", writer);
	else
		exec_cmd_add_arg(growisofs, "-Z%s", writer);
	/* We don't own the msinfo gchar datacd does 
	g_free(msinfo);*/
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
	/* we should not store FINALIZE etc in gconf */
	if(preferences_get_bool(GB_FINALIZE))
		exec_cmd_add_arg(growisofs, "%s", "-dvd-compat");
	/* -dvd-compat closes the session on DVD+RW's also */	
	preferences_set_bool(GB_FINALIZE,FALSE);
	gtk_tree_model_foreach(datamodel, mkisofs_foreach_func, growisofs);
}


/*******************************************************************************
 * MPG123
 ******************************************************************************/


void
mpg123_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	progressdlg_set_status(_("<b>Converting mp3 to cd audio...</b>"));
	progressdlg_increment_exec_number();
}


void
mpg123_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);
/*	
	const gchar* frame = strstr(buffer, "Frame#");
	if(frame != NULL)
	{
		guint current, total;
		if(sscanf(frame, "%*s\t%d/%d", &current, &total) > 0)
		{
			g_message("track [%d] [%d]", current, total);		
			progressdlg_set_fraction((gfloat)current/(gfloat)total);
		}
	}
	else
	{
		progressdlg_append_output(buffer);
	}
*/
	const gchar* frame = strstr(buffer, "Frame#");
	if(frame != NULL)
	{
		guint current, remaining;		
		if(sscanf(frame, "%*s\t%d [%d]", &current, &remaining) > 0)
			progressdlg_set_fraction((gfloat)current/(gfloat)(current + remaining));
	}
	else
	{
		progressdlg_append_output(buffer);
	}	
}


void 
mpg123_add_mp3_args(ExecCmd* cmd, gchar* file, gchar** convertedfile)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);	
	g_return_if_fail(file != NULL);	
	
	/*exec_cmd_add_arg(cmd, "%s", "lame");
	exec_cmd_add_arg(cmd, "%s", "--decode");
	exec_cmd_add_arg(cmd, "%s", "--resample 44.1");
	exec_cmd_add_arg(cmd, "%s", "-ms");
	exec_cmd_add_arg(cmd, "%s", "--brief");	
	exec_cmd_add_arg(cmd, "%s", file);*/
	
	//mpg123 -v --rate 44100 --stereo -w honda.wav HondaServiceCall.mp3
	
	exec_cmd_add_arg(cmd, "%s", "mpg123");
	exec_cmd_add_arg(cmd, "%s", "-v");
	exec_cmd_add_arg(cmd, "%s", "--resync");
	exec_cmd_add_arg(cmd, "%s", "-r44100");
	exec_cmd_add_arg(cmd, "%s", "--stereo");
	
	
	gchar* trackdir = preferences_get_convert_audio_track_dir();
	
	gchar* filename = g_path_get_basename(file);
	if(filename != NULL)
	{
		*convertedfile = g_build_filename(trackdir, filename, NULL);
		gchar* suffix = strstr(*convertedfile, ".mp3");
		if(suffix != NULL)
			strncpy(suffix, ".wav", 4);
				
		exec_cmd_add_arg(cmd, "-w%s", *convertedfile);
		
		g_message(_("Converted file is [%s]"), *convertedfile);
		g_free(filename);
		
		exec_cmd_add_arg(cmd, "%s", file);
	}
	
	g_free(trackdir);		
	
	cmd->preProc = mpg123_pre_proc;
	cmd->readProc = mpg123_read_proc;
}


/*******************************************************************************
 * OGGDEC
 ******************************************************************************/


void
oggdec_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	progressdlg_set_status(_("<b>Converting ogg to cd audio...</b>"));
	progressdlg_increment_exec_number();
}


void
oggdec_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);
	
/*	
	const gchar* frame = strstr(buffer, "Time:");
	if(frame != NULL)
	{
		guint currentsecs, currentmins, totalmins, totalsecs;
		if(sscanf(frame, "Time: %d:%d.%*d [%*d:%*d.%*d] of %d:%d.%*d", 
				&currentmins, &currentsecs, &totalmins, &totalsecs) > 0)
		{
			g_message("track [%d] [%d] [%d] [%d]", currentmins, currentsecs, totalmins, totalsecs);		
			progressdlg_set_fraction(
				(gfloat)((currentmins * 60) + currentsecs) / 
				(gfloat)((totalmins * 60) + totalsecs));
		}
	}
*/
	const gchar* frame = strstr(buffer, "[");
	if(frame != NULL)
	{
		gfloat percentage;
		if(sscanf(frame, "[%f%%]", &percentage) > 0)
			progressdlg_set_fraction(percentage/100.0);
	}
	else
	{
		progressdlg_append_output(buffer);
	}
}


void 
oggdec_add_args(ExecCmd* cmd, gchar* file, gchar** convertedfile)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);	
	g_return_if_fail(file != NULL);	

/*	
	exec_cmd_add_arg(cmd, "%s", "oggdec");
	exec_cmd_add_arg(cmd, "%s", file);
	exec_cmd_add_arg(cmd, "%s", "-dwav");	
	
	gchar* trackdir = preferences_get_convert_audio_track_dir();
	
	gchar* filename = strrchr(file, '/');
	if(filename != NULL)
	{
		++filename;
		*convertedfile = g_build_filename(trackdir, filename, NULL);
		gchar* suffix = strstr(*convertedfile, ".ogg");
		if(suffix != NULL)
			strncpy(suffix, ".wav", 4);
		
		exec_cmd_add_arg(cmd, "-f%s", *convertedfile);
		
		g_message("Converted file is [%s]", *convertedfile);
	}
	
	g_free(trackdir);		
*/	

	exec_cmd_add_arg(cmd, "%s", "oggdec");
	exec_cmd_add_arg(cmd, "%s", "-b16");
	exec_cmd_add_arg(cmd, "%s", file);
	
	gchar* trackdir = preferences_get_convert_audio_track_dir();
	
	gchar* filename = g_path_get_basename(file);
	if(filename != NULL)
	{
		*convertedfile = g_build_filename(trackdir, filename, NULL);
		gchar* suffix = strstr(*convertedfile, ".ogg");
		if(suffix != NULL)
			strncpy(suffix, ".wav", 4);
		
		exec_cmd_add_arg(cmd, "-o%s", *convertedfile);
		
		g_message(_("Converted file is [%s]"), *convertedfile);
		g_free(filename);
	}
	
	g_free(trackdir);		
	cmd->preProc = oggdec_pre_proc;
	cmd->readProc = oggdec_read_proc;	
}


/*******************************************************************************
 * READCD
 ******************************************************************************/


void
readcd_pre_proc(void *ex, void *buffer)
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
	
	if(readcd_totalguchars == -1)		
	{
		gchar* end = strstr(text, "end:");	
		if(end != NULL)
		{			
			if(sscanf(end, "%*s %d", &readcd_totalguchars) > 0)
				g_message("readcd size is %d", readcd_totalguchars);
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
				/*g_message( "read %d, total %d", readguchars, readcd_totalguchars);*/
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
	/*exec_cmd_add_arg(e, "%s", "-notrunc");
	exec_cmd_add_arg(e, "%s", "-clone");
	exec_cmd_add_arg(e, "%s", "-silent");*/

	e->preProc = readcd_pre_proc;	
	e->readProc = readcd_read_proc;
	e->postProc = readcd_post_proc;
}


void
sox_pre_proc(void *ex, void *buffer)
{	
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	progressdlg_set_status(_("<b>Converting wav to cd audio...</b>"));
	progressdlg_increment_exec_number();
}


/*******************************************************************************
 * SOX
 ******************************************************************************/


void
sox_read_proc(void *ex, void *buffer)
{
	GB_LOG_FUNC
	
	g_return_if_fail(ex != NULL);
	g_return_if_fail(buffer != NULL);
	
	progressdlg_append_output(buffer);
}


void 
sox_add_wav_args(ExecCmd* cmd, gchar* file, gchar** convertedfile)
{
	GB_LOG_FUNC
	g_return_if_fail(cmd != NULL);	
	g_return_if_fail(file != NULL);	
	
	exec_cmd_add_arg(cmd, "%s", "sox");
	exec_cmd_add_arg(cmd, "%s", "-V");	
	exec_cmd_add_arg(cmd, "%s", file);
	exec_cmd_add_arg(cmd, "%s", "-r 44100");
	exec_cmd_add_arg(cmd, "%s", "-c 2");
	exec_cmd_add_arg(cmd, "%s", "-w");
	
	gchar* trackdir = preferences_get_convert_audio_track_dir();
	
	gchar* filename = g_path_get_basename(file);
	if(filename != NULL)
	{
		*convertedfile = g_build_filename(trackdir, filename, NULL);
		
		exec_cmd_add_arg(cmd, "%s", *convertedfile);
		
		g_message("Converted file is [%s]", *convertedfile);
		g_free(filename);
	}
	
	g_free(trackdir);		
	
	cmd->preProc = sox_pre_proc;
	cmd->readProc = sox_read_proc;
}
