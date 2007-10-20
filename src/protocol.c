
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <gcrypt.h>

#include "protocol.h"

#define CLIENT_VERSION "0.1"
#define CLIENT_PLATFORM "linux"

const char *username = "username";
const char *password = "password";

static const char *handshake_url =
       "http://ws.audioscrobbler.com/radio/handshake.php"
       "?version=" CLIENT_VERSION
       "&platform=" CLIENT_PLATFORM;

static char *
response_getline(const char **buffer, size_t *bufsize)
{
        g_return_val_if_fail(buffer && bufsize && *bufsize, NULL);
        /* Read whole buffer unless a newline is found */
        size_t linesize = *bufsize;
        size_t readbytes = *bufsize;
        char *output;
        const char *endline = memchr(*buffer, '\n', *bufsize);
        if (endline != NULL) {
                linesize = endline - *buffer;
                readbytes = linesize + 1;
        }

        output = g_malloc(linesize + 1);
        memcpy(output, *buffer, linesize);
        output[linesize] = '\0';
        *buffer += readbytes;
        *bufsize -= readbytes;
        return output;
}

static size_t
read_http(void *buffer, size_t size, size_t nmemb, void *response)
{
        GHashTable *hash = (GHashTable *) response;
        const char *pos, *line;
        size_t bufsize = size * nmemb;
        pos = buffer;
        while (bufsize > 0) {
                line = response_getline(&pos, &bufsize);
                char *c = strchr(line, '=');
                if (c != NULL) {
                        g_hash_table_insert(hash,
                                            g_strndup(line, c-line),
                                            g_strdup(c+1));
                }
                g_free((gpointer)line);
        }
        return size * nmemb;
}

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

static GHashTable *
http_send_request(const char *url)
{
        CURL *handle;
        GHashTable *response;
        curl_global_init(CURL_GLOBAL_ALL);

        handle = curl_easy_init();
        response = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         g_free, g_free);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, read_http);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, response);
        curl_easy_perform(handle);
        curl_easy_cleanup(handle);

        return response;

}

GHashTable *
lastfm_handshake(void)
{
        char *md5password = get_md5_hash(password);
        char *url = g_strconcat(handshake_url, "&username=", username,
                          "&passwordmd5=", md5password, NULL);
        GHashTable *response = http_send_request(url);
        g_free(md5password);
        g_free(url);

        return response;
}
