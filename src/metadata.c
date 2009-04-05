/*
 * metadata.h -- Last.fm metadata (friends, tags, ...)
 *
 * Copyright (C) 2007-2008 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include "metadata.h"
#include "http.h"
#include "util.h"
#include <libxml/parser.h>
#include <strings.h>

#if 0
static const char friends_url[] =
       "http://ws.audioscrobbler.com/1.0/user/%s/friends.txt";
static const char user_tags_url[] =
       "http://ws.audioscrobbler.com/1.0/user/%s/tags.xml";
#endif
static const char artist_tags_url[] =
       "http://ws.audioscrobbler.com/1.0/artist/%s/toptags.xml";
static const char album_tags_url[] =
       "http://ws.audioscrobbler.com/1.0/album/%s/%s/toptags.xml";
static const char track_tags_url[] =
       "http://ws.audioscrobbler.com/1.0/track/%s/%s/toptags.xml";
static const char user_artist_tags_url[] =
       "http://ws.audioscrobbler.com/1.0/user/%s/artisttags.xml?artist=%s";
static const char user_album_tags_url[] =
 "http://ws.audioscrobbler.com/1.0/user/%s/albumtags.xml?artist=%s&album=%s";
static const char user_track_tags_url[] =
 "http://ws.audioscrobbler.com/1.0/user/%s/tracktags.xml?artist=%s&track=%s";

/**
 * Obtain the list of friends from a user
 * @param username The user name
 * @param friendlist Where the list of friends (char *) will be written
 * @return Whether the operation has been successful or not
 */
#if 0 /* Superseded by lastfm_ws_*() */
gboolean
lastfm_get_friends                      (const char  *username,
                                         GList      **friendlist)
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
#endif
/**
 * Parse an XML tag list
 * @param buffer A buffer where the XML document is stored
 * @param bufsize The size of the buffer
 * @param tags Where the list of tags will be written
 * @return Whether a list (even an empty list) has been found or not
 */
static gboolean
parse_xml_tags                          (const char  *buffer,
                                         size_t       bufsize,
                                         GList      **tags)
{
        g_return_val_if_fail(buffer && bufsize > 0 && tags, FALSE);
        xmlDoc *doc = NULL;
        const xmlNode *node = NULL;
        gboolean retval = FALSE;
        *tags = NULL;
        doc = xmlParseMemory(buffer, bufsize);
        if (doc != NULL) {
                node = xmlDocGetRootElement(doc);
                if (xmlStrEqual (node->name, (xmlChar *) "toptags") ||
                    xmlStrEqual (node->name, (xmlChar *) "albumtags") ||
                    xmlStrEqual (node->name, (xmlChar *) "artisttags") ||
                    xmlStrEqual (node->name, (xmlChar *) "tracktags")) {
                        node = node->xmlChildrenNode;
                        retval = TRUE;
                } else {
                        g_warning("Tag list not in the expected format");
                        node = NULL;
                }
        } else {
                g_warning("Tag list is not an XML document");
        }
        node = xml_find_node (node, "tag");
        while (node != NULL) {
                char *tagname;
                xml_get_string (doc, node->xmlChildrenNode, "name", &tagname);
                if (tagname != NULL && tagname[0] != '\0') {
                        *tags = g_list_append (*tags, tagname);
                } else {
                        g_warning ("Found <tag> element with no name");
                        g_free (tagname);
                }
                node = xml_find_node (node->next, "tag");
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
lastfm_get_tags                         (const char  *url,
                                         GList      **taglist)
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
#if 0 /* Superseded by lastfm_ws_*() */
gboolean
lastfm_get_user_tags                    (const char  *username,
                                         GList      **taglist)
{
        g_return_val_if_fail(username != NULL && taglist != NULL, FALSE);
        char *url = g_strdup_printf(user_tags_url, username);
        gboolean found = lastfm_get_tags(url, taglist);
        g_free(url);
        return found;
}
#endif
/**
 * Obtain the list of tags that the user set to a track
 * @param username The user ID
 * @param track The track
 * @param type Which tags to obtain (artist, album, track)
 * @param taglist Where the list of tags (char *) will be written
 * @return Whether the operation has been successful or not
 */
gboolean
lastfm_get_user_track_tags              (const char            *username,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent   type,
                                         GList                **taglist)
{
        g_return_val_if_fail(username && track && taglist, FALSE);
        char *artist = NULL;
        char *album = NULL;
        char *title = NULL;
        char *url = NULL;
        switch (type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                artist = escape_url(track->artist, TRUE);
                url = g_strdup_printf(user_artist_tags_url, username, artist);
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_val_if_fail(track->album[0] != '\0', FALSE);
                artist = escape_url(track->album_artist, TRUE);
                album = escape_url(track->album, TRUE);
                url = g_strdup_printf(user_album_tags_url, username,
                                      artist, album);
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                artist = escape_url(track->artist, TRUE);
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
 * @param type Which tags to obtain (artist, album, track)
 * @param taglist Where the list of tags (char *) will be written
 * @return Whether the operation has been successful or not
 */
gboolean
lastfm_get_track_tags                   (const LastfmTrack     *track,
                                         LastfmTrackComponent   type,
                                         GList                **taglist)
{
        g_return_val_if_fail(track != NULL && taglist != NULL, FALSE);
        char *artist = NULL;
        char *album = NULL;
        char *title = NULL;
        char *url = NULL;
        switch (type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                artist = lastfm_url_encode (track->artist);
                url = g_strdup_printf(artist_tags_url, artist);
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_val_if_fail(track->album[0] != '\0', FALSE);
                artist = lastfm_url_encode (track->album_artist);
                album = lastfm_url_encode (track->album);
                url = g_strdup_printf(album_tags_url, artist, album);
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                artist = lastfm_url_encode (track->artist);
                title = lastfm_url_encode (track->title);
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

/**
 * Download the album cover of a track, and store it in the track
 * object. This function can take a long time so better use threads or
 * make sure it won't block anything important. The cover of the same
 * track won't be downloaded twice, not even if you call this function
 * while other thread is downloading it.
 * @param track The track
 */
void
lastfm_get_track_cover_image            (LastfmTrack *track)
{
        g_return_if_fail(track != NULL);
        static GStaticMutex static_mutex = G_STATIC_MUTEX_INIT;
        static GList *dloads_in_progress = NULL;
        static GCond *cond = NULL;
        GMutex *mutex = g_static_mutex_get_mutex (&static_mutex);

        /* If this track has no cover image then we have nothing to do */
        if (track->image_url == NULL) return;

        /* Critical section: decide if the cover needs to be downloaded */
        g_mutex_lock(mutex);

        if (G_UNLIKELY (cond == NULL)) {
                cond = g_cond_new();
        }

        if (!track->image_data_available) {
                if (g_list_find(dloads_in_progress, track) != NULL) {
                        /* If the track is being downloaded, then wait */
                        while (!track->image_data_available) {
                                g_cond_wait(cond, mutex);
                        }
                } else {
                        /* Otherwise, mark the track as being downloaded */
                        dloads_in_progress =
                                g_list_prepend(dloads_in_progress, track);
                }
        }

        g_mutex_unlock(mutex);

        if (!track->image_data_available) {
                char *imgdata;
                size_t imgsize;

                /* Download the cover and save it on the track */
                http_get_buffer(track->image_url, &imgdata, &imgsize);
                lastfm_track_set_cover_image(track, imgdata, imgsize);

                /* Remove from the download list and tell everyone */
                g_mutex_lock(mutex);
                dloads_in_progress = g_list_remove(dloads_in_progress, track);
                g_cond_broadcast(cond);
                g_mutex_unlock(mutex);
        }
}
