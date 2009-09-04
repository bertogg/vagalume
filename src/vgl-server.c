/*
 * vgl-server.c -- Server data
 *
 * Copyright (C) 2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "vgl-server.h"
#include "globaldefs.h"

static gboolean initialized = FALSE;

#define RSP_PARAMS "?hs=true&p=1.2&c=" LASTFM_APP_ID "&v=" LASTFM_APP_VERSION

/*
 * This list contains the API key and secret, used to identify the
 * Last.fm client.
 *
 * Note that these values are specific to Vagalume and meant to be
 * secret, but since this is an open source program we can't really
 * hide them. Please do not use them in other applications.
 *
 * To obtain a free Last.fm API key and secret for your app (or for
 * testing purposes) follow this link:
 *
 * http://www.last.fm/api/account
 *
 * Other services (e.g. Libre.fm) don't check the API key, so any
 * random value is allowed.
 */
static const VglServer default_srv_list[] = {
        {
                "Last.fm",
                "http://ws.audioscrobbler.com/2.0/",
                "http://post.audioscrobbler.com/" RSP_PARAMS,
                "c00772ea9e00787179ce56e53bc51ec7",
                "10d704729842d9ef0129694be78d529a"
        },
        {
                "Libre.fm",
                "http://alpha.libre.fm/2.0/",
                "http://turtle.libre.fm/" RSP_PARAMS,
                "db2c2184ad684eac4adce3ed1bb4a3a0",
                "14dbb2640e6856bd56d2179db4dcc0ff"
        }
};

static GList *srv_list = NULL;

static VglServer *
vgl_server_new                          (const char *name,
                                         const char *ws_base_url,
                                         const char *rsp_base_url,
                                         const char *api_key,
                                         const char *api_secret)
{
        VglServer *srv;

        g_return_val_if_fail (name && ws_base_url && rsp_base_url &&
                              api_key && api_secret, NULL);

        srv = g_slice_new (VglServer);

        srv->name         = g_strdup (name);
        srv->ws_base_url  = g_strdup (ws_base_url);
        srv->rsp_base_url = g_strdup (rsp_base_url);
        srv->api_key      = g_strdup (api_key);
        srv->api_secret   = g_strdup (api_secret);

        srv->refcount     = 1;

        return srv;
}

VglServer *
vgl_server_ref                          (VglServer *srv)
{
        g_return_val_if_fail (srv != NULL, NULL);
        g_atomic_int_inc (&(srv->refcount));
        return srv;
}

void
vgl_server_unref                        (VglServer *srv)
{
        g_return_if_fail (srv != NULL);

        if (g_atomic_int_dec_and_test (&(srv->refcount))) {
                g_free ((gpointer) srv->name);
                g_free ((gpointer) srv->ws_base_url);
                g_free ((gpointer) srv->rsp_base_url);
                g_free ((gpointer) srv->api_key);
                g_free ((gpointer) srv->api_secret);

                g_slice_free (VglServer, srv);
        }
}

gboolean
vgl_server_list_add                     (const char *name,
                                         const char *ws_base_url,
                                         const char *rsp_base_url,
                                         const char *api_key,
                                         const char *api_secret)
{
        VglServer *srv;

        g_return_val_if_fail (initialized, FALSE);
        g_return_val_if_fail (name && ws_base_url && rsp_base_url &&
                              api_key && api_secret, FALSE);

        srv = vgl_server_new (name, ws_base_url, rsp_base_url,
                              api_key, api_secret);
        srv_list = g_list_append (srv_list, srv);

        return TRUE;
}

VglServer *
vgl_server_list_find_by_name            (const char *name)
{
        GList *iter;

        g_return_val_if_fail (initialized, NULL);
        g_return_val_if_fail (name != NULL, NULL);

        for (iter = srv_list; iter != NULL; iter = iter->next) {
                VglServer *srv = iter->data;
                if (g_str_equal (name, srv->name)) {
                        return vgl_server_ref (srv);
                }
        }

        return NULL;
}

gboolean
vgl_server_list_remove                  (const char *name)
{
        GList *iter;

        g_return_val_if_fail (name != NULL, FALSE);

        for (iter = srv_list; iter != NULL; iter = iter->next) {
                VglServer *srv = iter->data;
                if (g_str_equal (name, srv->name)) {
                        vgl_server_unref (srv);
                        srv_list = g_list_delete_link (srv_list, iter);
                        return TRUE;
                }
        }

        return FALSE;
}

void
vgl_server_list_init                    (void)
{
        int i;

        g_return_if_fail (!initialized);
        initialized = TRUE;

        for (i = 0; i < G_N_ELEMENTS (default_srv_list); i++) {
                const VglServer *s = default_srv_list+i;
                vgl_server_list_add (s->name,
                                     s->ws_base_url, s->rsp_base_url,
                                     s->api_key, s->api_secret);
                g_debug ("Added server: %s", s->name);
        }
}

void
vgl_server_list_finalize                (void)
{
        g_return_if_fail (initialized);

        g_list_foreach (srv_list, (GFunc) vgl_server_unref, NULL);
        g_list_free (srv_list);

        srv_list = NULL;
        initialized = FALSE;
}

GList *
vgl_server_list_get                     (void)
{
        g_return_val_if_fail (initialized, NULL);

        return g_list_copy (srv_list);
}

VglServer *
vgl_server_get_default                  (void)
{
        g_return_val_if_fail (initialized, NULL);
        g_return_val_if_fail (srv_list, NULL);

        return vgl_server_ref (srv_list->data);
}
