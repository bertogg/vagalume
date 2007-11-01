/*
 * tags.c -- XMLRPC-based functions to tag artists, tracks, albums,
 *           loves and hates
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#include "tags.h"
#include "http.h"
#include "protocol.h"

static const char *xmlrpc_url =
"http://ws.audioscrobbler.com:80/1.0/rw/xmlrpc.php";

/*
 * Yes yes this is ugly but it's the easiest way to do it.
 * TODO: Allow action 'set' besides 'append'
 */
static const char *request_hdr =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<methodCall>"
  "<methodName>%s</methodName>" /* Method */
  "<params>";
static const char *request_strparam =
    "<param><value><string>%s</string></value></param>";
static const char *request_ftr =
  "</params>"
"</methodCall>";

static const char *array_param_hdr = "<param><value><array><data>";
static const char *array_param_body = "<value><string>%s</string></value>";
static const char *array_param_ftr = "</data></array></value></param>";

static char *
method_header(const char *value)
{
        g_return_val_if_fail(value != NULL, NULL);
        return g_strdup_printf(request_hdr, value);
}

static char *
string_param(const char *value)
{
        g_return_val_if_fail(value != NULL, NULL);
        return g_strdup_printf(request_strparam, value);
}

static char *
array_param(const GSList *params)
{
        char *tmp = NULL;
        char *ret = g_strdup(array_param_hdr);
        const GSList *iter;
        for (iter = params; iter != NULL; iter = g_slist_next(iter)) {
                tmp = ret;
                char *val = g_strdup_printf(array_param_body, iter->data);
                ret = g_strconcat(ret, val, NULL);
                g_free(tmp);
                g_free(val);
        }
        tmp = ret;
        ret = g_strconcat(ret, array_param_ftr, NULL);
        g_free(tmp);
        return ret;
}

/**
 * The first part of the xmlrpc request is always the same: method
 * name, user, timestamp, and cryptographic authentication. This
 * function computes all this.
 */
static char *
auth_header(const char *user, const char *password, const char *method)
{
        g_return_val_if_fail(user && password && method, NULL);
        char *md5password = get_md5_hash(password);
        char *timestamp = g_strdup_printf("%lu", time(NULL));
        char *auth1 = g_strconcat(md5password, timestamp, NULL);
        char *auth2 = get_md5_hash(auth1);
        char *hdr = method_header(method);
        char *param1 = string_param(user);
        char *param2 = string_param(timestamp);
        char *param3 = string_param(auth2);
        char *auth_hdr = g_strconcat(hdr, param1, param2, param3, NULL);
        g_free(md5password);
        g_free(timestamp);
        g_free(auth1);
        g_free(auth2);
        g_free(hdr);
        g_free(param1);
        g_free(param2);
        g_free(param3);
        return auth_hdr;
}

/**
 * Tags an artist, track or album, Previous tags won't be overwritten.
 *
 * @param user The user's Last.fm ID
 * @param password The user's password
 * @param track The track to tag
 * @param type The type of the tag (artist/track/album)
 * @param tags A list of tags to set
 */
void
tag_track(const char *user, const char *password, const lastfm_track *track,
          tag_type type, GSList *tags)
{
        g_return_if_fail(user && password && track && tags);
        GSList *headers = NULL;
        char *request;
        char *retbuf = NULL;
        char *hdr;
        char *param1, *param2, *param3, *param4;
        const char *method;
        if (type == TAG_ARTIST) {
                method = "tagArtist";
                param2 = g_strdup("");
        } else if (type == TAG_TRACK) {
                method = "tagTrack";
                param2 = string_param(track->title);
        } else {
                method = "tagAlbum";
                param2 = string_param(track->album);
        }
        hdr = auth_header(user, password, method);
        param1 = string_param(track->artist);
        param3 = array_param(tags);
        param4 = string_param("append");
        request = g_strconcat(hdr, param1, param2, param3, param4,
                              request_ftr, NULL);
        /* Send request */
        headers = g_slist_append(headers, "Content-Type: text/xml");
        http_post_buffer(xmlrpc_url, request, &retbuf, headers);

        /* Check its return value */
        if (retbuf != NULL && g_strrstr(retbuf, "OK")) {
                g_debug("Correctly tagged");
        } else if (retbuf != NULL) {
                g_debug("Error tagging, response: %s", retbuf);
        } else {
                g_debug("Problem tagging, connection error?");
        }

        /* Cleanup */
        g_slist_free(headers);
        g_free(request);
        g_free(retbuf);
        g_free(hdr);
        g_free(param1);
        g_free(param2);
        g_free(param3);
        g_free(param4);
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
 */
void
love_ban_track(const char *user, const char *password,
               const lastfm_track *track, gboolean love)
{
        g_return_if_fail(user && password && track);
        GSList *headers = NULL;
        char *retbuf = NULL;
        char *request, *hdr, *artist, *title;
        const char *method = love ? "loveTrack" : "banTrack";
        hdr = auth_header(user, password, method);
        artist = string_param(track->artist);
        title = string_param(track->title);
        request = g_strconcat(hdr, artist, title, request_ftr, NULL);
        /* Send request */
        headers = g_slist_append(headers, "Content-Type: text/xml");
        http_post_buffer(xmlrpc_url, request, &retbuf, headers);

        /* Check its return value */
        if (retbuf != NULL && g_strrstr(retbuf, "OK")) {
                g_debug("Correctly marked with %s", method);
        } else if (retbuf != NULL) {
                g_debug("Error marking with %s, response: %s", method, retbuf);
        } else {
                g_debug("Problem marking with %s, connection error?", method);
        }

        /* Cleanup */
        g_slist_free(headers);
        g_free(retbuf);
        g_free(request);
        g_free(hdr);
        g_free(artist);
        g_free(title);
}
