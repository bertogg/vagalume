/*
 * lastfm-ws.c -- Last.fm Web Services v2.0
 *
 * Copyright (C) 2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "lastfm-ws.h"
#include "http.h"
#include "util.h"

#include <string.h>
#include <stdarg.h>

typedef enum {
        HTTP_REQUEST_GET,
        HTTP_REQUEST_POST
} HttpRequestType;

static const char lastfm_ws_base_url[] = "http://ws.audioscrobbler.com/2.0/";

/*
 * API key and secret, used to identify the Last.fm client.
 *
 * Note that these values are specific to Vagalume and meant to be
 * secret, but since this is an open source program we can't really
 * hide them. Please do not use them in other applications.
 *
 * To obtain a free API key and secret for your app (or for testing
 * purposes) follow this link:
 *
 * http://www.last.fm/api/account
 */
static const char vgl_lastfm_api_key[]    = "c00772ea9e00787179ce56e53bc51ec7";
static const char vgl_lastfm_api_secret[] = "10d704729842d9ef0129694be78d529a";

struct _LastfmWsSession {
        char *username;
        char *key;
        gboolean subscriber;
        int refcount;
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

static LastfmWsSession *
lastfm_ws_session_new                   (const char *username,
                                         const char *key,
                                         gboolean    subscriber)
{
        LastfmWsSession *session;

        g_return_val_if_fail (username && key, NULL);

        session = g_slice_new (LastfmWsSession);

        session->username   = g_strdup (username);
        session->key        = g_strdup (key);
        session->subscriber = subscriber;
        session->refcount   = 1;

        return session;
}

LastfmWsSession *
lastfm_ws_session_ref                   (LastfmWsSession *session)
{
        g_return_val_if_fail (session != NULL, NULL);
        g_atomic_int_inc (&(session->refcount));
        return session;
}

void
lastfm_ws_session_unref                 (LastfmWsSession *session)
{
        g_return_if_fail (session != NULL);
        if (g_atomic_int_dec_and_test (&(session->refcount))) {
                g_free (session->username);
                g_free (session->key);
                g_slice_free (LastfmWsSession, session);
        }
}

static void
lastfm_ws_sign_url                      (GString     *url,
                                         const GList *params)
{
        const GList *iter;
        char *md5;
        GString *sig = g_string_sized_new (200);

        for (iter = params; iter != NULL; iter = iter->next) {
                LastfmWsParameter *p = (LastfmWsParameter *) iter->data;
                g_string_append_printf (sig, "%s%s", p->name, p->value);
        }

        g_string_append (sig, vgl_lastfm_api_secret);

        md5 = get_md5_hash (sig->str);
        g_string_append_printf (url, "&api_sig=%s", md5);

        g_free (md5);
        g_string_free (sig, TRUE);
}

static char *
lastfm_ws_format_params                 (const char  *base_url,
                                         gboolean     add_api_sig,
                                         const GList *params)
{
        GString *url;
        const GList *iter;

        /* 200 bytes should be enough for all method calls */
        url = g_string_sized_new (200);

        if (base_url != NULL) {
                g_string_assign (url, base_url);
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
                lastfm_ws_sign_url (url, params);
        }

        /* Return */
        return g_string_free (url, FALSE);
}

static gboolean
lastfm_ws_http_request                  (const char       *method,
                                         HttpRequestType   type,
                                         gboolean          add_api_sig,
                                         xmlDoc          **doc,
                                         const xmlNode   **node,
                                         ...)
{
        gboolean retvalue = FALSE;
        const char *name;
        char *buffer;
        size_t bufsize;
        GList *l = NULL;
        va_list args;

        g_return_val_if_fail (method && doc && node, FALSE);

        *doc  = NULL;
        *node = NULL;

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
                                                        vgl_lastfm_api_key));

        /* Sort list alphabetically */
        l = g_list_sort (l, (GCompareFunc) lastfm_ws_parameter_compare);

        /* Create URL and make HTTP request */
        if (type == HTTP_REQUEST_GET) {
                char *url = lastfm_ws_format_params (lastfm_ws_base_url,
                                                     add_api_sig, l);
                http_get_buffer (url, &buffer, &bufsize);
                g_free (url);
        } else {
                char *postdata = lastfm_ws_format_params (NULL, add_api_sig, l);
                http_post_buffer (lastfm_ws_base_url, postdata,
                                  &buffer, &bufsize, NULL);
                g_free (postdata);
        }

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
                        }
                        if (retvalue) {
                                *node = n->xmlChildrenNode;
                        } else {
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
lastfm_ws_get_auth_token                (char **auth_url)
{
        char *retvalue = NULL;
        xmlDoc *doc;
        const xmlNode *node;

        lastfm_ws_http_request ("auth.getToken", HTTP_REQUEST_GET, TRUE,
                                &doc, &node, NULL);

        if (doc != NULL) {
                xml_get_string (doc, node, "token", &retvalue);
                xmlFreeDoc (doc);
        }

        if (retvalue != NULL) {
                if (auth_url != NULL) {
                        *auth_url = g_strconcat (
                                "http://www.last.fm/api/auth/"
                                "?api_key=", vgl_lastfm_api_key,
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
lastfm_ws_get_session_from_token        (const char *token)
{
        LastfmWsSession *retvalue = NULL;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (token, NULL);

        lastfm_ws_http_request ("auth.getSession", HTTP_REQUEST_GET, TRUE,
                                &doc, &node, "token", token, NULL);

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
                                        user, key, subscriber);
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
lastfm_ws_get_session                   (const char *user,
                                         const char *pass)
{
        LastfmWsSession *retvalue = NULL;
        char *md5pw, *usermd5pw, *authtoken;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (user && pass, NULL);

        md5pw = get_md5_hash (pass);
        usermd5pw = g_strconcat (user, md5pw, NULL);
        authtoken = get_md5_hash (usermd5pw);

        lastfm_ws_http_request ("auth.getMobileSession",
                                HTTP_REQUEST_GET, TRUE, &doc, &node,
                                "authToken", authtoken,
                                "username", user,
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
                                        user, key, subscriber);
                        }
                        g_free (key);
                }
                xmlFreeDoc (doc);
        }

        if (!retvalue) {
                g_warning ("Unable to get session");
        }

        g_free (md5pw);
        g_free (usermd5pw);
        g_free (authtoken);

        return retvalue;
}

gboolean
lastfm_ws_get_friends                   (const char  *user,
                                         GList      **friendlist)
{
        GList *list = NULL;
        gboolean retvalue = FALSE;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (user && friendlist, FALSE);

        lastfm_ws_http_request ("user.getFriends",
                                HTTP_REQUEST_GET, FALSE, &doc, &node,
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
                        list = g_list_sort (list, (GCompareFunc) strcasecmp);
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
lastfm_ws_get_user_tags                 (const char  *username,
                                         GList      **taglist)
{
        gboolean retvalue = FALSE;
        xmlDoc *doc;
        const xmlNode *node;

        g_return_val_if_fail (username && taglist && !*taglist, FALSE);

        lastfm_ws_http_request ("user.getTopTags", HTTP_REQUEST_GET, FALSE,
                                &doc, &node, "user", username, NULL);

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

        lastfm_ws_http_request (method, HTTP_REQUEST_GET, TRUE, &doc, &node,
                                "artist", artist,
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

gboolean
lastfm_ws_get_track_tags                (const LastfmTrack     *track,
                                         LastfmTrackComponent   type,
                                         GList                **taglist)
{
        gboolean retvalue = FALSE;
        xmlDoc *doc;
        const xmlNode *node;
        const char *method, *extraparam, *extraparamvalue, *artist;

        g_return_val_if_fail (track && taglist, FALSE);

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
                /* FIXME: Last.fm don't seem to be sending album tags
                 * with the new API. Consider using
                 * metadata.c:lastfm_get_tags() instead */
                g_return_val_if_fail (track->album[0] != '\0', FALSE);
                artist = track->album_artist;
                method = "album.getInfo";
                extraparam = "album";
                extraparamvalue = track->album;
                break;
        default:
                g_return_val_if_reached (FALSE);
        }

        lastfm_ws_http_request (method, HTTP_REQUEST_GET, FALSE, &doc, &node,
                                "artist", track->artist,
                                extraparam, extraparamvalue,
                                NULL);

        if (doc != NULL) {
                if (type == LASTFM_TRACK_COMPONENT_ALBUM) {
                        if ((node = xml_find_node (node, "album"))) {
                                node = node->xmlChildrenNode;
                        }
                }
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

        lastfm_ws_http_request (method, HTTP_REQUEST_POST, TRUE, &doc, &node,
                                "artist", track->artist,
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

        lastfm_ws_http_request (method, HTTP_REQUEST_POST, TRUE, &doc, &node,
                                "artist", track->artist,
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
                g_warning ("Sharing albums is not supported in 2.0 API!!");
                return FALSE;
        default:
                g_return_val_if_reached (FALSE);
        }

        lastfm_ws_http_request (method, HTTP_REQUEST_POST, TRUE, &doc, &node,
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

        lastfm_ws_http_request ("track.love",
                                HTTP_REQUEST_POST, TRUE, &doc, &node,
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

        lastfm_ws_http_request ("track.ban",
                                HTTP_REQUEST_POST, TRUE, &doc, &node,
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