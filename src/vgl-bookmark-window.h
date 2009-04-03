/*
 * vgl-bookmark-window.h -- Bookmark manager window
 *
 * Copyright (C) 2007-2008 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_BOOKMARK_WINDOW_H
#define VGL_BOOKMARK_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VGL_TYPE_BOOKMARK_WINDOW                                        \
   (vgl_bookmark_window_get_type())
#define VGL_BOOKMARK_WINDOW(obj)                                        \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                VGL_TYPE_BOOKMARK_WINDOW,               \
                                VglBookmarkWindow))
#define VGL_BOOKMARK_WINDOW_CLASS(klass)                                \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             VGL_TYPE_BOOKMARK_WINDOW,                  \
                             VglBookmarkWindowClass))
#define VGL_IS_BOOKMARK_WINDOW(obj)                                     \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                VGL_TYPE_BOOKMARK_WINDOW))
#define VGL_IS_BOOKMARK_WINDOW_CLASS(klass)                             \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             VGL_TYPE_BOOKMARK_WINDOW))
#define VGL_BOOKMARK_WINDOW_GET_CLASS(obj)                              \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               VGL_TYPE_BOOKMARK_WINDOW,                \
                               VglBookmarkWindowClass))

typedef struct _VglBookmarkWindow        VglBookmarkWindow;
typedef struct _VglBookmarkWindowClass   VglBookmarkWindowClass;
typedef struct _VglBookmarkWindowPrivate VglBookmarkWindowPrivate;

struct _VglBookmarkWindowClass
{
    GtkDialogClass parent_class;
};

struct _VglBookmarkWindow
{
    GtkDialog parent;
    VglBookmarkWindowPrivate *priv;
};

GType
vgl_bookmark_window_get_type            (void) G_GNUC_CONST;

void
vgl_bookmark_window_show                (GtkWindow *parent);

G_END_DECLS

#endif /* VGL_BOOKMARK_WINDOW_H */
