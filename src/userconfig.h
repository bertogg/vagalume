
#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <glib.h>

gboolean read_user_cfg(void);
const char *user_cfg_get_usename(void);
const char *user_cfg_get_password(void);

#endif
