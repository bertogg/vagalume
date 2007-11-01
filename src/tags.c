/*
 * tags.c -- Functions to tag artists, tracks and albums
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
  "<params>"
    "<param><value><string>%s</string></value></param>" /* User */
    "<param><value><string>%s</string></value></param>" /* Time */
    "<param><value><string>%s</string></value></param>" /* Auth */
    "<param><value><string>%s</string></value></param>" /* Artist */
    "%s" /* Track or album, optional depending on the type of tag */
    "<param><value><array><data>";
static const char *request_data =
      "<value><string>%s</string></value>"; /* Each tag goes here */
static const char *request_ftr =
    "</data></array></value></param>"
    "<param><value><string>append</string></value></param>"
  "</params>"
"</methodCall>";

static const char *request_strparam =
    "<param><value><string>%s</string></value></param>";

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
        char *tmpbuf = NULL, *retbuf = NULL;
        char *md5password = get_md5_hash(password);
        char *timestamp = g_strdup_printf("%lu", time(NULL));
        char *auth1 = g_strconcat(md5password, timestamp, NULL);
        char *auth2 = get_md5_hash(auth1);
        char *extraparam = NULL; /* Track or album, if applicable */
        const char *method;
        if (type == TAG_ARTIST) {
                method = "tagArtist";
                extraparam = g_strdup("");
        } else if (type == TAG_TRACK) {
                method = "tagTrack";
                extraparam = g_strdup_printf(request_strparam, track->title);
        } else {
                method = "tagAlbum";
                extraparam = g_strdup_printf(request_strparam, track->album);
        }
        /* Create the first part of the request */
        char *request = g_strdup_printf(request_hdr, method, user,
                                        timestamp, auth2, track->artist,
                                        extraparam);

        /* Insert tags */
        GSList *iter;
        for (iter = tags; iter != NULL; iter = g_slist_next(iter)) {
                const char *tag = iter->data;
                if (tag != NULL) {
                        tmpbuf = request;
                        char *tmp = g_strdup_printf(request_data, tag);
                        request = g_strconcat(request, tmp, NULL);
                        g_free(tmpbuf);
                        g_free(tmp);
                }
        }

        /* Append footer */
        tmpbuf = request;
        request = g_strconcat(request, request_ftr, NULL);
        g_free(tmpbuf);

        /* Send request */
        headers = g_slist_append(headers, "Content-Type: text/xml");
        http_post_buffer(xmlrpc_url, request, &retbuf, headers);
        g_slist_free(headers);

        /* Check its return value */
        if (retbuf != NULL && g_strrstr(retbuf, "OK")) {
                g_debug("Correctly tagged");
        } else if (retbuf != NULL) {
                g_debug("Error tagging, response: %s", retbuf);
        } else {
                g_debug("Problem tagging, connection error?");
        }

        /* Cleanup */
        g_free(md5password);
        g_free(timestamp);
        g_free(auth1);
        g_free(auth2);
        g_free(extraparam);
        g_free(request);
        g_free(retbuf);
}
