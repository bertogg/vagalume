/*
 * xmlrpc.h -- XMLRPC-based functions to tag artists, tracks, albums,
 *             loves and hates
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#ifndef XMLRPC_H
#define XMLRPC_H

#include <glib.h>
#include "playlist.h"

typedef enum {
        REQUEST_ARTIST,
        REQUEST_TRACK,
        REQUEST_ALBUM
} request_type;

void tag_track(const char *user, const char *password,
               const lastfm_track *track, request_type type, GSList *tags);
void love_ban_track(const char *user, const char *password,
                    const lastfm_track *track, gboolean love);
void recommend_track(const char *user, const char *password,
                     const lastfm_track *track, const char *text,
                     request_type type, const char *rcpt);

#endif
