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
 * Copyright: User <Email>
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
	gchar* mimetype;
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
