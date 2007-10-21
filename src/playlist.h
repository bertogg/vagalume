
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
} lastfm_track;

typedef struct {
        char *title;
        GSList *tracks;
} lastfm_pls;

void lastfm_track_destroy(lastfm_track *track);
lastfm_track *lastfm_pls_get_track(lastfm_pls *pls);
void lastfm_pls_add_track(lastfm_pls *pls, lastfm_track *track);
guint lastfm_pls_size(lastfm_pls *pls);
lastfm_pls *lastfm_pls_new(const char *title);
void lastfm_pls_set_title(lastfm_pls *pls, const char *title);
void lastfm_pls_destroy(lastfm_pls *pls);

#endif
