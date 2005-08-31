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

static GSList *media_registered_plugins = NULL;
static MediaPipeline* mediaplayer = NULL;


static PluginInfo* 
media_plugininfo_new(const gchar* mimetype, const gchar* pluginname)
{
	GB_LOG_FUNC
	g_return_val_if_fail(mimetype != NULL,NULL);
	g_return_val_if_fail(pluginname != NULL,NULL);
	
	PluginInfo* info = g_new0(PluginInfo,1);
	info->mimetype = g_string_new(mimetype);
	info->gst_plugin_name = g_string_new(pluginname);
	info->status = NOT_INSTALLED;
	return info;
}


static void
media_plugininfo_delete(PluginInfo* info)
{
    GB_LOG_FUNC   
    g_return_if_fail(info != NULL);
    
    g_string_free(info->mimetype, TRUE);
    g_string_free(info->gst_plugin_name, TRUE);
    g_free(info);
}


static void 
media_info_get_wav_info(MediaInfo* info, const gchar* wavfile)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != info);
    g_return_if_fail(NULL != wavfile);
    
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
                { "Performer", "Albumtitle", "Tracktitle" };
            
            GString* copyto[3] = 
                { info->artist, info->album, info->title };                 
                
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
}


static void 
media_info_set_formatted_length(MediaInfo* info) 
{
    GB_LOG_FUNC
    g_return_if_fail(info != NULL);
    
    gint seconds = info->duration % 60;
    gint minutes = (info->duration-seconds) / 60;
    g_string_printf(info->formattedduration, "%d:%.2d", minutes, seconds);
}


static void
media_fakesink_handoff(GstElement* element, GstBuffer* buffer, GstPad* pad, gboolean* handoff)
{
    GB_LOG_FUNC
    g_return_if_fail(handoff != NULL);
    *handoff = TRUE;
}


static void
media_fakesink_endofstream(GstElement* element, gboolean* endofstream)
{
    GB_LOG_FUNC
    g_return_if_fail(endofstream != NULL);
    *endofstream = TRUE;
}


static void
media_fakesink_error(GstElement* element, GstElement* source,
             GError* error, gchar* debug, GError** gberror)
{
    GB_LOG_FUNC
    g_return_if_fail(error != NULL);
    g_return_if_fail(gberror != NULL);
    g_critical("media_fakesink_error - [%s]", error->message);
    *gberror = g_error_copy(error);
}


static void
media_tags_foreach(const GstTagList *list, const gchar *tag, MediaInfo* info)
{
   GB_LOG_FUNC
   int count = gst_tag_list_get_tag_size(list, tag);
   if (count < 1)
       return;

   const GValue *val  = gst_tag_list_get_value_index (list, tag, 0);
   if (g_ascii_strcasecmp (tag, GST_TAG_TITLE) == 0)
       g_string_assign(info->title, g_value_get_string (val));
   else if (g_ascii_strcasecmp (tag, GST_TAG_ARTIST) == 0)
       g_string_assign(info->artist, g_value_get_string (val));
   else if (g_ascii_strcasecmp (tag, GST_TAG_DURATION) == 0)
       info->duration = g_value_get_uint64 (val) / GST_SECOND;
   else if (g_ascii_strcasecmp (tag, GST_TAG_ALBUM) == 0)
       g_string_assign(info->album, g_value_get_string (val));
   else if (g_ascii_strcasecmp (tag, GST_TAG_BITRATE) == 0)
       info->bitrate = g_value_get_uint (val);
}


static void
media_fakesink_tag(GObject *pipeline, GstElement *source, GstTagList *tags, MediaInfo* info)
{
    gst_tag_list_foreach (tags, (GstTagForeachFunc) media_tags_foreach, info); 
}


static void
media_type_found(GstElement *typefind,
          guint       probability,
          GstCaps    *caps,
          gpointer    data)
{
    GB_LOG_FUNC
    gchar* type = gst_caps_to_string (caps);
    GB_TRACE("Media type %s found, probability %d%%\n", type, probability);
    g_free (type);
    /*((MediaInfo*)data)->status = INSTALLED;*/
}


static void
media_info_get_mediafile_info(MediaInfo* info, const gchar* mediafile)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != info);
    g_return_if_fail(NULL != mediafile);
   
    GstElement *pipeline = gst_pipeline_new ("pipeline");
    GstElement *spider = gst_element_factory_make ("spider", "spider");
    g_assert (spider);
    GstElement *source = gst_element_factory_make ("filesrc", "source");
    g_assert (source);
    GstElement *typefind = gst_element_factory_make ("typefind", "typefind");
    g_assert (typefind);
    g_object_set (G_OBJECT (source), "location", mediafile, NULL);
    GstElement *destination = gst_element_factory_make ("fakesink", "fakesink");
    g_assert (destination);
    
    gst_bin_add_many(GST_BIN (pipeline), source, typefind, spider, destination, NULL);    
    gst_element_link_many (source, typefind, spider, NULL);

    GstCaps* filtercaps = gst_caps_new_simple ("audio/x-raw-int", NULL);
    gst_element_link_filtered (spider, destination, filtercaps);
    gst_caps_free (filtercaps);
    
    gboolean handoff = FALSE, endofstream = FALSE; 
    g_signal_connect(pipeline, "found-tag", G_CALLBACK(media_fakesink_tag), info);
    g_signal_connect(pipeline, "error", G_CALLBACK(media_fakesink_error), &info->error);
    g_object_set(G_OBJECT(destination), "signal-handoffs", TRUE, NULL);
    g_signal_connect(destination, "handoff", G_CALLBACK(media_fakesink_handoff), &handoff);
    g_signal_connect(destination, "eos", G_CALLBACK(media_fakesink_endofstream), &endofstream);
    g_signal_connect (typefind, "have-type", G_CALLBACK (media_type_found), info);
       
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
    /* have to iterate a few times before the total time is set - signalled by handoff */    
    while(gst_bin_iterate (GST_BIN (pipeline)) && (info->error == NULL) && !handoff && !endofstream);
    if(info->duration == 0)
    {
        GstFormat format = GST_FORMAT_TIME;
        gint64 total = 0;
        gst_element_query (destination, GST_QUERY_TOTAL, &format, &total);
        GB_TRACE("*** track length [%ld]", (long int)(total / GST_SECOND));    
        info->duration = total / GST_SECOND;
    }
    gst_element_set_state (pipeline, GST_STATE_NULL);
    
    if(info->error != NULL)
    {
        info->status = NOT_INSTALLED;
    }
    else 
    {
        if(g_ascii_strcasecmp(info->mimetype, "audio/x-wav") == 0)
            media_info_get_wav_info(info, mediafile);        
        media_info_set_formatted_length(info);
    }
    g_object_unref(pipeline);        
}


static PluginStatus
media_get_plugin_status(const gchar* mimetype)
{
    GB_LOG_FUNC
    PluginStatus status = NOT_INSTALLED;
    g_return_val_if_fail(mimetype != NULL, status);
    
    const GSList* node = media_registered_plugins;
    for(; node != NULL; node = node->next)
    {
        PluginInfo* plugininfo = (PluginInfo*)node->data;
        GB_TRACE("plugin mimetype [%s] requested [%s]", plugininfo->mimetype->str, mimetype);
        if(g_ascii_strcasecmp(plugininfo->mimetype->str, mimetype) == 0)
        {
            status = plugininfo->status;
            break;
        }
    }
    return status;
}


static void 
media_register_plugin(const gchar* mimetype, const gchar* pluginname)
{
    GB_LOG_FUNC
    g_return_if_fail(mimetype != NULL);
    g_return_if_fail(pluginname != NULL);
    
    PluginInfo* info = media_plugininfo_new(mimetype, pluginname);
    GstElement* element = gst_element_factory_make(pluginname, pluginname);
    if(element != NULL)
    {
        info->status = INSTALLED;
        gst_object_unref (GST_OBJECT (element));
    }
    media_registered_plugins = g_slist_append(media_registered_plugins,info);
}


void 
media_init()
{
    GB_LOG_FUNC        
    media_register_plugin("application/x-id3", "mad");
    media_register_plugin("audio/mpeg", "mad");
    media_register_plugin("audio/x-mp3", "mad");
    media_register_plugin("application/ogg", "vorbisdec");
    media_register_plugin("application/x-flac", "flacdec");
    media_register_plugin("audio/x-flac", "flacdec");
    /* media_register_plugin("application/x-ape", "monkeysaudio"); */
    media_register_plugin("application/x-mod", "modplug");
    media_register_plugin("application/x-wav", "wavparse");
    media_register_plugin("audio/x-wav", "wavparse");
    media_register_plugin("video/x-ms-asf", "ffdec_wmav1");
}


void 
media_finalise()
{
    GB_LOG_FUNC        
    
    const GSList* node = media_registered_plugins;
    for(; node != NULL; node = node->next)
        media_plugininfo_delete((PluginInfo*)node->data);
    g_slist_free(media_registered_plugins);
    if(mediaplayer != NULL)
    {
        gst_object_unref (GST_OBJECT (mediaplayer->pipeline));
        g_free(mediaplayer);
    }
}


void 
media_info_delete(MediaInfo* info)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != info);
        
    g_free(info->filename);
    g_free(info->mimetype);
    g_string_free(info->artist, TRUE);
    g_string_free(info->album, TRUE);
    g_string_free(info->title, TRUE);
    info->duration = 0;
    info->bitrate = 0;
    g_string_free(info->formattedduration, TRUE);
    if(info->error != NULL) g_error_free(info->error);
    g_free(info);
}


MediaInfo*  
media_info_new(const gchar* mediafile)
{
    GB_LOG_FUNC 
    g_return_val_if_fail(mediafile != NULL, NULL);
    
    MediaInfo* info = g_new0(MediaInfo, 1);
    info->filename = g_strdup(mediafile);
    info->status = NOT_INSTALLED;
    info->artist = g_string_new("");
    info->album = g_string_new("");
    info->title = g_string_new("");
    info->duration = 0;
    info->bitrate = 0;
    info->formattedduration = g_string_new("");    
    info->mimetype = gbcommon_get_mime_type(mediafile);
    info->status = media_get_plugin_status(info->mimetype);
    if(info->status == INSTALLED)
        media_info_get_mediafile_info(info, mediafile);
    else 
        info->error = g_error_new(MEDIA_ERROR, MEDIA_ERROR_MISSING_PLUGIN, 
            _("The plugin to handle a file of type %s is not installed."), info->mimetype); 
    return info;
}


void 
media_info_create_inf_file(const MediaInfo* info, const int trackno, const gchar* inffile, int* trackstart)
{
    GB_LOG_FUNC
    const gint sectors = info->duration * 75;
    
    gchar* contents = g_strdup_printf(
        "#created by GnomeBaker\n"
        "#\n"
//        "CDINDEX_DISCID= 'JGlmiKWbkdhpVbENfKkJNnr37e8-'\n"
//        "CDDB_DISCID=    0xbd0cb80e\n"
        "MCN=\n"
        "ISRC=\n"
        "#\n"
        "Albumperformer= '%s'\n"
        "Performer=      '%s'\n"
        "Albumtitle=     '%s'\n"
        "Tracktitle=     '%s'\n"
        "Tracknumber=    %d\n"
        "Trackstart=     %d\n"
        "# track length in sectors (1/75 seconds each), rest samples\n"
        "Tracklength=    %d, 0\n"
        "Pre-emphasis=   no\n"
        "Channels=       2\n"
        "Copy_permitted= once (copyright protected)\n"
        "Endianess=      little\n"
        "# index list\n"
        "Index=          0\n",
        info->artist->str, 
        info->artist->str,
        info->album->str,
        info->title->str,
        trackno,
        *trackstart,
        sectors);
        
    *trackstart = sectors;
    /* g_file_set_contents(inffile, contents, -1, NULL); glib 2.8 */
    FILE* file = fopen(inffile, "w");
    if(file != NULL)
    {
        fwrite(contents, sizeof(gchar), strlen(contents), file);
        fclose(file);    
    }
    /* TODO check errors here */
    g_free(contents);
}


void
media_start_playing(const gchar* file) 
{
    GB_LOG_FUNC
    g_return_if_fail(file != NULL);
    
    gchar* mimetype = gbcommon_get_mime_type(file);
    if(media_get_plugin_status(mimetype) == INSTALLED)
    {
        if(mediaplayer == NULL)
        {
            mediaplayer = g_new0(MediaPipeline, 1);
            mediaplayer->pipeline = gst_thread_new ("mediaplayer");
            mediaplayer->source = gst_element_factory_make ("filesrc", "source");
            GstElement* spider = gst_element_factory_make ("spider", "spider");
            mediaplayer->dest = gst_element_factory_make ("osssink", "destination");
            gst_bin_add_many(GST_BIN (mediaplayer->pipeline), mediaplayer->source, 
                spider, mediaplayer->dest, NULL);    
            gst_element_link_many(mediaplayer->source, spider, mediaplayer->dest, NULL);
        }
        
        gst_element_set_state (mediaplayer->pipeline, GST_STATE_NULL); 
        g_object_set (G_OBJECT (mediaplayer->source), "location", file, NULL);
        gst_element_set_state (mediaplayer->pipeline, GST_STATE_PLAYING); 
    }
}


void
media_stop_playing()
{
    GB_LOG_FUNC
    gst_element_set_state (mediaplayer->pipeline, GST_STATE_NULL); 
}



