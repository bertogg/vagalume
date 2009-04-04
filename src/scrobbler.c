/*
 * scrobbler.c -- Audioscrobbler Realtime Submission Protocol
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * Version 1.2 implemented, see here:
 * http://www.audioscrobbler.net/development/protocol/
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <glib.h>
#include <string.h>
#include <time.h>

#include "protocol.h"
#include "playlist.h"
#include "scrobbler.h"
#include "http.h"
#include "globaldefs.h"
#include "util.h"
#include "userconfig.h"

static const char rsp_session_url[] =
       "http://post.audioscrobbler.com/?hs=true&p=1.2"
       "&c=" LASTFM_APP_ID "&v=" LASTFM_APP_VERSION;

typedef enum {
        RSP_RESPONSE_OK,
        RSP_RESPONSE_BADSESSION,
        RSP_RESPONSE_ERROR
} RspResponse;

typedef struct {
        const char *id;
        const char *np_url;
        const char *post_url;
        const char *user;
        const char *pass;
        /* Private */
        int refcount;
} RspSession;

typedef struct {
        char *username;
        LastfmTrack *track;
        time_t start_time;
        RspRating rating;
} RspTrack;

static GCond *rsp_thread_cond = NULL;
static GMutex *rsp_mutex = NULL;
static GSList *rsp_queue = NULL;
static gboolean rsp_initialized = FALSE;
static RspSession *global_rsp_session = NULL;
static RspTrack *current_track = NULL;
static GString *username = NULL;
static GString *password = NULL;
static gboolean enable_scrobbling = FALSE;

static void
rsp_track_destroy                       (RspTrack *track)
{
        g_return_if_fail (track != NULL);
        g_free (track->username);
        lastfm_track_unref (track->track);
        g_slice_free (RspTrack, track);
}

static RspTrack *
rsp_track_new                           (const char  *user,
                                         LastfmTrack *track,
                                         time_t       start_time,
                                         RspRating    rating)
{
        RspTrack *t;
        g_return_val_if_fail (username && track && start_time > 0, NULL);
        t = g_slice_new (RspTrack);
        t->username = g_strdup (user);
        t->track = lastfm_track_ref (track);
        t->start_time = start_time;
        t->rating = rating;
        return t;
}

static void
rsp_session_unref                       (RspSession *s)
{
        g_return_if_fail (s != NULL);
        if (g_atomic_int_dec_and_test (&(s->refcount))) {
                g_free ((gpointer) s->id);
                g_free ((gpointer) s->np_url);
                g_free ((gpointer) s->post_url);
                g_free ((gpointer) s->user);
                g_free ((gpointer) s->pass);
                g_slice_free (RspSession, s);
        }
}

static RspSession *
rsp_session_ref                         (RspSession *s)
{
        g_return_val_if_fail (s != NULL, NULL);
        g_atomic_int_inc (&(s->refcount));
        return s;
}

static RspSession *
rsp_session_new                         (const char *username,
                                         const char *password,
                                         LastfmErr  *err)
{
        g_return_val_if_fail(username != NULL && password != NULL, NULL);
        RspSession *s = NULL;
        char *timestamp, *auth, *url;
        char *buffer = NULL;
        timestamp = g_strdup_printf("%lu", time(NULL));
        auth = compute_auth_token(password, timestamp);
        url = g_strconcat(rsp_session_url, "&u=", username, "&t=", timestamp,
                          "&a=", auth, NULL);
        http_get_buffer(url, &buffer, NULL);
        if (buffer == NULL) {
                g_warning("Unable to initiate rsp session");
                if (err != NULL) *err = LASTFM_ERR_CONN;
        } else {
                if (!strncmp(buffer, "OK\n", 3)) {
                        /* Split in 5 parts and not 4 to prevent
                           trailing garbage from going to r[3] */
                        char **r = g_strsplit(buffer, "\n", 5);
                        s = g_slice_new0(RspSession);
                        if (r[0] && r[1] && r[2] && r[3]) {
                                s->refcount = 1;
                                s->id = g_strdup(r[1]);
                                s->np_url = g_strdup(r[2]);
                                s->post_url = g_strdup(r[3]);
                                s->user = g_strdup (username);
                                s->pass = g_strdup (password);
                        }
                        g_strfreev(r);
                }
                if (!s || !(s->id) || !(s->np_url) || !(s->post_url)) {
                        g_warning("Error building rsp session");
                        if (err != NULL) *err = LASTFM_ERR_LOGIN;
                        if (s) {
                                rsp_session_unref (s);
                                s = NULL;
                        }
                } else if (err != NULL) {
                        *err = LASTFM_ERR_NONE;
                }
        }
        g_free(auth);
        g_free(timestamp);
        g_free(url);
        g_free(buffer);
        return s;
}

static RspResponse
rsp_set_nowplaying                      (const RspSession  *rsp,
                                         const LastfmTrack *t)
{
        g_return_val_if_fail(rsp != NULL && t != NULL, RSP_RESPONSE_ERROR);
        RspResponse retvalue = RSP_RESPONSE_ERROR;
        char *buffer;
        char *retbuf = NULL;
        char *duration = NULL;
        char *artist = escape_url(t->artist, TRUE);
        char *title = escape_url(t->title, TRUE);
        char *album = escape_url(t->album, TRUE);
        if (t->duration != 0) {
                duration = g_strdup_printf("&l=%u", t->duration / 1000);
        }
        buffer = g_strconcat("s=", rsp->id, "&a=", artist,
                             "&t=", title, "&b=", album,
                             "&m=&n=", duration, NULL);
        http_post_buffer(rsp->np_url, buffer, &retbuf, NULL);
        if (retbuf != NULL && !strncmp(retbuf, "OK", 2)) {
                g_debug("Correctly set Now Playing");
                retvalue = RSP_RESPONSE_OK;
        } else if (retbuf != NULL) {
                g_debug("Problem setting Now Playing, response: %s", retbuf);
                if (g_str_has_prefix (retbuf, "BADSESSION")) {
                        retvalue = RSP_RESPONSE_BADSESSION;
                }
        } else {
                g_debug("Problem setting Now Playing, connection error?");
        }
        g_free(buffer);
        g_free(retbuf);
        g_free(duration);
        g_free(artist);
        g_free(title);
        g_free(album);
        return retvalue;
}

static RspResponse
rsp_scrobble                            (const RspSession  *rsp,
                                         const LastfmTrack *t,
                                         time_t             start,
                                         RspRating          rating)
{
        g_return_val_if_fail(rsp != NULL && t != NULL, RSP_RESPONSE_ERROR);
        RspResponse retvalue = RSP_RESPONSE_ERROR;
        char *buffer;
        char *retbuf = NULL;
        char *duration = NULL;
        char *timestamp = g_strdup_printf("%lu", start);
        char *artist = escape_url(t->artist, TRUE);
        char *title = escape_url(t->title, TRUE);
        char *album = escape_url(t->album, TRUE);
        char *ratingstr;
        switch (rating) {
        case RSP_RATING_LOVE:
                ratingstr = "L"; break;
        case RSP_RATING_BAN:
                ratingstr = "B"; break;
        case RSP_RATING_SKIP:
                ratingstr = "S"; break;
        default:
                ratingstr = ""; break;
        }
        if (t->duration != 0) {
                duration = g_strdup_printf("&l[0]=%u", t->duration/1000);
        }
        buffer = g_strconcat("s=", rsp->id, "&a[0]=", artist,
                             "&t[0]=", title, "&b[0]=", album,
                             "&i[0]=", timestamp,
                             "&o[0]=L", t->trackauth,
                             "&n[0]=&m[0]=&r[0]=", ratingstr,
                             duration, NULL);
        http_post_buffer(rsp->post_url, buffer, &retbuf, NULL);
        if (retbuf != NULL && !strncmp(retbuf, "OK", 2)) {
                g_debug("Track scrobbled");
                retvalue = RSP_RESPONSE_OK;
        } else if (retbuf != NULL) {
                g_debug("Problem scrobbling track, response: %s", retbuf);
                if (g_str_has_prefix (retbuf, "BADSESSION")) {
                        retvalue = RSP_RESPONSE_BADSESSION;
                }
        } else {
                g_debug("Problem scrobbling track, connection error?");
        }
        g_free(buffer);
        g_free(retbuf);
        g_free(duration);
        g_free(timestamp);
        g_free(artist);
        g_free(title);
        g_free(album);
        return retvalue;
}

/* Clears rsp_global_session if it has the same id as @session */
static void
rsp_global_session_clear                (const RspSession *session)
{
        g_return_if_fail (session != NULL);
        g_mutex_lock (rsp_mutex);
        if (global_rsp_session != NULL &&
            g_str_equal (global_rsp_session->id, session->id)) {
                rsp_session_unref (global_rsp_session);
                global_rsp_session = NULL;
        }
        g_mutex_unlock (rsp_mutex);
}

static RspSession *
rsp_session_get_or_renew                (void)
{
        RspSession *session;

        g_mutex_lock (rsp_mutex);
        if (global_rsp_session) {
                session = rsp_session_ref (global_rsp_session);
        } else {
                char *user = g_strdup (username->str);
                char *pass = g_strdup (password->str);
                g_mutex_unlock (rsp_mutex);
                session = rsp_session_new (user, pass, NULL);
                g_free (user);
                g_free (pass);
                g_mutex_lock (rsp_mutex);
                if (session) {
                        if (global_rsp_session) {
                                rsp_session_unref (global_rsp_session);
                        }
                        global_rsp_session = rsp_session_ref (session);
                }
        }
        g_mutex_unlock (rsp_mutex);

        return session;
}

static void
rsp_scrobbler_thread_scrobble           (RspTrack *track)
{
        RspSession *s;
        int sleep_seconds = 0;

        g_return_if_fail (track != NULL);

        /* Get RSP session (or create one if necessary) */
        s = rsp_session_get_or_renew ();

        /* If there's no session, don't try to scrobble anything */
        if (s == NULL) {
                sleep_seconds = 5;
                track = NULL;
        }

        while (track != NULL) {
                RspResponse ret;

                /* Love/ban if necessary */
                if (track->rating == RSP_RATING_LOVE) {
                        love_ban_track (s->user, s->pass, track->track, TRUE);
                } else if (track->rating == RSP_RATING_BAN) {
                        love_ban_track (s->user, s->pass, track->track, FALSE);
                }

                /* Scrobble track */
                ret = rsp_scrobble (s, track->track,
                                    track->start_time, track->rating);

                if (ret == RSP_RESPONSE_OK) {
                        rsp_track_destroy (track);
                        g_mutex_lock (rsp_mutex);
                        rsp_queue = g_slist_delete_link (rsp_queue, rsp_queue);
                        track = rsp_queue ? rsp_queue->data : NULL;
                        g_mutex_unlock (rsp_mutex);
                } else if (ret == RSP_RESPONSE_BADSESSION) {
                        rsp_global_session_clear (s);
                        track = NULL;
                        sleep_seconds = 5;
                } else {
                        /* Server down? Try again later */
                        sleep_seconds = 60;
                }
        }

        if (sleep_seconds > 0) {
                g_debug ("Sleeping for %d seconds before retrying",
                         sleep_seconds);
                g_usleep (sleep_seconds * G_USEC_PER_SEC);
        }

        if (s != NULL) {
                rsp_session_unref (s);
        }
}

static gpointer
rsp_scrobbler_thread                    (gpointer data)
{
        while (rsp_initialized) {
                RspTrack *track;

                g_mutex_lock (rsp_mutex);
                /* Wait till there's a track in the scrobbling queue */
                while (rsp_queue == NULL) {
                        g_cond_wait (rsp_thread_cond, rsp_mutex);
                }
                track = rsp_queue->data;

                /* If that track belongs to another user, ignore it */
                if (g_ascii_strcasecmp (track->username, username->str)) {
                        g_debug ("Not scrobbling track played by %s "
                                 "(current user is %s)",
                                 track->username, username->str);
                        rsp_track_destroy (track);
                        track = NULL;
                        rsp_queue = g_slist_delete_link (rsp_queue, rsp_queue);
                }
                g_mutex_unlock (rsp_mutex);

                if (track) {
                        rsp_scrobbler_thread_scrobble (track);
                }
        }
        g_string_free (username, TRUE);
        g_string_free (password, TRUE);
        g_mutex_free (rsp_mutex);
        g_cond_free (rsp_thread_cond);
        username = password = NULL;
        rsp_mutex = NULL;
        rsp_thread_cond = NULL;
        return NULL;
}

static gpointer
set_nowplaying_thread                   (gpointer data)
{
        LastfmTrack *track = (LastfmTrack *) data;
        RspSession *session = NULL;
        gboolean set_nowplaying = FALSE;

        g_return_val_if_fail (track != NULL, NULL);

        g_usleep (10 * G_USEC_PER_SEC);

        g_mutex_lock (rsp_mutex);
        if (current_track && current_track->track->id == track->id) {
                g_mutex_unlock (rsp_mutex);
                session = rsp_session_get_or_renew ();
                g_mutex_lock (rsp_mutex);
                set_nowplaying = (session != NULL);
        }
        g_mutex_unlock (rsp_mutex);

        if (set_nowplaying) {
                RspResponse ret = rsp_set_nowplaying (session, track);
                if (ret == RSP_RESPONSE_BADSESSION) {
                        rsp_global_session_clear (session);
                        rsp_session_unref (session);
                        session = rsp_session_get_or_renew ();
                        if (session) {
                                rsp_set_nowplaying (session, track);
                        }
                }
        }

        if (session) {
                rsp_session_unref (session);
        }

        lastfm_track_unref (track);
        return NULL;
}

static void
usercfg_changed_cb                      (VglController *ctrl,
                                         VglUserCfg    *cfg,
                                         gpointer       data)
{
        gboolean changed = FALSE;
        g_mutex_lock (rsp_mutex);
        if (!g_str_equal (cfg->username, username->str)) {
                g_string_assign (username, cfg->username);
                changed = TRUE;
        }
        if (!g_str_equal (cfg->password, password->str)) {
                g_string_assign (password, cfg->password);
                changed = TRUE;
        }
        enable_scrobbling = cfg->enable_scrobbling;
        if (changed && global_rsp_session) {
                rsp_session_unref (global_rsp_session);
                global_rsp_session = NULL;
        }
        g_mutex_unlock (rsp_mutex);
}

static void
track_stopped_cb                        (VglController *ctrl,
                                         RspRating      rating,
                                         gpointer       data)
{
        g_mutex_lock (rsp_mutex);
        if (enable_scrobbling && current_track &&
            current_track->track->duration > 30000) {
                RspTrack *rsp_track;
                int played = time (NULL) - current_track->start_time;
                if (rating == RSP_RATING_NONE && played < 240 &&
                    played < current_track->track->duration/2000) {
                        rating = RSP_RATING_SKIP;
                }
                rsp_track = current_track;
                rsp_track->rating = rating;
                current_track = NULL;
                rsp_queue = g_slist_append (rsp_queue, rsp_track);
                g_cond_signal (rsp_thread_cond);
        }

        /* Clear track */
        if (current_track) {
                rsp_track_destroy (current_track);
                current_track = NULL;
        }
        g_mutex_unlock (rsp_mutex);
}

static void
track_started_cb                        (VglController *ctrl,
                                         LastfmTrack   *track,
                                         gpointer       data)
{
        g_mutex_lock (rsp_mutex);
        if (current_track) {
                rsp_track_destroy (current_track);
        }
        current_track = rsp_track_new (username->str, track, time (NULL),
                                       RSP_RATING_NONE);
        g_mutex_unlock (rsp_mutex);
        if (enable_scrobbling) {
                g_thread_create (set_nowplaying_thread,
                                 lastfm_track_ref (track), FALSE, NULL);
        }
}

static void
controller_destroyed_cb                 (gpointer  data,
                                         GObject  *controller)
{
        /* rsp_scrobbler_thread will destroying everything else */
        rsp_initialized = FALSE;
}

void
rsp_init                                (VglController *controller)
{
        g_return_if_fail (!rsp_initialized);
        g_return_if_fail (VGL_IS_CONTROLLER (controller));
        rsp_initialized = TRUE;
        rsp_mutex = g_mutex_new ();
        rsp_thread_cond = g_cond_new ();
        username = g_string_sized_new (20);
        password = g_string_sized_new (20);
        g_signal_connect (controller, "usercfg-changed",
                          G_CALLBACK (usercfg_changed_cb), NULL);
        g_signal_connect (controller, "track-stopped",
                          G_CALLBACK (track_stopped_cb), NULL);
        g_signal_connect (controller, "track-started",
                          G_CALLBACK (track_started_cb), NULL);
        g_object_weak_ref (G_OBJECT (controller),
                           controller_destroyed_cb, NULL);
        g_thread_create (rsp_scrobbler_thread, NULL, FALSE, NULL);
}
