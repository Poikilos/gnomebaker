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

GSList *media_registered_plugins = NULL;


static PluginInfo* 
media_create_plugin_info(const gchar* mimetype,const gchar* pluginname)
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
media_info_get_wav_info(MediaInfo* self, const gchar* wavfile)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != self);
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
}


static void 
media_info_set_formatted_length(MediaInfo* info) 
{
    GB_LOG_FUNC
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
    *gberror = g_error_copy(error);
}


static void
media_tags_foreach(const GstTagList *list, const gchar *tag, MediaInfo* self)
{
   GB_LOG_FUNC
   int count = gst_tag_list_get_tag_size(list, tag);
   if (count < 1)
       return;

   const GValue *val  = gst_tag_list_get_value_index (list, tag, 0);
   if (g_ascii_strcasecmp (tag, GST_TAG_TITLE) == 0)
       g_string_assign(self->title, g_value_get_string (val));
   else if (g_ascii_strcasecmp (tag, GST_TAG_ARTIST) == 0)
       g_string_assign(self->artist, g_value_get_string (val));
   else if (g_ascii_strcasecmp (tag, GST_TAG_DURATION) == 0)
       self->duration = g_value_get_uint64 (val) / GST_SECOND;
   else if (g_ascii_strcasecmp (tag, GST_TAG_ALBUM) == 0)
       g_string_assign(self->album, g_value_get_string (val));
   else if (g_ascii_strcasecmp (tag, GST_TAG_BITRATE) == 0)
       self->bitrate = g_value_get_uint (val);
}


static void
media_fakesink_tag(GObject *pipeline, GstElement *source, GstTagList *tags, MediaInfo* self)
{
    gst_tag_list_foreach (tags, (GstTagForeachFunc) media_tags_foreach, self); 
}


static void
media_info_get_mediafile_info(MediaInfo* self, const gchar* mediafile)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != self);
    g_return_if_fail(NULL != mediafile);
   
    GstElement *pipeline = gst_pipeline_new ("pipeline");
    GstElement *spider = gst_element_factory_make ("spider", "spider");
    g_assert (spider);
    GstElement *source = gst_element_factory_make ("filesrc", "source");
    g_assert (source);
    GstElement *typefind = gst_element_factory_make ("typefind", "typefind");
    g_assert (typefind);
    g_object_set (G_OBJECT (source), "location", mediafile, NULL);
    GstElement *wavenc = gst_element_factory_make ("wavenc", "parser");
    g_assert (wavenc);
    GstElement *destination = gst_element_factory_make ("fakesink", "fakesink");
    g_assert (destination);
    gst_bin_add (GST_BIN (pipeline), source);
    gst_bin_add (GST_BIN (pipeline), typefind);
    gst_bin_add (GST_BIN (pipeline), spider);
    gst_bin_add (GST_BIN (pipeline), wavenc);
    gst_bin_add (GST_BIN (pipeline), destination);
    gst_element_link (source, typefind);
    gst_element_link (typefind, spider);
    gst_element_link (spider, wavenc);
    gst_element_link (wavenc, destination);
    
    GError* error = NULL;
    g_signal_connect(pipeline, "found-tag", G_CALLBACK(media_fakesink_tag), self);
    gboolean handoff = FALSE, endofstream = FALSE; 
    g_signal_connect(pipeline, "error", G_CALLBACK(media_fakesink_error), &error);
    g_object_set(G_OBJECT(destination), "signal-handoffs", TRUE, NULL);
    g_signal_connect(destination, "handoff", G_CALLBACK(media_fakesink_handoff), &handoff);
    g_signal_connect(destination, "eos", G_CALLBACK(media_fakesink_endofstream), &endofstream);
       
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
    /* have to iterate a few times before the total time is set - signalled by handoff */    
    while(gst_bin_iterate (GST_BIN (pipeline)) && (error == NULL) && !handoff && !endofstream);
    if(self->duration == 0)
    {
        GstFormat format = GST_FORMAT_TIME;
        gint64 total = 0;
        gst_element_query (destination, GST_QUERY_TOTAL, &format, &total);
        gst_element_set_state (pipeline, GST_STATE_NULL);
        GB_TRACE("*** track length [%ld]", (long int)(total / GST_SECOND));    
        self->duration = total / GST_SECOND;
    }
    
    if(g_ascii_strcasecmp(self->mimetype, "audio/x-wav") == 0)
        media_info_get_wav_info(self, mediafile);
    
    media_info_set_formatted_length(self);
    g_object_unref(pipeline);
}


void 
media_register_plugins()
{
    GB_LOG_FUNC    
    
    PluginInfo* info = media_create_plugin_info("application/x-id3","mad");
    PluginInfo* info2 = media_create_plugin_info("audio/mpeg","mad");
    PluginInfo* info3 = media_create_plugin_info("audio/x-mp3","mad");
    GstElement* element;        
    if((element = gst_element_factory_make("mad","mad")) != NULL)
    {
        info->status = INSTALLED;
        info2->status = INSTALLED;
        info3->status = INSTALLED;
        gst_object_unref (GST_OBJECT (element));
        element = NULL;
    }
    media_registered_plugins = g_slist_append(media_registered_plugins,info);
    media_registered_plugins = g_slist_append(media_registered_plugins,info2);
    media_registered_plugins = g_slist_append(media_registered_plugins,info3);

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
media_info_delete(MediaInfo* self)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != self);
        
    g_free(self->filename);
    g_free(self->mimetype);
    g_string_free(self->artist, TRUE);
    g_string_free(self->album, TRUE);
    g_string_free(self->title, TRUE);
    self->duration = 0;
    self->bitrate = 0;
    g_string_free(self->formattedduration, TRUE);
    g_free(self);
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
    
    GSList* node = media_registered_plugins;
    for(; node != NULL; node = node->next)
    {
        PluginInfo* plugininfo = (PluginInfo*)node->data;
        GB_TRACE("plugin mimetype [%s] requested [%s]", plugininfo->mimetype->str, info->mimetype);
        if(g_ascii_strcasecmp(plugininfo->mimetype->str, info->mimetype) == 0)
        {
            info->status = plugininfo->status;
            if(info->status == INSTALLED) 
                media_info_get_mediafile_info(info, mediafile);
            break;
        }
    }
    return info;
}


void 
media_info_create_inf_file(const MediaInfo* info, const int trackno, const gchar* inffile, int* trackstart)
{
    GB_LOG_FUNC
    const gint sectors = info->duration * 75;
//    gint alignment = 2352;
//    const gint originalsectors = info->duration * 75;
//    gint sectors = ((originalsectors % alignment>0) ? 
//        ((originalsectors / alignment) + 1) * alignment: originalsectors);
//    GB_TRACE("Track [%s] original sectors [%d] adjusted sectors [%d]", 
//        g_basename(info->filename), originalsectors, sectors);
    
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


