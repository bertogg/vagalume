
#ifndef AUDIO_H
#define AUDIO_H

#include <glib.h>

gboolean lastfm_audio_init(void);
gboolean lastfm_audio_play(const char *url);
gboolean lastfm_audio_stop(void);
void lastfm_audio_clear(void);

#endif
