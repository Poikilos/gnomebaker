/*
 * File: audioinfo.h
 * Created by: User <Email>
 * Created on: Thu Sep 16 20:16:51 2004
 */

#ifndef _AUDIOINFO_H_
#define _AUDIOINFO_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>


typedef struct 
{
	GString* artist;
	GString* album;
	GString* title;
	gulong duration;
	size_t filesize;
	gulong bitrate;
	GString* formattedduration;
} AudioInfo, *AudioInfoPtr;


AudioInfo* audioinfo_new(const gchar* audiofile);
void audioinfo_delete(AudioInfo* self);
gboolean audioinfo_init(AudioInfo* self);
void audioinfo_end(AudioInfo* self);


#endif	/*_AUDIOINFO_H_*/
