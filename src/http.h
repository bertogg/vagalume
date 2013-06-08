/*
 * http.h -- All the HTTP-specific functions
 *
 * Copyright (C) 2007-2008 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef HTTP_H
#define HTTP_H

#include <glib.h>

typedef gboolean
(*http_download_progress_cb)            (gpointer userdata,
                                         double   dltotal,
                                         double   dlnow);

void
http_set_proxy                          (const char *proxy,
                                         gboolean    use_system_proxy);

char *
escape_url                              (const char *url,
                                         gboolean    escape);

gboolean
http_get_buffer                         (const char  *url,
                                         char       **buffer,
                                         size_t      *bufsize);

gboolean
http_get_to_fd                          (const char   *url,
                                         int           fd,
                                         const GSList *headers);

gboolean
http_download_file                      (const char                *url,
                                         const char                *filename,
                                         http_download_progress_cb  cb,
                                         gpointer                   userdata);

gboolean
http_post_buffer                        (const char    *url,
                                         const char    *postdata,
                                         char         **retbuf,
                                         size_t        *retbufsize,
                                         const GSList  *headers);

void
http_init                               (void);

#endif
