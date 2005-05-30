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

typedef GstElement MediaElement;


typedef struct 
{
	MediaElement* pipeline;
	MediaElement* decoder;
	MediaElement* converter;
	MediaElement* scale;
	MediaElement* encoder;
	MediaElement* dest;
	MediaElement* last_element;
	gchar* convertedfile;
} MediaInfo, *MediaInfoPtr;

typedef enum
{
	INSTALLED = 0,
	NOT_INSTALLED
}PluginStatus;

typedef struct
{
	GString *mimetype;
	GString *gst_plugin_name;
	PluginStatus status;	
} PluginInfo, *PluginInfoPtr;

 
void media_convert_to_wav(gchar* file,gchar* convertedfile,MediaInfoPtr gstdata);
void media_query_progress_bytes(MediaElement* element,gint64* pos,gint64* len);
void media_start_playing(MediaElement* element);
void media_pause_playing(MediaElement* element);
void media_cleanup(MediaElement* element);
void media_register_plugins(void);
PluginStatus media_query_plugin_status(const gchar* mimetype);
void media_connect_error_callback(MediaElement* element,void * func);
void media_connect_eos_callback(MediaElement* element,void * func);
void media_start_iteration(MediaElement* pipeline);
gint64 media_calculate_track_length(const gchar* filename);

#endif	/* _MEDIA_H_ */
