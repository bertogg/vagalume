/*
 * scrobbler.c -- Audioscrobbler Realtime Submission Protocol
 *
 * Version 1.2 implemented, see here:
 * http://www.audioscrobbler.net/development/protocol/
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#include <glib.h>
#include <string.h>

#include "scrobbler.h"
#include "http.h"
#include "globaldefs.h"

void
rsp_session_destroy(rsp_session *s)
{
        if (s == NULL) return;
        g_return_if_fail(s->id && s->np_url && s->post_url);
        g_free(s->id);
        g_free(s->np_url);
        g_free(s->post_url);
        g_free(s);
}

rsp_session *
rsp_session_copy(const rsp_session *s)
{
        if (s == NULL) return NULL;
        g_return_val_if_fail(s->id && s->np_url && s->post_url, NULL);
        rsp_session *s2 = g_new0(rsp_session, 1);
        s2->id = g_strdup(s->id);
        s2->np_url = g_strdup(s->np_url);
        s2->post_url = g_strdup(s->post_url);
        return s2;
}

rsp_session *
rsp_session_new(const char *username, const char *password,
                lastfm_err *err)
{
        g_return_val_if_fail(username != NULL && password != NULL, NULL);
        rsp_session *s = NULL;
        char *md5password, *timestamp, *auth1, *auth2, *url;
        char *buffer = NULL;
        md5password = get_md5_hash(password);
        timestamp = g_strdup_printf("%lu", time(NULL));
        auth1 = g_strconcat(md5password, timestamp, NULL);
        auth2 = get_md5_hash(auth1);
        url = g_strconcat("http://post.audioscrobbler.com/?hs=true"
                          "&p=1.2&c=tst&v=" APP_VERSION,
                          "&u=", username, "&t=", timestamp,
                          "&a=", auth2, NULL);
        http_get_buffer(url, &buffer, NULL);
        if (buffer == NULL) {
                g_warning("Unable to initiate rsp session");
                if (err != NULL) *err = LASTFM_ERR_CONN;
        } else {
                if (!strncmp(buffer, "OK\n", 3)) {
                        /* Split in 5 parts and not 4 to prevent
                           trailing garbage from going to r[3] */
                        char **r = g_strsplit(buffer, "\n", 5);
                        s = g_new0(rsp_session, 1);
                        if (r[0] && r[1] && r[2] && r[3]) {
                                s->id = g_strdup(r[1]);
                                s->np_url = g_strdup(r[2]);
                                s->post_url = g_strdup(r[3]);
                        }
                        g_strfreev(r);
                }
                if (!s || !(s->id) || !(s->np_url) || !(s->post_url)) {
                        g_warning("Error building rsp session");
                        if (err != NULL) *err = LASTFM_ERR_LOGIN;
                        rsp_session_destroy(s);
                } else if (err != NULL) {
                        *err = LASTFM_ERR_NONE;
                }
        }
        g_free(auth1);
        g_free(auth2);
        g_free(timestamp);
        g_free(md5password);
        g_free(url);
        g_free(buffer);
        return s;
}

void
rsp_set_nowplaying(const rsp_session *rsp, const lastfm_track *t)
{
        g_return_if_fail(rsp != NULL && t != NULL);
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
        } else if (retbuf != NULL) {
                g_debug("Problem setting Now Playing, response: %s", retbuf);
        } else {
                g_debug("Problem setting Now Playing, connection error?");
        }
        g_free(buffer);
        g_free(retbuf);
        g_free(duration);
        g_free(artist);
        g_free(title);
        g_free(album);
}

void
rsp_scrobble(const rsp_session *rsp, const lastfm_track *t, time_t start,
             rsp_rating rating)
{
        g_return_if_fail(rsp != NULL && t != NULL);
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
        } else if (retbuf != NULL) {
                g_debug("Problem scrobbling track, response: %s", retbuf);
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
}
