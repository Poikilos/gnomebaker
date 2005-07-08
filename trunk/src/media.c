/***************************************************************************
 *            media.c
 *
 *  Sun Feb  6 21:47:19 2005
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
 
#include "gbcommon.h"
#include "media.h"

/*MediaElement *pipeline, *decoder, *converter,*encoder,*scale,*dest;*/

GSList *media_registered_plugins = NULL;


static void
media_cb_newpad (GstElement *element,
	   GstPad     *pad,
	   gboolean last,
	   MediaInfoPtr  mip)
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


void
media_convert_to_wav(gchar* file,gchar* convertedfile,MediaInfoPtr gstdata)
{
	GB_LOG_FUNC	
	g_return_if_fail(file != NULL);
	g_return_if_fail(convertedfile != NULL);
	g_return_if_fail(gstdata != NULL);
		
	/* create a new pipeline to hold the elements */
	gstdata->pipeline = gst_pipeline_new ("media-convert-to-wav-pipeline");
	g_return_if_fail(gstdata->pipeline != NULL);
	
	GstElement *src = gst_element_factory_make("filesrc","file-source");
	g_return_if_fail(src != NULL);
	
	g_object_set (G_OBJECT (src), "location", file, NULL);
		
	/* decoder */
	gstdata->decoder = gst_element_factory_make ("decodebin", "decoder");
	g_assert (gstdata->decoder != NULL);
	g_signal_connect (gstdata->decoder, "new-decoded-pad", G_CALLBACK (media_cb_newpad), gstdata);
	
	/* create an audio converter */
	gstdata->converter = gst_element_factory_make ("audioconvert", "converter");
	g_assert (gstdata->converter != NULL);
	
	/* audioscale resamples audio */
	gstdata->scale = gst_element_factory_make("audioscale","scale");
	g_assert (gstdata->scale != NULL);
	
	GstCaps *filtercaps = gst_caps_new_simple ("audio/x-raw-int",
                                          "channels", G_TYPE_INT, 2,
                                          "rate",     G_TYPE_INT, 44100,
                                          "width",    G_TYPE_INT, 16,
                                          "depth",    G_TYPE_INT, 16,
                                          NULL);
    /* and an wav encoder */
	gstdata->encoder = gst_element_factory_make ("wavenc", "encoder");
	g_return_if_fail(gstdata->encoder != NULL);	
	
	gst_element_link_filtered (gstdata->scale,gstdata->encoder,filtercaps);
    gst_caps_free (filtercaps);	
	
	/* finally the output filesink */	
	gstdata->dest = gst_element_factory_make("filesink","file-out");
	g_return_if_fail(gstdata->dest != NULL);
	gstdata->last_element = gstdata->dest;
		
	g_object_set (G_OBJECT (gstdata->dest), "location", convertedfile, NULL);
	
	gst_bin_add_many (GST_BIN (gstdata->pipeline), src, gstdata->decoder, NULL);
	
	gst_element_link(gstdata->converter,gstdata->scale);
	gst_element_link(gstdata->encoder,gstdata->dest);
	gst_element_link(src,gstdata->decoder);
}


void 
media_connect_eos_callback(MediaElement* element,void * func)
{
	GB_LOG_FUNC
	g_return_if_fail(element != NULL);
	g_signal_connect (element, "eos", G_CALLBACK (func), NULL);
}


void 
media_connect_error_callback(MediaElement* element,void * func)
{
	GB_LOG_FUNC
	g_return_if_fail(element != NULL);
	g_signal_connect (element, "error", G_CALLBACK (func), NULL);
}


void 
media_query_progress_bytes(MediaElement* element,gint64* pos,gint64* len)
{
	GB_LOG_FUNC
	g_return_if_fail(element != NULL);
	g_return_if_fail(pos != NULL);
	g_return_if_fail(len != NULL);
	
	GstFormat fmt = GST_FORMAT_BYTES;
    gst_element_query (element, GST_QUERY_POSITION, &fmt, pos);
   	gst_element_query (element, GST_QUERY_TOTAL, &fmt, len);
}


void
media_start_iteration(MediaElement* pipeline)
{
	GB_LOG_FUNC
	g_return_if_fail(pipeline != NULL);
	while (gst_bin_iterate (GST_BIN (pipeline)));
}


void
media_start_playing(MediaElement* element)
{
	GB_LOG_FUNC
	g_return_if_fail(element != NULL);
	gst_element_set_state (element, GST_STATE_PLAYING);
}


void
media_pause_playing(MediaElement* element)
{
	GB_LOG_FUNC
	g_return_if_fail(element != NULL);
	gst_element_set_state (element, GST_STATE_PAUSED);
}


void
media_cleanup(MediaElement* element)
{
	GB_LOG_FUNC
	g_return_if_fail(element != NULL);
  	gst_element_set_state (element, GST_STATE_NULL);
  	gst_object_unref (GST_OBJECT (element));
}


PluginStatus 
media_query_plugin_status(const gchar* mimetype)
{
	GB_LOG_FUNC	
	g_return_val_if_fail(mimetype != NULL,NOT_INSTALLED);

	PluginStatus status = NOT_INSTALLED;
	GString* mime = g_string_new(mimetype);
	gboolean found = FALSE;
 	GSList* node = media_registered_plugins;
	while(node && !found)
	{
		if(node->data)
		{
			PluginInfoPtr plugininfo = (PluginInfoPtr)node->data;
			GB_TRACE("plugin mimetype [%s] requested [%s]", plugininfo->mimetype->str, mime->str);
			if(g_string_equal(plugininfo->mimetype,mime))
			{
				status = plugininfo->status;
				found = TRUE;
			}
		}
		node = node->next;
	}
	g_string_free(mime,FALSE);
	return status;
}


PluginInfoPtr 
media_create_plugin_info(const gchar* mimetype,const gchar* pluginname)
{
	GB_LOG_FUNC
	g_return_val_if_fail(mimetype != NULL,NULL);
	g_return_val_if_fail(pluginname != NULL,NULL);
	
	PluginInfoPtr info = g_new0(PluginInfo,1);
	info->mimetype = g_string_new(mimetype);
	info->gst_plugin_name = g_string_new(pluginname);
	info->status = NOT_INSTALLED;
	return info;
}


void 
media_register_plugins(void)
{
	GB_LOG_FUNC
	
	if(media_registered_plugins)
		g_slist_free(media_registered_plugins);
	
	media_registered_plugins = g_slist_alloc();
	PluginInfoPtr info = media_create_plugin_info("application/x-id3","mad");
	PluginInfoPtr info2 = media_create_plugin_info("audio/mpeg","mad");
	GstElement* element;		
	if((element = gst_element_factory_make("mad","mad")) != NULL)
	{
		info->status = INSTALLED;
		info2->status = INSTALLED;
		gst_object_unref (GST_OBJECT (element));
		element = NULL;
	}
	media_registered_plugins = g_slist_append(media_registered_plugins,info);
	media_registered_plugins = g_slist_append(media_registered_plugins,info2);

	info = media_create_plugin_info("application/ogg","vorbisdec");
	
	if((element = gst_element_factory_make("vorbisdec","vorbisdec")) != NULL)
	{
		/*TODO: do we need to check for oggdemux/parse? */
		info->status = INSTALLED;
		gst_object_unref (GST_OBJECT (element));
		element = NULL;
	}
	media_registered_plugins = g_slist_append(media_registered_plugins,info);
	
	info = media_create_plugin_info("application/x-flac","flacdec");
    info2 = media_create_plugin_info("audio/x-flac","flacdec");
	if((element = gst_element_factory_make("flacdec","flacdec")) != NULL)
	{
		info->status = INSTALLED;
        info2->status = INSTALLED;
		gst_object_unref (GST_OBJECT (element));
		element = NULL;
	}
	media_registered_plugins = g_slist_append(media_registered_plugins,info);
    media_registered_plugins = g_slist_append(media_registered_plugins,info2);
	/*
	TODO: get info for the monkey plugin
	if((element = gst_element_factory_make("monkeysaudio","monkeysaudio")) != NULL)
	{
		PluginInfo* info = g_new (PluginInfo,1);
		info->mimetype = g_string_new("application/x-ape");
		info->gst_plugin_name = g_string_new("monkeysaudio");
		g_slist_append(media_registered_plugins,info);
		gst_object_unref (GST_OBJECT (element));
		element = NULL;
	}
	*/
	info = media_create_plugin_info("application/x-mod","modplug");
	if((element = gst_element_factory_make("modplug","modplug")) != NULL)
	{
		info->status = INSTALLED;
		gst_object_unref (GST_OBJECT (element));
		element = NULL;
	}
	media_registered_plugins = g_slist_append(media_registered_plugins,info);
	
	info = media_create_plugin_info("application/x-wav","wavparse");
	info2 = media_create_plugin_info("audio/x-wav","wavparse");
	if((element = gst_element_factory_make("wavparse","wavparse")) != NULL)
	{
		info->status = INSTALLED;
		info2->status = INSTALLED;
		gst_object_unref (GST_OBJECT (element));
		element = NULL;
	}
	media_registered_plugins = g_slist_append(media_registered_plugins,info);
	media_registered_plugins = g_slist_append(media_registered_plugins,info2);
	
	info = media_create_plugin_info("video/x-ms-asf","ffdec_wmav1");
	if((element = gst_element_factory_make("ffdec_wmav1","ffdec_wmav1")) != NULL)
	{
		/* yes, indeed a weird mimetype */
		info->status = INSTALLED;
		/*TODO: what about ffdec_wmav2?*/
		gst_object_unref (GST_OBJECT (element));
		element = NULL;
	}
	media_registered_plugins = g_slist_append(media_registered_plugins,info);
}


void
media_fakesink_handoff(GstElement* element, GstBuffer* buffer, GstPad* pad, gboolean* handoff)
{
	GB_LOG_FUNC
    g_return_if_fail(handoff != NULL);
	*handoff = TRUE;
}


void
media_fakesink_endofstream(GstElement* element, gboolean* endofstream)
{
    GB_LOG_FUNC
    g_return_if_fail(endofstream != NULL);
	*endofstream = TRUE;
}


void
media_fakesink_error(GstElement* element, GstElement* source,
			  GError* error, gchar* debug, GError** gberror)
{
	GB_LOG_FUNC
    g_return_if_fail(error != NULL);
    g_return_if_fail(gberror != NULL);    
    *gberror = g_error_copy(error);
}


gint64
media_calculate_track_length(const gchar* filename)
{	
    GB_LOG_FUNC
    g_return_val_if_fail(filename != NULL, 0);
        
	GstElement *pipeline = gst_pipeline_new ("pipeline");
	GstElement *spider = gst_element_factory_make ("spider", "spider");
	g_assert (spider);
	GstElement *source = gst_element_factory_make ("filesrc", "source");
	g_assert (source);
	g_object_set (G_OBJECT (source), "location", filename, NULL);
	GstElement *wavenc = gst_element_factory_make ("wavenc", "parser");
	g_assert (wavenc);
	GstElement *destination = gst_element_factory_make ("fakesink", "fakesink");
	g_assert (destination);
	gst_bin_add (GST_BIN (pipeline), source);
	gst_bin_add (GST_BIN (pipeline), spider);
	gst_bin_add (GST_BIN (pipeline), wavenc);
	gst_bin_add (GST_BIN (pipeline), destination);
	gst_element_link (source, spider);
	gst_element_link (spider, wavenc);
	gst_element_link (wavenc, destination);
    
    GError* error = NULL;
    gboolean handoff = FALSE, endofstream = FALSE; 
    g_signal_connect(pipeline, "error", G_CALLBACK(media_fakesink_error), &error);
    g_object_set(G_OBJECT(destination), "signal-handoffs", TRUE, NULL);
	g_signal_connect(destination, "handoff", G_CALLBACK(media_fakesink_handoff), &handoff);
	g_signal_connect(destination, "eos", G_CALLBACK(media_fakesink_endofstream), &endofstream);
		
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
    /* have to iterate a few times before the total time is set - signalled by handoff */    
    while(gst_bin_iterate (GST_BIN (pipeline)) && (error == NULL) && !handoff && !endofstream);
    GstFormat format = GST_FORMAT_TIME;
    gint64 total = 0;   
	gst_element_query (destination, GST_QUERY_TOTAL, &format, &total);
    gst_element_set_state (pipeline, GST_STATE_NULL);
	GB_TRACE("*** track length [%d]", total / GST_SECOND);    
	g_object_unref(pipeline);
	return total / GST_SECOND;	
}
