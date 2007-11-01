/*
 * util.c -- Misc util functions
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#include "util.h"
#include "md5.h"
#include <glib.h>
#include <string.h>
#include <stdio.h>

/**
 * Computes the MD5 hash of a NULL-terminated string
 * @param str The string
 * @return The MD5 hash, in hexadecimal
 */
char *
get_md5_hash(const char *str)
{
        g_return_val_if_fail(str != NULL, NULL);
        const int digestlen = 16;
        md5_state_t state;
        md5_byte_t digest[digestlen];
        int i;

        md5_init(&state);
        md5_append(&state, (const md5_byte_t *)str, strlen(str));
        md5_finish(&state, digest);

        char *hexdigest = g_malloc(digestlen*2 + 1);
        for (i = 0; i < digestlen; i++) {
                sprintf(hexdigest + 2*i, "%02x", digest[i]);
        }
        return hexdigest;
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
