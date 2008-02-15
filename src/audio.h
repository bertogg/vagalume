/*
 * audio.h -- All the audio related stuff
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <glib.h>

gboolean lastfm_audio_init(void);
gboolean lastfm_audio_play(const char *url, GCallback audio_started_cb,
                           const char *session_id);
gboolean lastfm_audio_stop(void);
int lastfm_audio_get_running_time(void);
int lastfm_audio_get_volume(void);
void lastfm_audio_set_volume(int vol);
int lastfm_audio_increase_volume(int inc);
void lastfm_audio_clear(void);

#endif
