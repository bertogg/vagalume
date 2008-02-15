/*
 * playlist.c -- Functions to control playlist objects
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include "playlist.h"

/**
 * Destroy a lastfm_track object freeing all its allocated memory
 * @param track Track to be destroyed, or NULL
 */
void
lastfm_track_destroy(lastfm_track *track)
{
        if (track == NULL) return;
        g_free(track->stream_url);
        g_free(track->title);
        g_free(track->artist);
        g_free(track->album);
        g_free(track->pls_title);
        g_free(track->image_url);
        g_free(track->trackauth);
        g_free(track->free_track_url);
        g_free(track);
}

/**
 * Copy a lastfm_track object
 * @param track The object to copy, or NULL
 * @return A newly created lastfm_track object, or NULL
 */
lastfm_track *
lastfm_track_copy(const lastfm_track *track)
{
        if (track == NULL) return NULL;
        lastfm_track *ret;
        ret = g_new0(lastfm_track, 1);
        *ret = *track;
        ret->stream_url = g_strdup(track->stream_url);
        ret->title = g_strdup(track->title);
        ret->artist = g_strdup(track->artist);
        ret->album = g_strdup(track->album);
        ret->pls_title = g_strdup(track->pls_title);
        ret->image_url = g_strdup(track->image_url);
        ret->trackauth = g_strdup(track->trackauth);
        ret->free_track_url = g_strdup(track->free_track_url);
        return ret;
}

/**
 * Get the size of a playlist
 * @param pls The playlist
 * @return The number of tracks in the playlist
 */
guint
lastfm_pls_size(lastfm_pls *pls)
{
        g_return_val_if_fail(pls != NULL, 0);
        return g_queue_get_length(pls->tracks);
}

/**
 * Get the next track in a playlist. Note that this function removes
 * the track from the playlist (as it can only be played once).
 * @param pls The playlist
 * @return The next track (or NULL if the list is empty). It should be
 * destroyed with lastfm_track_destroy() when no longer used.
 */
lastfm_track *
lastfm_pls_get_track(lastfm_pls *pls)
{
        g_return_val_if_fail(pls != NULL, NULL);
        lastfm_track *track = NULL;
        if (!g_queue_is_empty(pls->tracks)) {
                track = (lastfm_track *) g_queue_pop_head(pls->tracks);
        }
        return track;
}

/**
 * Append a track to the end of a playlist.
 * @param pls The playlist
 * @param track The track to be appended
 */
void
lastfm_pls_add_track(lastfm_pls *pls, lastfm_track *track)
{
        g_return_if_fail(pls != NULL && track != NULL);
        g_queue_push_tail(pls->tracks, track);
}

/**
 * Create a new, empty playlist
 * @return A new playlist. It should be destroyed with
 * lastfm_pls_destroy() when no longer used
 */
lastfm_pls *
lastfm_pls_new(void)
{
        lastfm_pls *pls = g_new0(lastfm_pls, 1);
        pls->tracks = g_queue_new();
        return pls;
}

/**
 * Remove all tracks from a playlist
 * @param pls The playlist to be cleared
 */
void
lastfm_pls_clear(lastfm_pls *pls)
{
        g_return_if_fail (pls != NULL);
        lastfm_track *track;
        while ((track = lastfm_pls_get_track(pls)) != NULL) {
                lastfm_track_destroy(track);
        }
}

/**
 * Destroy a playlist, including all the tracks that it contains
 * @param pls The playlist to be destroyed, or NULL.
 */
void
lastfm_pls_destroy(lastfm_pls *pls)
{
        if (pls == NULL) return;
        lastfm_pls_clear(pls);
        g_queue_free(pls->tracks);
        g_free(pls);
}

/**
 * Merges two playlists, appending the contents of the second to the
 * end of the first one. The second playlist is cleared (but not
 * destroyed).
 * @param pls1 The first playlist
 * @param pls2 The second playlist (empty after this operation)
 */
void
lastfm_pls_merge(lastfm_pls *pls1, lastfm_pls *pls2)
{
        g_return_if_fail(pls1 != NULL && pls2 != NULL);
        lastfm_track *track;
        while ((track = lastfm_pls_get_track(pls2)) != NULL) {
                lastfm_pls_add_track(pls1, track);
        }
}
