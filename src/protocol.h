/*
 * protocol.h -- Last.fm streaming protocol and XSPF
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

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
        gboolean subscriber;
        char *base_url;
        char *base_path;
} lastfm_session;

lastfm_session *lastfm_session_new(const char *username,
                                   const char *password,
                                   lastfm_err *err);
lastfm_pls *lastfm_request_playlist(lastfm_session *s, gboolean discovery);
lastfm_pls *lastfm_request_custom_playlist(lastfm_session *s,
                                           const char *radio_url);
lastfm_session *lastfm_session_copy(const lastfm_session *session);
void lastfm_session_destroy(lastfm_session *session);
gboolean lastfm_set_radio(lastfm_session *s, const char *radio_url);
GList *lastfm_get_friends(const char *username);

#endif
