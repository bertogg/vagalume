/*
 * util.h -- Misc util functions
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */
#ifndef UTIL_H
#define UTIL_H

char *get_md5_hash(const char *str);
char *compute_auth_token(const char *password, const char *timestamp);

#endif
