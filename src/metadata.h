/*
 * metadata.h -- Last.fm metadata (friends, tags, ...)
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
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
                                    const lastfm_track *track,
                                    request_type req, GList **taglist);
gboolean lastfm_get_track_tags(const lastfm_track *track,
                               request_type req, GList **taglist);

#endif
