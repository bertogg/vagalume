/*
 * controller.h -- Where the control of the program is
 *
 * Copyright (C) 2007-2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "vgl-main-window.h"
#include "radio.h"
#include "vgl-controller.bp.h"

G_BEGIN_DECLS

struct _VglControllerClass
{
    GObjectClass parent_class;
};

struct _VglController
{
    GObject parent;
};

typedef enum {
        STOP_AFTER_DONT_STOP,
        STOP_AFTER_THIS_TRACK,
        STOP_AFTER_N_MINUTES
} StopAfterType;

typedef enum {
        BOOKMARK_TYPE_EMPTY,
        BOOKMARK_TYPE_ARTIST,
        BOOKMARK_TYPE_TRACK,
        BOOKMARK_TYPE_CURRENT_RADIO
} BookmarkType;

void
controller_stop_playing                 (void);

void
controller_disconnect                   (void);

void
controller_start_playing                (void);

void
controller_skip_track                   (void);

void
controller_set_stop_after               (void);

void
controller_love_track                   (gboolean interactive);

void
controller_ban_track                    (gboolean interactive);

void
controller_play_radio_by_url            (const char *url);

void
controller_play_radio                   (LastfmRadio type);

void
controller_play_others_radio            (LastfmRadio type);

void
controller_play_others_radio_by_user    (const char  *username,
                                         LastfmRadio  type);

void
controller_play_radio_ask_url           (void);

void
controller_play_group_radio             (void);

void
controller_play_globaltag_radio         (void);

void
controller_play_similarartist_radio     (void);

void
controller_tag_track                    (void);

void
controller_recomm_track                 (void);

void
controller_add_to_playlist              (void);

void
controller_download_track               (gboolean background);

void
controller_manage_bookmarks             (void);

void
controller_add_bookmark                 (BookmarkType type);

void
controller_open_usercfg                 (void);

void
controller_import_servers_file          (void);

void
controller_quit_app                     (void);

void
controller_run_app                      (const char *radio_url);

void
controller_show_error                   (const char *text);

void
controller_show_warning                 (const char *text);

void
controller_show_info                    (const char *text);

void
controller_show_banner                  (const char *text);

void
controller_show_about                   (void);

gboolean
controller_confirm_dialog               (const char *text,
                                         gboolean    show_always);

void
controller_show_mainwin                 (gboolean show);

void
controller_toggle_mainwin_visibility    (void);

int
controller_increase_volume              (int inc);

void
controller_set_volume                   (int vol);

void
controller_show_cover                   (void);

LastfmTrack *
controller_get_current_track            (void);

const GList *
controller_get_friend_list              (void);

void
controller_close_mainwin                (void);

G_END_DECLS

#endif
