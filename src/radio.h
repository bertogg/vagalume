/*
 * radio.h -- Functions to build radio URLs
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#ifndef RADIO_H
#define RADIO_H

typedef enum {
        LASTFM_GLOBALTAG_RADIO = 0,
        LASTFM_GROUP_RADIO,
        LASTFM_LOVEDTRACKS_RADIO,
        LASTFM_NEIGHBOURS_RADIO,
        LASTFM_PERSONAL_RADIO,
        LASTFM_SIMILAR_ARTIST_RADIO,
        LASTFM_RECOMMENDED_RADIO,
        LASTFM_USERPLAYLIST_RADIO,
        LASTFM_USERTAG_RADIO
} lastfm_radio;

char *lastfm_radio_url(lastfm_radio type, const char *data);
char *lastfm_recommended_radio_url(const char *user, int value);
char *lastfm_usertag_radio_url(const char *user, const char *tag);

#endif
