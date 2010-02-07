/*
 * vgl-bookmark-window.h -- Bookmark manager window
 *
 * Copyright (C) 2007-2008, 2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_BOOKMARK_WINDOW_H
#define VGL_BOOKMARK_WINDOW_H

#include "vgl-bookmark-window.bp.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

struct _VglBookmarkWindowClass
{
    GtkDialogClass parent_class;
};

struct _VglBookmarkWindow
{
    GtkDialog parent;
    VglBookmarkWindowPrivate *priv;
};

void
vgl_bookmark_window_show                (GtkWindow *parent);

G_END_DECLS

#endif /* VGL_BOOKMARK_WINDOW_H */
