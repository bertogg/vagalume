
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <gcrypt.h>
#include <libxml/parser.h>

#include "protocol.h"

#define CLIENT_VERSION "0.1"
#define CLIENT_PLATFORM "linux"

typedef struct {
        char *buffer;
        size_t size;
} curl_buffer;

static const char *handshake_url =
       "http://ws.audioscrobbler.com/radio/handshake.php"
       "?version=" CLIENT_VERSION
       "&platform=" CLIENT_PLATFORM;

static char *
get_md5_hash(const char *str)
{
        g_return_val_if_fail(str != NULL, NULL);
        int i;
        int digestlen = gcry_md_get_algo_dlen (GCRY_MD_MD5);
        unsigned char *digest = g_malloc(digestlen);
        char *hexdigest = g_malloc(digestlen*2 + 1);
        gcry_md_hash_buffer(GCRY_MD_MD5, digest, str, strlen(str));
        for (i = 0; i < digestlen; i++) {
                sprintf(hexdigest + 2*i, "%02x", digest[i]);
        }
        g_free(digest);
        return hexdigest;
}

static void
init_curl(void)
{
        static gboolean initialized = FALSE;
        if (!initialized) {
                curl_global_init(CURL_GLOBAL_ALL);
                initialized = TRUE;
        }
}

static char *
escape_url(const char *url)
{
        g_return_val_if_fail(url != NULL, NULL);
        CURL *handle;
        char *str, *curl_str;
        init_curl();
        handle = curl_easy_init();
        curl_str = curl_easy_escape(handle, url, 0);
        str = g_strdup(curl_str);
        curl_free(curl_str);
        curl_easy_cleanup(handle);
        return str;
}

static size_t
http_copy_buffer(void *src, size_t size, size_t nmemb, void *dest)
{
        curl_buffer *dstbuf = (curl_buffer *) dest;
        size_t datasize = size*nmemb;
        size_t writefrom = dstbuf->size;;
        if (datasize == 0) return 0;
        dstbuf->size += datasize;
        /* Allocate an extra byte to place the final \0 */
        dstbuf->buffer = g_realloc(dstbuf->buffer, dstbuf->size + 1);
        memcpy(dstbuf->buffer + writefrom, src, datasize);
        return datasize;
}

static void
http_get_buffer(const char *url, char **buffer, size_t *bufsize)
{
        g_return_if_fail(url != NULL && buffer != NULL);
        curl_buffer dstbuf = { NULL, 0 };
        CURL *handle;

        g_debug("Requesting URL %s", url);
        init_curl();
        handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, http_copy_buffer);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &dstbuf);
        curl_easy_perform(handle);
        curl_easy_cleanup(handle);

        if (dstbuf.buffer == NULL) {
                g_warning("Error getting URL %s", url);
                return;
        }

        dstbuf.buffer[dstbuf.size] = '\0';
        *buffer = dstbuf.buffer;
        if (bufsize != NULL) {
                *bufsize = dstbuf.size;
        }
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
                        g_hash_table_insert(hash, key, val);
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
lastfm_session_new(const char *username, const char *password)
{
        g_return_val_if_fail(username != NULL && password != NULL, NULL);
        char *buffer = NULL;
        char *md5password = get_md5_hash(password);
        char *url = g_strconcat(handshake_url, "&username=", username,
                                "&passwordmd5=", md5password, NULL);

        http_get_buffer(url, &buffer, NULL);
        g_free(md5password);
        g_free(url);
        g_return_val_if_fail(buffer != NULL, NULL);

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
                if (!xmlStrcmp(name, (xmlChar *) "location")) {
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
                        lastfm_pls_set_title(pls, title);
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
        char *radio_url_escaped = escape_url(radio_url);

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
