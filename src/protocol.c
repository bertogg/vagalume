
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libxml/parser.h>

#include "http.h"
#include "md5.h"
#include "protocol.h"

#define CLIENT_VERSION "0.1"
#define CLIENT_PLATFORM "linux"

static const char *handshake_url =
       "http://ws.audioscrobbler.com/radio/handshake.php"
       "?version=" CLIENT_VERSION
       "&platform=" CLIENT_PLATFORM;

static char *
get_md5_hash(const char *str)
{
        g_return_val_if_fail(str != NULL, NULL);
        const int digestlen = 16;
        md5_state_t state;
        md5_byte_t digest[digestlen];
        int i;

        md5_init(&state);
        md5_append(&state, (const md5_byte_t *)str, strlen(str));
        md5_finish(&state, digest);

        char *hexdigest = g_malloc(digestlen*2 + 1);
        for (i = 0; i < digestlen; i++) {
                sprintf(hexdigest + 2*i, "%02x", digest[i]);
        }
        return hexdigest;
}

static GHashTable *
lastfm_parse_handshake(const char *buffer)
{
        GHashTable *hash;
        int i;
        char **lines = g_strsplit(buffer, "\n", 50);
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
        lastfm_pls_destroy(session->playlist);
        g_free(session);
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
        s->playlist = lastfm_pls_new(NULL);
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
lastfm_request_xsfp(lastfm_session *s, char **buffer, size_t *size)
{
        char *url;
        g_return_if_fail(s != NULL);
        g_return_if_fail(s->id != NULL && s->base_url != NULL &&
                         s->base_path != NULL);

        url = g_strconcat("http://", s->base_url, s->base_path,
                          "/xspf.php?sk=", s->id,
                          "&discovery=0&desktop=" CLIENT_VERSION, NULL);
        http_get_buffer(url, buffer, size);
        g_free(url);
}

static gboolean
lastfm_parse_track(xmlDoc *doc, xmlNode *node, lastfm_pls *pls)
{
        g_return_val_if_fail(doc!=NULL && node!=NULL && pls!=NULL, FALSE);

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
                        track->stream_url = g_strdup(val);
                } else if (!xmlStrcmp(name, (xmlChar *) "title")) {
                        track->title = g_strdup(val);
                } else if (!xmlStrcmp(name, (xmlChar *) "id")) {
                        track->id = g_ascii_strtoll(val, NULL, 10);
                } else if (!xmlStrcmp(name, (xmlChar *) "creator")) {
                        track->artist = g_strdup(val);
                } else if (!xmlStrcmp(name, (xmlChar *) "album")) {
                        track->album = g_strdup(val);
                } else if (!xmlStrcmp(name, (xmlChar *) "duration")) {
                        track->duration = g_ascii_strtoll(val, NULL, 10);
                } else if (!xmlStrcmp(name, (xmlChar *) "image")) {
                        track->image_url = g_strdup(val);
                }
                xmlFree((xmlChar *)val);
                node = node->next;
        }
        if (track->stream_url == NULL || strlen(track->stream_url) == 0) {
                g_debug("Found track with no stream URL, discarding it");
                lastfm_track_destroy(track);
                return FALSE;
        } else {
                lastfm_pls_add_track(pls, track);
                return TRUE;
        }
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

gboolean
lastfm_request_playlist(lastfm_session *s)
{
        g_return_val_if_fail(s != NULL && s->playlist != NULL, FALSE);
        char *buffer = NULL;
        size_t bufsize = 0;
        xmlDoc *doc = NULL;
        gboolean retval = FALSE;

        lastfm_request_xsfp(s, &buffer, &bufsize);
        if (buffer != NULL) doc = xmlParseMemory(buffer, bufsize);
        if (doc != NULL) {
                retval = lastfm_parse_playlist(doc, s->playlist);
                xmlFreeDoc(doc);
        }
        xmlCleanupParser();
        g_free(buffer);
        return retval;
}

gboolean
lastfm_set_radio(lastfm_session *s, const char *radio_url)
{
        g_return_val_if_fail(s != NULL && s->playlist != NULL &&
                             s->id != NULL && s->base_url != NULL &&
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
                        lastfm_pls_clear(s->playlist);
                        retval = TRUE;
                }
        }
        g_free(buffer);

        return retval;
}
