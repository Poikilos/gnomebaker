/***************************************************************************
 *            media.h
 *
 *  Sun Feb  6 21:47:13 2005
 *  Copyright  2005  Christoffer Sørensen
 *  Email gnomebaker@curo.dk
 ****************************************************************************/

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
 
#ifndef _MEDIA_H_
#define _MEDIA_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gst/gst.h"

typedef struct 
{
	GstElement* pipeline;
	GstElement* decoder;
	GstElement* converter;
	GstElement* scale;
	GstElement* encoder;
	GstElement* dest;
} MediaPipeline;

typedef enum
{
	INSTALLED = 0,
	NOT_INSTALLED
} PluginStatus;

typedef struct
{
	GString *mimetype;
	GString *gst_plugin_name;
	PluginStatus status;	
} PluginInfo;

 
typedef struct 
{
    PluginStatus status;
    gchar* mimetype;
    gchar* filename;
    GString* artist;
    GString* album;
    GString* title; 
    gulong duration;
    gulong bitrate;
    GString* formattedduration;
} MediaInfo;
 
 
void media_register_plugins();
MediaInfo* media_info_new(const gchar* mediafile);
void media_info_delete(MediaInfo* self);
void media_info_create_inf_file(const MediaInfo* info, const int trackno, const gchar* inffile, int* trackstart);

#endif	/* _MEDIA_H_ */
