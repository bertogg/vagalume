/*
 * scrobbler.h -- Audioscrobbler Realtime Submission Protocol
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
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
} RspRating;

typedef enum {
        RSP_RESPONSE_OK,
        RSP_RESPONSE_BADSESSION,
        RSP_RESPONSE_ERROR
} RspResponse;

typedef struct {
        char *id;
        char *np_url;
        char *post_url;
} RspSession;

RspSession *rsp_session_new(const char *username, const char *password,
                             lastfm_err *err);
RspSession *rsp_session_copy(const RspSession *s);
void rsp_session_destroy(RspSession *session);
RspResponse rsp_set_nowplaying(const RspSession *rsp, const LastfmTrack *t);
RspResponse rsp_scrobble(const RspSession *rsp, const LastfmTrack *t,
                         time_t start, RspRating rating);

#endif
