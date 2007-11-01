/*
 * tags.c -- Functions to tag artists, tracks and albums
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#ifndef TAGS_H
#define TAGS_H

#include <glib.h>
#include "playlist.h"

typedef enum {
        TAG_ARTIST,
        TAG_TRACK,
        TAG_ALBUM
} tag_type;

void tag_track(const char *user, const char *password,
               const lastfm_track *track, tag_type type, GSList *tags);

#endif
