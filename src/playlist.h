
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

void lastfm_track_destroy(lastfm_track *track);
lastfm_track *lastfm_pls_get_track(void);
void lastfm_pls_add_track(lastfm_track *track);
guint lastfm_pls_size(void);

#endif
