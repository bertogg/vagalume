
#ifndef RADIO_H
#define RADIO_H

char *lastfm_globaltag_radio_url(const char* tag);
char *lastfm_group_radio_url(const char* group);
char *lastfm_lovedtracks_radio_url(const char* user);
char *lastfm_neighbours_radio_url(const char* user);
char *lastfm_personal_radio_url(const char* user);
char *lastfm_recommended_radio_url(const char *user, int value);
char *lastfm_similar_artist_radio_url(const char* artist);
char *lastfm_usertag_radio_url(const char *user, const char *tag);

#endif
