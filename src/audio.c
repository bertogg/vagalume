/*
 * audio.c -- All the audio related stuff
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#include "config.h"

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include "audio.h"
#include "controller.h"
#include "http.h"

static const char *gst_sink_envvar = "VAGALUME_GST_SINK";
#ifdef MAEMO
static const char *default_sink = "dspmp3sink";
#else
static const char *default_sink = "autoaudiosink";
#endif

static GstElement *pipeline = NULL;
static GstElement *source = NULL;
static GstElement *decoder = NULL;
static GstElement *sink = NULL;

static int failed_tracks = 0;
static GMutex *failed_tracks_mutex = NULL;

static int http_pipe[2] = { -1, -1 };
static GThread *http_thread = NULL;
static gboolean audio_started = FALSE;
static GCallback audio_started_callback = NULL;
static GMainLoop *loop = NULL;

static void
close_previous_playback(void)
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
get_audio_thread(gpointer userdata)
{
        g_return_val_if_fail(userdata != NULL && http_pipe[1] > 0, NULL);
        get_audio_thread_data *data = (get_audio_thread_data *) userdata;
        GSList *headers = NULL;
        char *cookie = NULL;
        gboolean transfer_ok;
        if (data->session_id != NULL) {
                cookie = g_strconcat("Cookie: Session=", data->session_id,
                                     NULL);
                headers = g_slist_append(headers, cookie);
        }
        transfer_ok = http_get_to_fd(data->url, http_pipe[1], headers);
        if (!audio_started) transfer_ok = FALSE;
        g_free(data->url);
        g_free(data->session_id);
        g_free(data);
        g_free(cookie);
        g_slist_free(headers);
        g_mutex_lock(failed_tracks_mutex);
        failed_tracks = transfer_ok ? 0 : (failed_tracks + 1);
        g_mutex_unlock(failed_tracks_mutex);
        return NULL;
}

static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR: {
                gchar *debug;
                GError *err;

                gst_message_parse_error (msg, &err, &debug);
                g_free (debug);

                g_warning ("Error: %s", err->message);
                g_error_free (err);
                g_mutex_lock(failed_tracks_mutex);
                failed_tracks++;
                g_mutex_unlock(failed_tracks_mutex);
        } /* No, I haven't forgotten the break here */
        case GST_MESSAGE_EOS:
                g_main_loop_quit (loop);
                gdk_threads_enter ();
                if (failed_tracks == 3) {
                        g_mutex_lock(failed_tracks_mutex);
                        failed_tracks = 0;
                        g_mutex_unlock(failed_tracks_mutex);
                        controller_stop_playing();
                        controller_show_warning("Connection error");
                } else {
                        controller_skip_track();
                }
                gdk_threads_leave ();
                break;
        case GST_MESSAGE_STATE_CHANGED:
                if (audio_started_callback != NULL) {
                        GstState st;
                        gst_message_parse_state_changed(msg, NULL, &st, NULL);
                        if (st == GST_STATE_PLAYING) {
                                gdk_threads_enter();
                                (*audio_started_callback)();
                                audio_started_callback = NULL;
                                audio_started = TRUE;
                                gdk_threads_leave();
                        }
                }
                break;
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

static const char *
audio_sink_name(void)
{
        const char *user_sink = g_getenv(gst_sink_envvar);
        if (user_sink == NULL || *user_sink == '\0') {
                return default_sink;
        } else {
                return user_sink;
        }
}

gboolean
lastfm_audio_init(void)
{
        GstBus *bus;
        failed_tracks_mutex = g_mutex_new();
        /* initialize GStreamer */
        gst_init (NULL, NULL);
        loop = g_main_loop_new (NULL, FALSE);

        /* set up */
        pipeline = gst_pipeline_new (NULL);
        source = gst_element_factory_make ("fdsrc", NULL);
#ifdef MAEMO
        decoder = source; /* Unused, this is only for the assertions */
#else
        decoder = gst_element_factory_make ("mad", NULL);
#endif
        sink = gst_element_factory_make (audio_sink_name(), NULL);
        if (!pipeline || !source || !decoder || !sink) {
                g_critical ("Error creating GStreamer elements");
                return FALSE;
        }
        bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
        gst_bus_add_watch (bus, bus_call, NULL);
        gst_object_unref (bus);

#ifdef MAEMO
        gst_bin_add_many (GST_BIN (pipeline), source, sink, NULL);
        gst_element_link_many (source, sink, NULL);
#else
        gst_bin_add_many (GST_BIN (pipeline), source, decoder, sink, NULL);
        gst_element_link_many (source, decoder, sink, NULL);
#endif

        lastfm_audio_set_volume(80);
        return TRUE;
}

gboolean
lastfm_audio_play(const char *url, GCallback audio_started_cb,
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
        data = g_new(get_audio_thread_data, 1);
        data->session_id = g_strdup(session_id);
        data->url = g_strdup(url);
        http_thread = g_thread_create(get_audio_thread, data, TRUE, NULL);
        g_object_set(G_OBJECT(source), "fd", http_pipe[0], NULL);
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
#ifdef MAEMO
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
lastfm_audio_stop(void)
{
        g_return_val_if_fail(pipeline != NULL, FALSE);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        close_previous_playback();
        return TRUE;
}

void
lastfm_audio_clear(void)
{
        g_return_if_fail(pipeline != NULL);
        gst_element_set_state (pipeline, GST_STATE_NULL);
        gst_object_unref (GST_OBJECT (pipeline));
        pipeline = NULL;
        close_previous_playback();
        if (http_thread != NULL) g_thread_join(http_thread);
        g_main_loop_unref(loop);
}

int
lastfm_audio_get_running_time(void)
{
        g_return_val_if_fail(pipeline != NULL, 0);
        gint64 t;
        GstFormat format = GST_FORMAT_TIME;
        if (gst_element_query_position(pipeline, &format, &t)) {
                /* Round to nearest integer */
                t = (t + 500000000) / 1000000000;
                return t;
        } else {
                return -1;
        }
}

int
lastfm_audio_get_volume(void)
{
        int vol = 0;
#ifdef MAEMO
        g_return_val_if_fail(sink != NULL, 0);
        g_return_val_if_fail(g_object_class_find_property(
                                     G_OBJECT_GET_CLASS(sink), "volume"), 0);
        g_object_get(G_OBJECT(sink), "volume", &vol, NULL);
        vol = ((double)vol) / 655.35 + 0.5; /* Round the number */
        if (vol > 100) vol = 100;
        if (vol < 0) vol = 0;
#endif
        return vol;
}

void
lastfm_audio_set_volume(int vol)
{
#ifdef MAEMO
        g_return_if_fail(sink != NULL);
        g_return_if_fail(g_object_class_find_property(
                                 G_OBJECT_GET_CLASS(sink),"volume"));
        vol *= 655.35;
        if (vol > 65535) vol = 65535;
        if (vol < 0) vol = 0;
        g_object_set(G_OBJECT(sink), "volume", vol, NULL);
#endif
}

int
lastfm_audio_increase_volume(int inc)
{
        lastfm_audio_set_volume(lastfm_audio_get_volume() + inc);
        return lastfm_audio_get_volume();
}
