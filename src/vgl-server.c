/*
 * vgl-server.c -- Server data
 *
 * Copyright (C) 2009 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
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
        g_free ((gpointer) srv->orig_ws_url);
        g_free ((gpointer) srv->orig_rsp_url);
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
        srv->orig_ws_url  = g_strdup (ws_base_url);
        srv->orig_rsp_url = g_strdup (rsp_base_url);
        srv->ws_base_url  = g_strconcat (ws_base_url, "2.0/", NULL);
        srv->rsp_base_url = g_strconcat (rsp_base_url, RSP_PARAMS, NULL);
        srv->old_hs_url   = g_strconcat (ws_base_url, HANDSHAKE_PARAMS, NULL);
        srv->api_key      = g_strdup (api_key);
        srv->api_secret   = g_strdup (api_secret);
        srv->old_str_api  = old_streaming_api;
        srv->free_streams = free_streams;

        return srv;
}

static VglServer *
vgl_server_find_by_name                 (GList      *l,
                                         const char *name)
{
        for (; l != NULL; l = l->next) {
                VglServer *srv = l->data;
                if (g_str_equal (name, srv->name)) {
                        return srv;
                }
        }

        return NULL;
}

static char *
get_user_servers_file                   (void)
{
        const char *cfgdir;
        cfgdir = vgl_user_cfg_get_cfgdir ();
        if (cfgdir != NULL) {
                return g_strconcat (cfgdir, "/servers.xml", NULL);
        }
        return NULL;
}

static void
write_user_servers_file                 (GList *l)
{
        char *filename;
        xmlDoc *doc;
        xmlNode *root;

        doc = xmlNewDoc ((xmlChar *) "1.0");
        root = xmlNewNode (NULL, (xmlChar *) "servers");
        xmlSetProp (root, (xmlChar *) "version", (xmlChar *) "1");
        xmlSetProp (root, (xmlChar *) "revision", (xmlChar *) "1");
        xmlDocSetRootElement (doc, root);

        for (; l != NULL; l = l->next) {
                const VglServer *srv = l->data;
                xmlNode *node = xmlNewNode (NULL, (xmlChar *) "server");
                xmlAddChild (root, node);
                xml_add_string (node, "name", srv->name);
                xml_add_string (node, "ws_base_url", srv->orig_ws_url);
                xml_add_string (node, "rsp_base_url", srv->orig_rsp_url);
                xml_add_string (node, "api_key", srv->api_key);
                xml_add_string (node, "api_secret", srv->api_secret);
                xml_add_string (node, "old_streaming_api",
                                srv->old_str_api ? "1" : "0");
                xml_add_string (node, "free_streams",
                                srv->free_streams ? "1" : "0");
        }

        filename = get_user_servers_file ();
        if (xmlSaveFormatFileEnc (filename, doc, "UTF-8", 1) == -1) {
                g_critical ("Error writing %s", filename);
        }
        g_free (filename);

        xmlFreeDoc (doc);
}

static GList *
remove_duplicates                       (GList *l)
{
        GList *dest = NULL;
        GList *iter;

        for (iter = l; iter != NULL; iter = iter->next) {
                VglServer *srv = iter->data;
                if (vgl_server_find_by_name (dest, srv->name)) {
                        g_debug ("Found duplicate server %s, ignoring...",
                                 srv->name);
                } else {
                        dest = g_list_append (dest, vgl_object_ref (srv));
                }
        }

        g_list_foreach (l, (GFunc) vgl_object_unref, NULL);
        g_list_free (l);

        return dest;
}

VglServer *
vgl_server_list_find_by_name            (const char *name)
{
        g_return_val_if_fail (initialized, NULL);
        g_return_val_if_fail (name != NULL, NULL);

        return vgl_server_find_by_name (srv_list, name);
}

static VglServer *
parse_server                            (xmlDoc  *doc,
                                         xmlNode *node)
{
        VglServer *srv = NULL;
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
                srv = vgl_server_new (name, ws_base_url, rsp_base_url,
                                      key, secret, old_str_api, free_streams);
                g_debug ("Parsed server: %s", name);
        } else {
                g_warning ("Error parsing server: %s",
                           (name && *name) ? name : "(unknown)");
        }

        g_free (name);
        g_free (ws_base_url);
        g_free (rsp_base_url);
        g_free (key);
        g_free (secret);

        return srv;
}

static GList *
parse_server_file                       (const char *filename)
{
        GList *servers = NULL;
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
                VglServer *srv = parse_server (doc, node->xmlChildrenNode);
                servers = g_list_append (servers, srv);
                node = (xmlNode *) xml_find_node (node->next, "server");
        }

        if (doc != NULL) xmlFreeDoc (doc);

        return servers;
}

gboolean
vgl_server_import_file                  (const char *filename)
{
        gboolean found_servers;
        GList *l;

        g_return_val_if_fail (initialized, FALSE);

        l = parse_server_file (filename);
        found_servers = (l != NULL);

        if (l != NULL) {
                char *usercfgfile = get_user_servers_file ();
                if (usercfgfile) {
                        l = g_list_concat (l, parse_server_file (usercfgfile));
                        g_free (usercfgfile);
                        l = remove_duplicates (l);
                }
                write_user_servers_file (l);

                /* Update the global server list with the new info */
                srv_list = g_list_concat (l, srv_list);
                srv_list = remove_duplicates (srv_list);
        } else {
                g_debug ("No servers found in file %s", filename);
        }

        return found_servers;
}

gboolean
vgl_server_list_init                    (void)
{
        char *usercfgfile;
        g_return_val_if_fail (!initialized, srv_list != NULL);
        initialized = TRUE;

        usercfgfile = get_user_servers_file ();
        srv_list = parse_server_file (usercfgfile);
        g_free (usercfgfile);

        srv_list = g_list_concat (srv_list,
                                  parse_server_file (DEFAULT_SERVER_LIST));

        srv_list = remove_duplicates (srv_list);

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

const GList *
vgl_server_list_get                     (void)
{
        g_return_val_if_fail (initialized, NULL);

        return srv_list;
}

VglServer *
vgl_server_get_default                  (void)
{
        g_return_val_if_fail (initialized, NULL);

        return srv_list ? srv_list->data : NULL;
}
