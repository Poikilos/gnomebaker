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
 * Created by: Luke - MP3 code ported to C from CMP3Info (written by Gustav Munkby)
 * Created on: Thu Sep 16 20:16:51 2004
 */

#include "audioinfo.h"
#include "gbcommon.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <vorbis/vorbisfile.h>

/* variables for storing the information about the MP3 */
gint intBitRate = 0;
glong lngFileSize = 0;
gint intFrequency = 0;
gint intLength = 0;

/* variables used in the process of reading in the MP3 files */
gulong bithdr = 0;
gint boolVBitRate = 0;
gint intVFrames = 0;


typedef struct WavHeader
{
	gchar title[4];
	gint totalguchars;
	gchar format[8];
	gint pcm1;
	gshort pcm2;
	gshort channels; 	
	gint samplefreq;
	gint gucharspersecond; 	
	gshort gucharspercapture;
	gshort bitspersample; 	
	gchar data[4];
	gint gucharsindata;		
} *pWavHeader  __attribute__ ((packed));


void audioinfo_get_wav_info(AudioInfo* self, const gchar* wavfile);
void audioinfo_get_ogg_info(AudioInfo* info, const gchar *oggfile);
void audioinfo_get_mp3_info(AudioInfo* info, const gchar *mp3file);
void audioinfo_load_mp3_header(guchar c[]);
gboolean audioinfo_load_mp3_vbr_header(guchar inputheader[]);
gboolean audioinfo_mp3_is_valid_header();
gint audioinfo_mp3_get_frame_sync();
gint audioinfo_mp3_get_version_index();  
gint audioinfo_mp3_get_layer_index();    
gint audioinfo_mp3_get_protection_bit(); 
gint audioinfo_mp3_get_bitrate_index();  
gint audioinfo_mp3_get_frequency_index();
gint audioinfo_mp3_get_padding_bit();    
gint audioinfo_mp3_get_private_bit();    
gint audioinfo_mp3_get_mode_index() ;    
gint audioinfo_mp3_get_mode_ext_index();  
gint audioinfo_mp3_get_copyright_bit();   
gint audioinfo_mp3_get_original_bit();    
gint audioinfo_mp3_get_emphasis_index(); 
gdouble audioinfo_mp3_get_version(); 
gint audioinfo_mp3_get_layer(); 
gint audioinfo_mp3_get_bitrate(); 
gint audioinfo_mp3_get_frequency(); 
const gchar* audioinfo_mp3_get_mode(); 
gint audioinfo_mp3_get_length(); 
void audioinfo_set_formatted_length(AudioInfo* info); 
gint audioinfo_mp3_get_num_frames(); 


AudioInfo* 
audioinfo_new(const gchar* audiofile)
{
	GB_LOG_FUNC
	
	AudioInfo* self = g_new(AudioInfo, 1);
	if(NULL != self)
	{
		if(!audioinfo_init(self))
		{
			g_free(self);
			self = NULL;
		}
		else
		{			
			GString* vfsfile = g_string_new(audiofile);
		
			/* Get the file name that's been dropped and if there's a 
			   file url at the start then strip it off */				
			if(g_ascii_strncasecmp(audiofile, fileurl, strlen(fileurl)) == 0)
				audiofile += strlen(fileurl);
			else
				g_string_insert(vfsfile, 0, fileurl);
			
			gchar* mime = gnome_vfs_get_mime_type(vfsfile->str);
			g_message( "mime type is %s for %s %s", mime, audiofile, vfsfile->str);
			if(mime != NULL)
			{
				/* Check that the file extension is one we support */			
				if(g_ascii_strcasecmp(mime, "audio/x-wav") == 0)
					audioinfo_get_wav_info(self, audiofile);				
				else if(g_ascii_strcasecmp(mime, "audio/x-mp3") == 0)
					audioinfo_get_mp3_info(self, audiofile);
				else if(g_ascii_strcasecmp(mime, "application/ogg") == 0)
					audioinfo_get_ogg_info(self, audiofile);
				/*else if(g_ascii_strcasecmp(mime, "audio/x-flac") == 0);	*/
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
				
				g_free(mime);
			}
			
			g_string_free(vfsfile, TRUE);
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
/* 
Field 	guchars 	format 	contains
1 	0...3 	str4 	"RIFF" in ASCII
2 	4...7 	int4 	Total guchars minus 8
3 	8...15 	str8 	"WAVEfmt" Eigth character is a space
4 	16...19 	int4 	16 for PCM format
5 	20...21 	int2 	1 for PCM format
6 	22...23 	int2 	channels
7 	24...27 	int4 	sampling frequency
8 	28...31 	int4 	guchars per second
9 	32...33 	int2 	guchars by capture
10 	34...35 	int2 	bits per sample
11 	36:39 	str4 	"data"
12 	40:43 	int4 	guchars in data
*/		
	GB_LOG_FUNC
	g_return_if_fail(NULL != self);
	g_return_if_fail(NULL != wavfile);
	
	FILE* st = fopen(wavfile, "rb");
	if (st == NULL)
	{
		printf("File %s was not opened \n", wavfile);
	}
	else
	{		
		struct WavHeader wav;
		static const size_t StructSize = sizeof(struct WavHeader);
		if(fread(&wav, 1, StructSize, st) == StructSize)
		{					
			g_message("[%d] [%d] [%d]", wav.totalguchars, wav.gucharspersecond,
				wav.gucharsindata);			
			self->bitrate = wav.gucharspersecond;
			
			fseek(st, 0, SEEK_END);
        	self->filesize = ftell(st);
			
			/* This is a slight approximation as I have seen wavs where
			   the gucharsindata is incorrect in the header. Therefore I've 
			   played it safe and assumed the actual sound guchars are filesize
			   minus header size */
			self->duration = (self->filesize - StructSize) / wav.gucharspersecond;
		}
		else
		{
			g_warning("Error reading wav header for file [%s]", wavfile);
		}	
		
		fclose(st);
	
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
					{ "Performer","Albumtitle","Tracktitle" };
				
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
		g_warning("File %s was not opened", FileName);
	}
	else
	{		
		/* Set the file size */
		fseek(st, 0, SEEK_END);
        lngFileSize = ftell(st);	
		info->filesize = lngFileSize;
	
		guchar bytHeader[4];
		guchar bytVBitRate[12];
		gint intPos = 0;
		
		/* Keep reading 4 guchars from the header until we know for sure that in 
		 fact it's an MP3 */
		do
		{
			fseek(st, intPos, SEEK_SET);
			fread(bytHeader, 1, 4, st);
			intPos++;
			audioinfo_load_mp3_header(bytHeader);
		}
		while(!audioinfo_mp3_is_valid_header() && (ftell(st) < lngFileSize));
		
		if(ftell(st) < lngFileSize)
		{
			intPos += 3;
	
			if(audioinfo_mp3_get_version_index() == 3) /* MPEG Version 1 */
			{
				if(audioinfo_mp3_get_mode_index() == 3) /* Single Channel */
					intPos += 17;
				else
					intPos += 32;
			}
			else /* MPEG Version 2.0 or 2.5 */
			{
				if(audioinfo_mp3_get_mode_index() == 3)    /* Single Channel */
					intPos += 9;
				else
					intPos += 17;
			}
			
			/* Check to see if the MP3 has a variable bitrate */
			fseek(st, intPos, SEEK_SET);
			fread(bytVBitRate, 1, 12, st);
			boolVBitRate = audioinfo_load_mp3_vbr_header(bytVBitRate);
	
			info->bitrate = audioinfo_mp3_get_bitrate();
			intFrequency = audioinfo_mp3_get_frequency();
			info->duration = audioinfo_mp3_get_length();
			
			/* get tag from the last 128 guchars in an .mp3-file    */
			gchar tagchars[128];
	
			/* get last 128 guchars */
			fseek(st, lngFileSize - 128, SEEK_SET);
			fread(tagchars, 1, 128, st);
			/*tagchars[127] = '\0';*/
			
			if(g_ascii_strncasecmp(tagchars, "TAG", 3) == 0)
			{
				g_message("%ld [%s] [%s] [%s]", lngFileSize - 128, 
					tagchars + 3, tagchars + 33, tagchars + 63);
					
				g_string_append_len(info->artist, tagchars + 33, 30);
				g_strstrip(info->artist->str);
				g_string_append_len(info->album, tagchars + 63, 30);
				g_strstrip(info->album->str);
				g_string_append_len(info->title, tagchars + 3, 30);
				g_strstrip(info->title->str);
			}
		}

		fclose(st);
	}
}


void 
audioinfo_load_mp3_header(guchar c[])
{
	GB_LOG_FUNC
	bithdr = (gulong)(((c[0] & 255) << 24) | ((c[1] & 255) << 16) | ((c[2] & 255) <<  8) | ((c[3] & 255))); 
}


gboolean 
audioinfo_load_mp3_vbr_header(guchar inputheader[])
{
	GB_LOG_FUNC
	if(inputheader[0] == 88 && inputheader[1] == 105 && 
		inputheader[2] == 110 && inputheader[3] == 103)
	{
		gint flags = (gint)(((inputheader[4] & 255) << 24) | ((inputheader[5] & 255) << 16) | ((inputheader[6] & 255) <<  8) | ((inputheader[7] & 255)));
		if((flags & 0x0001) == 1)
		{
			intVFrames = (gint)(((inputheader[8] & 255) << 24) | ((inputheader[9] & 255) << 16) | ((inputheader[10] & 255) <<  8) | ((inputheader[11] & 255)));
			return TRUE;
		}
		else
		{
			intVFrames = -1;
			return TRUE;
		}
	}
	return FALSE;
}


gboolean 
audioinfo_mp3_is_valid_header() 
{
	GB_LOG_FUNC
	return (((audioinfo_mp3_get_frame_sync()      & 2047)==2047) &&
			((audioinfo_mp3_get_version_index()   &    3)!=   1) &&
			((audioinfo_mp3_get_layer_index()     &    3)!=   0) && 
			((audioinfo_mp3_get_bitrate_index()   &   15)!=   0) &&
			((audioinfo_mp3_get_bitrate_index()   &   15)!=  15) &&
			((audioinfo_mp3_get_frequency_index() &    3)!=   3) &&
			((audioinfo_mp3_get_emphasis_index()  &    3)!=   2)    );
}


gint 
audioinfo_mp3_get_frame_sync()     
{
	GB_LOG_FUNC
	return (gint)((bithdr>>21) & 2047); 
}


gint 
audioinfo_mp3_get_version_index()  
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>19) & 3);  
}


gint 
audioinfo_mp3_get_layer_index()    
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>17) & 3);  
}


gint 
audioinfo_mp3_get_protection_bit() 
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>16) & 1);  
}


gint 
audioinfo_mp3_get_bitrate_index()  
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>12) & 15); 
}


gint 
audioinfo_mp3_get_frequency_index()
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>10) & 3);  
}


gint 
audioinfo_mp3_get_padding_bit()    
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>9) & 1);  
}


gint 
audioinfo_mp3_get_private_bit()    
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>8) & 1);  
}


gint 
audioinfo_mp3_get_mode_index()     
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>6) & 3);  
}


gint 
audioinfo_mp3_get_mode_ext_index()  
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>4) & 3);  
}


gint 
audioinfo_mp3_get_copyright_bit()   
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>3) & 1);  
}


gint 
audioinfo_mp3_get_original_bit()    
{ 
	GB_LOG_FUNC
	return (gint)((bithdr>>2) & 1);  
}


gint 
audioinfo_mp3_get_emphasis_index() 
{ 
	GB_LOG_FUNC
	return (gint)(bithdr & 3);  
}


gdouble 
audioinfo_mp3_get_version() 
{
	GB_LOG_FUNC
	gdouble table[] = {2.5, 0.0, 2.0, 1.0};
	return table[audioinfo_mp3_get_version_index()];
}


gint 
audioinfo_mp3_get_layer() 
{
	GB_LOG_FUNC
	return (gint)(4 - audioinfo_mp3_get_layer_index());
}


gint 
audioinfo_mp3_get_bitrate() 
{
	GB_LOG_FUNC
	if(boolVBitRate)
	{
		gdouble medFrameSize = (gdouble)lngFileSize / (gdouble)audioinfo_mp3_get_num_frames();
		return (gint)((medFrameSize * (gdouble)audioinfo_mp3_get_frequency()) / (1000.0 * ((audioinfo_mp3_get_layer_index()==3) ? 12.0 : 144.0)));
	}
	else
	{
		if((audioinfo_mp3_get_version_index() & 1) == 0)
		{
			/* MPEG 2 & 2.5 */
			gint table[3][16] = {
								{0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,0}, // Layer III
								{0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,0}, // Layer II
								{0, 32, 48, 56, 64, 80, 96,112,128,144,160,176,192,224,256,0}  // Layer I
							};
			return table[audioinfo_mp3_get_layer_index()-1][audioinfo_mp3_get_bitrate_index()];
		}
		else
		{
			/* MPEG 1 */
			gint table[3][16] = { 
								{0, 32, 40, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,0}, // Layer III
								{0, 32, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,384,0}, // Layer II
								{0, 32, 64, 96,128,160,192,224,256,288,320,352,384,416,448,0}  // Layer I
							};			
			return table[audioinfo_mp3_get_layer_index()-1][audioinfo_mp3_get_bitrate_index()];
		}
	}
}


gint 
audioinfo_mp3_get_frequency() 
{
	GB_LOG_FUNC
	gint table[4][3] =    {    
						{32000, 16000,  8000}, /* MPEG 2.5 */
						{    0,     0,     0}, /* reserved */
						{22050, 24000, 16000}, /* MPEG 2 */
						{44100, 48000, 32000}  /*  MPEG 1 */
					};
	return table[audioinfo_mp3_get_version_index()][audioinfo_mp3_get_frequency_index()];
}


const gchar* 
audioinfo_mp3_get_mode() 
{
	GB_LOG_FUNC
	switch(audioinfo_mp3_get_mode_index()) 
	{
		default:
		{
			static const gchar* stereo = "Stereo";
			return stereo;
		}
		case 1:
		{
			static const gchar* joint_stereo = "Joint Stereo";
			return joint_stereo;
		}
		case 2:
		{
			static const gchar* dual_channel = "Dual Channel";
			return dual_channel;
		}
		case 3:
		{
			static const gchar* single_channel = "Single Channel";
			return single_channel;
		}
	};
}


gint 
audioinfo_mp3_get_length() 
{	
	GB_LOG_FUNC
	return (gint)(((gint)((8 * lngFileSize) / 1000))/audioinfo_mp3_get_bitrate());
}


void 
audioinfo_set_formatted_length(AudioInfo* info) 
{
	GB_LOG_FUNC
	gint seconds = info->duration % 60;
	gint minutes = (info->duration-seconds) / 60;
	
	g_string_printf(info->formattedduration, "%d:%.2d", minutes, seconds);
}


gint 
audioinfo_mp3_get_num_frames() 
{
	GB_LOG_FUNC
	if (!boolVBitRate) 
	{
		gdouble medFrameSize = (gdouble)(((audioinfo_mp3_get_layer_index()==3) ? 12 : 144) *((1000.0 * (gfloat)audioinfo_mp3_get_bitrate())/(gfloat)audioinfo_mp3_get_frequency()));
		return (gint)(lngFileSize/medFrameSize);
	}
	else 
		return intVFrames;
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
		printf("File %s was not opened \n", oggfile);
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
			g_message("[%s]", comment->user_comments[i]);				
			if(comment->user_comments[i] == NULL)
				continue;			
			else if(g_ascii_strncasecmp(comment->user_comments[i], "TITLE=", 6) == 0)
				g_string_append(info->title, comment->user_comments[i] + 6);
			else if(g_ascii_strncasecmp(comment->user_comments[i], "ARTIST=", 7) == 0)
				g_string_append(info->artist, comment->user_comments[i] + 7);
			else if(g_ascii_strncasecmp(comment->user_comments[i], "ALBUM=", 6) == 0)
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
