/*
 * playlist.c -- Functions to control playlist objects
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include "playlist.h"

/**
 * Creates a new LastfmTrack object
 * @return The new track
 */
LastfmTrack *
lastfm_track_new(void)
{
        LastfmTrack *track = g_slice_new0(LastfmTrack);
        track->refcount = 1;
        track->mutex = g_mutex_new();
        return track;
}

/**
 * Destroy a LastfmTrack object freeing all its allocated memory
 * @param track Track to be destroyed, or NULL
 */
static void
lastfm_track_destroy(LastfmTrack *track)
{
        if (track == NULL) return;
        g_mutex_free(track->mutex);
        g_free(track->stream_url);
        g_free(track->title);
        g_free(track->artist);
        g_free(track->album);
        g_free(track->pls_title);
        g_free(track->image_url);
        g_free(track->image_data);
        g_free(track->trackauth);
        g_free(track->free_track_url);
        g_free(track->album_page_url);
        g_slice_free(LastfmTrack, track);
}

/**
 * Increments the reference counter of a LastfmTrack object
 * @param track The track
 * @return The same track
 */
LastfmTrack *
lastfm_track_ref(LastfmTrack *track)
{
        g_return_val_if_fail(track != NULL, NULL);
        g_mutex_lock(track->mutex);
        track->refcount++;
        g_mutex_unlock(track->mutex);
        return track;
}

/**
 * Decrements the reference counter of a LastfmTrack object. If it
 * reaches 0, then the track is destroyed
 * @param track The track
 */
void
lastfm_track_unref(LastfmTrack *track)
{
        g_return_if_fail(track != NULL);
        gboolean destroy;
        g_mutex_lock(track->mutex);
        destroy = (--(track->refcount) == 0);
        g_mutex_unlock(track->mutex);
        if (destroy) lastfm_track_destroy(track);
}

/**
 * Set the cover image of a track object, erasing the previous one (if
 * any). After this, the new image data will be owned by this object
 * and will be destroyed automatically when the track is destroyed.
 * @param track The track
 * @param data The image data
 * @param size Size of the data buffer
 */
void
lastfm_track_set_cover_image(LastfmTrack *track, char *data, size_t size)
{
        g_return_if_fail(track != NULL);
        g_mutex_lock(track->mutex);
        g_free(track->image_data);
        track->image_data = data;
        track->image_data_size = size;
        track->image_data_available = TRUE;
        g_mutex_unlock(track->mutex);
}

/**
 * Get the size of a playlist
 * @param pls The playlist
 * @return The number of tracks in the playlist
 */
guint
lastfm_pls_size(LastfmPls *pls)
{
        g_return_val_if_fail(pls != NULL, 0);
        return g_queue_get_length(pls->tracks);
}

/**
 * Get the next track in a playlist. Note that this function removes
 * the track from the playlist (as it can only be played once).
 * @param pls The playlist
 * @return The next track (or NULL if the list is empty). It should be
 * unref'ed with lastfm_track_unref() when no longer used.
 */
LastfmTrack *
lastfm_pls_get_track(LastfmPls *pls)
{
        g_return_val_if_fail(pls != NULL, NULL);
        LastfmTrack *track = NULL;
        if (!g_queue_is_empty(pls->tracks)) {
                track = (LastfmTrack *) g_queue_pop_head(pls->tracks);
        }
        return track;
}

/**
 * Append a track to the end of a playlist.
 * @param pls The playlist
 * @param track The track to be appended
 */
void
lastfm_pls_add_track(LastfmPls *pls, LastfmTrack *track)
{
        g_return_if_fail(pls != NULL && track != NULL);
        g_queue_push_tail(pls->tracks, track);
}

/**
 * Create a new, empty playlist
 * @return A new playlist. It should be destroyed with
 * lastfm_pls_destroy() when no longer used
 */
LastfmPls *
lastfm_pls_new(void)
{
        LastfmPls *pls = g_slice_new0(LastfmPls);
        pls->tracks = g_queue_new();
        return pls;
}

/**
 * Remove all tracks from a playlist
 * @param pls The playlist to be cleared
 */
void
lastfm_pls_clear(LastfmPls *pls)
{
        g_return_if_fail (pls != NULL);
        LastfmTrack *track;
        while ((track = lastfm_pls_get_track(pls)) != NULL) {
                lastfm_track_unref(track);
        }
}

/**
 * Destroy a playlist, including all the tracks that it contains
 * @param pls The playlist to be destroyed, or NULL.
 */
void
lastfm_pls_destroy(LastfmPls *pls)
{
        if (pls == NULL) return;
        lastfm_pls_clear(pls);
        g_queue_free(pls->tracks);
        g_slice_free(LastfmPls, pls);
}

/**
 * Merges two playlists, appending the contents of the second to the
 * end of the first one. The second playlist is cleared (but not
 * destroyed).
 * @param pls1 The first playlist
 * @param pls2 The second playlist (empty after this operation)
 */
void
lastfm_pls_merge(LastfmPls *pls1, LastfmPls *pls2)
{
        g_return_if_fail(pls1 != NULL && pls2 != NULL);
        LastfmTrack *track;
        while ((track = lastfm_pls_get_track(pls2)) != NULL) {
                lastfm_pls_add_track(pls1, track);
        }
}
