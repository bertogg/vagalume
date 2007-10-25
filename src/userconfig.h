
#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <glib.h>

typedef struct {
        char *username; /* Never NULL, even if not defined */
        char *password; /* Never NULL, even if not defined */
        gboolean discovery_mode;
} lastfm_usercfg;

lastfm_usercfg *read_user_cfg(void);
gboolean write_user_cfg(lastfm_usercfg *cfg);

#endif
