/*
 * audio.c -- All the audio related stuff
 *
 * Copyright (C) 2007-2010, 2013 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "config.h"

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <string.h>

#include "audio.h"
#include "controller.h"
#include "http.h"
#include "util.h"
#include "compat.h"

#ifdef HAVE_DSPMP3SINK
static const char *default_decoders[] = { NULL };
static const char *default_sinks[] = { "dspmp3sink", NULL };
#else
static const char *default_decoders[] = { "decodebin", NULL };
static const char *default_sinks[] = { "autoaudiosink", NULL };
#endif

static GstElement *pipeline = NULL;
static GstElement *source = NULL;
static GstElement *decoder = NULL;
static GstElement *sink = NULL;

static volatile gboolean http_req_success = FALSE;

static int http_pipe[2] = { -1, -1 };
static GThread *http_thread = NULL;
static gboolean audio_started = FALSE;
static GCallback audio_started_callback = NULL;

static void
close_previous_playback                 (void)
{
        if (http_pipe[0] != -1) close(http_pipe[0]);
        if (http_pipe[1] != -1) close(http_pipe[1]);
        http_pipe[0] = http_pipe[1] = -1;
}

typedef struct {
        char *url;
        char *session_id;
} get_audio_thread_data;

static gpointer
get_audio_thread                        (gpointer userdata)
{
        g_return_val_if_fail(userdata != NULL && http_pipe[1] > 0, NULL);
        get_audio_thread_data *data = (get_audio_thread_data *) userdata;
        GSList *headers = NULL;
        char *cookie = NULL;
        http_req_success = FALSE;
        if (data->session_id != NULL) {
                cookie = g_strconcat("Cookie: Session=", data->session_id,
                                     NULL);
                headers = g_slist_append(headers, cookie);
        }
        http_req_success = http_get_to_fd (data->url, http_pipe[1], headers);
        if (!audio_started) http_req_success = FALSE;
        g_free(data->url);
        g_free(data->session_id);
        g_free(cookie);
        g_slice_free(get_audio_thread_data, data);
        g_slist_free(headers);
        return NULL;
}

static gboolean
audio_started_cb_handler                (gpointer data)
{
        GCallback callback = data;
        (*callback) ();
        return FALSE;
}

static gboolean
gst_eos_handler                         (gpointer data)
{
        static int failed_tracks = 0;
        if (http_req_success) {
                failed_tracks = 0;
                controller_skip_track ();
        } else if (++failed_tracks == 3) {
                failed_tracks = 0;
                controller_stop_playing ();
                controller_show_warning ("Connection error");
        } else {
                controller_skip_track ();
        }
        return FALSE;
}

static gboolean
bus_call                                (GstBus     *bus,
                                         GstMessage *msg,
                                         gpointer    data)
{
        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR: {
                gchar *debug;
                GError *err;

                gst_message_parse_error (msg, &err, &debug);
                g_free (debug);

                g_warning ("Error: %s", err->message);
                g_error_free (err);
        } /* No, I haven't forgotten the break here */
        case GST_MESSAGE_EOS:
                gdk_threads_add_idle (gst_eos_handler, NULL);
                break;
        case GST_MESSAGE_STATE_CHANGED:
                if (audio_started_callback != NULL) {
                        GstState st;
                        gst_message_parse_state_changed(msg, NULL, &st, NULL);
                        if (st == GST_STATE_PLAYING) {
                                gdk_threads_add_idle (audio_started_cb_handler,
                                                      audio_started_callback);
                                audio_started_callback = NULL;
                                audio_started = TRUE;
                        }
                }
                break;
        case GST_MESSAGE_TAG:
        case GST_MESSAGE_CLOCK_PROVIDE:
        case GST_MESSAGE_NEW_CLOCK:
        case GST_MESSAGE_STREAM_STATUS:
#if GST_VERSION_MICRO >= 13
        case GST_MESSAGE_ASYNC_DONE:
#endif
                break;
        default:
                g_debug ("GStreamer message received: %d",
                         GST_MESSAGE_TYPE(msg));
                break;
        }

        return TRUE;
}

static GstElement *
audio_element_create                    (const char **elem_names,
                                         const char  *envvar)
{
        g_return_val_if_fail(elem_names != NULL, NULL);
        GstElement *retval = NULL;
        const char *user_elem_name = NULL;

        if (envvar != NULL) user_elem_name = g_getenv(envvar);
        if (user_elem_name != NULL && !strcmp(user_elem_name, "none")) {
                return NULL;
        }

        /* Try to create the element given in the environment variable */
        if (user_elem_name != NULL && *user_elem_name != '\0') {
                retval = gst_element_factory_make(user_elem_name, NULL);
                g_debug("Creating GStreamer element %s: %s", user_elem_name,
                        retval ? "success" : "ERROR");
        } else {
                /* Else try each element in the list */
                int i;
                for (i = 0; !retval && elem_names[i]; i++) {
                        retval = gst_element_factory_make(elem_names[i], NULL);
                        g_debug("Creating GStreamer element %s: %s",
                                elem_names[i], retval ? "success" : "ERROR");
                }
        }
        return retval;
}

#ifndef HAVE_DSPMP3SINK
static GstElement *
audio_decoder_create                    (void)
{
        return audio_element_create(default_decoders, GST_DECODER_ENVVAR);
}

static void
pad_added_cb                            (GstElement *decoder,
                                         GstPad     *pad,
                                         GstElement *sink)
{
        GstPad *sinkpad = gst_element_get_static_pad (sink, "sink");
        if (!GST_PAD_IS_LINKED (sinkpad)) {
                gst_pad_link (pad, sinkpad);
        }
        g_object_unref (sinkpad);
}
#endif

const char *
lastfm_audio_default_decoder_name       (void)
{
        return default_decoders[0] ? default_decoders[0] : "none";
}

const char *
lastfm_audio_default_sink_name          (void)
{
        return default_sinks[0] ? default_sinks[0] : "none";
}

static GstElement *
audio_sink_create                       (void)
{
        return audio_element_create(default_sinks, GST_SINK_ENVVAR);
}

gboolean
lastfm_audio_init                       (void)
{
        GstBus *bus;
        /* initialize GStreamer */
        gst_init (NULL, NULL);

        /* set up */
        pipeline = gst_pipeline_new (NULL);
        source = gst_element_factory_make ("fdsrc", NULL);
#ifdef HAVE_DSPMP3SINK
        decoder = source; /* Unused, this is only for the assertions */
#else
        decoder = audio_decoder_create();
#endif
        sink = audio_sink_create();
        if (!pipeline || !source || !decoder || !sink) {
                g_critical ("Error creating GStreamer elements");
                return FALSE;
        }
        bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
        gst_bus_add_watch (bus, bus_call, NULL);
        gst_object_unref (bus);

#ifdef HAVE_DSPMP3SINK
        gst_bin_add_many (GST_BIN (pipeline), source, sink, NULL);
        gst_element_link_many (source, sink, NULL);
        lastfm_audio_set_volume(80);
#else
        gst_bin_add_many (GST_BIN (pipeline), source, decoder, sink, NULL);
        gst_element_link (source, decoder);
        g_signal_connect (decoder, "pad-added",
                          G_CALLBACK (pad_added_cb), sink);
#endif

        return TRUE;
}

gboolean
lastfm_audio_play                       (const char *url,
                                         GCallback   audio_started_cb,
                                         const char *session_id)
{
        g_return_val_if_fail(pipeline && source && url, FALSE);
        get_audio_thread_data *data = NULL;
        close_previous_playback();
        if (http_thread != NULL) g_thread_join(http_thread);
        http_thread = NULL;
        audio_started = FALSE;
        audio_started_callback = audio_started_cb;
        pipe(http_pipe);
        data = g_slice_new(get_audio_thread_data);
        data->session_id = g_strdup(session_id);
        data->url = g_strdup (url);
        http_thread = g_thread_create(get_audio_thread, data, TRUE, NULL);
        g_object_set(G_OBJECT(source), "fd", http_pipe[0], NULL);
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
#ifdef HAVE_DSPMP3SINK
        /* It seems that dspmp3sink ignores the previous volume level
         * when playing a new song. Workaround: change it and then
         * restore the original value */
        int volume = lastfm_audio_get_volume();
        lastfm_audio_set_volume(volume == 0 ? 1 : 0);
        lastfm_audio_set_volume(volume);
#endif
        return TRUE;
}

gboolean
lastfm_audio_stop                       (void)
{
        g_return_val_if_fail(pipeline != NULL, FALSE);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        close_previous_playback();
        return TRUE;
}

void
lastfm_audio_clear                      (void)
{
        g_return_if_fail(pipeline != NULL);
        gst_element_set_state (pipeline, GST_STATE_NULL);
        gst_object_unref (GST_OBJECT (pipeline));
        pipeline = NULL;
        close_previous_playback();
        if (http_thread != NULL) g_thread_join(http_thread);
}

int
lastfm_audio_get_running_time           (void)
{
        g_return_val_if_fail(pipeline != NULL, 0);
        gint64 t;
        GstFormat format = GST_FORMAT_TIME;
#if GST_CHECK_VERSION(1,0,0)
        if (gst_element_query_position(pipeline, format, &t)) {
#else
        if (gst_element_query_position(pipeline, &format, &t)) {
#endif
                /* Round to nearest integer */
                t = (t + 500000000) / 1000000000;
                return t;
        } else {
                return -1;
        }
}

int
lastfm_audio_get_volume                 (void)
{
        int vol = 0;
#ifdef HAVE_DSPMP3SINK
        g_return_val_if_fail(sink != NULL, 0);
        g_return_val_if_fail(g_object_class_find_property(
                                     G_OBJECT_GET_CLASS(sink), "volume"), 0);
        g_object_get(G_OBJECT(sink), "volume", &vol, NULL);
        vol = ((double)vol) / 655.35 + 0.5; /* Round the number */
#endif
        vol = CLAMP (vol, 0, 100);
        return vol;
}

void
lastfm_audio_set_volume                 (int vol)
{
#ifdef HAVE_DSPMP3SINK
        g_return_if_fail(sink != NULL);
        g_return_if_fail(g_object_class_find_property(
                                 G_OBJECT_GET_CLASS(sink),"volume"));
        vol *= 655.35;
        vol = CLAMP (vol, 0, 65535);
        g_object_set(G_OBJECT(sink), "volume", vol, NULL);
#endif
}

int
lastfm_audio_increase_volume            (int inc)
{
        lastfm_audio_set_volume(lastfm_audio_get_volume() + inc);
        return lastfm_audio_get_volume();
}
