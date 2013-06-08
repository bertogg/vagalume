/*
 * protocol.h -- Last.fm legacy streaming protocol and XSPF
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <glib.h>
#include <libxml/parser.h>

#include "playlist.h"

typedef enum {
        LASTFM_ERR_NONE,
        LASTFM_ERR_CONN,
        LASTFM_ERR_LOGIN
} LastfmErr;

typedef struct {
        char *id;
        char *base_url;
        char *base_path;
        LastfmPls *custom_pls;
        gboolean free_streams;
} LastfmSession;

LastfmSession *
lastfm_session_new                      (const char *username,
                                         const char *password,
                                         const char *handshake_url,
                                         LastfmErr  *err,
                                         gboolean    free_streams);

LastfmPls *
lastfm_request_playlist                 (LastfmSession *s,
                                         gboolean       discovery,
                                         const char    *pls_title);

LastfmPls *
lastfm_parse_playlist                   (xmlDoc     *doc,
                                         const char *default_pls_title,
                                         gboolean    free_streams);

LastfmPls *
lastfm_request_custom_playlist          (LastfmSession *s,
                                         const char    *radio_url);

void
lastfm_session_destroy                  (LastfmSession *session);

gboolean
lastfm_set_radio                        (LastfmSession  *s,
                                         const char     *radio_url,
                                         char          **pls_title);

#endif
