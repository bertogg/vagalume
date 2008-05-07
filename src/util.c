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
        const char *pos = str->str;
        while ((pos = strstr(pos, old)) != NULL) {
                g_string_erase(str, pos - str->str, strlen(old));
                g_string_insert(str, pos - str->str, new);
                pos += strlen(new);
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
