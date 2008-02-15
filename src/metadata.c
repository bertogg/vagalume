/*
 * metadata.h -- Last.fm metadata (friends, tags, ...)
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include "metadata.h"
#include "http.h"
#include <libxml/parser.h>
#include <strings.h>

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
 * @return A newly-allocate string with the name of the tag (or NULL)
 */
static char *
get_xml_tag_name(xmlDoc *doc, const xmlNode *node)
{
        g_return_val_if_fail(node != NULL, NULL);
        const xmlNode *child = node->xmlChildrenNode;
        while (child != NULL) {
                const xmlChar *name = child->name;
                if (!xmlStrcmp(name, (const xmlChar *) "name")) {
                        char *tagname = NULL;
                        xmlNode *content = child->xmlChildrenNode;
                        xmlChar *tag = xmlNodeListGetString(doc, content, 1);
                        if (tag != NULL) tagname = g_strdup((char *) tag);
                        xmlFree(tag);
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
                        char *tagname = get_xml_tag_name(doc, node);
                        if (tagname != NULL) {
                                *tags = g_list_append(*tags, tagname);
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
