/*
 * protocol.h -- Last.fm streaming protocol and XSPF
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <glib.h>

#include "playlist.h"

typedef enum {
        LASTFM_ERR_NONE,
        LASTFM_ERR_CONN,
        LASTFM_ERR_LOGIN
} LastfmErr;

typedef struct {
        char *id;
        gboolean subscriber;
        char *base_url;
        char *base_path;
} LastfmSession;

LastfmSession *lastfm_session_new(const char *username,
                                   const char *password,
                                   LastfmErr *err);
LastfmPls *lastfm_request_playlist (LastfmSession *s, gboolean discovery,
                                    const char *pls_title);
LastfmPls *lastfm_request_custom_playlist(LastfmSession *s,
                                           const char *radio_url);
LastfmSession *lastfm_session_copy(const LastfmSession *session);
void lastfm_session_destroy(LastfmSession *session);
gboolean lastfm_set_radio (LastfmSession *s, const char *radio_url,
                           char **pls_title);

#endif
