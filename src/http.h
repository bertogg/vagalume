/*
 * http.h -- All the HTTP-specific functions
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#ifndef HTTP_H
#define HTTP_H

#include <glib.h>

void http_set_proxy(const char *proxy);
char *escape_url(const char *url, gboolean escape);
gboolean http_get_buffer(const char *url, char **buffer, size_t *bufsize);
gboolean http_get_to_fd(const char *url, int fd, const GSList *headers);
gboolean http_download_file(const char *url, const char *filename);
gboolean http_post_buffer(const char *url, const char *postdata,
                          char **retdata, const GSList *headers);
void http_init(void);

#endif
