/*
 * protocol.c -- Last.fm streaming protocol and XSPF
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#include <libxml/parser.h>
#include <string.h>

#include "http.h"
#include "util.h"
#include "protocol.h"
#include "globaldefs.h"

static const char *handshake_url =
       "http://ws.audioscrobbler.com/radio/handshake.php"
       "?version=" APP_VERSION "&platform=" APP_PLATFORM;
static const char *friends_url =
       "http://ws.audioscrobbler.com/1.0/user/%s/friends.txt";

static GHashTable *
lastfm_parse_handshake(const char *buffer)
{
        GHashTable *hash;
        int i;
        char **lines = g_strsplit(buffer, "\n", 0);
        hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                     g_free, g_free);
        for (i = 0; lines[i] != NULL; i++) {
                char **line = g_strsplit(lines[i], "=", 2);
                if (line[0] != NULL && line[1] != NULL) {
                        char *key = g_strstrip(g_strdup(line[0]));
                        char *val = g_strstrip(g_strdup(line[1]));
                        if (key[0] != '\0' && val[0] != '\0') {
                                g_hash_table_insert(hash, key, val);
                        } else {
                                g_free(key);
                                g_free(val);
                        }
                }
                g_strfreev(line);
        }
        g_strfreev(lines);
        return hash;
}

void
lastfm_session_destroy(lastfm_session *session)
{
        if (session == NULL) return;
        g_free(session->id);
        g_free(session->stream_url);
        g_free(session->base_url);
        g_free(session->base_path);
        g_free(session);
}

lastfm_session *
lastfm_session_copy(const lastfm_session *session)
{
        if (session == NULL) return NULL;
        lastfm_session *s = g_new0(lastfm_session, 1);
        *s = *session;
        s->id = g_strdup(session->id);
        s->stream_url = g_strdup(session->stream_url);
        s->base_url = g_strdup(session->base_url);
        s->base_path = g_strdup(session->base_path);
        return s;
}

lastfm_session *
lastfm_session_new(const char *username, const char *password,
                   lastfm_err *err)
{
        g_return_val_if_fail(username != NULL && password != NULL, NULL);
        char *buffer = NULL;
        char *md5password = get_md5_hash(password);
        char *url = g_strconcat(handshake_url, "&username=", username,
                                "&passwordmd5=", md5password, NULL);

        http_get_buffer(url, &buffer, NULL);
        g_free(md5password);
        g_free(url);
        if (buffer == NULL) {
                g_warning("Unable to initiate handshake");
                if (err != NULL) *err = LASTFM_ERR_CONN;
                return NULL;
        }

        GHashTable *response = lastfm_parse_handshake(buffer);

        g_free(buffer);

        lastfm_session *s = g_new0(lastfm_session, 1);
        s->id = g_strdup(g_hash_table_lookup(response, "session"));
        s->stream_url = g_strdup(g_hash_table_lookup(response, "stream_url"));
        s->base_url = g_strdup(g_hash_table_lookup(response, "base_url"));
        s->base_path = g_strdup(g_hash_table_lookup(response, "base_path"));
        const char *subs = g_hash_table_lookup(response, "subscriber");
        s->subscriber = (subs != NULL && *subs == '1');

        g_hash_table_destroy(response);

        if (s->id == NULL || s->base_url == NULL || s->base_path == NULL) {
                g_warning("Error building Last.fm session");
                if (err != NULL) *err = LASTFM_ERR_LOGIN;
                lastfm_session_destroy(s);
                s = NULL;
        } else if (err != NULL) {
                *err = LASTFM_ERR_NONE;
        }

        return s;
}

static void
lastfm_request_xsfp(lastfm_session *s, gboolean discovery, char **buffer,
                    size_t *size)
{
        char *url;
        const char *disc_mode = discovery ? "1" : "0";
        g_return_if_fail(s != NULL);
        g_return_if_fail(s->id != NULL && s->base_url != NULL &&
                         s->base_path != NULL);

        url = g_strconcat("http://", s->base_url, s->base_path,
                          "/xspf.php?sk=", s->id,
                          "&discovery=", disc_mode, "&desktop="
                          APP_VERSION, NULL);
        http_get_buffer(url, buffer, size);
        g_free(url);
}

static gboolean
lastfm_parse_track(xmlDoc *doc, xmlNode *node, lastfm_pls *pls)
{
        g_return_val_if_fail(doc!=NULL && node!=NULL && pls!=NULL, FALSE);

        gboolean retval = FALSE;
        const xmlChar *name;
        char *val;
        lastfm_track *track = g_new0(lastfm_track, 1);

        while (node != NULL) {
                name = node->name;
                val = (char *) xmlNodeListGetString(doc,
                                                    node->xmlChildrenNode,
                                                    1);
                if (val == NULL) {
                        /* Ignore empty nodes */;
                } else if (!xmlStrcmp(name, (xmlChar *) "location")) {
                        track->stream_url = g_strstrip(g_strdup(val));
                } else if (!xmlStrcmp(name, (xmlChar *) "title")) {
                        track->title = g_strstrip(g_strdup(val));
                } else if (!xmlStrcmp(name, (xmlChar *) "id")) {
                        track->id = strtoll(val, NULL, 10);
                } else if (!xmlStrcmp(name, (xmlChar *) "creator")) {
                        track->artist = g_strstrip(g_strdup(val));
                } else if (!xmlStrcmp(name, (xmlChar *) "album")) {
                        track->album = g_strstrip(g_strdup(val));
                } else if (!xmlStrcmp(name, (xmlChar *) "duration")) {
                        track->duration = strtoll(val, NULL, 10);
                } else if (!xmlStrcmp(name, (xmlChar *) "image")) {
                        track->image_url = g_strstrip(g_strdup(val));
                } else if (!xmlStrcmp(name, (xmlChar *) "trackauth")) {
                        track->trackauth = g_strstrip(g_strdup(val));
                }
                xmlFree((xmlChar *)val);
                node = node->next;
        }
        if (track->stream_url == NULL || track->stream_url[0] == '\0') {
                g_debug("Found track with no stream URL, discarding it");
        } else if (track->title == NULL || track->title[0] == '\0') {
                g_debug("Found track with no title, discarding it");
        } else if (track->artist == NULL || track->artist[0] == '\0') {
                g_debug("Found track with no artist, discarding it");
        } else {
                if (track->album == NULL) track->album = g_strdup("");
                if (track->trackauth == NULL) track->trackauth = g_strdup("");
                if (track->image_url == NULL) track->image_url = g_strdup("");
                lastfm_pls_add_track(pls, track);
                retval = TRUE;
        }
        if (!retval) lastfm_track_destroy(track);
        return retval;
}

static gboolean
lastfm_parse_playlist(xmlDoc *doc, lastfm_pls *pls)
{
        xmlNode *node, *tracklist;
        const xmlChar *name;
        gint pls_size;
        g_return_val_if_fail(doc != NULL && pls != NULL, FALSE);

        node = xmlDocGetRootElement(doc);
        if (xmlStrcmp(node->name, (const xmlChar *) "playlist")) {
                g_warning("Playlist file not in the expected format");
                return FALSE;
        }
        tracklist = NULL;
        node = node->xmlChildrenNode;
        while (node != NULL) {
                name = node->name;
                if (!xmlStrcmp(name, (const xmlChar *) "title")) {
                        char *title = (char *) xmlNodeListGetString(doc,
                                               node->xmlChildrenNode, 1);
                        if (title != NULL) {
                                char *unescaped = escape_url(title, FALSE);
                                int i;
                                for (i = 0; unescaped[i] != 0; i++) {
                                        if (unescaped[i] == '+')
                                                unescaped[i] = ' ';
                                }
                                lastfm_pls_set_title(pls, unescaped);
                                g_free(unescaped);
                        }
                        xmlFree((xmlChar *)title);
                } else if (!xmlStrcmp(name, (const xmlChar *) "trackList")) {
                        tracklist = node;
                }
                node = node->next;
        }
        if (tracklist == NULL) {
                g_warning("No tracks found in playlist");
                return FALSE;
        }
        node = tracklist->xmlChildrenNode;
        pls_size = lastfm_pls_size(pls);
        while (node != NULL) {
                if (!xmlStrcmp(node->name, (const xmlChar *) "track")) {
                        lastfm_parse_track(doc, node->xmlChildrenNode, pls);
                }
                node = node->next;
        }
        /* Return TRUE if the playlist has grown */
        return (pls_size < lastfm_pls_size(pls));
}

lastfm_pls *
lastfm_request_playlist(lastfm_session *s, gboolean discovery)
{
        g_return_val_if_fail(s != NULL, FALSE);
        char *buffer = NULL;
        size_t bufsize = 0;
        xmlDoc *doc = NULL;
        lastfm_pls *pls = NULL;

        lastfm_request_xsfp(s, discovery, &buffer, &bufsize);
        if (buffer != NULL) doc = xmlParseMemory(buffer, bufsize);
        if (doc != NULL) {
                pls = lastfm_pls_new(NULL);
                lastfm_parse_playlist(doc, pls);
                if (lastfm_pls_size(pls) == 0) {
                        lastfm_pls_destroy(pls);
                        pls = NULL;
                }
                xmlFreeDoc(doc);
        }
        xmlCleanupParser();
        g_free(buffer);
        return pls;
}

gboolean
lastfm_set_radio(lastfm_session *s, const char *radio_url)
{
        g_return_val_if_fail(s != NULL && s->id != NULL &&
                             s->base_url != NULL &&
                             s->base_path && radio_url != NULL, FALSE);
        char *buffer = NULL;
        gboolean retval = FALSE;
        char *url;
        char *radio_url_escaped = escape_url(radio_url, TRUE);

        url = g_strconcat("http://", s->base_url, s->base_path,
                          "/adjust.php?session=", s->id,
                          "&lang=en&url=", radio_url_escaped, NULL);
        http_get_buffer(url, &buffer, NULL);
        g_free(url);
        g_free(radio_url_escaped);

        if (buffer != NULL) {
                if (g_strrstr(buffer, "OK")) {
                        retval = TRUE;
                }
        }
        g_free(buffer);

        return retval;
}

GList *
lastfm_get_friends(const char *username)
{
        g_return_val_if_fail(username != NULL, NULL);
        GList *list = NULL;
        char *url = g_strdup_printf(friends_url, username);
        char *buffer = NULL;
        http_get_buffer(url, &buffer, NULL);
        if (buffer != NULL) {
                char **friends = g_strsplit(buffer, "\n", 0);
                int i;
                for (i = 0; friends[i] != NULL; i++) {
                        char *elem = g_strstrip(friends[i]);
                        if (*elem != '\0') {
                                list = g_list_append(list, g_strdup(elem));
                        }
                }
                g_strfreev(friends);
                if (list != NULL) {
                        list = g_list_sort(list, (GCompareFunc) strcasecmp);
                }
        }
        g_free(url);
        g_free(buffer);
        return list;
}
