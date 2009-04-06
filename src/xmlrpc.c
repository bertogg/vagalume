/*
 * xmlrpc.c -- XMLRPC-based functions to tag artists, tracks, albums,
 *             loves and hates
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include <libxml/parser.h>
#include <stdarg.h>
#include <time.h>

#include "xmlrpc.h"
#include "http.h"
#include "util.h"

static const char xmlrpc_url[] =
"http://ws.audioscrobbler.com:80/1.0/rw/xmlrpc.php";

/**
 * Create a new xmlNode * with a string parameter to be added to the
 * request
 * @param str The value of the string
 * @return A new xmlNode *
 */
static xmlNode *
string_param                            (const char *str)
{
        g_return_val_if_fail(str != NULL, NULL);
        xmlNode *param = xmlNewNode(NULL, (xmlChar *) "param");
        xmlNode *value = xmlNewNode(NULL, (xmlChar *) "value");
        xmlAddChild(param, value);
        xml_add_string (value, "string", str);
        return param;
}

/**
 * Create a new xmlNode * with an array parameter to be added to the
 * request
 * @param params A lists of strings
 * @return A new xmlNode *
 */
static xmlNode *
array_param                             (const GSList *params)
{
        const GSList *iter;
        xmlNode *param = xmlNewNode(NULL, (xmlChar *) "param");
        xmlNode *value = xmlNewNode(NULL, (xmlChar *) "value");
        xmlNode *array = xmlNewNode(NULL, (xmlChar *) "array");
        xmlNode *data = xmlNewNode(NULL, (xmlChar *) "data");
        xmlAddChild(param, value);
        xmlAddChild(value, array);
        xmlAddChild(array, data);
        for (iter = params; iter != NULL; iter = g_slist_next(iter)) {
                xmlNode *val = xmlNewNode(NULL, (xmlChar *) "value");
                xmlAddChild(data, val);
                xml_add_string (val, "string", (const char *) iter->data);
        }
        return param;
}

/**
 * Create a full XML document with the xmlrpc request. It receives a
 * variable, NULL-terminated list of nodes (xmlNode *) to be added as
 * parameters of the XMLRPC request
 * @param user User's Last.fm ID
 * @param password User's password
 * @param method The method
 * @return A new string containing the full XML request. Must be freed
 *         by the caller
 */
static char *
new_request                             (const char *user,
                                         const char *password,
                                         const char *method,
                                         ...)
{
        g_return_val_if_fail(user && password && method, NULL);
        va_list ap;
        char *retval;
        xmlChar *xmlbuff;
        int buffersize;
        xmlDocPtr doc = xmlNewDoc((xmlChar *) "1.0");
        xmlNode *root = xmlNewNode(NULL, (xmlChar *) "methodCall");
        xmlNode *name = xmlNewNode(NULL, (xmlChar *) "methodName");
        xmlNode *params = xmlNewNode(NULL, (xmlChar *) "params");
        xmlNode *elem;
        char *timestamp = g_strdup_printf("%lu", time(NULL));
        char *auth = compute_auth_token(password, timestamp);
        xmlDocSetRootElement(doc, root);
        xmlNodeSetContent(name, (xmlChar *) method);
        xmlAddChild(root, name);
        xmlAddChild(root, params);
        xmlAddChild(params, string_param(user));
        xmlAddChild(params, string_param(timestamp));
        xmlAddChild(params, string_param(auth));

        va_start(ap, method);
        elem = va_arg(ap, xmlNode *);
        while (elem != NULL) {
                xmlAddChild(params, elem);
                elem = va_arg(ap, xmlNode *);
        }
        va_end(ap);

        xmlDocDumpMemoryEnc(doc, &xmlbuff, &buffersize, "UTF-8");
        retval = g_strdup((char *) xmlbuff);

        xmlFree(xmlbuff);
        xmlFreeDoc(doc);
        g_free(timestamp);
        g_free(auth);
        return retval;
}

/**
 * Send the actual request
 * @param request String containing the request in XML
 * @param name Name of the method (for debugging purposes only)
 * @return Whether the operation was successful or not
 */
static gboolean
xmlrpc_send_request                     (const char *request,
                                         const char *name)
{
        g_return_val_if_fail(request != NULL, FALSE);
        gboolean retval = FALSE;
        GSList *headers = NULL;
        char *retbuf = NULL;
        headers = g_slist_append(headers, "Content-Type: text/xml");
        http_post_buffer (xmlrpc_url, request, &retbuf, NULL, headers);

        /* Check its return value */
        if (retbuf != NULL && g_strrstr(retbuf, "OK")) {
                g_debug("XMLRPC call (%s) OK", name);
                retval = TRUE;
        } else if (retbuf != NULL) {
                g_debug("Error in XMLRPC call (%s): %s", name, retbuf);
        } else {
                g_debug("Error in XMLRPC call (%s), connection error?", name);
        }

        /* Cleanup */
        g_slist_free(headers);
        g_free(retbuf);
        return retval;
}

/**
 * Tags an artist, track or album, Previous tags will be overwritten.
 *
 * @param user The user's Last.fm ID
 * @param password The user's password
 * @param track The track to tag
 * @param type The type of the tag (artist/track/album)
 * @param tags A list of tags to set
 * @return Whether the operation was successful or not
 */
gboolean
tag_track                               (const char           *user,
                                         const char           *password,
                                         const LastfmTrack    *track,
                                         LastfmTrackComponent  type,
                                         GSList               *tags)
{
        g_return_val_if_fail(user && password && track, FALSE);
        gboolean retval;
        char *request;
        xmlNode *param1, *param2, *param3, *param4;
        const char *method;
        param3 = array_param(tags);
        param4 = string_param("set");                  /* or use "append" */
        if (type == LASTFM_TRACK_COMPONENT_ARTIST) {
                method = "tagArtist";
                param1 = string_param(track->artist);
                param2 = NULL;
        } else if (type == LASTFM_TRACK_COMPONENT_TRACK) {
                method = "tagTrack";
                param1 = string_param(track->artist);
                param2 = string_param(track->title);
        } else {
                method = "tagAlbum";
                param1 = string_param (track->album_artist);
                param2 = string_param (track->album);
        }
        if (param2 != NULL) {
                request = new_request(user, password, method, param1, param2,
                                      param3, param4, NULL);
        } else {
                request = new_request(user, password, method, param1,
                                      param3, param4, NULL);
        }

        retval = xmlrpc_send_request(request, method);
        g_free(request);
        return retval;
}

/**
 * Mark a track as loved or banned. This is the old protocol and will
 * be overriden by the Audioscrobbler Realtime Submission Protocol
 * v1.2 (see rsp_rscrobble()), but now it's still necessary
 *
 * @param user The user's Last.fm ID
 * @param password The user's password
 * @param track The track to mark as loved or banned
 * @param love TRUE to love, FALSE to ban
 * @return Whether the operation was successful or not
 */
gboolean
love_ban_track                          (const char        *user,
                                         const char        *password,
                                         const LastfmTrack *track,
                                         gboolean           love)
{
        g_return_val_if_fail(user && password && track, FALSE);
        gboolean retval;
        char *request;
        xmlNode *artist, *title;
        const char *method = love ? "loveTrack" : "banTrack";
        artist = string_param(track->artist);
        title = string_param(track->title);
        request = new_request(user, password, method, artist, title, NULL);
        retval = xmlrpc_send_request(request, method);
        g_free(request);
        return retval;
}

/**
 * Recommend a track to a user
 *
 * @param user The user's Last.fm ID
 * @param password The user's password
 * @param track The track to recommend
 * @param text The text of the recommendation
 * @param type Whether to recommend an artist, track or album
 * @param rcpt The user who will receive the recommendation
 * @return Whether the operation was successful or not
 */
gboolean
recommend_track                         (const char           *user,
                                         const char           *password,
                                         const LastfmTrack    *track,
                                         const char           *text,
                                         LastfmTrackComponent  type,
                                         const char           *rcpt)
{
        g_return_val_if_fail(user && password && track && text && rcpt, FALSE);
        gboolean retval;
        char *request;
        const char *method = "recommendItem";
        xmlNode *artist, *title, *recomm_type, *recomm_to;
        xmlNode *recomm_body, *language;
        if (type == LASTFM_TRACK_COMPONENT_ARTIST) {
                artist = string_param(track->artist);
                title = string_param("");
                recomm_type = string_param("artist");
        } else if (type == LASTFM_TRACK_COMPONENT_TRACK) {
                artist = string_param(track->artist);
                title = string_param(track->title);
                recomm_type = string_param("track");
        } else {
                artist = string_param (track->album_artist);
                title = string_param(track->album);
                recomm_type = string_param("album");
        }
        recomm_to = string_param(rcpt);
        recomm_body = string_param(text);
        language = string_param("en");
        request = new_request(user, password, method, artist,
                              title, recomm_type, recomm_to,
                              recomm_body, language, NULL);
        retval = xmlrpc_send_request(request, method);
        g_free(request);
        return retval;
}

/**
 * Add a track to the user's playlist
 *
 * @param user The user's Last.fm ID
 * @param password The user's password
 * @param track The track to add to the playlist
 * @return Whether the operation was successful or not
 */
gboolean
add_to_playlist                         (const char        *user,
                                         const char        *password,
                                         const LastfmTrack *track)
{
        g_return_val_if_fail(user && password && track, FALSE);
        gboolean retval;
        char *request;
        const char *method = "addTrackToUserPlaylist";
        xmlNode *artist, *title;
        artist = string_param(track->artist);
        title = string_param(track->title);
        request = new_request(user, password, method, artist, title, NULL);
        retval = xmlrpc_send_request(request, method);
        g_free(request);
        return retval;
}
