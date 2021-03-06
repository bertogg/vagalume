/*
 * lastfm-ws.c -- Last.fm Web Services v2.0
 *
 * Copyright (C) 2009-2011, 2013 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "lastfm-ws.h"
#include "http.h"
#include "radio.h"
#include "util.h"
#include "xmlrpc.h"

#include <string.h>
#include <stdarg.h>

/* Uncomment this to use the new album.getInfo() web service to obtain
 * album tags. Note that it gives less results than the old method */
/* #define VGL_USE_NEW_ALBUM_TAGS_API 1 */

typedef enum {
        HTTP_REQUEST_GET,
        HTTP_REQUEST_POST
} HttpRequestType;

struct _LastfmWsSession {
        VglObject parent;
        char *username;
        char *password;
        char *key;
        char *radio_name;
        VglServer *srv;
        gboolean subscriber;
        LastfmSession *v1sess;
        GMutex *mutex;
};

/* Name/value pairs for URL parameters */
typedef struct {
        char *name;
        char *value;
} LastfmWsParameter;

static LastfmWsParameter *
lastfm_ws_parameter_new                 (const char *name,
                                         const char *value)
{
        LastfmWsParameter *p = g_slice_new (LastfmWsParameter);
        p->name  = g_strdup (name);
        p->value = g_strdup (value);
        return p;
}

static int
lastfm_ws_parameter_compare             (const LastfmWsParameter *a,
                                         const LastfmWsParameter *b)
{
        return strcmp (a->name, b->name);
}

static void
lastfm_ws_parameter_destroy             (LastfmWsParameter *param)
{
        g_free (param->name);
        g_free (param->value);
        g_slice_free (LastfmWsParameter, param);
}

static void
lastfm_ws_session_destroy               (LastfmWsSession *session)
{
        g_free (session->username);
        g_free (session->password);
        g_free (session->key);
        vgl_object_unref (session->srv);
        g_free (session->radio_name);
        g_mutex_free (session->mutex);
        if (session->v1sess) {
                lastfm_session_destroy (session->v1sess);
        }
}

static LastfmWsSession *
lastfm_ws_session_new                   (const char *username,
                                         const char *password,
                                         const char *key,
                                         VglServer  *srv,
                                         gboolean    subscriber)
{
        LastfmWsSession *session;

        g_return_val_if_fail (username && key && srv, NULL);

        session = vgl_object_new (LastfmWsSession,
                                  (GDestroyNotify) lastfm_ws_session_destroy);

        session->username   = g_strdup (username);
        session->password   = g_strdup (password);
        session->key        = g_strdup (key);
        session->srv        = vgl_object_ref (srv);
        session->radio_name = NULL;
        session->v1sess     = NULL;
        session->subscriber = subscriber;
        session->mutex      = g_mutex_new ();

        return session;
}

LastfmSession *
lastfm_ws_session_get_v1_session        (LastfmWsSession *session)
{
        g_return_val_if_fail (session != NULL, NULL);
        return session->v1sess;
}

static void
lastfm_ws_sign_url                      (GString     *url,
                                         const GList *params,
                                         const char  *api_secret)
{
        const GList *iter;
        char *md5;
        GString *sig = g_string_sized_new (200);

        for (iter = params; iter != NULL; iter = iter->next) {
                LastfmWsParameter *p = (LastfmWsParameter *) iter->data;
                g_string_append_printf (sig, "%s%s", p->name, p->value);
        }

        g_string_append (sig, api_secret);

        md5 = get_md5_hash (sig->str);
        g_string_append_printf (url, "&api_sig=%s", md5);

        g_free (md5);
        g_string_free (sig, TRUE);
}

static char *
lastfm_ws_format_params                 (const VglServer *srv,
                                         HttpRequestType  type,
                                         gboolean         add_api_sig,
                                         const GList     *params)
{
        GString *url;
        const GList *iter;

        g_return_val_if_fail (srv != NULL, NULL);

        /* 200 bytes should be enough for all method calls */
        url = g_string_sized_new (200);

        if (type == HTTP_REQUEST_GET) {
                g_string_assign (url, srv->ws_base_url);
                g_string_append_c (url, '?');
        }

        for (iter = params; iter != NULL; iter = iter->next) {
                LastfmWsParameter *p = (LastfmWsParameter *) iter->data;
                char *encvalue = escape_url (p->value, TRUE);
                g_string_append_printf (url, "%s=%s", p->name, encvalue);
                g_free (encvalue);

                if (iter->next != NULL) {
                        g_string_append_c (url, '&');
                }
        }

        if (add_api_sig) {
                /* Compute and append API signature */
                lastfm_ws_sign_url (url, params, srv->api_secret);
        }

        /* Return */
        return g_string_free (url, FALSE);
}

static gboolean
lastfm_ws_http_request                  (const VglServer  *srv,
                                         const char       *method,
                                         HttpRequestType   type,
                                         gboolean          add_api_sig,
                                         gint             *error_code,
                                         xmlDoc          **doc,
                                         const xmlNode   **node,
                                         ...)
{
        gboolean retvalue = FALSE;
        const char *name;
        char *buffer, *url;
        size_t bufsize;
        GList *l = NULL;
        va_list args;

        g_return_val_if_fail (srv && method && doc && node, FALSE);

        *doc  = NULL;
        *node = NULL;
        if (error_code != NULL) {
                *error_code = 0;
        }

        /* Add all parameters to a list */
        va_start (args, node);
        name = va_arg (args, char *);
        while (name != NULL) {
                const char *value = va_arg (args, char *);
                l = g_list_prepend (l, lastfm_ws_parameter_new (name, value));
                name = va_arg (args, char *);
        }
        va_end (args);

        /* Add 'method' and 'api_key' parameters */
        l = g_list_prepend (l, lastfm_ws_parameter_new ("method", method));
        l = g_list_prepend (l, lastfm_ws_parameter_new ("api_key",
                                                        srv->api_key));

        /* Sort list alphabetically */
        l = g_list_sort (l, (GCompareFunc) lastfm_ws_parameter_compare);

        /* Create URL and make HTTP request */
        url = lastfm_ws_format_params (srv, type, add_api_sig, l);
        if (type == HTTP_REQUEST_GET) {
                http_get_buffer (url, &buffer, &bufsize);
        } else {
                http_post_buffer (srv->ws_base_url, url,
                                  &buffer, &bufsize, NULL);
        }
        g_free (url);

        /* Parse response, create XML doc and validate the <lfm> root node */
        if (buffer != NULL) {
                *doc = xmlParseMemory (buffer, bufsize);
                if (*doc != NULL) {
                        xmlNode *n = xmlDocGetRootElement (*doc);
                        if ((n = (xmlNode *) xml_find_node (n, "lfm"))) {
                                xmlChar *status;
                                status = xmlGetProp (n, (xmlChar *) "status");
                                if (status) {
                                        retvalue = xmlStrEqual (
                                                status, (xmlChar *) "ok");
                                        xmlFree (status);
                                }
                                n = n->xmlChildrenNode;
                                if (retvalue) {
                                        *node = n;
                                }
                        }
                        if (!retvalue && error_code != NULL) {
                                n = (xmlNode *) xml_find_node (n, "error");
                                if (n != NULL) {
                                        xmlChar *err;
                                        err = xmlGetProp (n,(xmlChar *)"code");
                                        if (err) {
                                                *error_code =
                                                        atol ((char *) err);
                                                xmlFree (err);
                                        }
                                }
                        }
                        if (!retvalue) {
                                xmlFreeDoc (*doc);
                                *doc = NULL;
                        }
                }
        }

        /* Cleanup */
        g_list_foreach (l, (GFunc) lastfm_ws_parameter_destroy, NULL);
        g_list_free (l);
        g_free (buffer);

        return retvalue;
}

char *
lastfm_ws_get_auth_token                (const VglServer  *srv,
                                         char            **auth_url)
{
        char *retvalue = NULL;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (srv, NULL);

        lastfm_ws_http_request (srv, "auth.getToken", HTTP_REQUEST_GET,
                                TRUE, NULL, &doc, &node, NULL);

        if (doc != NULL) {
                xml_get_string (doc, node, "token", &retvalue);
                xmlFreeDoc (doc);
        }

        if (retvalue != NULL) {
                if (auth_url != NULL) {
                        *auth_url = g_strconcat (
                                "http://www.last.fm/api/auth/"
                                "?api_key=", srv->api_key,
                                "&token=", retvalue, NULL);
                }
        } else {
                if (auth_url != NULL) {
                        *auth_url = NULL;
                }
                g_warning ("Unable to get auth token");
        }

        return retvalue;
}

LastfmWsSession *
lastfm_ws_get_session_from_token        (VglServer  *srv,
                                         const char *token)
{
        LastfmWsSession *retvalue = NULL;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (srv && token, NULL);

        lastfm_ws_http_request (srv, "auth.getSession", HTTP_REQUEST_GET,
                                TRUE, NULL, &doc, &node, "token", token, NULL);

        if (doc != NULL) {
                node = xml_find_node (node, "session");
                if (node != NULL) {
                        char *user, *key;
                        gboolean subscriber;
                        node = node->xmlChildrenNode;
                        xml_get_string (doc, node, "name", &user);
                        xml_get_string (doc, node, "key", &key);
                        xml_get_bool (doc, node, "subscriber", &subscriber);
                        if (user && *user != '\0' && key && *key != '\0') {
                                retvalue = lastfm_ws_session_new (
                                        user, user, key, srv, subscriber);
                        }
                        g_free (user);
                        g_free (key);
                }
                xmlFreeDoc (doc);
        }

        if (!retvalue) {
                g_warning ("Unable to get session");
        }

        return retvalue;
}

LastfmWsSession *
lastfm_ws_get_session                   (VglServer  *srv,
                                         const char *user,
                                         const char *pass,
                                         LastfmErr  *err)
{
        LastfmWsSession *retvalue = NULL;
        int errcode;
        char *lcuser, *md5pw, *usermd5pw, *authtoken;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (srv && user && pass && err, NULL);

        lcuser = g_ascii_strdown (user, -1);
        md5pw = get_md5_hash (pass);
        usermd5pw = g_strconcat (lcuser, md5pw, NULL);
        authtoken = get_md5_hash (usermd5pw);

        lastfm_ws_http_request (srv, "auth.getMobileSession",
                                HTTP_REQUEST_GET, TRUE, &errcode, &doc, &node,
                                "authToken", authtoken,
                                "username", lcuser,
                                NULL);

        if (doc != NULL) {
                node = xml_find_node (node, "session");
                if (node != NULL) {
                        char *key;
                        gboolean subscriber;
                        node = node->xmlChildrenNode;
                        xml_get_string (doc, node, "key", &key);
                        xml_get_bool (doc, node, "subscriber", &subscriber);
                        if (key && key[0] != '\0') {
                                retvalue = lastfm_ws_session_new (
                                        lcuser, pass, key, srv, subscriber);
                        }
                        g_free (key);
                }
                xmlFreeDoc (doc);
        }

        if (retvalue) {
                retvalue->v1sess = lastfm_session_new (lcuser, pass,
                                                       srv->old_hs_url, err,
                                                       srv->free_streams);
        } else {
                g_warning ("Unable to get session");
                *err = errcode == 4 ? LASTFM_ERR_LOGIN : LASTFM_ERR_CONN;
        }

        g_free (lcuser);
        g_free (md5pw);
        g_free (usermd5pw);
        g_free (authtoken);

        return retvalue;
}

LastfmErrorCode
lastfm_ws_radio_tune                    (LastfmWsSession *session,
                                         const char      *radio_url,
                                         const char      *lang)
{
        xmlDoc *doc;
        const xmlNode *node;
        gint error_code;

        g_return_val_if_fail (session && radio_url, LASTFM_NOT_FOUND);

        /* If this is a custom URL, store the playlist for later */
        if (session->v1sess) {
                if (lastfm_radio_url_is_custom (radio_url)) {
                        LastfmPls *pls = lastfm_request_custom_playlist (
                                session->v1sess, radio_url);
                        g_mutex_lock (session->mutex);
                        lastfm_pls_destroy (session->v1sess->custom_pls);
                        session->v1sess->custom_pls = pls;
                        g_mutex_unlock (session->mutex);
                        return (pls != NULL) ? LASTFM_OK : LASTFM_NOT_FOUND;
                } else if (session->v1sess->custom_pls) {
                        g_mutex_lock (session->mutex);
                        lastfm_pls_destroy (session->v1sess->custom_pls);
                        session->v1sess->custom_pls = NULL;
                        g_mutex_unlock (session->mutex);
                }
                if (session->srv->old_str_api) {
                        gboolean set;
                        g_free (session->radio_name);
                        session->radio_name = NULL;
                        set = lastfm_set_radio (session->v1sess,
                                                radio_url,
                                                &(session->radio_name));
                        return set ? LASTFM_OK : LASTFM_NOT_FOUND;
                }
        }

        lastfm_ws_http_request (session->srv, "radio.tune",
                                HTTP_REQUEST_POST, TRUE,
                                &error_code, &doc, &node,
                                "sk", session->key,
                                "station", radio_url,
                                lang ? "lang" : NULL, lang,
                                NULL);

        if (doc != NULL) {
                node = xml_find_node (node, "station");
                if (node != NULL) {
                        node = node->xmlChildrenNode;
                }
                g_mutex_lock (session->mutex);
                g_free (session->radio_name);
                xml_get_string (doc, node, "name", &(session->radio_name));
                g_mutex_unlock (session->mutex);
                xmlFreeDoc (doc);
        } else {
                /* Fall back to the old streaming API if the new one
                 * doesn't work */
                if (session->v1sess && !session->subscriber) {
                        session->srv->old_str_api = TRUE;
                        return lastfm_ws_radio_tune (session, radio_url, lang);
                }
        }

        return error_code;
}

LastfmPls *
lastfm_ws_radio_get_playlist            (const LastfmWsSession *session,
                                         gboolean               discovery,
                                         gboolean               low_bitrate,
                                         gboolean               scrobbling)
{
        LastfmPls *pls = NULL;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (session, NULL);

        if (session->v1sess) {
                /* If there's a custom playlist available, return it
                   (see lastfm_ws_radio_tune()) */
                if (session->v1sess->custom_pls) {
                        g_mutex_lock (session->mutex);
                        pls = session->v1sess->custom_pls;
                        session->v1sess->custom_pls = NULL;
                        g_mutex_unlock (session->mutex);
                        return pls;
                }
                if (session->srv->old_str_api) {
                        return lastfm_request_playlist (session->v1sess,
                                                        discovery,
                                                        session->radio_name);
                }
        }

        lastfm_ws_http_request (session->srv, "radio.getPlaylist",
                                HTTP_REQUEST_GET, TRUE, NULL, &doc, &node,
                                "discovery", discovery ? "1" : "0",
                                "rtp", scrobbling ? "1" : "0",
                                "sk", session->key,
                                low_bitrate ? "bitrate" : NULL, "64",
                                NULL);

        if (doc != NULL) {
                g_mutex_lock (session->mutex);
                pls = lastfm_parse_playlist (doc, session->radio_name,
                                             session->srv->free_streams);
                g_mutex_unlock (session->mutex);
                xmlFreeDoc (doc);
        } else {
                /* Fall back to the old streaming API if the new one
                 * doesn't work */
                if (session->v1sess && !session->subscriber) {
                        session->srv->old_str_api = TRUE;
                        return lastfm_ws_radio_get_playlist (
                                session, discovery, low_bitrate, scrobbling);
                }
        }

        return pls;
}

gboolean
lastfm_ws_get_friends                   (const VglServer  *srv,
                                         const char       *user,
                                         GList           **friendlist)
{
        GList *list = NULL;
        gboolean retvalue = FALSE;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (srv && user && friendlist, FALSE);

        lastfm_ws_http_request (srv, "user.getFriends",
                                HTTP_REQUEST_GET, FALSE, NULL, &doc, &node,
                                "limit", "0",
                                "recenttracks", "0",
                                "user", user,
                                NULL);

        if (doc != NULL) {
                node = xml_find_node (node, "friends");
                if (node != NULL) {
                        node = node->xmlChildrenNode;
                        retvalue = TRUE;
                }
                while ((node = xml_find_node (node, "user"))) {
                        char *name;
                        xml_get_string (doc, node->xmlChildrenNode,
                                        "name", &name);
                        if (name) {
                                list = g_list_append (list, name);
                        }
                        node = node->next;
                }
                if (list != NULL) {
                        list = g_list_sort (list,
                                            (GCompareFunc) g_ascii_strcasecmp);
                }
                xmlFreeDoc (doc);
        }

        if (!retvalue) {
                g_warning ("Unable to get friend list");
        }

        *friendlist = list;

        return retvalue;
}

static gboolean
parse_xml_tags                          (xmlDoc         *doc,
                                         const xmlNode  *node,
                                         const char     *parent_name,
                                         GList         **list)
{
        gboolean retvalue = FALSE;
        const xmlNode *iter = node;

        g_return_val_if_fail (doc && list && !*list, FALSE);

        iter = xml_find_node (iter, parent_name);
        if (iter != NULL) {
                iter = xml_find_node (iter->xmlChildrenNode, "tag");
                retvalue = TRUE;
        }

        while (iter != NULL) {
                char *tag;
                xml_get_string (doc, iter->xmlChildrenNode, "name", &tag);
                if (tag != NULL && tag[0] != '\0') {
                        *list = g_list_prepend (*list, tag);
                } else {
                        g_warning ("Found tag with no name");
                        g_free (tag);
                }
                iter = xml_find_node (iter->next, "tag");
        }
        *list = g_list_reverse (*list);

        return retvalue;
}

gboolean
lastfm_ws_get_user_tags                 (const VglServer  *srv,
                                         const char       *username,
                                         GList           **taglist)
{
        gboolean retvalue = FALSE;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (srv && username && taglist && !*taglist,
                              FALSE);

        lastfm_ws_http_request (srv, "user.getTopTags", HTTP_REQUEST_GET,
                                FALSE, NULL, &doc, &node,
                                "user", username, NULL);

        if (doc != NULL) {
                retvalue = parse_xml_tags (doc, node, "toptags", taglist);
                xmlFreeDoc (doc);
        }

        if (!retvalue) {
                g_warning ("Unable to get user tags");
        }

        return retvalue;
}

gboolean
lastfm_ws_get_user_track_tags           (const LastfmWsSession  *session,
                                         const LastfmTrack      *track,
                                         LastfmTrackComponent    type,
                                         GList                 **taglist)
{
        gboolean retvalue = FALSE;
        xmlDoc *doc;
        const xmlNode *node;
        const char *method, *extraparam, *extraparamvalue, *artist;

        g_return_val_if_fail (track && session && taglist, FALSE);

        artist   = track->artist;
        *taglist = NULL;

        switch (type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                method = "artist.getTags";
                extraparam = extraparamvalue = NULL;
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                method = "track.getTags";
                extraparam = "track";
                extraparamvalue = track->title;
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_val_if_fail (track->album[0] != '\0', FALSE);
                artist = track->album_artist;
                method = "album.getTags";
                extraparam = "album";
                extraparamvalue = track->album;
                break;
        default:
                g_return_val_if_reached (FALSE);
        }

        lastfm_ws_http_request (session->srv, method, HTTP_REQUEST_GET,
                                TRUE, NULL, &doc, &node, "artist", artist,
                                "sk", session->key,
                                extraparam, extraparamvalue,
                                NULL);

        if (doc != NULL) {
                retvalue = parse_xml_tags (doc, node, "tags", taglist);
                xmlFreeDoc (doc);
        }

        if (!retvalue) {
                g_warning ("Unable to get user track tags");
        }

        return retvalue;
}

#ifndef VGL_USE_NEW_ALBUM_TAGS_API
static gboolean
lastfm_ws_old_get_album_tags            (const LastfmTrack     *track,
                                         GList                **taglist)
{
        const char album_tags_url[] =
                "http://ws.audioscrobbler.com/1.0/album/%s/%s/toptags.xml";
        char *artist, *album, *url, *buffer;
        size_t bufsize;
        gboolean found = FALSE;

        artist = lastfm_url_encode (track->album_artist);
        album = lastfm_url_encode (track->album);
        url = g_strdup_printf (album_tags_url, artist, album);
        http_get_buffer (url, &buffer, &bufsize);

        if (buffer != NULL) {
                xmlDoc *doc = xmlParseMemory (buffer, bufsize);
                if (doc != NULL) {
                        xmlNode *node = xmlDocGetRootElement (doc);
                        found = parse_xml_tags (doc, node, "toptags", taglist);
                        xmlFreeDoc (doc);
                }
        }

        g_free (artist);
        g_free (album);
        g_free (url);
        g_free (buffer);

        return found;
}
#endif /* VGL_USE_NEW_ALBUM_TAGS_API */

gboolean
lastfm_ws_get_track_tags                (const LastfmWsSession  *session,
                                         const LastfmTrack      *track,
                                         LastfmTrackComponent    type,
                                         GList                 **taglist)
{
        gboolean retvalue = FALSE;
        xmlDoc *doc;
        const xmlNode *node;
        const char *method, *extraparam, *extraparamvalue, *artist;

        g_return_val_if_fail (session && track && taglist, FALSE);

        artist   = track->artist;
        *taglist = NULL;

        switch (type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                method = "artist.getTopTags";
                extraparam = extraparamvalue = NULL;
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                method = "track.getTopTags";
                extraparam = "track";
                extraparamvalue = track->title;
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                /* The new API to get album tags seems to return less
                 * results than the old API */
                g_return_val_if_fail (track->album[0] != '\0', FALSE);
#ifdef VGL_USE_NEW_ALBUM_TAGS_API
                artist = track->album_artist;
                method = "album.getInfo";
                extraparam = "album";
                extraparamvalue = track->album;
                break;
#else
                return lastfm_ws_old_get_album_tags (track, taglist);
#endif
        default:
                g_return_val_if_reached (FALSE);
        }

        lastfm_ws_http_request (session->srv, method,
                                HTTP_REQUEST_GET, FALSE, NULL, &doc, &node,
                                "artist", artist,
                                extraparam, extraparamvalue,
                                NULL);

        if (doc != NULL) {
#ifdef VGL_USE_NEW_ALBUM_TAGS_API
                if (type == LASTFM_TRACK_COMPONENT_ALBUM) {
                        if ((node = xml_find_node (node, "album"))) {
                                node = node->xmlChildrenNode;
                        }
                }
#endif
                retvalue = parse_xml_tags (doc, node, "toptags", taglist);
                xmlFreeDoc (doc);
        }

        if (!retvalue) {
                g_warning ("Unable to get track tags");
        }

        return retvalue;
}

gboolean
lastfm_ws_add_tags                      (const LastfmWsSession *session,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent   type,
                                         const char            *tags)
{
        xmlDoc *doc;
        const xmlNode *node;
        const char *method, *extraparam, *extraparamvalue, *artist;

        g_return_val_if_fail (session && track && tags, FALSE);

        artist = track->artist;

        switch (type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                method = "artist.addTags";
                extraparam = extraparamvalue = NULL;
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                method = "track.addTags";
                extraparam = "track";
                extraparamvalue = track->title;
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_val_if_fail (track->album[0] != '\0', FALSE);
                artist = track->album_artist;
                method = "album.addTags";
                extraparam = "album";
                extraparamvalue = track->album;
                break;
        default:
                g_return_val_if_reached (FALSE);
        }

        lastfm_ws_http_request (session->srv, method, HTTP_REQUEST_POST,
                                TRUE, NULL, &doc, &node,
                                "artist", artist,
                                "sk", session->key,
                                "tags", tags,
                                extraparam, extraparamvalue,
                                NULL);

        if (doc != NULL) {
                xmlFreeDoc (doc);
                return TRUE;
        } else {
                g_warning ("Error adding tags");
                return FALSE;
        }
}

gboolean
lastfm_ws_remove_tag                    (const LastfmWsSession *session,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent   type,
                                         const char            *tag)
{
        xmlDoc *doc;
        const xmlNode *node;
        const char *method, *extraparam, *extraparamvalue, *artist;

        g_return_val_if_fail (session && track && tag, FALSE);

        artist = track->artist;

        switch (type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                method = "artist.removeTag";
                extraparam = extraparamvalue = NULL;
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                method = "track.removeTag";
                extraparam = "track";
                extraparamvalue = track->title;
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_val_if_fail (track->album[0] != '\0', FALSE);
                artist = track->album_artist;
                method = "album.removeTag";
                extraparam = "album";
                extraparamvalue = track->album;
                break;
        default:
                g_return_val_if_reached (FALSE);
        }

        lastfm_ws_http_request (session->srv, method, HTTP_REQUEST_POST,
                                TRUE, NULL, &doc, &node,
                                "artist", artist,
                                "sk", session->key,
                                "tag", tag,
                                extraparam, extraparamvalue,
                                NULL);

        if (doc != NULL) {
                xmlFreeDoc (doc);
                return TRUE;
        } else {
                g_warning ("Error adding tags");
                return FALSE;
        }
}

gboolean
lastfm_ws_share_track                   (const LastfmWsSession *session,
                                         const LastfmTrack     *track,
                                         const char            *text,
                                         LastfmTrackComponent   type,
                                         const char            *rcpt)
{
        xmlDoc *doc;
        const xmlNode *node;
        const char *method, *extraparam, *extraparamvalue;

        g_return_val_if_fail (session && track && text && rcpt, FALSE);

        switch (type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                method = "artist.share";
                extraparam = extraparamvalue = NULL;
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                method = "track.share";
                extraparam = "track";
                extraparamvalue = track->title;
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_val_if_fail (track->album[0] != '\0', FALSE);
                /* Use v1 API: sharing albums is not supported in v2 */
                return recommend_track (session->username, session->password,
                                        track, text, type, rcpt);
        default:
                g_return_val_if_reached (FALSE);
        }

        lastfm_ws_http_request (session->srv, method, HTTP_REQUEST_POST,
                                TRUE, NULL, &doc, &node,
                                "artist", track->artist,
                                "message", text,
                                "recipient", rcpt,
                                "sk", session->key,
                                extraparam, extraparamvalue,
                                NULL);

        if (doc != NULL) {
                xmlFreeDoc (doc);
                return TRUE;
        } else {
                return FALSE;
        }
}

gboolean
lastfm_ws_love_track                    (const LastfmWsSession *session,
                                         const LastfmTrack     *track)
{
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (session && track, FALSE);

        lastfm_ws_http_request (session->srv, "track.love",
                                HTTP_REQUEST_POST, TRUE, NULL, &doc, &node,
                                "artist", track->artist,
                                "sk", session->key,
                                "track", track->title,
                                NULL);

        if (doc != NULL) {
                xmlFreeDoc (doc);
                return TRUE;
        } else {
                return FALSE;
        }
}

gboolean
lastfm_ws_ban_track                     (const LastfmWsSession *session,
                                         const LastfmTrack     *track)
{
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (session && track, FALSE);

        lastfm_ws_http_request (session->srv, "track.ban",
                                HTTP_REQUEST_POST, TRUE, NULL, &doc, &node,
                                "artist", track->artist,
                                "sk", session->key,
                                "track", track->title,
                                NULL);

        if (doc != NULL) {
                xmlFreeDoc (doc);
                return TRUE;
        } else {
                return FALSE;
        }
}

gboolean
lastfm_ws_tag_track                     (const LastfmWsSession *session,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent   type,
                                         GSList                *tags)
{
        g_return_val_if_fail (session && track, FALSE);

        /* Fall back to the old API while the new one isn't implemented */
        return tag_track (session->username, session->password,
                          track, type, tags);
}

gboolean
lastfm_ws_add_to_playlist               (const LastfmWsSession *session,
                                         const LastfmTrack     *track)
{
        g_return_val_if_fail (session && track, FALSE);

        /* Fall back to the old API while the new one isn't implemented */
        return add_to_playlist (session->username, session->password, track);
}
