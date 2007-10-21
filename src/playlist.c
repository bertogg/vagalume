
#include "playlist.h"

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
lastfm_pls_size(lastfm_pls *pls)
{
        g_return_val_if_fail(pls != NULL, 0);
        return g_slist_length(pls->tracks);
}

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

void
lastfm_pls_add_track(lastfm_pls *pls, lastfm_track *track)
{
        g_return_if_fail(pls != NULL && track != NULL);
        pls->tracks = g_slist_append(pls->tracks, track);
}

void
lastfm_pls_set_title(lastfm_pls *pls, const char *title)
{
        g_return_if_fail(pls != NULL);
        g_free(pls->title);
        pls->title = g_strdup(title);
}

lastfm_pls *
lastfm_pls_new(const char *title)
{
        lastfm_pls *pls = g_new0(lastfm_pls, 1);
        pls->title = g_strdup(title);
        return pls;
}

void
lastfm_pls_destroy(lastfm_pls *pls)
{
        if (pls == NULL) return;
        lastfm_track *track;
        while ((track = lastfm_pls_get_track(pls)) != NULL) {
                lastfm_track_destroy(track);
        }
        g_free(pls->title);
        g_free(pls);
}
