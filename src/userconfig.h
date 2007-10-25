
#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <glib.h>

typedef struct {
        char *username; /* Never NULL, even if not defined */
        char *password; /* Never NULL, even if not defined */
        gboolean discovery_mode;
} lastfm_usercfg;

lastfm_usercfg *lastfm_usercfg_new(void);
void lastfm_usercfg_destroy(lastfm_usercfg *cfg);
void lastfm_usercfg_set_username(lastfm_usercfg *cfg, const char *username);
void lastfm_usercfg_set_password(lastfm_usercfg *cfg, const char *password);
lastfm_usercfg *read_usercfg(void);
gboolean write_usercfg(lastfm_usercfg *cfg);

#endif
