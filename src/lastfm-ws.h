/*
 * lastfm-ws.h -- Last.fm Web Services v2.0
 *
 * Copyright (C) 2009, 2011, 2013 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef LASTFM_WS_H
#define LASTFM_WS_H

#include "playlist.h"
#include "protocol.h"
#include "vgl-server.h"

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
        LASTFM_OK,
        LASTFM_NOT_FOUND = 25,
        LASTFM_GEO_RESTRICTED = 28
} LastfmErrorCode;

/* Opaque type that represents a Last.fm Web Services session */
typedef struct _LastfmWsSession         LastfmWsSession;

LastfmSession *
lastfm_ws_session_get_v1_session        (LastfmWsSession *session);

char *
lastfm_ws_get_auth_token                (const VglServer  *srv,
                                         char            **auth_url);

LastfmWsSession *
lastfm_ws_get_session_from_token        (VglServer  *srv,
                                         const char *token);

LastfmWsSession *
lastfm_ws_get_session                   (VglServer  *srv,
                                         const char *user,
                                         const char *pass,
                                         LastfmErr  *err);

LastfmErrorCode
lastfm_ws_radio_tune                    (LastfmWsSession *session,
                                         const char      *radio_url,
                                         const char      *lang);

LastfmPls *
lastfm_ws_radio_get_playlist            (const LastfmWsSession *session,
                                         gboolean               discovery,
                                         gboolean               low_bitrate,
                                         gboolean               scrobbling);

gboolean
lastfm_ws_get_friends                   (const VglServer  *srv,
                                         const char       *user,
                                         GList           **friendlist);

gboolean
lastfm_ws_get_user_tags                 (const VglServer  *srv,
                                         const char       *username,
                                         GList           **taglist);

gboolean
lastfm_ws_get_user_track_tags           (const LastfmWsSession  *session,
                                         const LastfmTrack      *track,
                                         LastfmTrackComponent    type,
                                         GList                 **taglist);

gboolean
lastfm_ws_get_track_tags                (const LastfmWsSession  *session,
                                         const LastfmTrack      *track,
                                         LastfmTrackComponent    type,
                                         GList                 **taglist);

gboolean
lastfm_ws_add_tags                      (const LastfmWsSession *session,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent   type,
                                         const char            *tags);

gboolean
lastfm_ws_remove_tag                    (const LastfmWsSession *session,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent   type,
                                         const char            *tag);

gboolean
lastfm_ws_share_track                   (const LastfmWsSession *session,
                                         const LastfmTrack     *track,
                                         const char            *text,
                                         LastfmTrackComponent   type,
                                         const char            *rcpt);

gboolean
lastfm_ws_love_track                    (const LastfmWsSession *session,
                                         const LastfmTrack     *track);

gboolean
lastfm_ws_ban_track                     (const LastfmWsSession *session,
                                         const LastfmTrack     *track);

gboolean
lastfm_ws_tag_track                     (const LastfmWsSession *session,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent   type,
                                         GSList                *tags);

gboolean
lastfm_ws_add_to_playlist               (const LastfmWsSession *session,
                                         const LastfmTrack     *track);

G_END_DECLS

#endif /* LASTFM_WS_H */
