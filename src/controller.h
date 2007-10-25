
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
void controller_quit_app(void);
void controller_set_mainwin(lastfm_mainwin *win);
void controller_set_session(lastfm_session *s);
void controller_set_usercfg(lastfm_usercfg *cfg);

#endif
