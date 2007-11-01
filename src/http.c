/*
 * http.c -- All the HTTP-specific functions
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#include "http.h"
#include "globaldefs.h"
#include <curl/curl.h>
#include <string.h>

typedef struct {
        char *buffer;
        size_t size;
} curl_buffer;

void
http_init(void)
{
        curl_global_init(CURL_GLOBAL_ALL);
}

char *
escape_url(const char *url, gboolean escape)
{
        g_return_val_if_fail(url != NULL, NULL);
        CURL *handle;
        char *str, *curl_str;
        handle = curl_easy_init();
        if (escape) {
                curl_str = curl_easy_escape(handle, url, 0);
        } else {
                curl_str = curl_easy_unescape(handle, url, 0, NULL);
        }
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
        size_t writefrom = dstbuf->size;
        if (datasize == 0) return 0;
        dstbuf->size += datasize;
        /* Allocate an extra byte to place the final \0 */
        dstbuf->buffer = g_realloc(dstbuf->buffer, dstbuf->size + 1);
        memcpy(dstbuf->buffer + writefrom, src, datasize);
        return datasize;
}

void
http_get_buffer(const char *url, char **buffer, size_t *bufsize)
{
        g_return_if_fail(url != NULL && buffer != NULL);
        curl_buffer dstbuf = { NULL, 0 };
        CURL *handle;
        struct curl_slist *hdrs = NULL;

        g_debug("Requesting URL %s", url);
        handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, http_copy_buffer);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &dstbuf);
        hdrs = curl_slist_append(hdrs, "User-Agent: " APP_FULLNAME);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        if (hdrs != NULL) curl_slist_free_all(hdrs);

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

void
http_post_buffer(const char *url, const char *postdata, char **retdata,
                 GSList *headers)
{
        g_return_if_fail(url != NULL && postdata != NULL);
        curl_buffer dstbuf = { NULL, 0 };
        CURL *handle;
        struct curl_slist *hdrs = NULL;
        handle = curl_easy_init();

        if (retdata != NULL) {
                curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION,
                                 http_copy_buffer);
                curl_easy_setopt(handle, CURLOPT_WRITEDATA, &dstbuf);
        }

        hdrs = curl_slist_append(hdrs, "User-Agent: " APP_FULLNAME);
        if (headers != NULL) {
                GSList *iter = headers;
                for (; iter != NULL; iter = g_slist_next(iter)) {
                        hdrs = curl_slist_append(hdrs, iter->data);
                }
        }

        g_debug("Posting to URL %s\n%s", url, postdata);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, postdata);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        if (hdrs != NULL) curl_slist_free_all(hdrs);
        if (retdata != NULL) {
                if (dstbuf.buffer != NULL) {
                        dstbuf.buffer[dstbuf.size] = '\0';
                }
                *retdata = dstbuf.buffer;
        }
}
