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

#ifndef GDK_KEY_space
#        define GDK_KEY_space GDK_space
#        define GDK_KEY_Right GDK_Right
#        define GDK_KEY_KP_Add GDK_KP_Add
#        define GDK_KEY_KP_Subtract GDK_KP_Subtract
#        define GDK_KEY_plus GDK_plus
#        define GDK_KEY_minus GDK_minus
#        define GDK_KEY_F6 GDK_F6
#        define GDK_KEY_F7 GDK_F7
#        define GDK_KEY_F8 GDK_F8
#        define GDK_KEY_b GDK_b
#        define GDK_KEY_l GDK_l
#        define GDK_KEY_r GDK_r
#        define GDK_KEY_t GDK_t
#        define GDK_KEY_a GDK_a
#        define GDK_KEY_p GDK_p
#        define GDK_KEY_q GDK_q
#endif /* GDK_KEY_space */

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

#ifndef HAVE_GTK_DIALOG_GET_CONTENT_AREA

#     define gtk_dialog_get_content_area(X) (((GtkDialog *) (X))->vbox)
#     define gtk_dialog_get_action_area(X)  (((GtkDialog *) (X))->action_area)

#endif /* HAVE_GTK_DIALOG_GET_CONTENT_AREA */

#endif /* VGL_COMPAT_H */
