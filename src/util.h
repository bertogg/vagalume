/*
 * util.h -- Misc util functions
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */
#ifndef UTIL_H
#define UTIL_H

#include <glib.h>

char *get_md5_hash(const char *str);
char *compute_auth_token(const char *password, const char *timestamp);
char *str_glist_join(const GList *list, const char *separator);
gboolean file_exists(const char *filename);

#endif
