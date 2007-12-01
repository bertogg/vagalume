/*
 * protocol.c -- Last.fm streaming protocol and XSPF
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#include <libxml/parser.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "http.h"
#include "util.h"
#include "protocol.h"
#include "globaldefs.h"

static const char *handshake_url =
       "http://ws.audioscrobbler.com/radio/handshake.php"
       "?version=" LASTFM_APP_VERSION "&platform=" APP_OS_LC;
static const char *friends_url =
       "http://ws.audioscrobbler.com/1.0/user/%s/friends.txt";
static const char *user_tags_url =
       "http://ws.audioscrobbler.com/1.0/user/%s/tags.xml";
static const char *artist_tags_url =
       "http://ws.audioscrobbler.com/1.0/artist/%s/toptags.xml";
static const char *album_tags_url =
       "http://ws.audioscrobbler.com/1.0/album/%s/%s/toptags.xml";
static const char *track_tags_url =
       "http://ws.audioscrobbler.com/1.0/track/%s/%s/toptags.xml";
static const char *user_artist_tags_url =
       "http://ws.audioscrobbler.com/1.0/user/%s/artisttags.xml?artist=%s";
static const char *user_album_tags_url =
 "http://ws.audioscrobbler.com/1.0/user/%s/albumtags.xml?artist=%s&album=%s";
static const char *user_track_tags_url =
 "http://ws.audioscrobbler.com/1.0/user/%s/tracktags.xml?artist=%s&track=%s";
static const char *custom_pls_path =
       "/1.0/webclient/getresourceplaylist.php";
static const xmlChar *free_track_rel = (xmlChar *)
       "http://www.last.fm/freeTrackURL";

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
 * Destroy a lastfm_session object, freeing all of its memory
 * @param session The session
 */
void
lastfm_session_destroy(lastfm_session *session)
{
        if (session == NULL) return;
        g_free(session->id);
        g_free(session->base_url);
        g_free(session->base_path);
        g_free(session);
}

/**
 * Copy a lastfm_session object
 * @param session The original object
 * @return A new object
 */
lastfm_session *
lastfm_session_copy(const lastfm_session *session)
{
        if (session == NULL) return NULL;
        lastfm_session *s = g_new0(lastfm_session, 1);
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
 * @return A new lastfm_session or NULL if it couldn't be created
 */
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
lastfm_parse_track(xmlDoc *doc, xmlNode *node, lastfm_pls *pls,
                   const char *pls_title, gboolean custom_pls)
{
        g_return_val_if_fail(doc!=NULL && node!=NULL && pls!=NULL, FALSE);

        gboolean retval = FALSE;
        const xmlChar *name;
        char *val;
        lastfm_track *track = g_new0(lastfm_track, 1);
        track->pls_title = g_strdup(pls_title);
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
                if (track->album == NULL) track->album = g_strdup("");
                if (track->trackauth == NULL) track->trackauth = g_strdup("");
                lastfm_pls_add_track(pls, track);
                retval = TRUE;
        }
        if (!retval) lastfm_track_destroy(track);
        return retval;
}

/**
 * Parse a playlist in XSPF form and add its tracks to a lastfm_pls
 * @param buffer A buffer containing the playlist in XSPF format
 * @param bufsize Size of the buffer
 * @return A new playlist, or NULL if none was found
 */
static lastfm_pls *
lastfm_parse_playlist(const char *buffer, size_t bufsize)
{
        xmlDoc *doc = NULL;
        xmlNode *node = NULL;
        xmlNode *tracklist = NULL;
        lastfm_pls *pls = NULL;
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
                                pls_title = escape_url(title, FALSE);
                                int i;
                                for (i = 0; pls_title[i] != 0; i++) {
                                        if (pls_title[i] == '+')
                                                pls_title[i] = ' ';
                                }
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
 * @return A new playlist or NULL if none has been obtained
 */
lastfm_pls *
lastfm_request_playlist(lastfm_session *s, gboolean discovery)
{
        g_return_val_if_fail(s && s->id && s->base_url && s->base_path, NULL);
        const char *disc_mode = discovery ? "1" : "0";
        char *url;
        char *buffer = NULL;
        size_t bufsize = 0;
        lastfm_pls *pls = NULL;

        url = g_strconcat("http://", s->base_url, s->base_path,
                          "/xspf.php?sk=", s->id, "&discovery=", disc_mode,
                          "&desktop=" LASTFM_APP_VERSION, NULL);
        http_get_buffer(url, &buffer, &bufsize);
        if (buffer != NULL) {
                pls = lastfm_parse_playlist(buffer, bufsize);
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
lastfm_pls *
lastfm_request_custom_playlist(lastfm_session *s, const char *radio_url)
{
        g_return_val_if_fail(s != NULL && radio_url != NULL, NULL);
        char *buffer = NULL;
        size_t bufsize = 0;
        lastfm_pls *pls = NULL;
        char *url = NULL;
        char *radio_url_escaped = escape_url(radio_url, TRUE);
        url = g_strconcat("http://", s->base_url, custom_pls_path,
                          "?sk=", s->id, "&url=", radio_url_escaped,
                          "&desktop=" LASTFM_APP_VERSION, NULL);
        http_get_buffer(url, &buffer, &bufsize);
        if (buffer != NULL) {
                pls = lastfm_parse_playlist(buffer, bufsize);
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
 * @return Whether the radio has been set correctly
 */
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

/**
 * Obtain the list of friends from a user
 * @param username The user name
 * @param friendlist Where the list of friends (char *) will be written
 * @return Whether the operation has been successful or not
 */
gboolean
lastfm_get_friends(const char *username, GList **friendlist)
{
        g_return_val_if_fail(username != NULL && friendlist != NULL, FALSE);
        GList *list = NULL;
        char *url = g_strdup_printf(friends_url, username);
        char *buffer = NULL;
        gboolean found = http_get_buffer(url, &buffer, NULL);
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
        *friendlist = list;
        return found;
}

/**
 * Obtain the name of a <tag> attribute in an XML tag list
 * @param doc The XML document
 * @param node The xmlNode pointing to the <tag> attribute
 * @return The name of the tag
 */
static const char *
get_xml_tag_name(xmlDoc *doc, const xmlNode *node)
{
        g_return_val_if_fail(node != NULL, NULL);
        const xmlNode *child = node->xmlChildrenNode;
        while (child != NULL) {
                const xmlChar *name = child->name;
                if (!xmlStrcmp(name, (const xmlChar *) "name")) {
                        xmlNode *content = child->xmlChildrenNode;
                        const char *tagname = (const char *)
                                xmlNodeListGetString(doc, content, 1);
                        return tagname;
                }
                child = child->next;
        }
        g_warning("Found <tag> element with no name");
        return NULL;
}

/**
 * Parse an XML tag list
 * @param buffer A buffer where the XML document is stored
 * @param bufsize The size of the buffer
 * @param tags Where the list of tags will be written
 * @return Whether a list (even an empty list) has been found or not
 */
static gboolean
parse_xml_tags(const char *buffer, size_t bufsize, GList **tags)
{
        g_return_val_if_fail(buffer && bufsize > 0 && tags, FALSE);
        xmlDoc *doc = NULL;
        const xmlNode *node = NULL;
        gboolean retval = FALSE;
        *tags = NULL;
        doc = xmlParseMemory(buffer, bufsize);
        if (doc != NULL) {
                node = xmlDocGetRootElement(doc);
                if (!xmlStrcmp(node->name, (const xmlChar *) "toptags") ||
                    !xmlStrcmp(node->name, (const xmlChar *) "albumtags") ||
                    !xmlStrcmp(node->name, (const xmlChar *) "artisttags") ||
                    !xmlStrcmp(node->name, (const xmlChar *) "tracktags")) {
                        node = node->xmlChildrenNode;
                        retval = TRUE;
                } else {
                        g_warning("Tag list not in the expected format");
                        node = NULL;
                }
        } else {
                g_warning("Tag list is not an XML document");
        }
        while (node != NULL) {
                const xmlChar *name = node->name;
                if (!xmlStrcmp(name, (const xmlChar *) "tag")) {
                        const char *tagname = get_xml_tag_name(doc, node);
                        if (tagname != NULL) {
                                *tags = g_list_append(*tags,
                                                      g_strdup(tagname));
                        }
                }
                node = node->next;
        }
        if (doc != NULL) xmlFreeDoc(doc);
        return retval;
}

/**
 * Obtain the list of tags from a given URL
 * @param username The user name
 * @param taglist Where the list of tags (char *) will be written
 * @return Whether the operation has been successful or not
 */
static gboolean
lastfm_get_tags(const char *url, GList **taglist)
{
        g_return_val_if_fail(url != NULL && taglist != NULL, FALSE);
        char *buffer = NULL;
        size_t bufsize;
        gboolean found = FALSE;
        http_get_buffer(url, &buffer, &bufsize);
        if (buffer != NULL) {
                found = parse_xml_tags(buffer, bufsize, taglist);
        } else {
                *taglist = NULL;
        }
        g_free(buffer);
        return found;
}

/**
 * Obtain the list of tags from a user
 * @param username The user name
 * @param taglist Where the list of tags (char *) will be written
 * @return Whether the operation has been successful or not
 */
gboolean
lastfm_get_user_tags(const char *username, GList **taglist)
{
        g_return_val_if_fail(username != NULL && taglist != NULL, FALSE);
        char *url = g_strdup_printf(user_tags_url, username);
        gboolean found = lastfm_get_tags(url, taglist);
        g_free(url);
        return found;
}

/**
 * Obtain the list of tags that the user set to a track
 * @param username The user ID
 * @param track The track
 * @param req Which tags to obtain (artist, album, track)
 * @param taglist Where the list of tags (char *) will be written
 * @return Whether the operation has been successful or not
 */
gboolean
lastfm_get_user_track_tags(const char *username, const lastfm_track *track,
                           request_type req, GList **taglist)
{
        g_return_val_if_fail(username && track && taglist, FALSE);
        char *artist = escape_url(track->artist, TRUE);
        char *album = NULL;
        char *title = NULL;
        char *url = NULL;
        switch (req) {
        case REQUEST_ARTIST:
                url = g_strdup_printf(user_artist_tags_url, username, artist);
                break;
        case REQUEST_ALBUM:
                g_return_val_if_fail(track->album[0] != '\0', FALSE);
                album = escape_url(track->album, TRUE);
                url = g_strdup_printf(user_album_tags_url, username,
                                      artist, album);
                break;
        case REQUEST_TRACK:
                title = escape_url(track->title, TRUE);
                url = g_strdup_printf(user_track_tags_url, username,
                                      artist, title);
                break;
        default:
                g_return_val_if_reached(FALSE);
                break;
        }
        gboolean found = lastfm_get_tags(url, taglist);
        g_free(artist);
        g_free(album);
        g_free(title);
        g_free(url);
        return found;
}

/**
 * Obtain the list of tags from a track
 * @param track The track
 * @param req Which tags to obtain (artist, album, track)
 * @param taglist Where the list of tags (char *) will be written
 * @return Whether the operation has been successful or not
 */
gboolean
lastfm_get_track_tags(const lastfm_track *track, request_type req,
                      GList **taglist)
{
        g_return_val_if_fail(track != NULL && taglist != NULL, FALSE);
        char *artist = escape_url(track->artist, TRUE);
        char *album = NULL;
        char *title = NULL;
        char *url = NULL;
        switch (req) {
        case REQUEST_ARTIST:
                url = g_strdup_printf(artist_tags_url, artist);
                break;
        case REQUEST_ALBUM:
                g_return_val_if_fail(track->album[0] != '\0', FALSE);
                album = escape_url(track->album, TRUE);
                url = g_strdup_printf(album_tags_url, artist, album);
                break;
        case REQUEST_TRACK:
                title = escape_url(track->title, TRUE);
                url = g_strdup_printf(track_tags_url, artist, title);
                break;
        default:
                g_return_val_if_reached(FALSE);
                break;
        }
        gboolean found = lastfm_get_tags(url, taglist);
        g_free(artist);
        g_free(album);
        g_free(title);
        g_free(url);
        return found;
}
