/*
 * controller.h -- Where the control of the program is
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "mainwin.h"
#include "radio.h"
#include "xmlrpc.h"

void controller_stop_playing(void);
void controller_start_playing(void);
void controller_skip_track(void);
void controller_love_track(void);
void controller_ban_track(void);
void controller_play_radio_by_url(const char *url);
void controller_play_radio(lastfm_radio type);
void controller_play_others_radio(lastfm_radio type);
void controller_play_radio_ask_url(void);
void controller_play_group_radio(void);
void controller_play_globaltag_radio(void);
void controller_play_similarartist_radio(void);
void controller_tag_track(void);
void controller_recomm_track(request_type type);
void controller_add_to_playlist(void);
void controller_download_track(void);
void controller_open_usercfg(void);
void controller_quit_app(void);
void controller_run_app(lastfm_mainwin *win, const char *radio_url);
void controller_show_error(const char *text);
void controller_show_warning(const char *text);
void controller_show_info(const char *text);
gboolean controller_confirm_dialog(const char *text);
void controller_show_mainwin(gboolean show);
void controller_increase_volume(int inc);
void controller_show_cover(void);

#endif
