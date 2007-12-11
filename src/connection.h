/*
 * connection.h -- Internet connection handling
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#ifndef CONNECTION_H
#define CONNECTION_H

#include "config.h"
#include <glib.h>

typedef void (*connection_go_online_cb)(gpointer data);

#ifdef HAVE_CONIC

const char *connection_init(void);
void connection_go_online(connection_go_online_cb cb, gpointer data);

#else

const char *connection_init(void) { return NULL; }
void connection_go_online(connection_go_online_cb cb, gpointer data) {
        (*cb)(data);
}

#endif

#endif
