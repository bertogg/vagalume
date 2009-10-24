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
#include "userconfig.h"
#include "util.h"

static gboolean initialized = FALSE;

#define DEFAULT_SERVER_LIST APP_DATA_DIR "/servers.xml"

#define RSP_PARAMS "?hs=true&p=1.2&c=" LASTFM_APP_ID "&v=" LASTFM_APP_VERSION
#define HANDSHAKE_PARAMS "radio/handshake.php?version=1.5&platform=" APP_OS_LC

static GList *srv_list = NULL;

static void
vgl_server_destroy                      (VglServer *srv)
{
        g_free ((gpointer) srv->name);
        g_free ((gpointer) srv->ws_base_url);
        g_free ((gpointer) srv->rsp_base_url);
        g_free ((gpointer) srv->old_hs_url);
        g_free ((gpointer) srv->api_key);
        g_free ((gpointer) srv->api_secret);
}

static VglServer *
vgl_server_new                          (const char *name,
                                         const char *ws_base_url,
                                         const char *rsp_base_url,
                                         const char *api_key,
                                         const char *api_secret,
                                         gboolean    old_streaming_api,
                                         gboolean    free_streams)
{
        VglServer *srv;

        g_return_val_if_fail (name && ws_base_url && rsp_base_url &&
                              api_key && api_secret, NULL);

        srv = vgl_object_new (VglServer, (GDestroyNotify) vgl_server_destroy);

        srv->name         = g_strdup (name);
        srv->ws_base_url  = g_strconcat (ws_base_url, "2.0/", NULL);
        srv->rsp_base_url = g_strconcat (rsp_base_url, RSP_PARAMS, NULL);
        srv->old_hs_url   = g_strconcat (ws_base_url, HANDSHAKE_PARAMS, NULL);
        srv->api_key      = g_strdup (api_key);
        srv->api_secret   = g_strdup (api_secret);
        srv->old_str_api  = old_streaming_api;
        srv->free_streams = free_streams;

        return srv;
}

gboolean
vgl_server_list_add                     (const char *name,
                                         const char *ws_base_url,
                                         const char *rsp_base_url,
                                         const char *api_key,
                                         const char *api_secret,
                                         gboolean    old_streaming_api,
                                         gboolean    free_streams)
{
        VglServer *srv;

        g_return_val_if_fail (initialized, FALSE);
        g_return_val_if_fail (name && ws_base_url && rsp_base_url &&
                              api_key && api_secret, FALSE);

        /* Do nothing if there's already a server with the same name */
        srv = vgl_server_list_find_by_name (name);
        if (srv != NULL) {
                g_debug ("Trying to add %s server again, ignoring...", name);
                vgl_object_unref (srv);
                return FALSE;
        }

        srv = vgl_server_new (name, ws_base_url, rsp_base_url, api_key,
                              api_secret, old_streaming_api, free_streams);
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
                        return vgl_object_ref (srv);
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
                        vgl_object_unref (srv);
                        srv_list = g_list_delete_link (srv_list, iter);
                        return TRUE;
                }
        }

        return FALSE;
}

static void
parse_server                            (xmlDoc  *doc,
                                         xmlNode *node)
{
        char *name, *ws_base_url, *rsp_base_url, *key, *secret;
        gboolean old_str_api, free_streams;

        xml_get_string (doc, node, "name", &name);
        xml_get_string (doc, node, "ws_base_url", &ws_base_url);
        xml_get_string (doc, node, "rsp_base_url", &rsp_base_url);
        xml_get_string (doc, node, "api_key", &key);
        xml_get_string (doc, node, "api_secret", &secret);
        xml_get_bool (doc, node, "old_streaming_api", &old_str_api);
        xml_get_bool (doc, node, "free_streams", &free_streams);

        if ( name &&  ws_base_url &&  rsp_base_url &&  key &&  secret &&
            *name && *ws_base_url && *rsp_base_url && *key && *secret) {
                vgl_server_list_add (name, ws_base_url, rsp_base_url,
                                     key, secret, old_str_api, free_streams);
                g_debug ("Added server: %s", name);
        } else {
                g_warning ("Error parsing server: %s",
                           (name && *name) ? name : "(unknown)");
        }

        g_free (name);
        g_free (ws_base_url);
        g_free (rsp_base_url);
        g_free (key);
        g_free (secret);
}

static void
parse_server_file                       (const char *filename)
{
        xmlDoc *doc = NULL;
        xmlNode *root = NULL;
        xmlNode *node = NULL;

        if (file_exists (filename)) {
                doc = xmlParseFile (filename);
        }

        if (doc != NULL) {
                xmlChar *version;
                root = xmlDocGetRootElement (doc);
                version = xmlGetProp (root, (xmlChar *) "version");
                if (version == NULL ||
                    !xmlStrEqual (root->name, (xmlChar *) "servers")) {
                        g_warning ("Error parsing servers file");
                } else if (!xmlStrEqual (version, (xmlChar *) "1")) {
                        g_warning ("This servers file is version %s, but "
                                   "Vagalume " APP_VERSION " can only "
                                   "read version 1", version);
                } else {
                        node = (xmlNode *) xml_find_node (
                                root->xmlChildrenNode, "server");
                }
                if (version != NULL) xmlFree (version);
        }

        if (node != NULL) {
                g_debug ("Parsing server file %s", filename);
        } else {
                g_debug ("Unable to read %s, skipping", filename);
        }

        while (node != NULL) {
                parse_server (doc, node->xmlChildrenNode);
                node = (xmlNode *) xml_find_node (node->next, "server");
        }

        if (doc != NULL) xmlFreeDoc (doc);
}

gboolean
vgl_server_list_init                    (void)
{
        const char *cfgdir;
        g_return_val_if_fail (!initialized, srv_list != NULL);
        initialized = TRUE;

        cfgdir = vgl_user_cfg_get_cfgdir ();
        if (cfgdir != NULL) {
                char *filename = g_strconcat (cfgdir, "/servers.xml", NULL);
                parse_server_file (filename);
                g_free (filename);
        }

        parse_server_file (DEFAULT_SERVER_LIST);

        return (srv_list != NULL);
}

void
vgl_server_list_finalize                (void)
{
        g_return_if_fail (initialized);

        g_list_foreach (srv_list, (GFunc) vgl_object_unref, NULL);
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

        return srv_list ? vgl_object_ref (srv_list->data) : NULL;
}
