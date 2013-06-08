/*
 * http.c -- All the HTTP-specific functions
 *
 * Copyright (C) 2007-2008 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "http.h"
#include "globaldefs.h"
#include "util.h"
#include "compat.h"
#include <curl/curl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_LIBPROXY
#    include <proxy.h>

static pxProxyFactory *proxy_factory = NULL;
static gboolean use_global_proxy = FALSE;
#endif

static GStaticRWLock proxy_lock = G_STATIC_RW_LOCK_INIT;
static char *proxy_url = NULL;

/* HTTP connections will abort after this time */
static const int http_timeout = 20;

typedef struct {
        char *buffer;
        size_t size;
} curl_buffer;

typedef struct {
        http_download_progress_cb cb;
        gpointer userdata;
} http_dl_progress_wrapper_data;

static void
update_proxy_url                        (const char *proxy)
{
        if (g_strcmp0 (proxy, proxy_url) != 0) {
                g_free (proxy_url);
                proxy_url = g_strdup (proxy);
        }
}

void
http_set_proxy                          (const char *proxy,
                                         gboolean    use_system_proxy)
{
        g_static_rw_lock_writer_lock (&proxy_lock);
#ifdef HAVE_LIBPROXY
        use_global_proxy = use_system_proxy;
        if (!use_global_proxy) update_proxy_url (proxy);
#else
        update_proxy_url (proxy);
#endif
        g_static_rw_lock_writer_unlock (&proxy_lock);
}

void
http_init                               (void)
{
        curl_global_init(CURL_GLOBAL_ALL);
#ifdef HAVE_LIBPROXY
        proxy_factory = px_proxy_factory_new ();
#endif
}

char *
escape_url                              (const char *url,
                                         gboolean    escape)
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
http_copy_buffer                        (void   *src,
                                         size_t  size,
                                         size_t  nmemb,
                                         void   *dest)
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

static CURL *
create_curl_handle                      (const char *url)
{
        CURL *handle = curl_easy_init ();
        curl_easy_setopt (handle, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt (handle, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt (handle, CURLOPT_LOW_SPEED_LIMIT, 1);
        curl_easy_setopt (handle, CURLOPT_LOW_SPEED_TIME, http_timeout);
        curl_easy_setopt (handle, CURLOPT_CONNECTTIMEOUT, http_timeout);

#ifdef HAVE_LIBPROXY
        if (use_global_proxy) {
                char **proxies;
                const char *found = NULL;
                proxies = px_proxy_factory_get_proxies (proxy_factory,
                                                        (char *) url);
                if (proxies != NULL) {
                        char **p;
                        for (p = proxies; found == NULL && *p != NULL; p++) {
                                if (g_str_has_prefix (*p, "direct://") ||
                                    g_str_has_prefix (*p, "http://") ||
                                    g_str_has_prefix (*p, "socks://") ||
                                    g_str_has_prefix (*p, "socks4://") ||
                                    g_str_has_prefix (*p, "socks5://")) {
                                        found = *p;
                                }
                        }
                }

                g_static_rw_lock_writer_lock (&proxy_lock);
                if (use_global_proxy) update_proxy_url (found);
                g_static_rw_lock_writer_unlock (&proxy_lock);

                g_strfreev (proxies);
        }
#endif

        g_static_rw_lock_reader_lock (&proxy_lock);
        if (proxy_url != NULL) {
                curl_easy_setopt (handle, CURLOPT_PROXY, proxy_url);
                if (!g_str_equal (proxy_url, "direct://")) {
                        curl_proxytype type = CURLPROXY_HTTP;
                        if (g_str_has_prefix (proxy_url, "socks://") ||
                            g_str_has_prefix (proxy_url, "socks5://")) {
                                /* TODO: make socks:// fall back to Socks 4 */
                                type = CURLPROXY_SOCKS5;
                        } else if (g_str_has_prefix (proxy_url, "socks4://")) {
                                type = CURLPROXY_SOCKS4;
                        }
                        curl_easy_setopt (handle, CURLOPT_PROXYTYPE, type);
                }
        }
        g_static_rw_lock_reader_unlock (&proxy_lock);

        return handle;
}

gboolean
http_get_to_fd                          (const char   *url,
                                         int           fd,
                                         const GSList *headers)
{
        g_return_val_if_fail(url != NULL && fd > 0, FALSE);
        CURLcode retcode;
        CURL *handle;
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
        handle = create_curl_handle (url);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, f);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, hdrs);
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
http_dl_progress_wrapper                (void   *data,
                                         double  dltotal,
                                         double  dlnow,
                                         double  ultotal,
                                         double  ulnow)
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
http_download_file                      (const char                *url,
                                         const char                *filename,
                                         http_download_progress_cb  cb,
                                         gpointer                   userdata)
{
        g_return_val_if_fail(url != NULL && filename != NULL, FALSE);
        http_dl_progress_wrapper_data *wrapdata = NULL;
        CURLcode retcode;
        CURL *handle;
        FILE *f = NULL;
        if (!file_exists(filename)) {
                f = fopen(filename, "wb");
        } else {
                g_warning("File %s already exists", filename);
        }
        if (f == NULL) {
                g_warning("Unable to open %s for writing", filename);
                return FALSE;
        }

        handle = create_curl_handle (url);
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
http_get_buffer                         (const char  *url,
                                         char       **buffer,
                                         size_t      *bufsize)
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
        handle = create_curl_handle (url);
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
http_post_buffer                        (const char    *url,
                                         const char    *postdata,
                                         char         **retbuf,
                                         size_t        *retbufsize,
                                         const GSList  *headers)
{
        g_return_val_if_fail(url != NULL && postdata != NULL, FALSE);
        curl_buffer dstbuf = { NULL, 0 };
        CURLcode retcode;
        CURL *handle;
        struct curl_slist *hdrs = NULL;
        handle = create_curl_handle (url);

        if (retbuf != NULL) {
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

        if (retbuf != NULL) {
                if (dstbuf.buffer != NULL) {
                        dstbuf.buffer[dstbuf.size] = '\0';
                }
                *retbuf = dstbuf.buffer;
                if (retbufsize != NULL) {
                        *retbufsize = dstbuf.size;
                }
        }
        return (retcode == CURLE_OK);
}
