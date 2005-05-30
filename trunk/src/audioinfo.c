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
 * File: audioinfo.h
 * Copyright: Luke - MP3 code ported to C from CMP3Info (written by Gustav Munkby)
 * Created on: Thu Sep 16 20:16:51 2004
 */

#include "audioinfo.h"
#include "gbcommon.h"
#include <stdio.h>
#include <vorbis/vorbisfile.h>
#include "media.h"

void audioinfo_get_wav_info(AudioInfo* self, const gchar* wavfile);
void audioinfo_get_ogg_info(AudioInfo* info, const gchar *oggfile);
void audioinfo_get_mp3_info(AudioInfo* info, const gchar *mp3file);
void audioinfo_set_formatted_length(AudioInfo* info); 


AudioInfo* 
audioinfo_new(const gchar* audiofile)
{
	GB_LOG_FUNC
	
	AudioInfo* self = g_new0(AudioInfo, 1);
	if(NULL != self)
	{
		if(!audioinfo_init(self))
		{
			g_free(self);
			self = NULL;
		}
		else
		{			
			self->mimetype = gbcommon_get_mime_type(audiofile);
			GB_TRACE("audioinfo_new - mime type is %s for %s", self->mimetype, audiofile);
			if(self->mimetype != NULL)
			{
				/* Check that the file extension is one we support */			
				if(g_ascii_strcasecmp(self->mimetype, "audio/x-wav") == 0)
					audioinfo_get_wav_info(self, audiofile);				
				else if((g_ascii_strcasecmp(self->mimetype, "audio/x-mp3") == 0) ||  
					(g_ascii_strcasecmp(self->mimetype, "audio/mpeg") == 0))
					audioinfo_get_mp3_info(self, audiofile);
				else if(g_ascii_strcasecmp(self->mimetype, "application/ogg") == 0)
					audioinfo_get_ogg_info(self, audiofile);
				else if((g_ascii_strcasecmp(self->mimetype, "audio/x-flac") == 0) ||  
					(g_ascii_strcasecmp(self->mimetype, "application/x-flac") == 0))
                    self->duration = media_calculate_track_length(audiofile);
                else
				{
					audioinfo_delete(self);			
					self = NULL;
				}
				
				if(self != NULL)
				{
                    /* This is a bit of a cludge. Add 2 seconds to the duration 
					to factor in cdrecord -pad times when calculating the size of the cd */
					self->duration += 2;
					audioinfo_set_formatted_length(self);
				}
			}
		}
	}
	return self;
}


void 
audioinfo_delete(AudioInfo* self)
{
	GB_LOG_FUNC
	g_return_if_fail(NULL != self);
	
	audioinfo_end(self);
	g_free(self);
}


gboolean 
audioinfo_init(AudioInfo* self)
{
	GB_LOG_FUNC
	g_return_val_if_fail(NULL != self, FALSE);

	self->mimetype = NULL;
	self->artist = g_string_new("");
	self->album = g_string_new("");
	self->title = g_string_new("");
	self->duration = 0;
	self->filesize = 0;
	self->bitrate = 0;
	self->formattedduration = g_string_new("");
	
	return TRUE;
}


void 
audioinfo_end(AudioInfo* self)
{
	GB_LOG_FUNC
	g_return_if_fail(NULL != self);
	
	g_free(self->mimetype);
	g_string_free(self->artist, TRUE);
	g_string_free(self->album, TRUE);
	g_string_free(self->title, TRUE);
	self->duration = 0;
	self->filesize = 0;
	self->bitrate = 0;
	g_string_free(self->formattedduration, TRUE);
}


void 
audioinfo_get_wav_info(AudioInfo* self, const gchar* wavfile)
{
	GB_LOG_FUNC
	g_return_if_fail(NULL != self);
	g_return_if_fail(NULL != wavfile);
	
	FILE* st = fopen(wavfile, "rb");
	if (st == NULL)
	{
		printf(_("File %s was not opened \n"), wavfile);
	}
	else
	{		
		fseek(st, 0, SEEK_END);
        self->filesize = ftell(st);
		fclose(st);
        
        self->duration = media_calculate_track_length(wavfile);
	
		/* Have a look for a .inf file corresponding to the wavfile
		   so we can display the artist, album and track */
		gchar* inffile = g_strdup(wavfile);
		gchar* extension = strstr(inffile, ".wav");
		if(extension != NULL)
		{
			strcpy(extension, ".inf");
			gchar* infcontents = NULL;
			gsize length = 0;
			
			/* read in the entire inf file */
			if(g_file_get_contents(inffile, &infcontents, &length, NULL))
			{
				const gint numlabels = 3;
				const gchar* labels[3] = 
					{ N_ ("Performer"),N_ ("Albumtitle"),N_ ("Tracktitle") };
				
				GString* copyto[3] = 
					{ self->artist, self->album, self->title };					
					
				gint i = 0;
				for(; i < numlabels; i++)
				{
					const gchar* label = strstr(infcontents, labels[i]);
					if(label != NULL)
					{
						const gchar* ptr = label;
						gboolean reading = FALSE;
						while(*ptr != '\n')
						{
							if(*ptr == '\'')
							{
								reading = !reading;							
								if(reading)
									ptr++;
								else
									break;
							}
							
							if(reading)
								g_string_append_c(copyto[i], *ptr);
							
							ptr++;
						}
					}				
				}
			}		

			g_free(infcontents);			
		}
		
		g_free(inffile);
	}	
}


void 
audioinfo_get_mp3_info(AudioInfo* info, const gchar *FileName)
{
	GB_LOG_FUNC
	FILE* st = fopen(FileName, "rb");
	if (st == NULL)
	{
		g_warning(_("File %s was not opened"), FileName);
	}
	else
	{		
		/* Set the file size */
		fseek(st, 0, SEEK_END);
		info->filesize = ftell(st);
        info->duration = media_calculate_track_length(FileName);
        
        /* get tag from the last 128 guchars in an .mp3-file    */
        gchar tagchars[128];

        /* get last 128 guchars */
        fseek(st, info->filesize - 128, SEEK_SET);
        fread(tagchars, 1, 128, st);
        /*tagchars[127] = '\0';*/
        
        if(g_ascii_strncasecmp(tagchars, "TAG", 3) == 0)
        {
            GB_TRACE("%ld [%s] [%s] [%s]", info->filesize - 128, 
                tagchars + 3, tagchars + 33, tagchars + 63);
                
            g_string_append_len(info->artist, tagchars + 33, 30);
            g_strstrip(info->artist->str);
            g_string_append_len(info->album, tagchars + 63, 30);
            g_strstrip(info->album->str);
            g_string_append_len(info->title, tagchars + 3, 30);
            g_strstrip(info->title->str);
        }

		fclose(st);
	}
}


void 
audioinfo_set_formatted_length(AudioInfo* info) 
{
	GB_LOG_FUNC
	gint seconds = info->duration % 60;
	gint minutes = (info->duration-seconds) / 60;
	
	g_string_printf(info->formattedduration, "%d:%.2d", minutes, seconds);
}


void 
audioinfo_get_ogg_info(AudioInfo* info, const gchar *oggfile)
{
	GB_LOG_FUNC
	g_return_if_fail(NULL != info);
	g_return_if_fail(NULL != oggfile);
	OggVorbis_File file;
	
	FILE* st = fopen(oggfile, "rb");
	if (st == NULL)
	{
		printf(_("File %s was not opened \n"), oggfile);
	}
	else if(ov_open(st, &file, NULL, 0) == 0)
	{
		info->duration = (gulong)ov_time_total(&file, -1);
		info->bitrate = ov_bitrate(&file, -1);
		fseek(st, 0, SEEK_END);
		info->filesize = ftell(st);
		
		vorbis_comment* comment = ov_comment(&file, -1);				
		
		gint i = 0;
		for(; i < comment->comments; i++)
		{
			GB_TRACE("[%s]", comment->user_comments[i]);				
			if(comment->user_comments[i] == NULL)
				continue;			
			else if(g_ascii_strncasecmp(comment->user_comments[i], _("TITLE="), 6) == 0)
				g_string_append(info->title, comment->user_comments[i] + 6);
			else if(g_ascii_strncasecmp(comment->user_comments[i], _("ARTIST="), 7) == 0)
				g_string_append(info->artist, comment->user_comments[i] + 7);
			else if(g_ascii_strncasecmp(comment->user_comments[i], _("ALBUM="), 6) == 0)
				g_string_append(info->album, comment->user_comments[i] + 6);
		}
		
		ov_clear(&file);		
	}
	else
	{
		/* no fclose if ov_open works as vorbis takes ownership. */		
		fclose(st);
	}
}


/* WIP

void 
audioinfo_get_mp3_info(AudioInfo* info, const gchar* mp3file)
{
	GB_LOG_FUNC
	g_return_if_fail(NULL != info);
	g_return_if_fail(NULL != mp3file);
		
	const struct id3_tag* id3tag = NULL;
	struct id3_file* id3file = id3_file_open(mp3file, ID3_FILE_MODE_READONLY);
	if(id3file == NULL)
	{
		g_critical("Failed to open mp3 file [%s]", mp3file);
	}
	else if((id3tag = id3_file_tag(id3file)) == NULL)
	{
		g_critical("Failed to get id3 file tag for mp3 file [%s]", mp3file);
	}
	else
	{
		audioinfo_mp3_get_id3_value(id3tag, ID3_FRAME_ARTIST, info->artist);
		audioinfo_mp3_get_id3_value(id3tag, ID3_FRAME_ALBUM, info->album);
		audioinfo_mp3_get_id3_value(id3tag, ID3_FRAME_TITLE, info->title);		
		
		GString* length = g_string_new("");
		audioinfo_mp3_get_id3_value(id3tag, "TLEN", length);
		if(length->len == 0)
			audioinfo_mp3_get_id3_value(id3tag, "TLE", length);
		g_string_free(length, TRUE);
	}
	
	id3_file_close(id3file);
}


void 
audioinfo_mp3_get_id3_value(const struct id3_tag* id3tag, const gchar* id3item, GString* value)
{
	GB_LOG_FUNC
	g_return_if_fail(id3tag != NULL);
	g_return_if_fail(id3item != NULL);
	g_return_if_fail(value != NULL);
	
	const struct id3_frame* id3frame = id3_tag_findframe(id3tag, id3item, 0);
	const union id3_field* framefield = NULL;
	const id3_ucs4_t* fieldvalue = NULL;				
	
	if(id3frame == NULL)
		g_warning("Failed to get id3_frame [%s]", id3item);
	else if((framefield = id3_frame_field(id3frame, 1)) == NULL)
		g_warning("Failed to get id3_field [%s]", id3item);
	else if((fieldvalue = id3_field_getstrings(framefield, 0)) == NULL)
		g_warning("Failed to get id3_field strings [%s]", id3item);
	else	
	{
		gchar* tag = id3_ucs4_utf8duplicate(fieldvalue);
		g_string_append(value, tag);
		g_free(tag);
	}
}

*/
