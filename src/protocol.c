/*
 * protocol.c -- Last.fm legacy streaming protocol and XSPF
 *
 * Copyright (C) 2007-2011 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include <string.h>

#include "http.h"
#include "util.h"
#include "protocol.h"
#include "globaldefs.h"

static const char custom_pls_path[] =
       "/1.0/webclient/getresourceplaylist.php";
static const xmlChar free_track_rel[] =
       "http://www.last.fm/freeTrackURL";
static const xmlChar album_page_rel[] =
       "http://www.last.fm/albumpage";
static const char lastfm_music_prefix[] =
       "http://www.last.fm/music/";

/**
 * Parse the output of a lastfm handshake and return a hashtable
 * containing keys and values
 * @param buffer A NULL string containing the handshake output
 * @return A new GHashTable containing the keys and values
 */
static GHashTable *
lastfm_parse_handshake                  (const char *buffer)
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
lastfm_session_destroy                  (LastfmSession *session)
{
        if (session == NULL) return;
        g_free(session->id);
        g_free(session->base_url);
        g_free(session->base_path);
        lastfm_pls_destroy(session->custom_pls);
        g_slice_free(LastfmSession, session);
}

/**
 * Create a new Last.fm session to play radios
 * @param username User's ID
 * @param password User's passwords
 * @param err If non-NULL, an error code will be written here
 * @return A new LastfmSession or NULL if it couldn't be created
 */
LastfmSession *
lastfm_session_new                      (const char *username,
                                         const char *password,
                                         const char *handshake_url,
                                         LastfmErr  *err,
                                         gboolean    free_streams)
{
        g_return_val_if_fail (username && password && handshake_url, NULL);
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
        s->free_streams = free_streams;

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
parse_old_track_parts                   (xmlDoc      *doc,
                                         xmlNode     *node,
                                         LastfmTrack *track)
{
        char *val;
        glong id, artistid, duration;

        xml_get_string (doc, node, "trackauth", (char **) &(track->trackauth));
        xml_get_glong (doc, node, "id", &id);
        xml_get_glong (doc, node, "artistId", &artistid);
        xml_get_glong (doc, node, "duration", &duration);

        track->id = id;
        track->artistid = artistid;
        track->duration = duration;

        node = (xmlNode *) xml_get_string (doc, node, "link", &val);
        while (node != NULL) {
                xmlChar *rel = xmlGetProp (node, (xmlChar *) "rel");
                if (rel != NULL && val != NULL && val[0] != '\0') {
                        if (xmlStrEqual (rel, free_track_rel)) {
                                g_free ((gpointer) track->free_track_url);
                                track->free_track_url = val;
                                val = NULL;
                        } else if (xmlStrEqual (rel, album_page_rel) &&
                                   g_str_has_prefix (
                                           val, lastfm_music_prefix)) {
                                char *artist, **parts;
                                parts = g_strsplit (val, "/", 6);
                                artist = lastfm_url_decode (parts[4]);
                                g_strfreev (parts);
                                g_free ((gpointer) track->album_artist);
                                track->album_artist = artist;
                        }
                        xmlFree (rel);
                }
                g_free (val);
                node = (xmlNode *) xml_get_string (doc, node->next,
                                                   "link", &val);
        }
}

static void
parse_new_track_parts                   (xmlDoc      *doc,
                                         xmlNode     *node,
                                         LastfmTrack *track)
{
        glong id, duration;

        xml_get_glong (doc, node, "identifier", &id);
        xml_get_glong (doc, node, "duration", &duration);

        track->id = id;
        track->duration = duration;

        node = (xmlNode *) xml_find_node (node, "extension");
        if (node != NULL) {
                glong artistid;
                char *albumpage, *auth, *free_track;
                node = node->xmlChildrenNode;

                xml_get_glong (doc, node, "artistid", &artistid);
                xml_get_string (doc, node, "trackauth", &auth);
                xml_get_string (doc, node, "freeTrackURL", &free_track);
                xml_get_string (doc, node, "albumpage", &albumpage);

                if (albumpage &&
                    g_str_has_prefix (albumpage, lastfm_music_prefix)) {
                        char *artist, **parts;
                        parts = g_strsplit (albumpage, "/", 6);
                        artist = lastfm_url_decode (parts[4]);
                        g_strfreev (parts);
                        g_free ((gpointer) track->album_artist);
                        track->album_artist = artist;
                }

                /* We don't want empty strings */
                if (auth && *auth == '\0') {
                        g_free (auth);
                        auth = NULL;
                }

                if (free_track && *free_track == '\0') {
                        g_free (free_track);
                        free_track = NULL;
                }

                track->trackauth = auth;
                track->free_track_url = free_track;
                track->artistid = artistid;

                g_free (albumpage);
        }
}

/**
 * Parse a <track> element from an XSPF and add it to a playlist
 * @param doc The XML document that is being parsed
 * @param node The node poiting to the track to parse
 * @param pls The playlist where the track will be added
 * @param pls_title Title of the playlist where this track is in
 * @param new_format Whether the track is in the new XML format or not
 * @return Whether a track has been found and added to the playlist
 */
static gboolean
lastfm_parse_track                      (xmlDoc     *doc,
                                         xmlNode    *node,
                                         LastfmPls  *pls,
                                         const char *pls_title,
                                         gboolean    new_format,
                                         gboolean    free_streams)
{
        g_return_val_if_fail (doc && node && pls, FALSE);

        gboolean retval = FALSE;
        LastfmTrack *track = lastfm_track_new ();
        track->pls_title =
                g_strdup (pls_title ? pls_title : _("(unknown radio)"));

        /* Common elements in both formats */
        xml_get_string (doc, node, "location", (char **) &(track->stream_url));
        xml_get_string (doc, node, "title", (char **) &(track->title));
        xml_get_string (doc, node, "creator", (char **) &(track->artist));
        xml_get_string (doc, node, "album", (char **) &(track->album));
        xml_get_string (doc, node, "image", (char **) &(track->image_url));

        if (new_format) {
                parse_new_track_parts (doc, node, track);
        } else {
                parse_old_track_parts (doc, node, track);
        }

        if (!track->stream_url || track->stream_url[0] == '\0') {
                g_debug("Found track with no stream URL, discarding it");
        } else if (!track->title || track->title[0] == '\0') {
                g_debug("Found track with no title, discarding it");
        } else if (!track->artist || track->artist[0] == '\0') {
                g_debug("Found track with no artist, discarding it");
        } else {
                if (track->album_artist == NULL ||
                    !strcmp (track->artist, track->album_artist)) {
                        /* Don't waste memory */
                        g_free ((gpointer) track->album_artist);
                        track->album_artist = track->artist;
                }
                if (free_streams) {
                        g_free ((char *) track->free_track_url);
                        track->free_track_url = g_strdup (track->stream_url);
                }
                lastfm_pls_add_track(pls, track);
                retval = TRUE;
        }
        if (!retval) vgl_object_unref (track);
        return retval;
}

/**
 * Parse a playlist in XSPF form and add its tracks to a lastfm_pls
 * @param doc An XML document containing the playlist
 * @param default_pls_title The title of the playlist in case the XML
 *        doesn't provide one
 * @return A new playlist, or NULL if none was found
 */
LastfmPls *
lastfm_parse_playlist                   (xmlDoc     *doc,
                                         const char *default_pls_title,
                                         gboolean    free_streams)
{
        const xmlNode *tracklist, *node, *root;
        gboolean new_format = FALSE;
        LastfmPls *pls = NULL;
        char *pls_title;
        g_return_val_if_fail (doc != NULL, NULL);

        /* First see whether the playlist is in the new format or not */
        root = xmlDocGetRootElement (doc);
        node = xml_find_node (root, "lfm");
        if (node != NULL) {
                new_format = TRUE;
                node = node->xmlChildrenNode;
        } else {
                node = root;
        }

        node = xml_find_node (node, "playlist");
        if (node != NULL) {
                node = node->xmlChildrenNode;
        } else {
                g_warning ("Playlist file not in the expected format");
        }

        /* Get playlist title */
        xml_get_string (doc, node, "title", &pls_title);
        if (pls_title && pls_title[0] != '\0') {
                char *tmp = lastfm_url_decode (pls_title);
                g_free (pls_title);
                pls_title = tmp;
        } else {
                g_free (pls_title);
                pls_title = g_strdup (default_pls_title);
        }

        /* Get trackList node */
        tracklist = xml_find_node (node, "trackList");
        if (tracklist != NULL) {
                node = xml_find_node (tracklist->xmlChildrenNode, "track");
                pls = lastfm_pls_new();
                while (node != NULL) {
                        lastfm_parse_track (doc, node->xmlChildrenNode,
                                            pls, pls_title, new_format,
                                            free_streams);
                        node = xml_find_node (node->next, "track");
                }
                if (lastfm_pls_size (pls) == 0) {
                        lastfm_pls_destroy (pls);
                        pls = NULL;
                }
        } else {
                g_warning("No tracks found in playlist");
        }

        g_free(pls_title);
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
lastfm_request_playlist                 (LastfmSession *s,
                                         gboolean       discovery,
                                         const char    *pls_title)
{
        g_return_val_if_fail(s && s->id && s->base_url && s->base_path, NULL);
        const char *disc_mode = discovery ? "1" : "0";
        char *url;
        char *buffer = NULL;
        size_t bufsize = 0;
        LastfmPls *pls = NULL;

        url = g_strconcat("http://", s->base_url, s->base_path,
                          "/xspf.php?sk=", s->id, "&discovery=", disc_mode,
                          "&desktop=1.5", NULL);
        http_get_buffer(url, &buffer, &bufsize);
        if (buffer != NULL) {
                xmlDoc *doc = xmlParseMemory(buffer, bufsize);
                if (doc != NULL) {
                        pls = lastfm_parse_playlist (doc, pls_title,
                                                     s->free_streams);
                        xmlFreeDoc (doc);
                } else {
                        g_warning ("Playlist is not an XML document");
                }
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
lastfm_request_custom_playlist          (LastfmSession *s,
                                         const char    *radio_url)
{
        g_return_val_if_fail(s != NULL && radio_url != NULL, NULL);
        char *buffer = NULL;
        size_t bufsize = 0;
        LastfmPls *pls = NULL;
        char *url = NULL;
        char *radio_url_escaped = escape_url(radio_url, TRUE);
        url = g_strconcat("http://", s->base_url, custom_pls_path,
                          "?sk=", s->id, "&url=", radio_url_escaped,
                          "&desktop=1.5", NULL);
        http_get_buffer(url, &buffer, &bufsize);
        if (buffer != NULL) {
                xmlDoc *doc = xmlParseMemory(buffer, bufsize);
                if (doc != NULL) {
                        pls = lastfm_parse_playlist (doc, NULL,
                                                     s->free_streams);
                        xmlFreeDoc (doc);
                } else {
                        g_warning ("Playlist is not an XML document");
                }
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
lastfm_set_radio                        (LastfmSession  *s,
                                         const char     *radio_url,
                                         char          **pls_title)
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
                retval = response && g_str_equal (response, "OK");
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

