#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <glib.h>

typedef struct {
        char *id;
        char *stream_url;
        gboolean subscriber;
        char *base_url;
        char *base_path;
} lastfm_session;

lastfm_session *lastfm_handshake(void);
gboolean lastfm_request_playlist(lastfm_session *s);
void lastfm_session_destroy(lastfm_session *session);

#endif
