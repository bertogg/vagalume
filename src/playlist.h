/*
 * playlist.h -- Functions to control playlist objects
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <glib.h>

typedef struct {
        char *stream_url;
        char *title;
        guint id;
        char *artist;
        char *album;
        guint duration;
        char *image_url;
        char *trackauth;
        char *free_track_url;
} lastfm_track;

typedef struct {
        char *title;
        GQueue *tracks;
} lastfm_pls;

void lastfm_track_destroy(lastfm_track *track);
lastfm_track *lastfm_track_copy(const lastfm_track *track);
lastfm_track *lastfm_pls_get_track(lastfm_pls *pls);
void lastfm_pls_add_track(lastfm_pls *pls, lastfm_track *track);
guint lastfm_pls_size(lastfm_pls *pls);
lastfm_pls *lastfm_pls_new(const char *title);
void lastfm_pls_set_title(lastfm_pls *pls, const char *title);
void lastfm_pls_clear(lastfm_pls *pls);
void lastfm_pls_destroy(lastfm_pls *pls);
void lastfm_pls_merge(lastfm_pls *pls1, lastfm_pls *pls2);

#endif
