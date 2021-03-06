/*
 * util.c -- Misc util functions
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "util.h"
#include "http.h"

#ifndef HAVE_GCHECKSUM
#   include "md5/md5.h"
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>

/**
 * Computes the MD5 hash of a NULL-terminated string
 * @param str The string
 * @return The MD5 hash, in hexadecimal
 */
char *
get_md5_hash                            (const char *str)
{
        g_return_val_if_fail(str != NULL, NULL);
#ifdef HAVE_GCHECKSUM
        return g_compute_checksum_for_string(G_CHECKSUM_MD5, str, -1);
#else
        const int digestlen = 16;
        unsigned char digest[digestlen];
        guint i;

        md5_state_t state;
        md5_init(&state);
        md5_append(&state, (const md5_byte_t *)str, strlen(str));
        md5_finish(&state, digest);
        char *hexdigest = g_malloc(digestlen*2 + 1);
        for (i = 0; i < digestlen; i++) {
                sprintf(hexdigest + 2*i, "%02x", digest[i]);
        }
        return hexdigest;
#endif /* HAVE_GCHECKSUM */
}

/**
 * Computes the authentication token used in some last.fm services
 * Consists on  MD5 ( MD5 ( password) + timestamp )
 * @param password The password, cleartext
 * @param timestamp The timestamp
 * @return The auth token
 */
char *
compute_auth_token                      (const char *password,
                                         const char *timestamp)
{
        g_return_val_if_fail(password && timestamp, NULL);
        char *md5password = get_md5_hash(password);
        char *tmp = g_strconcat(md5password, timestamp, NULL);
        char *retval = get_md5_hash(tmp);
        g_free(md5password);
        g_free(tmp);
        return retval;
}

/**
 * Joins all the strings (char *) in a GList using an optional
 * separator between them
 * @param list The GList
 * @param separator The separator
 * @return A newly allocated string
 */
char *
str_glist_join                          (const GList *list,
                                         const char  *separator)
{
        if (list == NULL) return g_strdup("");
        const GList *iter = list;
        GString *str = g_string_sized_new(100);
        for (; iter != NULL; iter = iter->next) {
                str = g_string_append(str, (char *) iter->data);
                if (iter->next != NULL && separator != NULL) {
                        str = g_string_append(str, separator);
                }
        }
        return g_string_free(str, FALSE);
}

/**
 * Checks whether a file exists
 * @param filename Full path to the file
 * @return TRUE if it exists, FALSE otherwise
 */
gboolean
file_exists                             (const char *filename)
{
        g_return_val_if_fail(filename, FALSE);
        return !access(filename, F_OK);
}

/**
 * Replaces all occurrences of a text within a string (GString)
 * @param str The string to be modified
 * @param old The original text
 * @param new The new text
 */
void
string_replace_gstr                     (GString    *str,
                                         const char *old,
                                         const char *new)
{
        g_return_if_fail(str && old && new);
        int oldlen = strlen(old);
        int newlen = strlen(new);
        const char *cur = str->str;
        while ((cur = strstr(cur, old)) != NULL) {
                int position = cur - str->str;
                g_string_erase(str, position, oldlen);
                g_string_insert(str, position, new);
                cur = str->str + position + newlen;
        }
}

/**
 * Replaces all occurrences of a text within a string (char *)
 * @param str The original string
 * @param old The original text
 * @param new The new text
 * @return A newly-created string
 */
char *
string_replace                          (const char *str,
                                         const char *old,
                                         const char *new)
{
        g_return_val_if_fail(str && old && new, NULL);
        GString *gstr = g_string_new(str);
        string_replace_gstr(gstr, old, new);
        return g_string_free(gstr, FALSE);
}

/**
 * Replaces all occurences of a char within a string (char *)
 * @param str The string to be modified
 * @param old The original char
 * @param new The new char
 **/
static void
string_replace_char                     (char *str,
                                         char  old,
                                         char  new)
{
        char *i;
        g_return_if_fail (str != NULL);
        for (i = str; *i != '\0'; i++) {
                if (*i == old) {
                        *i = new;
                }
        }
}

/**
 * Creates a GdkPixbuf from a image in any supported format
 * @param data The original image in memory, or NULL
 * @param size The size of the buffer containing the original image
 * @param imgsize The resulting pixbuf will be imgsize x imgsize pixels
 * @return The GdkPixbuf, or NULL
 */
GdkPixbuf *
get_pixbuf_from_image                   (const char *data,
                                         size_t      size,
                                         int         imgsize)
{
        g_return_val_if_fail(imgsize > 0, NULL);
        GdkPixbufLoader *ldr = NULL;
        GdkPixbuf *pixbuf = NULL;
        if (data != NULL) {
                g_return_val_if_fail(size > 0, NULL);
                GError *err = NULL;
                ldr = gdk_pixbuf_loader_new();
                gdk_pixbuf_loader_set_size(ldr, imgsize, imgsize);
                gdk_pixbuf_loader_write(ldr, (guchar *) data, size, NULL);
                gdk_pixbuf_loader_close(ldr, &err);
                if (err != NULL) {
                        g_warning("Error loading image: %s",
                                  err->message ? err->message : "unknown");
                        g_error_free(err);
                } else {
                        pixbuf = gdk_pixbuf_loader_get_pixbuf(ldr);
                        g_object_ref(pixbuf);
                }
        }
        if (ldr != NULL) g_object_unref(G_OBJECT(ldr));
        return pixbuf;
}

/**
 * Gets the home directory
 * @return The HOME directory, or NULL if not found
 */
const char *
get_home_directory                      (void)
{
        const char *homedir = g_getenv ("HOME");
        if (homedir == NULL) {
                homedir = g_get_home_dir ();
        }
        if (homedir == NULL) {
                g_warning ("Unable to get home directory");
        }
        return homedir;
}

/**
 * Get the 2-letter language code from the currently active language
 * @return The language code, or "en" if it is unset. This string is
 *         static and must not be modified
 */
const char *
get_language_code                       (void)
{
        static char lang[3] = "";
        if (lang[0] == '\0') {
                const char *env = g_getenv("LANGUAGE");
                if (!env) env = g_getenv("LC_MESSAGES");
                if (!env) env = g_getenv("LC_ALL");
                if (!env) env = g_getenv("LANG");
                if (env != NULL && strlen(env) > 1) {
                        strncpy(lang, env, 2);
                        lang[2] = '\0';
                } else {
                        strncpy(lang, "en", 3);
                }
        }
        return lang;
}

/**
 * Obfuscates an ASCII string. All characters between 33 and 126 will
 * be modified. The string will be modified in place. Calling this
 * function twice will return the original string.
 * @param str The string to be obfuscated. It will be modified in place
 * @return The same string, for convenience
 */
char *
obfuscate_string                        (char *str)
{
        guchar *ptr;

        g_return_val_if_fail (str != NULL, NULL);

        for (ptr = (guchar *) str; *ptr != '\0'; ptr++) {
                if (*ptr >= 33 && *ptr < 127) {
                        int offset = *ptr - 33;
                        offset = (offset + ((127 - 33) / 2)) % (127 - 33);
                        *ptr = (guchar) (33 + offset);
                }
        }

        return str;
}

/**
 * Decodes a string stored in the encoding used by Last.fm in URLs:
 * spaces are replaced by '+', and some chars such as the ampersand
 * are encoded twice
 * @param str The original string
 * @return The decoded string in a newly-allocated buffer
 */
char *
lastfm_url_decode                       (const char *str)
{
        char *tmp;
        GString *gstr;

        g_return_val_if_fail (str != NULL, NULL);

        tmp = escape_url (str, FALSE);
        gstr = g_string_new (tmp);
        g_free (tmp);

        string_replace_gstr (gstr, "&amp;", "&");
        string_replace_gstr (gstr, "%26", "&");
        string_replace_char (gstr->str, '+', ' ');

        return g_string_free (gstr, FALSE);
}

/**
 * Encodes a string with the encoding used by Last.fm in URLs: spaces
 * are replaced by '+', and some chars such as the ampersand are
 * encoded twice
 * @param str The original string
 * @return The encoded string in a newly-allocated buffer
 */
char *
lastfm_url_encode                       (const char *str)
{
        char *retvalue;
        GString *gstr;

        g_return_val_if_fail (str != NULL, NULL);

        gstr = g_string_new (str);

        string_replace_char (gstr->str, ' ', '+');
        string_replace_gstr (gstr, "&", "%26");
        retvalue = escape_url (gstr->str, TRUE);

        g_string_free (gstr, TRUE);
        return retvalue;
}

/**
 * Launch an URL using GIO library.
 * This function is available only if Vagalume has been compiled with
 * GIO support.
 * @param url The URL to show
 */
#ifdef HAVE_GIO
void
launch_url                              (const char        *url,
                                         GAppLaunchContext *context)
{
        GError *error = NULL;

        /* FIXME: check for gtk 2.14 and use GdkAppLaunchContext */
        if (!context)
                context = g_app_launch_context_new();
        else
                g_object_ref(context);

        if (!g_app_info_launch_default_for_uri (url, context, &error))
                g_warning ("Launching failed: %s\n", error->message);

        g_object_unref(context);
}
#endif

/**
 * Add a child element to an XML node consisting on a string.
 * @param parent Parent XML node
 * @param name Name of the child node to be created
 * @param value Value of the child node
 */
void
xml_add_string                          (xmlNode    *parent,
                                         const char *name,
                                         const char *value)
{
        xmlNode *node;
        xmlChar *enc;

        g_return_if_fail (parent && name && value);

        node = xmlNewNode (NULL, (xmlChar *) name);
        enc = xmlEncodeEntitiesReentrant (NULL, (xmlChar *) value);
        xmlNodeSetContent (node, enc);
        xmlFree (enc);

        xmlAddChild (parent, node);
}

/**
 * Add a child element to an XML node consisting on a boolean.
 * @param parent Parent XML node
 * @param name Name of the child node to be created
 * @param value Value of the child node
 */
void
xml_add_bool                            (xmlNode    *parent,
                                         const char *name,
                                         gboolean    value)
{
        xml_add_string (parent, name, value ? "1" : "0");
}

/**
 * Add a child element to an XML node consisting on a long int.
 * @param parent Parent XML node
 * @param name Name of the child node to be created
 * @param value Value of the child node
 */
void
xml_add_glong                           (xmlNode    *parent,
                                         const char *name,
                                         glong       value)
{
        const int maxlen = sizeof (glong) * 3; /* Enough to store a glong */
        char strvalue[maxlen];
        g_snprintf (strvalue, maxlen, "%ld", value);
        xml_add_string (parent, name, strvalue);
}

/**
 * Find an XML node in a list by its name
 * @param node The first node on the list to search
 * @param name Name of the node to find
 * @return The node, or NULL if it was not found
 */
const xmlNode *
xml_find_node                           (const xmlNode *node,
                                         const char    *name)
{
        const xmlNode *iter;

        g_return_val_if_fail (name != NULL, NULL);

        for (iter = node; iter != NULL; iter = iter->next) {
                if (xmlStrEqual (iter->name, (xmlChar *) name)) {
                        return iter;
                }
        }

        return NULL;
}

/**
 * Find an XML node in a list and get its value as a string.
 * @param doc The xmlDoc
 * @param node The first node on the list to search
 * @param name The name of the node to find
 * @param value A pointer to a string for the return value (or NULL if
 *              no value is found)
 * @return The node, or NULL if it was not found.
 */
const xmlNode *
xml_get_string                          (xmlDoc         *doc,
                                         const xmlNode  *node,
                                         const char     *name,
                                         char          **value)
{
        g_return_val_if_fail (doc && name && value, NULL);

        node = xml_find_node (node, name);
        if (node) {
                xmlChar *val;
                val = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
                if (val != NULL) {
                        *value = g_strstrip (g_strdup ((char *) val));
                        xmlFree (val);
                } else {
                        *value = g_strdup ("");
                }
        } else {
                *value = NULL;
        }

        return node;
}

/**
 * Find an XML node in a list and get its value as a boolean.
 * @param doc The xmlDoc
 * @param node The first node on the list to search
 * @param name The name of the node to find
 * @param value A pointer to a boolean for the return value.
 * @return The node, or NULL if it was not found.
 */
const xmlNode *
xml_get_bool                            (xmlDoc        *doc,
                                         const xmlNode *node,
                                         const char    *name,
                                         gboolean      *value)
{
        const xmlNode *position;
        char *strval;

        g_return_val_if_fail (doc && name && value, NULL);

        position = xml_get_string (doc, node, name, &strval);
        *value = strval ? g_str_equal (strval, "1") : FALSE;
        g_free (strval);

        return position;
}

/**
 * Find an XML node in a list and get its value as a glong.
 * @param doc The xmlDoc
 * @param node The first node on the list to search
 * @param name The name of the node to find
 * @param value A pointer to a glong for the return value.
 * @return The node, or NULL if it was not found.
 */
const xmlNode *
xml_get_glong                           (xmlDoc        *doc,
                                         const xmlNode *node,
                                         const char    *name,
                                         glong         *value)
{
        const xmlNode *position;
        char *strval = NULL;

        g_return_val_if_fail (doc && name && value, NULL);

        position = xml_get_string (doc, node, name, &strval);
        *value = strval ? atol (strval) : -1;
        g_free (strval);

        return position;
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
