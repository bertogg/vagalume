/*
 * metadata.h -- Last.fm metadata (friends, tags, ...)
 *
 * Copyright (C) 2007-2008 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef METADATA_H
#define METADATA_H

#include "playlist.h"
#include <glib.h>

gboolean lastfm_get_friends(const char *username, GList **friendlist);
gboolean lastfm_get_user_tags(const char *username, GList **taglist);
gboolean lastfm_get_user_track_tags(const char *username,
                                    const LastfmTrack *track,
                                    request_type req, GList **taglist);
gboolean lastfm_get_track_tags(const LastfmTrack *track,
                               request_type req, GList **taglist);
void lastfm_get_track_cover_image(LastfmTrack *track);

#endif
