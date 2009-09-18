/*
 * audio.c -- All the audio related stuff
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "config.h"

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_GST_MIXER
#    include <gst/interfaces/mixer.h>
#endif

#include "audio.h"
#include "controller.h"
#include "http.h"
#include "util.h"
#include "compat.h"

#ifdef MAEMO
static const char *default_decoders[] = { NULL };
static const char *default_sinks[] = { "dspmp3sink", NULL };
static const char *default_converters[] = { NULL };
#else
static const char *default_decoders[] = { "mad", "flump3dec", NULL };
static const char *default_sinks[] = { "gconfaudiosink", NULL };
static const char *default_converters[] = { "audioconvert", NULL };
#endif

static GstElement *pipeline = NULL;
static GstElement *source = NULL;
static GstElement *decoder = NULL;
static GstElement *convert = NULL;
static GstElement *sink = NULL;

#ifdef HAVE_GST_MIXER
static const char *default_mixers[] = { "alsamixer", "ossmixer", NULL };
typedef struct {
        GstMixer *mixer;
        GstMixerTrack *track;
        int *vols;
} VglMixer;
static VglMixer mixer = { 0 };
#else
static const char *default_mixers[] = { NULL };
#endif

static int failed_tracks = 0;

static int http_pipe[2] = { -1, -1 };
static GThread *http_thread = NULL;
static gboolean audio_started = FALSE;
static GCallback audio_started_callback = NULL;
static GMainLoop *loop = NULL;

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
        g_free(cookie);
        g_slice_free(get_audio_thread_data, data);
        g_slist_free(headers);
        if (transfer_ok) {
                g_atomic_int_set (&failed_tracks, 0);
        } else {
                g_atomic_int_inc (&failed_tracks);
        }
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
        if (failed_tracks == 3) {
                g_atomic_int_set (&failed_tracks, 0);
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
                g_atomic_int_inc (&failed_tracks);
        } /* No, I haven't forgotten the break here */
        case GST_MESSAGE_EOS:
                g_main_loop_quit (loop);
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

#ifndef MAEMO
static GstElement *
audio_decoder_create                    (void)
{
        return audio_element_create(default_decoders, GST_DECODER_ENVVAR);
}

static GstElement *
audio_convert_create                    (void)
{
        return audio_element_create(default_converters, GST_CONVERT_ENVVAR);
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

const char *
lastfm_audio_default_convert_name       (void)
{
        return default_converters[0] ? default_converters[0] : "none";
}

const char *
lastfm_audio_default_mixer_name         (void)
{
        return default_mixers[0] ? default_mixers[0] : "none";
}

static GstElement *
audio_sink_create                       (void)
{
        return audio_element_create(default_sinks, GST_SINK_ENVVAR);
}

#ifdef HAVE_GST_MIXER
static void
audio_mixer_init                        (VglMixer *mixer)
{
        GstElement *elem = NULL;
        GstMixerTrack *track = NULL;
        int *vols = NULL;

        g_return_if_fail (mixer != NULL);

        elem = audio_element_create (default_mixers, GST_MIXER_ENVVAR);
        if (elem != NULL) {
                gst_element_set_state (elem, GST_STATE_READY);
        }

        if (GST_IS_MIXER (elem)) {
                const GList *tracks, *i;
                tracks = gst_mixer_list_tracks (GST_MIXER (elem));

                /* Look for the PCM track */
                for (i = tracks; i != NULL && track == NULL; i = i->next) {
                        GstMixerTrack *t = GST_MIXER_TRACK (i->data);
                        if (!g_ascii_strcasecmp (t->label, "PCM")) {
                                track = t;
                        }
                }

                /* If the PCM track was not found, use the first one */
                if (track == NULL && tracks != NULL) {
                        track = GST_MIXER_TRACK (tracks->data);
                }

                if (track == NULL) {
                        gst_element_set_state (elem, GST_STATE_NULL);
                        gst_object_unref (GST_OBJECT (elem));
                        elem = NULL;
                } else {
                        vols = g_new (int, track->num_channels);
                        g_debug ("Using %s mixer track", track->label);
                }
        }

        if (track == NULL) {
                g_debug ("No mixer found");
        }

        mixer->mixer = GST_MIXER (elem);
        mixer->track = track;
        mixer->vols = vols;
}
#endif /* HAVE_GST_MIXER */

gboolean
lastfm_audio_init                       (void)
{
        GstBus *bus;
        /* initialize GStreamer */
        gst_init (NULL, NULL);
        loop = g_main_loop_new (NULL, FALSE);

        /* set up */
#ifdef HAVE_GST_MIXER
        audio_mixer_init (&mixer);
#endif
        pipeline = gst_pipeline_new (NULL);
        source = gst_element_factory_make ("fdsrc", NULL);
#ifdef MAEMO
        decoder = source; /* Unused, this is only for the assertions */
#else
        decoder = audio_decoder_create();
        convert = audio_convert_create(); /* This is optional */
#endif
        sink = audio_sink_create();
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
        lastfm_audio_set_volume(80);
#else
        if (convert != NULL) {
                gst_bin_add_many (GST_BIN (pipeline), source, decoder,
                                  convert, sink, NULL);
                gst_element_link_many (source, decoder, convert, sink, NULL);
        } else {
                gst_bin_add_many (GST_BIN (pipeline), source, decoder,
                                  sink, NULL);
                gst_element_link_many (source, decoder, sink, NULL);
        }
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
        /* This is a hack to make Jamendo stream music in MP3, which is
           the only format that Vagalume currently supports */
        data->url = string_replace (url, "streamencoding=ogg2",
                                    "streamencoding=mp31");
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
#ifdef HAVE_GST_MIXER
        if (mixer.mixer != NULL) {
                gst_element_set_state (GST_ELEMENT (mixer.mixer),
                                       GST_STATE_NULL);
                gst_object_unref (GST_OBJECT (mixer.mixer));
        }
        g_free (mixer.vols);
        memset (&mixer, 0, sizeof (mixer));
#endif
        g_main_loop_unref(loop);
}

int
lastfm_audio_get_running_time           (void)
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
lastfm_audio_get_volume                 (void)
{
        int vol = 0;
#ifdef MAEMO
        g_return_val_if_fail(sink != NULL, 0);
        g_return_val_if_fail(g_object_class_find_property(
                                     G_OBJECT_GET_CLASS(sink), "volume"), 0);
        g_object_get(G_OBJECT(sink), "volume", &vol, NULL);
        vol = ((double)vol) / 655.35 + 0.5; /* Round the number */
#elif defined(HAVE_GST_MIXER)
        if (mixer.track != NULL) {
                double dvol = 0;
                int i;
                gst_mixer_get_volume (mixer.mixer, mixer.track, mixer.vols);
                for (i = 0; i < mixer.track->num_channels; i++) {
                        dvol += mixer.vols[i];
                }
                dvol /= mixer.track->num_channels;
                dvol = 100.0 * (dvol - mixer.track->min_volume) /
                        (mixer.track->max_volume - mixer.track->min_volume);
                vol = dvol + 0.5; /* Round the number */
        }
#endif
        vol = CLAMP (vol, 0, 100);
        return vol;
}

void
lastfm_audio_set_volume                 (int vol)
{
#ifdef MAEMO
        g_return_if_fail(sink != NULL);
        g_return_if_fail(g_object_class_find_property(
                                 G_OBJECT_GET_CLASS(sink),"volume"));
        vol *= 655.35;
        vol = CLAMP (vol, 0, 65535);
        g_object_set(G_OBJECT(sink), "volume", vol, NULL);
#elif defined(HAVE_GST_MIXER)
        if (mixer.mixer != NULL) {
                double dvol;
                int i;
                dvol = (mixer.track->max_volume - mixer.track->min_volume) *
                        vol / 100;
                dvol += mixer.track->min_volume;
                dvol = CLAMP (dvol, mixer.track->min_volume,
                              mixer.track->max_volume);
                for (i = 0; i < mixer.track->num_channels; i++) {
                        mixer.vols[i] = dvol + 0.5;  /* Round the number */
                }
                gst_mixer_set_volume (mixer.mixer, mixer.track, mixer.vols);
        }
#endif
}

int
lastfm_audio_increase_volume            (int inc)
{
        lastfm_audio_set_volume(lastfm_audio_get_volume() + inc);
        return lastfm_audio_get_volume();
}
