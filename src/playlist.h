/*
 * playlist.h -- Functions to control playlist objects
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <glib.h>

typedef enum {
        REQUEST_ARTIST,
        REQUEST_TRACK,
        REQUEST_ALBUM
} request_type;

typedef struct {
        char *stream_url;
        char *title;
        guint id;
        char *artist;
        char *album; /* "" if empty, never NULL */
        char *pls_title;
        guint duration;
        char *image_url;
        char *image_data;
        size_t image_data_size;
        gboolean image_data_available;
        char *trackauth;
        char *free_track_url;
        gboolean custom_pls;
        /* Private */
        int refcount;
        GMutex *mutex;
} lastfm_track;

typedef struct {
        GQueue *tracks;
} lastfm_pls;

lastfm_track *lastfm_track_new(void);
lastfm_track *lastfm_track_ref(lastfm_track *track);
void lastfm_track_unref(lastfm_track *track);
void lastfm_track_set_cover_image(lastfm_track *track,char *data,size_t size);
lastfm_track *lastfm_pls_get_track(lastfm_pls *pls);
void lastfm_pls_add_track(lastfm_pls *pls, lastfm_track *track);
guint lastfm_pls_size(lastfm_pls *pls);
lastfm_pls *lastfm_pls_new(void);
void lastfm_pls_clear(lastfm_pls *pls);
void lastfm_pls_destroy(lastfm_pls *pls);
void lastfm_pls_merge(lastfm_pls *pls1, lastfm_pls *pls2);

#endif
