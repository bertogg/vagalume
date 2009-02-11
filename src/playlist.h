/*
 * playlist.h -- Functions to control playlist objects
 *
 * Copyright (C) 2007-2008 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
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
        const char *stream_url;
        const char *title;
        guint id;
        const char *artist;
        guint artistid;
        const char *album; /* "" if empty, never NULL */
        /* "" if empty, can also be the _same pointer_ as artist */
        const char *album_artist;
        const char *pls_title;
        guint duration;
        const char *image_url;
        const char *image_data;
        size_t image_data_size;
        gboolean image_data_available;
        const char *trackauth;
        const char *free_track_url;
        gboolean dl_in_progress;
        gboolean custom_pls;
        /* Private */
        int refcount;
        GMutex *mutex;
} LastfmTrack;

typedef struct {
        GQueue *tracks;
} LastfmPls;

LastfmTrack *lastfm_track_new(void);
LastfmTrack *lastfm_track_ref(LastfmTrack *track);
void lastfm_track_unref(LastfmTrack *track);
void lastfm_track_set_cover_image(LastfmTrack *track,char *data,size_t size);
LastfmTrack *lastfm_pls_get_track(LastfmPls *pls);
void lastfm_pls_add_track(LastfmPls *pls, LastfmTrack *track);
guint lastfm_pls_size(LastfmPls *pls);
LastfmPls *lastfm_pls_new(void);
void lastfm_pls_clear(LastfmPls *pls);
void lastfm_pls_destroy(LastfmPls *pls);
void lastfm_pls_merge(LastfmPls *pls1, LastfmPls *pls2);

#endif
