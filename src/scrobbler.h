/*
 * scrobbler.h -- Audioscrobbler Realtime Submission Protocol
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#ifndef SCROBBLER_H
#define SCROBBLER_H

#include <time.h>
#include "protocol.h"
#include "playlist.h"

typedef enum {
        RSP_RATING_NONE,
        RSP_RATING_LOVE,
        RSP_RATING_BAN,
        RSP_RATING_SKIP
} rsp_rating;

typedef struct {
        char *id;
        char *np_url;
        char *post_url;
} rsp_session;

rsp_session *rsp_session_new(const char *username, const char *password,
                             lastfm_err *err);
rsp_session *rsp_session_copy(const rsp_session *s);
void rsp_session_destroy(rsp_session *session);
void rsp_set_nowplaying(const rsp_session *rsp, const lastfm_track *t);
void rsp_scrobble(const rsp_session *rsp, const lastfm_track *t,
                  time_t start, rsp_rating rating);

#endif
