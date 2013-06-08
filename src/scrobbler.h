/*
 * scrobbler.h -- Audioscrobbler Realtime Submission Protocol
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef SCROBBLER_H
#define SCROBBLER_H

#include "controller.h"

typedef enum {
        RSP_RATING_NONE,
        RSP_RATING_LOVE,
        RSP_RATING_BAN,
        RSP_RATING_SKIP
} RspRating;

void
rsp_init                                (VglController *controller);

#endif
