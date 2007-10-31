/*
 * audio.c -- All the audio related stuff
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#include "config.h"

#include <gst/gst.h>
#include <gtk/gtk.h>

#include "audio.h"
#include "controller.h"

static GstElement *pipeline = NULL;
static GstElement *source = NULL;
static GstElement *decoder = NULL;
static GstElement *sink = NULL;

static int failed_tracks = 0;

static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
        GMainLoop *loop = (GMainLoop *) data;

        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
                g_main_loop_quit (loop);
                gdk_threads_enter ();
                failed_tracks = 0;
                controller_skip_track();
                gdk_threads_leave ();
                break;
        case GST_MESSAGE_ERROR: {
                gchar *debug;
                GError *err;

                gst_message_parse_error (msg, &err, &debug);
                g_free (debug);

                g_warning ("Error: %s", err->message);
                g_error_free (err);

                g_main_loop_quit (loop);
                gdk_threads_enter ();
                if (++failed_tracks == 3) {
                        failed_tracks = 0;
                        controller_show_warning("Connection error");
                        controller_stop_playing();
                } else {
                        controller_skip_track();
                }
                gdk_threads_leave ();
                break;
        }
        case GST_MESSAGE_STATE_CHANGED:
        case GST_MESSAGE_TAG:
        case GST_MESSAGE_CLOCK_PROVIDE:
        case GST_MESSAGE_NEW_CLOCK:
                break;
        default:
                g_debug ("GStreamer message received: %d",
                         GST_MESSAGE_TYPE(msg));
                break;
        }

        return TRUE;
}


gboolean
lastfm_audio_init(void)
{
        GMainLoop *loop;
        GstBus *bus;
        /* initialize GStreamer */
        gst_init (NULL, NULL);
        loop = g_main_loop_new (NULL, FALSE);

        /* set up */
        pipeline = gst_pipeline_new (NULL);
        source = gst_element_factory_make ("gnomevfssrc", NULL);
#ifdef MAEMO
        sink = gst_element_factory_make ("dspmp3sink", NULL);
        decoder = sink; /* Unused, this is only for the assertions */
#else
        decoder = gst_element_factory_make ("mad", NULL);
        sink = gst_element_factory_make ("autoaudiosink", NULL);
#endif
        if (!pipeline || !source || !decoder || !sink) {
                g_critical ("Error creating GStreamer elements");
                return FALSE;
        }
        bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
        gst_bus_add_watch (bus, bus_call, loop);
        gst_object_unref (bus);

#ifdef MAEMO
        gst_bin_add_many (GST_BIN (pipeline), source, sink, NULL);
        gst_element_link_many (source, sink, NULL);
#else
        gst_bin_add_many (GST_BIN (pipeline), source, decoder, sink, NULL);
        gst_element_link_many (source, decoder, sink, NULL);
#endif
        return TRUE;
}

gboolean
lastfm_audio_play(const char *url)
{
        g_return_val_if_fail(pipeline && source && url, FALSE);
        g_object_set(G_OBJECT(source), "location", url, NULL);
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        return TRUE;
}

gboolean
lastfm_audio_stop(void)
{
        g_return_val_if_fail(pipeline != NULL, FALSE);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        return TRUE;
}

void
lastfm_audio_clear(void)
{
        g_return_if_fail(pipeline != NULL);
        gst_element_set_state (pipeline, GST_STATE_NULL);
        gst_object_unref (GST_OBJECT (pipeline));
        pipeline = NULL;
}
