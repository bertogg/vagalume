/*
 * radio.h -- Functions to build radio URLs
 *
 * Copyright (C) 2007-2008, 2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef RADIO_H
#define RADIO_H

typedef enum {
        LASTFM_GLOBALTAG_RADIO = 0,
        LASTFM_GROUP_RADIO,
        LASTFM_LOVEDTRACKS_RADIO,
        LASTFM_MIX_RADIO,
        LASTFM_NEIGHBOURS_RADIO,
        LASTFM_LIBRARY_RADIO,
        LASTFM_SIMILAR_ARTIST_RADIO,
        LASTFM_RECOMMENDED_RADIO,
        LASTFM_USERPLAYLIST_RADIO,
        LASTFM_USERTAG_RADIO
} LastfmRadio;

char *
lastfm_radio_url                        (LastfmRadio  type,
                                         const char  *data);

char *
lastfm_recommended_radio_url            (const char *user,
                                         int         value);

char *
lastfm_usertag_radio_url                (const char *user,
                                         const char *tag);

gboolean
lastfm_radio_url_is_custom              (const char *url);

#endif
