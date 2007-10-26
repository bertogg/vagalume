
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "protocol.h"
#include "mainwin.h"
#include "radio.h"
#include "userconfig.h"

void controller_stop_playing(void);
void controller_start_playing(void);
void controller_skip_track(void);
void controller_play_radio_by_url(const char *url);
void controller_play_radio(lastfm_radio type);
void controller_play_radio_ask_url(void);
void controller_open_usercfg(void);
void controller_quit_app(void);
void controller_run_app(lastfm_mainwin *win, const char *radio_url);

#endif
