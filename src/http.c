/*
 * http.c -- All the HTTP-specific functions
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "http.h"
#include "globaldefs.h"
#include "util.h"
#include <curl/curl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
        char *buffer;
        size_t size;
} curl_buffer;

typedef struct {
        http_download_progress_cb cb;
        gpointer userdata;
} http_dl_progress_wrapper_data;

void
http_set_proxy(const char *proxy)
{
        if (proxy == NULL || *proxy == '\0') {
                g_unsetenv("http_proxy");
        } else {
                g_setenv("http_proxy", proxy, 1);
        }
}

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
#ifdef HAVE_CURL_EASY_ESCAPE
                curl_str = curl_easy_escape(handle, url, 0);
        } else {
                curl_str = curl_easy_unescape(handle, url, 0, NULL);
#else
                curl_str = curl_escape(url, 0);
        } else {
                curl_str = curl_unescape(url, 0);
#endif
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
        if (datasize == 0 || src == NULL) return 0;
        dstbuf->size += datasize;
        /* Allocate an extra byte to place the final \0 */
        dstbuf->buffer = g_realloc(dstbuf->buffer, dstbuf->size + 1);
        memcpy(dstbuf->buffer + writefrom, src, datasize);
        return datasize;
}

gboolean
http_get_to_fd(const char *url, int fd, const GSList *headers)
{
        g_return_val_if_fail(url != NULL && fd > 0, FALSE);
        CURLcode retcode;
        CURL *handle = curl_easy_init();
        FILE *f = fdopen(fd, "w");
        struct curl_slist *hdrs = NULL;

        if (f == NULL) {
                g_warning("Unable to fdopen() fd %d", fd);
                close(fd);
                return FALSE;
        }
        g_debug("Requesting URL %s", url);
        hdrs = curl_slist_append(hdrs, "User-Agent: " APP_FULLNAME);
        if (headers != NULL) {
                const GSList *iter = headers;
                for (; iter != NULL; iter = g_slist_next(iter)) {
                        hdrs = curl_slist_append(hdrs, iter->data);
                }
        }
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, f);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 1);
        curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, 5);
        retcode = curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        if (hdrs != NULL) curl_slist_free_all(hdrs);
        fclose(f);
        /* We only return false for _read_ errors */
        if (retcode == CURLE_OK || retcode == CURLE_WRITE_ERROR) {
                return TRUE;
        } else {
                g_warning("Error getting URL %s", url);
                return FALSE;
        }
}

static int
http_dl_progress_wrapper(void *data, double dltotal, double dlnow,
                             double ultotal, double ulnow)
{
        http_dl_progress_wrapper_data *wrapdata;
        wrapdata = (http_dl_progress_wrapper_data *) data;
        g_return_val_if_fail(wrapdata != NULL && wrapdata->cb != NULL, -1);
        if ((*(wrapdata->cb))(wrapdata->userdata, dltotal, dlnow)) {
                return 0;
        } else {
                return -1;
        }
}

gboolean
http_download_file(const char *url, const char *filename,
                   http_download_progress_cb cb, gpointer userdata)
{
        g_return_val_if_fail(url != NULL && filename != NULL, FALSE);
        http_dl_progress_wrapper_data *wrapdata = NULL;
        CURLcode retcode;
        CURL *handle = NULL;
        FILE *f = NULL;
        if (!file_exists(filename)) {
                f = fopen(filename, "w");
        } else {
                g_warning("File %s already exists", filename);
        }
        if (f == NULL) {
                g_warning("Unable to open %s for writing", filename);
                return FALSE;
        }

        handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, f);
        if (cb != NULL) {
                wrapdata = g_slice_new(http_dl_progress_wrapper_data);
                wrapdata->cb = cb;
                wrapdata->userdata = userdata;
                curl_easy_setopt(handle, CURLOPT_PROGRESSFUNCTION,
                                 http_dl_progress_wrapper);
                curl_easy_setopt(handle, CURLOPT_PROGRESSDATA, wrapdata);
                curl_easy_setopt(handle, CURLOPT_NOPROGRESS, FALSE);
        }
        retcode = curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        fclose(f);
        if (wrapdata != NULL) {
                g_slice_free(http_dl_progress_wrapper_data, wrapdata);
        }
        if (retcode == CURLE_OK) {
                return TRUE;
        } else {
                g_warning("Error downloading URL %s", url);
                return FALSE;
        }
}

gboolean
http_get_buffer(const char *url, char **buffer, size_t *bufsize)
{
        g_return_val_if_fail(url != NULL && buffer != NULL, FALSE);
        curl_buffer dstbuf = { NULL, 0 };
        CURLcode retcode;
        CURL *handle;
        const char *pwpos = strstr(url, "passwordmd5=");
        struct curl_slist *hdrs = NULL;

        if (pwpos == NULL) {
                g_debug("Requesting URL %s", url);
        } else {
                char *newurl = g_strndup(url, pwpos - url);
                g_debug("Requesting URL %spasswordmd5=<hidden>", newurl);
                g_free(newurl);
        }
        handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, http_copy_buffer);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &dstbuf);
        hdrs = curl_slist_append(hdrs, "User-Agent: " APP_FULLNAME);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, hdrs);
        retcode = curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        if (hdrs != NULL) curl_slist_free_all(hdrs);

        if (retcode != CURLE_OK) {
                g_warning("Error getting URL %s", url);
                g_free(dstbuf.buffer);
                dstbuf.buffer = NULL;
        }

        if (dstbuf.buffer != NULL) {
                dstbuf.buffer[dstbuf.size] = '\0';
        }
        *buffer = dstbuf.buffer;
        if (bufsize != NULL) {
                *bufsize = dstbuf.size;
        }
        return (retcode == CURLE_OK);
}

gboolean
http_post_buffer(const char *url, const char *postdata, char **retdata,
                 const GSList *headers)
{
        g_return_val_if_fail(url != NULL && postdata != NULL, FALSE);
        curl_buffer dstbuf = { NULL, 0 };
        CURLcode retcode;
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
                const GSList *iter = headers;
                for (; iter != NULL; iter = g_slist_next(iter)) {
                        hdrs = curl_slist_append(hdrs, iter->data);
                }
        }

        g_debug("Posting to URL %s\n%s", url, postdata);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, postdata);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, hdrs);
        retcode = curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        if (hdrs != NULL) curl_slist_free_all(hdrs);

        if (retcode != CURLE_OK) {
                g_warning("Error posting to URL %s", url);
                g_free(dstbuf.buffer);
                dstbuf.buffer = NULL;
        }

        if (retdata != NULL) {
                if (dstbuf.buffer != NULL) {
                        dstbuf.buffer[dstbuf.size] = '\0';
                }
                *retdata = dstbuf.buffer;
        }
        return (retcode == CURLE_OK);
}
