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
 * File: media.c
 * Copyright: gnomebaker@curo.dk
 * Created on: Sun Feb  6 21:47:19 2005
 */


#include "gbcommon.h"
#include "media.h"

static GSList *media_registered_plugins = NULL;

#ifdef GST_010
static GstElement *play = NULL;
#else
static MediaPipeline *media_player = NULL;
#endif


static PluginInfo*
media_plugininfo_new(const gchar *mime_type, const gchar *plugin_name)
{
	GB_LOG_FUNC
	g_return_val_if_fail(mime_type != NULL,NULL);
	g_return_val_if_fail(plugin_name != NULL,NULL);

	PluginInfo *info = g_new0(PluginInfo,1);
	info->mime_type = g_string_new(mime_type);
	info->gst_plugin_name = g_string_new(plugin_name);
	info->status = NOT_INSTALLED;
	return info;
}


static void
media_plugininfo_delete(PluginInfo *info)
{
    GB_LOG_FUNC
    g_return_if_fail(info != NULL);

    g_string_free(info->mime_type, TRUE);
    g_string_free(info->gst_plugin_name, TRUE);
    g_free(info);
}


static void
media_info_get_wav_info(MediaInfo *info, const gchar *wav_file)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != info);
    g_return_if_fail(NULL != wav_file);

    /* Have a look for a .inf file corresponding to the wav_file
       so we can display the artist, album and track */
    gchar *inf_file = g_strdup(wav_file);
    gchar *extension = strstr(inf_file, ".wav");
    if(extension != NULL)
    {
        strcpy(extension, ".inf");
        gchar *inf_contents = NULL;
        gsize length = 0;

        /* read in the entire inf file */
        if(g_file_get_contents(inf_file, &inf_contents, &length, NULL))
        {
            const gint num_labels = 3;
            const gchar *labels[3] =
                { "Performer", "Albumtitle", "Tracktitle" };

            GString *copy_to[3] =
                { info->artist, info->album, info->title };

            gint i = 0;
            for(; i < num_labels; i++)
            {
                const gchar *label = strstr(inf_contents, labels[i]);
                if(label != NULL)
                {
                    const gchar *ptr = label;
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
                            g_string_append_c(copy_to[i], *ptr);

                        ptr++;
                    }
                }
            }
        }
        g_free(inf_contents);
    }
}


static void
media_info_set_formatted_length(MediaInfo *info)
{
    GB_LOG_FUNC
    g_return_if_fail(info != NULL);

    gint seconds = info->duration % 60;
    gint minutes = (info->duration-seconds) / 60;
    g_string_printf(info->formatted_duration, "%d:%.2d", minutes, seconds);
}


static void
media_tags_foreach(const GstTagList *list, const gchar *tag, MediaInfo *info)
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
       info->bit_rate = g_value_get_uint (val);
}


#ifdef GST_010


static gboolean
media_bus_callback(GstBus *bus, GstMessage *message, MediaInfo *info)
{
    switch (GST_MESSAGE_TYPE (message))
    {
    case GST_MESSAGE_ERROR:
    {
        gchar *debug = NULL;
        gst_message_parse_error(message, &info->error, &debug);
        g_warning("media_bus_callback - Error [%s] Debug [%s]\n", info->error->message, debug);
        g_free(debug);
        break;
    }
    case GST_MESSAGE_TAG:
    {
        GstTagList *tag_list = NULL;
        gst_message_parse_tag(message, &tag_list);
        gst_tag_list_foreach (tag_list, (GstTagForeachFunc) media_tags_foreach, info);
        break;
    }
    default:
        /* unhandled message */
        break;
    }
    return TRUE;
}


static void
media_new_decoded_pad(GstElement *decodebin, GstPad *pad, gboolean last, GstElement* destination)
{
    GB_LOG_FUNC

    GstCaps *caps = gst_pad_get_caps (pad);
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *mimetype = gst_structure_get_name(structure);
    if(g_str_has_prefix(mimetype, "audio/x-raw"))
    {
        GstPad *sink_pad = gst_element_get_pad(destination, "sink");
        gst_pad_link (pad, sink_pad);
        gst_object_unref(sink_pad);
    }
    gst_caps_unref(caps);
}

#else

static void
media_fakesink_end_of_stream(GstElement *element, gboolean *end_of_stream)
{
    GB_LOG_FUNC
    g_return_if_fail(end_of_stream != NULL);
    *end_of_stream = TRUE;
}


static void
media_fakesink_error(GstElement *element, GstElement *source,
             GError *error, gchar *debug, GError **gberror)
{
    GB_LOG_FUNC
    g_return_if_fail(error != NULL);
    g_return_if_fail(gberror != NULL);
    g_warning("media_fakesink_error - [%s]", error->message);
    *gberror = g_error_copy(error);
}


static void
media_fakesink_tag(GObject *pipeline, GstElement *source, GstTagList *tags, MediaInfo *info)
{
    gst_tag_list_foreach (tags, (GstTagForeachFunc) media_tags_foreach, info);
}


static void
media_type_found(GstElement *typefind, guint probability, GstCaps *caps, gpointer data)
{
    GB_LOG_FUNC
    gchar *type = gst_caps_to_string (caps);
    GB_TRACE("media_type_found - [%s] found, probability [%d%%]\n", type, probability);
    g_free (type);
    /*((MediaInfo*)data)->status = INSTALLED;*/
}


static void
media_fakesink_hand_off(GstElement *element, GstBuffer *buffer, GstPad *pad, gboolean *hand_off)
{
    GB_LOG_FUNC
    g_return_if_fail(hand_off != NULL);
    *hand_off = TRUE;
}


#endif


static void
media_info_get_mediafile_info(MediaInfo *info, const gchar *media_file)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != info);
    g_return_if_fail(NULL != media_file);

#ifdef GST_010
    GstElement *pipeline = gst_pipeline_new("pipeline");
    g_assert(pipeline);
    gst_bus_add_watch (gst_pipeline_get_bus (GST_PIPELINE (pipeline)), (GstBusFunc)media_bus_callback, info);
    GstElement *source = gst_element_factory_make ("filesrc", "source");
    g_assert(source);
    g_object_set (G_OBJECT (source), "location", media_file, NULL);
    GstElement *decodebin = gst_element_factory_make ("decodebin", "decodebin");
    g_assert(decodebin);
    GstElement *destination = gst_element_factory_make ("fakesink", "fakesink");
    g_assert(destination);
    g_signal_connect_object(decodebin, "new-decoded-pad", G_CALLBACK (media_new_decoded_pad), destination, 0);
    gst_bin_add_many(GST_BIN (pipeline), source, decodebin, destination, NULL);
    gst_element_link(source, decodebin);

    gst_element_set_state (pipeline, GST_STATE_PAUSED);
    gboolean cont = TRUE;
    do
    {
        /* It seems that we must still pump the message loop after we are to 
         * stop processing the pipeline as there may be tag events remaining */
        cont = (GST_STATE(pipeline) != GST_STATE_PAUSED) && (info->error == NULL);
        while(gtk_events_pending())
            gtk_main_iteration();
    } while (cont);

    if(info->duration == 0)
    {
        GstFormat format = GST_FORMAT_TIME;
        gint64 total = 0;
        gst_element_query_duration(destination, &format, &total);
        GB_TRACE("media_info_get_mediafile_info - track length [%ld]\n", (long int)(total / GST_SECOND));
        info->duration = total / GST_SECOND;
    }
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
#else
    GstElement *pipeline = gst_pipeline_new ("pipeline");
    GstElement *spider = gst_element_factory_make ("spider", "spider");
    g_assert (spider);
    GstElement *source = gst_element_factory_make ("filesrc", "source");
    g_assert (source);
    GstElement *type_find = gst_element_factory_make ("typefind", "typefind");
    g_assert (type_find);
    g_object_set (G_OBJECT (source), "location", media_file, NULL);
    GstElement *destination = gst_element_factory_make ("fakesink", "fakesink");
    g_assert (destination);

    gst_bin_add_many(GST_BIN (pipeline), source, type_find, spider, destination, NULL);
    gst_element_link_many (source, type_find, spider, NULL);

    GstCaps *filtercaps = gst_caps_new_simple ("audio/x-raw-int", NULL);
    gst_element_link_filtered (spider, destination, filtercaps);
    gst_caps_free(filtercaps);

    gboolean hand_off = FALSE, end_of_stream = FALSE;
    g_signal_connect(pipeline, "found-tag", G_CALLBACK(media_fakesink_tag), info);
    g_signal_connect(pipeline, "error", G_CALLBACK(media_fakesink_error), &info->error);
    g_object_set(G_OBJECT(destination), "signal-handoffs", TRUE, NULL);
    g_signal_connect(destination, "handoff", G_CALLBACK(media_fakesink_hand_off), &hand_off);
    g_signal_connect(destination, "eos", G_CALLBACK(media_fakesink_end_of_stream), &end_of_stream);
    g_signal_connect (type_find, "have-type", G_CALLBACK (media_type_found), info);

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* have to iterate a few times before the total time is set - signalled by hand_off */
    while(gst_bin_iterate (GST_BIN (pipeline)) && (info->error == NULL) && !hand_off && !end_of_stream);
    if(info->duration == 0)
    {
        GstFormat format = GST_FORMAT_TIME;
        gint64 total = 0;
        gst_element_query (destination, GST_QUERY_TOTAL, &format, &total);
        GB_TRACE("media_info_get_mediafile_info - track length [%ld]\n", (long int)(total / GST_SECOND));
        info->duration = total / GST_SECOND;
    }
    gst_element_set_state (pipeline, GST_STATE_NULL);
    g_object_unref(pipeline);
#endif

    if(info->error != NULL)
    {
        info->status = NOT_INSTALLED;
    }
    else
    {
        if(g_ascii_strcasecmp(info->mime_type, "audio/x-wav") == 0)
            media_info_get_wav_info(info, media_file);
        media_info_set_formatted_length(info);
    }

}


PluginStatus
media_get_plugin_status(const gchar *mime_type)
{
    GB_LOG_FUNC
    PluginStatus status = NOT_INSTALLED;
    g_return_val_if_fail(mime_type != NULL, status);

    const GSList *node = media_registered_plugins;
    for(; node != NULL; node = node->next)
    {
        PluginInfo *plugin_info = (PluginInfo*)node->data;
        GB_TRACE("media_get_plugin_status - plugin mime_type [%s] requested [%s]\n", plugin_info->mime_type->str, mime_type);
        if(g_ascii_strcasecmp(plugin_info->mime_type->str, mime_type) == 0)
        {
            status = plugin_info->status;
            break;
        }
    }
    return status;
}


static void
media_register_plugin(const gchar *mime_type, const gchar *plugin_name)
{
    GB_LOG_FUNC
    g_return_if_fail(mime_type != NULL);
    g_return_if_fail(plugin_name != NULL);

    PluginInfo *info = media_plugininfo_new(mime_type, plugin_name);
    GstElement *element = gst_element_factory_make(plugin_name, plugin_name);
    if(element != NULL)
    {
        info->status = INSTALLED;
        gst_object_unref (GST_OBJECT (element));
    }
    media_registered_plugins = g_slist_append(media_registered_plugins,info);
}


static void
media_player_pipeline_eos(GstElement *gst_element, gpointer user_data)
{
    GB_LOG_FUNC
    media_stop_playing();
}


static void
media_player_pipeline_error(GstElement *gst_element,
                        GstElement *element,
                        GError *error,
                        gchar *message,
                        gpointer user_data)
{
    GB_LOG_FUNC
    g_return_if_fail(error != NULL);
    g_warning("media_player_pipeline_error - [%s] [%s]", error->message, message);
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
    media_register_plugin("audio/mp4", "ffdec_mpeg4");
    media_register_plugin("audio/x-m4a", "ffdec_mpeg4");
    media_register_plugin("audio/x-ms-wma", "ffdec_wmav2");

}


void
media_finalise()
{
    GB_LOG_FUNC

    const GSList *node = media_registered_plugins;
    for(; node != NULL; node = node->next)
        media_plugininfo_delete((PluginInfo*)node->data);
    g_slist_free(media_registered_plugins);
#ifdef GST_010
    if(play != NULL)
        gst_object_unref (GST_OBJECT (play));
#else
    if(media_player != NULL)
    {
        gst_object_unref (GST_OBJECT (media_player->pipeline));
        g_free(media_player);
    }
#endif
}


void
media_info_delete(MediaInfo *info)
{
    GB_LOG_FUNC
    g_return_if_fail(NULL != info);

    g_free(info->file_name);
    g_free(info->mime_type);
    g_string_free(info->artist, TRUE);
    g_string_free(info->album, TRUE);
    g_string_free(info->title, TRUE);
    info->duration = 0;
    info->bit_rate = 0;
    g_string_free(info->formatted_duration, TRUE);
    if(info->error != NULL) g_error_free(info->error);
    g_free(info);
}


MediaInfo*
media_info_new(const gchar *media_file)
{
    GB_LOG_FUNC
    g_return_val_if_fail(media_file != NULL, NULL);

    MediaInfo *info = g_new0(MediaInfo, 1);
    info->file_name = g_strdup(media_file);
    info->status = NOT_INSTALLED;
    info->artist = g_string_new("");
    info->album = g_string_new("");
    info->title = g_string_new("");
    info->duration = 0;
    info->bit_rate = 0;
    info->formatted_duration = g_string_new("");
    info->mime_type = gbcommon_get_mime_type(media_file);
    info->status = media_get_plugin_status(info->mime_type);
    if(info->status == INSTALLED)
        media_info_get_mediafile_info(info, media_file);
    else
        info->error = g_error_new(MEDIA_ERROR, MEDIA_ERROR_MISSING_PLUGIN,
            _("The plugin to handle a file of type %s is not installed."), info->mime_type);
    return info;
}


void
media_info_create_inf_file(const MediaInfo *info, const int track_no, const gchar *inf_file, int *track_start)
{
    GB_LOG_FUNC
    const gint sectors = info->duration * 75;

    gchar *contents = g_strdup_printf(
        "#created by GnomeBaker\n"
        "#\n"
        /*"CDINDEX_DISCID= 'JGlmiKWbkdhpVbENfKkJNnr37e8-'\n"*/
        /*"CDDB_DISCID=    0xbd0cb80e\n"*/
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
        track_no,
        *track_start,
        sectors);

    *track_start = sectors;
    /* g_file_set_contents(inf_file, contents, -1, NULL); glib 2.8 */
    FILE *file = fopen(inf_file, "w");
    if(file != NULL)
    {
        fwrite(contents, sizeof(gchar), strlen(contents), file);
        fclose(file);
    }
    /* TODO check errors here */
    g_free(contents);
}


void
media_start_playing(const gchar *file)
{
    GB_LOG_FUNC
    g_return_if_fail(file != NULL);

    gchar *mime_type = gbcommon_get_mime_type(file);
    if(media_get_plugin_status(mime_type) == INSTALLED)
    {
#ifdef GST_010
        if(play != NULL)
        {
            media_stop_playing();
            gst_object_unref (GST_OBJECT (play));
        }
        play = gst_element_factory_make ("playbin", "play");
        gchar *uri = g_strdup_printf("file://%s", file);
        g_object_set (G_OBJECT (play), "uri", uri, NULL);
        /*gst_bus_add_watch (gst_pipeline_get_bus (GST_PIPELINE (play)),
                     my_bus_callback, loop);*/
        gst_element_set_state (play, GST_STATE_PLAYING);
        g_free(uri);
#else
        if(media_player != NULL)
        {
            media_stop_playing();
            gst_object_unref (GST_OBJECT (media_player->pipeline));
            g_free(media_player);
        }
        media_player = g_new0(MediaPipeline, 1);
        media_player->pipeline = gst_thread_new ("media_player");
        media_player->source = gst_element_factory_make ("filesrc", "source");
        g_object_set(G_OBJECT(media_player->source), "location", file, NULL);
        GstElement *spider = gst_element_factory_make ("spider", "spider");
        media_player->dest = gst_element_factory_make ("osssink", "destination");
        gst_bin_add_many(GST_BIN (media_player->pipeline), media_player->source,
                spider, media_player->dest, NULL);
        gst_element_link_many(media_player->source, spider, media_player->dest, NULL);
        g_signal_connect (media_player->pipeline, "error", G_CALLBACK(media_player_pipeline_error), NULL);
        g_signal_connect (media_player->pipeline, "eos", G_CALLBACK (media_player_pipeline_eos), NULL);
        gst_element_set_state(media_player->pipeline, GST_STATE_PLAYING);
#endif
    }
}


void
media_stop_playing()
{
    GB_LOG_FUNC
#ifdef GST_010
    if(play != NULL)
        gst_element_set_state (play, GST_STATE_NULL);
#else
    if(media_player != NULL)
        gst_element_set_state (media_player->pipeline, GST_STATE_NULL);
#endif
}



