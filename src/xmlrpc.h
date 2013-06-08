/*
 * xmlrpc.h -- Legacy XMLRPC-based functions to tag artists, tracks,
 *             albums, loves and hates
 *
 * Copyright (C) 2007-2008 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef XMLRPC_H
#define XMLRPC_H

#include <glib.h>
#include "playlist.h"

gboolean
tag_track                               (const char           *user,
                                         const char           *password,
                                         const LastfmTrack    *track,
                                         LastfmTrackComponent  type,
                                         GSList               *tags);

gboolean
recommend_track                         (const char           *user,
                                         const char           *password,
                                         const LastfmTrack    *track,
                                         const char           *text,
                                         LastfmTrackComponent  type,
                                         const char           *rcpt);

gboolean
add_to_playlist                         (const char        *user,
                                         const char        *password,
                                         const LastfmTrack *track);

#endif
