/*
 * protocol.c -- Last.fm streaming protocol and XSPF
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <libxml/parser.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "http.h"
#include "util.h"
#include "protocol.h"
#include "globaldefs.h"

static const char *handshake_url =
       "http://ws.audioscrobbler.com/radio/handshake.php"
       "?version=" LASTFM_APP_VERSION "&platform=" APP_OS_LC;
static const char *custom_pls_path =
       "/1.0/webclient/getresourceplaylist.php";
static const xmlChar *free_track_rel = (xmlChar *)
       "http://www.last.fm/freeTrackURL";
static const xmlChar *album_page_rel = (xmlChar *)
       "http://www.last.fm/albumpage";
static const char *lastfm_music_prefix =
       "http://www.last.fm/music/";

/**
 * Get the 2-letter language code from the currently active language
 * @return The language code, or "en" if it is unset. This string is
 *         static and must not be modified
 */
static const char *
get_language_code(void)
{
        static char lang[3] = "";
        if (lang[0] == '\0') {
                const char *env = g_getenv("LANGUAGE");
                if (!env) env = g_getenv("LC_MESSAGES");
                if (!env) env = g_getenv("LC_ALL");
                if (!env) env = g_getenv("LANG");
                if (env != NULL && strlen(env) > 1) {
                        strncpy(lang, env, 2);
                        lang[2] = '\0';
                } else {
                        strncpy(lang, "en", 3);
                }
        }
        return lang;
}

/**
 * Parse the output of a lastfm handshake and return a hashtable
 * containing keys and values
 * @param buffer A NULL string containing the handshake output
 * @return A new GHashTable containing the keys and values
 */
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

/**
 * Destroy a LastfmSession object, freeing all of its memory
 * @param session The session
 */
void
lastfm_session_destroy(LastfmSession *session)
{
        if (session == NULL) return;
        g_free(session->id);
        g_free(session->base_url);
        g_free(session->base_path);
        g_slice_free(LastfmSession, session);
}

/**
 * Copy a LastfmSession object
 * @param session The original object
 * @return A new object
 */
LastfmSession *
lastfm_session_copy(const LastfmSession *session)
{
        if (session == NULL) return NULL;
        LastfmSession *s = g_slice_new0(LastfmSession);
        *s = *session;
        s->id = g_strdup(session->id);
        s->base_url = g_strdup(session->base_url);
        s->base_path = g_strdup(session->base_path);
        return s;
}

/**
 * Create a new Last.fm session to play radios
 * @param username User's ID
 * @param password User's passwords
 * @param err If non-NULL, an error code will be written here
 * @return A new LastfmSession or NULL if it couldn't be created
 */
LastfmSession *
lastfm_session_new(const char *username, const char *password,
                   LastfmErr *err)
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

        LastfmSession *s = g_slice_new0(LastfmSession);
        s->id = g_strdup(g_hash_table_lookup(response, "session"));
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

/**
 * Parse a <track> element from an XSPF and add it to a playlist
 * @param doc The XML document that is being parsed
 * @param node The node poiting to the track to parse
 * @param pls The playlist where the track will be added
 * @return Whether a track has been found and added to the playlist
 */
static gboolean
lastfm_parse_track(xmlDoc *doc, xmlNode *node, LastfmPls *pls,
                   const char *pls_title, gboolean custom_pls)
{
        g_return_val_if_fail(doc!=NULL && node!=NULL && pls!=NULL, FALSE);

        gboolean retval = FALSE;
        const xmlChar *name;
        char *val;
        LastfmTrack *track = lastfm_track_new();
        track->pls_title =
                g_strdup (pls_title ? pls_title : _("(unknown radio)"));
        track->custom_pls = custom_pls;

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
                        track->id = strtol(val, NULL, 10);
                } else if (!xmlStrcmp(name, (xmlChar *) "artistId")) {
                        track->artistid = strtol(val, NULL, 10);
                } else if (!xmlStrcmp(name, (xmlChar *) "creator")) {
                        track->artist = g_strstrip(g_strdup(val));
                } else if (!xmlStrcmp(name, (xmlChar *) "album")) {
                        track->album = g_strstrip(g_strdup(val));
                } else if (!xmlStrcmp(name, (xmlChar *) "duration")) {
                        track->duration = strtol(val, NULL, 10);
                } else if (!xmlStrcmp(name, (xmlChar *) "image")) {
                        track->image_url = g_strstrip(g_strdup(val));
                } else if (!xmlStrcmp(name, (xmlChar *) "trackauth")) {
                        track->trackauth = g_strstrip(g_strdup(val));
                } else if (!xmlStrcmp(name, (xmlChar *) "link")) {
                        xmlChar *rel = xmlGetProp(node, (xmlChar *) "rel");
                        if (rel != NULL) {
                                if (!xmlStrcmp(rel, free_track_rel)) {
                                        track->free_track_url =
                                                g_strstrip(g_strdup(val));
                                } else if (!xmlStrcmp(rel, album_page_rel) &&
                                           g_str_has_prefix (
                                                   val, lastfm_music_prefix)) {
                                        char *artist, **parts;
                                        parts = g_strsplit (val, "/", 6);
                                        artist = lastfm_url_decode (parts[4]);
                                        g_strfreev (parts);
                                        track->album_artist = artist;
                                }
                                xmlFree(rel);
                        }
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
                if (track->album == NULL) {
                        /* album_artist != NULL unlikely if album == NULL,
                           but we free it just in case */
                        g_free ((gpointer) track->album_artist);
                        track->album = g_strdup("");
                        track->album_artist = g_strdup("");
                }
                if (track->trackauth == NULL) track->trackauth = g_strdup("");
                if (track->album_artist == NULL ||
                    !strcmp (track->artist, track->album_artist)) {
                        /* Don't waste memory */
                        g_free ((gpointer) track->album_artist);
                        track->album_artist = track->artist;
                }
                lastfm_pls_add_track(pls, track);
                retval = TRUE;
        }
        if (!retval) lastfm_track_unref(track);
        return retval;
}

/**
 * Parse a playlist in XSPF form and add its tracks to a lastfm_pls
 * @param buffer A buffer containing the playlist in XSPF format
 * @param bufsize Size of the buffer
 * @return A new playlist, or NULL if none was found
 */
static LastfmPls *
lastfm_parse_playlist (const char *buffer, size_t bufsize,
                       const char *default_pls_title)
{
        xmlDoc *doc = NULL;
        xmlNode *node = NULL;
        xmlNode *tracklist = NULL;
        LastfmPls *pls = NULL;
        const xmlChar *name;
        char *pls_title = NULL;
        gboolean custom_pls = FALSE;
        g_return_val_if_fail(buffer != NULL && bufsize > 0, NULL);

        doc = xmlParseMemory(buffer, bufsize);
        if (doc != NULL) {
                node = xmlDocGetRootElement(doc);
                if (!xmlStrcmp(node->name, (const xmlChar *) "playlist")) {
                        node = node->xmlChildrenNode;
                } else {
                        g_warning("Playlist file not in the expected format");
                        node = NULL;
                }
        } else {
                g_warning("Playlist is not an XML document");
        }
        while (node != NULL) {
                name = node->name;
                if (!xmlStrcmp(name, (const xmlChar *) "title")) {
                        char *title = (char *) xmlNodeListGetString(doc,
                                               node->xmlChildrenNode, 1);
                        if (title != NULL && pls_title == NULL) {
                                /* The playlist comes encoded */
                                pls_title = lastfm_url_decode (title);
                        }
                        xmlFree((xmlChar *)title);
                } else if (!xmlStrcmp(name, (const xmlChar *) "trackList")) {
                        tracklist = node;
                } else if (!xmlStrcmp(name, (const xmlChar *) "allflp")) {
                        custom_pls = TRUE;
                }
                node = node->next;
        }
        if (tracklist != NULL) {
                node = tracklist->xmlChildrenNode;
                pls = lastfm_pls_new();
        } else {
                g_warning("No tracks found in playlist");
                node = NULL;
        }
        if (pls_title == NULL) {
                pls_title = g_strdup (default_pls_title);
        }
        while (node != NULL) {
                if (!xmlStrcmp(node->name, (const xmlChar *) "track")) {
                        lastfm_parse_track(doc, node->xmlChildrenNode, pls,
                                           pls_title, custom_pls);
                }
                node = node->next;
        }
        g_free(pls_title);
        if (doc != NULL) xmlFreeDoc(doc);
        if (pls != NULL && lastfm_pls_size(pls) == 0) {
                lastfm_pls_destroy(pls);
                pls = NULL;
        }
        return pls;
}

/**
 * Request a new playlist from the currently active radio.
 * @param s The session
 * @param discovery Whether to use discovery mode or not
 * @param pls_title Playlist title to use if no name is found in the response.
 * @return A new playlist or NULL if none has been obtained
 */
LastfmPls *
lastfm_request_playlist (LastfmSession *s, gboolean discovery,
                         const char *pls_title)
{
        g_return_val_if_fail(s && s->id && s->base_url && s->base_path, NULL);
        const char *disc_mode = discovery ? "1" : "0";
        char *url;
        char *buffer = NULL;
        size_t bufsize = 0;
        LastfmPls *pls = NULL;

        url = g_strconcat("http://", s->base_url, s->base_path,
                          "/xspf.php?sk=", s->id, "&discovery=", disc_mode,
                          "&desktop=" LASTFM_APP_VERSION, NULL);
        http_get_buffer(url, &buffer, &bufsize);
        if (buffer != NULL) {
                pls = lastfm_parse_playlist (buffer, bufsize, pls_title);
                g_free(buffer);
        }
        g_free(url);
        return pls;
}

/**
 * Request a custom playlist (those starting with lastfm://play/).
 * These are handled different from the usual playlists
 * @param s The session
 * @param radio_url URL of the playlist
 * @return A new playlist, or NULL if none has been obtained
 */
LastfmPls *
lastfm_request_custom_playlist(LastfmSession *s, const char *radio_url)
{
        g_return_val_if_fail(s != NULL && radio_url != NULL, NULL);
        char *buffer = NULL;
        size_t bufsize = 0;
        LastfmPls *pls = NULL;
        char *url = NULL;
        char *radio_url_escaped = escape_url(radio_url, TRUE);
        url = g_strconcat("http://", s->base_url, custom_pls_path,
                          "?sk=", s->id, "&url=", radio_url_escaped,
                          "&desktop=" LASTFM_APP_VERSION, NULL);
        http_get_buffer(url, &buffer, &bufsize);
        if (buffer != NULL) {
                pls = lastfm_parse_playlist (buffer, bufsize, NULL);
                g_free(buffer);
        }
        g_free(url);
        g_free(radio_url_escaped);
        return pls;
}

/**
 * Set a radio URL. All the following playlist requests will return
 * tracks from this radio.
 * @param s The session
 * @param radio_url URL of the radio to set
 * @param pls_title If non-NULL, the playlist title will be stored there.
 * @return Whether the radio has been set correctly
 */
gboolean
lastfm_set_radio (LastfmSession *s, const char *radio_url, char **pls_title)
{
        g_return_val_if_fail(s != NULL && s->id != NULL &&
                             s->base_url != NULL &&
                             s->base_path && radio_url != NULL, FALSE);
        char *buffer = NULL;
        char *title = NULL;
        gboolean retval = FALSE;
        char *url;
        char *radio_url_escaped = escape_url(radio_url, TRUE);

        url = g_strconcat("http://", s->base_url, s->base_path,
                          "/adjust.php?session=", s->id,
                          "&lang=", get_language_code(), "&url=",
                          radio_url_escaped, NULL);
        http_get_buffer(url, &buffer, NULL);
        g_free(url);
        g_free(radio_url_escaped);

        if (buffer != NULL) {
                GHashTable *ht = lastfm_parse_handshake (buffer);
                const char *response = g_hash_table_lookup (ht, "response");
                retval = g_str_equal (response, "OK");
                title = g_strdup (g_hash_table_lookup (ht, "stationname"));
                g_hash_table_destroy (ht);
        }
        g_free(buffer);

        if (pls_title != NULL) {
                *pls_title = title;
        } else {
                g_free (title);
        }

        return retval;
}

