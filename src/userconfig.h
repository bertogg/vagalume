
#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <glib.h>

gboolean read_user_cfg(void);
gboolean write_user_cfg(void);
const char *user_cfg_get_username(void);
const char *user_cfg_get_password(void);

#endif
