
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <gcrypt.h>
#include <libxml/parser.h>

#include "protocol.h"
#include "playlist.h"

#define CLIENT_VERSION "0.1"
#define CLIENT_PLATFORM "linux"

const char *username = "username";
const char *password = "password";

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
http_get_file(const char *url)
{
        g_return_val_if_fail(url != NULL, NULL);
        CURL *handle;
        char *filename = g_strdup("/tmp/lastfm.XXXXXX");
        int fd = mkstemp(filename);
        if (fd == -1) {
                g_free(filename);
                g_critical("Unable to create temporary file");
                return NULL;
        }
        FILE *file = fdopen(fd, "w");

        g_debug("Requesting URL %s", url);
        init_curl();
        handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, file);
        curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        fclose(file);

        return filename;
}

GHashTable *
lastfm_parse_handshake(const char *filename)
{
        const size_t bufsize = 256;
        char buf[bufsize];
        FILE *file;
        GHashTable *hash;
        hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                     g_free, g_free);
        file = fopen(filename, "r");
        while (fgets(buf, bufsize, file)) {
                char *c;
                size_t last = strlen(buf) - 1;
                if (last < 2) continue; /* Good lines should have len>2 */
                while (buf[last] == '\n' || buf[last] == ' ') {
                        buf[last--] = '\0'; /* Remove trailing whitespace */
                }
                c = strchr(buf, '=');
                if (c != NULL && c[1] != '\0') {
                        char *key = g_strndup(buf, c-buf);
                        char *val = g_strdup(c+1);
                        g_hash_table_insert(hash, key, val);
                }
        }
        fclose(file);
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
lastfm_handshake(void)
{
        char *md5password = get_md5_hash(password);
        char *url = g_strconcat(handshake_url, "&username=", username,
                                "&passwordmd5=", md5password, NULL);

        char *filename = http_get_file(url);
        g_free(md5password);
        g_free(url);
        g_return_val_if_fail(filename != NULL, NULL);

        GHashTable *response = lastfm_parse_handshake(filename);

        unlink(filename);
        g_free(filename);

        lastfm_session *s = g_new0(lastfm_session, 1);
        s->id = g_strdup(g_hash_table_lookup(response, "session"));
        s->stream_url = g_strdup(g_hash_table_lookup(response, "stream_url"));
        s->base_url = g_strdup(g_hash_table_lookup(response, "base_url"));
        s->base_path = g_strdup(g_hash_table_lookup(response, "base_path"));
        const char *subs = g_hash_table_lookup(response, "subscriber");
        s->subscriber = (subs != NULL && *subs == '1');

        g_hash_table_destroy(response);

        return s;
}

char *
lastfm_request_xsfp(lastfm_session *s)
{
        char *url, *filename;
        g_return_val_if_fail(s != NULL, NULL);
        g_return_val_if_fail(s->id != NULL &&
                             s->base_url != NULL &&
                             s->base_path != NULL, NULL);

        url = g_strconcat("http://", s->base_url, s->base_path,
                          "/xspf.php?sk=", s->id,
                          "&discovery=0&desktop=" CLIENT_VERSION, NULL);
        filename = http_get_file(url);
        g_free(url);

        return filename;
}

static gboolean
lastfm_parse_track(xmlDoc *doc, xmlNode *node)
{
        g_return_val_if_fail(node != NULL, FALSE);

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
                lastfm_pls_add_track(track);
                return TRUE;
        }
}

static gboolean
lastfm_parse_playlist(xmlDoc *doc)
{
        xmlNode *node, *tracklist;
        gint pls_size;
        g_return_val_if_fail(doc != NULL, FALSE);

        node = xmlDocGetRootElement(doc);
        if (xmlStrcmp(node->name, (const xmlChar *) "playlist")) {
                g_warning("Playlist file not in the expected format");
                return FALSE;
        }
        tracklist = NULL;
        node = node->xmlChildrenNode;
        while (node != NULL) {
                if (!xmlStrcmp(node->name, (const xmlChar *) "trackList")) {
                        tracklist = node;
                }
                node = node->next;
        }
        if (tracklist == NULL) {
                g_warning("No tracks found in playlist");
                return FALSE;
        }
        node = tracklist->xmlChildrenNode;
        pls_size = lastfm_pls_size();
        while (node != NULL) {
                if (!xmlStrcmp(node->name, (const xmlChar *) "track")) {
                        lastfm_parse_track(doc, node->xmlChildrenNode);
                }
                node = node->next;
        }
        /* Return TRUE if the playlist has grown */
        return (pls_size < lastfm_pls_size());
}

gboolean
lastfm_request_playlist(lastfm_session *s)
{
        g_return_val_if_fail(s != NULL, FALSE);

        char *xmlfilename;
        xmlDoc *doc;
        gboolean retval = FALSE;

        xmlfilename = lastfm_request_xsfp(s);
        doc = xmlParseFile(xmlfilename);
        if (doc != NULL) {
                retval = lastfm_parse_playlist(doc);
                xmlFreeDoc(doc);
        }

        xmlCleanupParser();
        unlink(xmlfilename);
        g_free(xmlfilename);
        return retval;
}
