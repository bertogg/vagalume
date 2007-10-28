/*
 * playlist.c -- Functions to control playlist objects
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
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
        g_free(track->image_url);
        g_free(track->trackauth);
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
        ret->image_url = g_strdup(track->image_url);
        ret->trackauth = g_strdup(track->trackauth);
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
        return g_slist_length(pls->tracks);
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
        GSList *list = pls->tracks;
        if (list == NULL) return NULL;
        lastfm_track *track = (lastfm_track *) list->data;
        pls->tracks = g_slist_delete_link(list, list);
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
        pls->tracks = g_slist_append(pls->tracks, track);
}

/**
 * Set the title of a playlist
 * @param pls The playlist
 * @param title Title to set (the playlist will store a copy)
 */
void
lastfm_pls_set_title(lastfm_pls *pls, const char *title)
{
        g_return_if_fail(pls != NULL);
        g_free(pls->title);
        pls->title = g_strdup(title);
}

/**
 * Create a new, empty playlist
 * @param title The title of this playlist, or NULL
 * @return A new playlist. It should be destroyed with
 * lastfm_pls_destroy() when no longer used
 */
lastfm_pls *
lastfm_pls_new(const char *title)
{
        lastfm_pls *pls = g_new0(lastfm_pls, 1);
        pls->title = g_strdup(title);
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
        g_free(pls->title);
        g_free(pls);
}
