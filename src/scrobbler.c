
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
        char *md5password, *strtimestamp, *auth1, *auth2, *url;
        char *buffer = NULL;
        time_t timestamp = time(NULL);
        md5password = get_md5_hash(password);
        strtimestamp = g_strdup_printf("%lu", timestamp);
        auth1 = g_strdup_printf("%s%s", md5password, strtimestamp);
        auth2 = get_md5_hash(auth1);
        url = g_strconcat("http://post.audioscrobbler.com/?hs=true"
                          "&p=1.2&c=tst&v=" APP_VERSION,
                          "&u=", username, "&t=", strtimestamp,
                          "&a=", auth2, NULL);
        http_get_buffer(url, &buffer, NULL);
        if (buffer == NULL) {
                g_warning("Unable to initiate rsp session");
                if (err != NULL) *err = LASTFM_ERR_CONN;
        } else {
                if (!strncmp(buffer, "OK\n", 3)) {
                        char **r = g_strsplit(buffer, "\n", 4);
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
        g_free(strtimestamp);
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
        char *artist, *title, *album;
        artist = t->artist ? escape_url(t->artist, TRUE) : g_strdup("");
        title = t->title ? escape_url(t->title, TRUE) : g_strdup("");
        album = t->album ? escape_url(t->album, TRUE) : g_strdup("");
        if (t->duration != 0) {
                duration = g_strdup_printf("&l=%u", t->duration / 1000);
        }
        buffer = g_strconcat("s=", rsp->id, "&a=", artist,
                             "&t=", title, "&b=", album,
                             "&m=&n=", duration, NULL);
        http_post_buffer(rsp->np_url, buffer, &retbuf);
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
rsp_scrobble(const rsp_session *rsp, const lastfm_track *t, time_t start)
{
        g_return_if_fail(rsp != NULL && t != NULL);
        char *buffer;
        char *retbuf = NULL;
        char *duration = NULL;
        char *timestamp = g_strdup_printf("%lu", start);
        char *artist, *title, *album;
        artist = t->artist ? escape_url(t->artist, TRUE) : g_strdup("");
        title = t->title ? escape_url(t->title, TRUE) : g_strdup("");
        album = t->album ? escape_url(t->album, TRUE) : g_strdup("");
        if (t->duration != 0) {
                duration = g_strdup_printf("&l[0]=%u", t->duration/1000);
        }
        buffer = g_strconcat("s=", rsp->id, "&a[0]=", artist,
                             "&t[0]=", title, "&b[0]=", album,
                             "&i[0]=", timestamp,
                             "&o[0]=L", t->trackauth,
                             "&n[0]=&m[0]=&r[0]=", duration, NULL);
        http_post_buffer(rsp->post_url, buffer, &retbuf);
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
