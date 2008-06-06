/*
 * util.c -- Misc util functions
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "config.h"
#include "util.h"

#ifndef HAVE_GCHECKSUM
#   ifdef HAVE_LIBGCRYPT
#      include <gcrypt.h>
#   else
#      include "md5/md5.h"
#   endif
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
get_md5_hash(const char *str)
{
        g_return_val_if_fail(str != NULL, NULL);
#ifdef HAVE_GCHECKSUM
        return g_compute_checksum_for_string(G_CHECKSUM_MD5, str, -1);
#else
        const int digestlen = 16;
        unsigned char digest[digestlen];
        guint i;

#   ifdef HAVE_LIBGCRYPT
        gcry_md_hash_buffer(GCRY_MD_MD5, digest, str, strlen(str));
#   else
        md5_state_t state;
        md5_init(&state);
        md5_append(&state, (const md5_byte_t *)str, strlen(str));
        md5_finish(&state, digest);
#   endif /* HAVE_LIBGCRYPT */
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
compute_auth_token(const char *password, const char *timestamp)
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
str_glist_join(const GList *list, const char *separator)
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
file_exists(const char *filename)
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
string_replace_gstr(GString *str, const char *old, const char *new)
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
 * @param str The string to be modified
 * @param old The original text
 * @param new The new text
 * @return A newly-created string
 */
char *
string_replace(const char *str, const char *old, const char *new)
{
        g_return_val_if_fail(str && old && new, NULL);
        GString *gstr = g_string_new(str);
        string_replace_gstr(gstr, old, new);
        return g_string_free(gstr, FALSE);
}

/**
 * Creates a GdkPixbuf from a image in any supported format
 * @param data The original image in memory, or NULL
 * @param size The size of the buffer containing the original image
 * @param imgsize The resulting pixbuf will be imgsize x imgsize pixels
 * @return The GdkPixbuf, or NULL
 */
GdkPixbuf *
get_pixbuf_from_image(const char *data, size_t size, int imgsize)
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
get_home_directory (void)
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
