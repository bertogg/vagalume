
#include "playlist.h"

static GSList *playlist = NULL;

void
lastfm_track_destroy(lastfm_track *track)
{
        if (track == NULL) return;
        g_free(track->stream_url);
        g_free(track->title);
        g_free(track->artist);
        g_free(track->album);
        g_free(track->image_url);
        g_free(track);
}

guint
lastfm_pls_size(void)
{
        return g_slist_length(playlist);
}

lastfm_track *
lastfm_pls_get_track(void)
{
        if (playlist == NULL) return NULL;
        lastfm_track *track = (lastfm_track *) playlist->data;
        playlist = g_slist_delete_link(playlist, playlist);
        return track;
}

void
lastfm_pls_add_track(lastfm_track *track)
{
        g_return_if_fail(track != NULL);
        playlist = g_slist_append(playlist, track);
}

void
lastfm_pls_clear(void)
{
        lastfm_track *track;
        while ((track = lastfm_pls_get_track()) != NULL) {
                lastfm_track_destroy(track);
        }
}
