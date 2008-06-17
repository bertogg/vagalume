/*
 * vgl-main-window.h -- Main program window
 * Copyright (C) 2007, 2008 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef VGL_MAIN_WINDOW_H
#define VGL_MAIN_WINDOW_H

#include "globaldefs.h"
#include "playlist.h"
#include <gtk/gtk.h>

#if defined(HILDON_LIBS)
#        include <hildon-widgets/hildon-program.h>
#elif defined(HILDON_1)
#        include <hildon/hildon-program.h>
#endif

G_BEGIN_DECLS

#define VGL_TYPE_MAIN_WINDOW                                            \
   (vgl_main_window_get_type())
#define VGL_MAIN_WINDOW(obj)                                            \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                VGL_TYPE_MAIN_WINDOW,                   \
                                VglMainWindow))
#define VGL_MAIN_WINDOW_CLASS(klass)                                    \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             VGL_TYPE_MAIN_WINDOW,                      \
                             VglMainWindowClass))
#define VGL_IS_MAIN_WINDOW(obj)                                         \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                VGL_TYPE_MAIN_WINDOW))
#define VGL_IS_MAIN_WINDOW_CLASS(klass)                                 \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             VGL_TYPE_MAIN_WINDOW))
#define VGL_MAIN_WINDOW_GET_CLASS(obj)                                  \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               VGL_TYPE_MAIN_WINDOW,                    \
                               VglMainWindowClass))

typedef struct _VglMainWindow      VglMainWindow;
typedef struct _VglMainWindowClass VglMainWindowClass;

struct _VglMainWindowClass
{
#ifdef USE_HILDON_WINDOW
    HildonWindowClass parent_class;
#else
    GtkWindowClass parent_class;
#endif
};

struct _VglMainWindow
{
#ifdef USE_HILDON_WINDOW
    HildonWindow parent;
#else
    GtkWindow parent;
#endif
};

/* UI state */
typedef enum {
        VGL_MAIN_WINDOW_STATE_DISCONNECTED,
        VGL_MAIN_WINDOW_STATE_STOPPED,
        VGL_MAIN_WINDOW_STATE_PLAYING,
        VGL_MAIN_WINDOW_STATE_CONNECTING
} VglMainWindowState;

GType vgl_main_window_get_type (void) G_GNUC_CONST;

GtkWidget *vgl_main_window_new(void);
void vgl_main_window_destroy(VglMainWindow *w);
void vgl_main_window_show(VglMainWindow *w, gboolean show);
void vgl_main_window_toggle_visibility(VglMainWindow *w);
void vgl_main_window_set_state(VglMainWindow *w, VglMainWindowState s,
                               const lastfm_track *t);
void vgl_main_window_show_progress(VglMainWindow *w,
                                   guint length, guint played);
void vgl_main_window_show_status_msg(VglMainWindow *w, const char *text);
void vgl_main_window_set_album_cover(VglMainWindow *w,
                                     const char *data, int size);
void vgl_main_window_set_track_as_loved(VglMainWindow *w);
void vgl_main_window_set_track_as_added_to_playlist(VglMainWindow *w,
                                                    gboolean added);
void vgl_main_window_run_app(void);
GtkWindow *vgl_main_window_get_window(VglMainWindow *w,gboolean get_if_hidden);
gboolean vgl_main_window_is_hidden(VglMainWindow *w);

G_END_DECLS

#endif /* VGL_MAIN_WINDOW_H */
