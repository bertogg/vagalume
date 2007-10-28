#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <glib.h>

#include "playlist.h"

typedef enum {
        LASTFM_ERR_NONE,
        LASTFM_ERR_CONN,
        LASTFM_ERR_LOGIN
} lastfm_err;

typedef struct {
        char *id;
        char *stream_url;
        gboolean subscriber;
        char *base_url;
        char *base_path;
        lastfm_pls *playlist;
} lastfm_session;

lastfm_session *lastfm_session_new(const char *username,
                                   const char *password,
                                   lastfm_err *err);
gboolean lastfm_request_playlist(lastfm_session *s);
void lastfm_session_destroy(lastfm_session *session);
gboolean lastfm_set_radio(lastfm_session *s, const char *radio_url);
char *get_md5_hash(const char *str);

#endif
