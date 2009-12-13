/*
 * compat.c -- Implementations of some common functions for old systems
 *
 * Copyright (C) 2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "compat.h"

#include <gdk/gdk.h>

#ifndef HAVE_GDK_THREADS_ADD_API
typedef struct
{
  GSourceFunc func;
  gpointer data;
  GDestroyNotify destroy;
} GdkThreadsDispatch;

static gboolean
gdk_threads_dispatch                    (gpointer data)
{
        GdkThreadsDispatch *dispatch = data;
        gboolean ret = FALSE;

        gdk_threads_enter ();
        ret = dispatch->func (dispatch->data);
        gdk_threads_leave ();

        return ret;
}

static void
gdk_threads_dispatch_free               (gpointer data)
{
        GdkThreadsDispatch *dispatch = data;

        if (dispatch->destroy && dispatch->data)
                dispatch->destroy (dispatch->data);

        g_slice_free (GdkThreadsDispatch, data);
}

guint
gdk_threads_add_idle_full               (gint           priority,
		                         GSourceFunc    function,
		                         gpointer       data,
		                         GDestroyNotify notify)
{
        GdkThreadsDispatch *dispatch;

        g_return_val_if_fail (function != NULL, 0);

        dispatch = g_slice_new (GdkThreadsDispatch);
        dispatch->func = function;
        dispatch->data = data;
        dispatch->destroy = notify;

        return g_idle_add_full (priority, gdk_threads_dispatch, dispatch,
                                gdk_threads_dispatch_free);
}

guint
gdk_threads_add_idle                    (GSourceFunc function,
		                         gpointer    data)
{
        return gdk_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE,
                                          function, data, NULL);
}

guint
gdk_threads_add_timeout_full            (gint           priority,
                                         guint          interval,
                                         GSourceFunc    function,
                                         gpointer       data,
                                         GDestroyNotify notify)
{
        GdkThreadsDispatch *dispatch;

        g_return_val_if_fail (function != NULL, 0);

        dispatch = g_slice_new (GdkThreadsDispatch);
        dispatch->func = function;
        dispatch->data = data;
        dispatch->destroy = notify;

        return g_timeout_add_full (priority, interval,
                                   gdk_threads_dispatch, dispatch,
                                   gdk_threads_dispatch_free);
}

guint
gdk_threads_add_timeout                 (guint       interval,
                                         GSourceFunc function,
                                         gpointer    data)
{
        return gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT,
                                             interval, function, data, NULL);
}
#endif /* HAVE_GDK_THREADS_ADD_API */
