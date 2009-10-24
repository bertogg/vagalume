/*
 * vgl-server.h -- Server data
 *
 * Copyright (C) 2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_SERVER_H
#define VGL_SERVER_H

#include "vgl-object.h"

G_BEGIN_DECLS

typedef struct {
        VglObject parent;
        const char *name;
        const char *ws_base_url;
        const char *rsp_base_url;
        const char *old_hs_url;
        const char *api_key;
        const char *api_secret;
        gboolean old_str_api;
        gboolean free_streams;
} VglServer;

gboolean
vgl_server_list_init                    (void);

void
vgl_server_list_finalize                (void);

GList *
vgl_server_list_get                     (void);

VglServer *
vgl_server_list_find_by_name            (const char *name);

gboolean
vgl_server_list_add                     (const char *name,
                                         const char *ws_base_url,
                                         const char *rsp_base_url,
                                         const char *api_key,
                                         const char *api_secret,
                                         gboolean    old_streaming_api,
                                         gboolean    free_streams);

gboolean
vgl_server_list_remove                  (const char *name);

VglServer *
vgl_server_get_default                  (void);

G_END_DECLS

#endif /* VGL_SERVER_H */
