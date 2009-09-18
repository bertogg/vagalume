/*
 * compat.h -- Implementations of some common functions for old systems
 *
 * Copyright (C) 2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_COMPAT_H
#define VGL_COMPAT_H

#include "config.h"

#include <glib.h>

#ifndef HAVE_GDK_THREADS_ADD_TIMEOUT_SECONDS
#        define gdk_threads_add_timeout_seconds(A,B,C) \
                gdk_threads_add_timeout ((A)*1000,B,C)
#endif

#ifndef HAVE_GDK_THREADS_ADD_API
G_BEGIN_DECLS

guint
gdk_threads_add_idle_full               (gint           priority,
		                         GSourceFunc    function,
		                         gpointer       data,
		                         GDestroyNotify notify);

guint
gdk_threads_add_idle                    (GSourceFunc function,
		                         gpointer    data);

guint
gdk_threads_add_timeout_full            (gint           priority,
                                         guint          interval,
                                         GSourceFunc    function,
                                         gpointer       data,
                                         GDestroyNotify notify);

guint
gdk_threads_add_timeout                 (guint       interval,
                                         GSourceFunc function,
                                         gpointer    data);

G_END_DECLS
#endif /* HAVE_GDK_THREADS_ADD_API */

#endif /* VGL_COMPAT_H */
