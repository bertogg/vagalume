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

gboolean tag_track(const char *user, const char *password,
                   const lastfm_track *track, request_type type, GSList *tags);
gboolean love_ban_track(const char *user, const char *password,
                        const lastfm_track *track, gboolean love);
gboolean recommend_track(const char *user, const char *password,
                         const lastfm_track *track, const char *text,
                         request_type type, const char *rcpt);
gboolean add_to_playlist(const char *user, const char *password,
                         const lastfm_track *track);

#endif
