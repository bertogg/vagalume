/*
 * mainwin.h -- Functions to control the main program window
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#ifndef MAINWIN_H
#define MAINWIN_H

#include "playlist.h"
#include <gtk/gtk.h>

typedef struct {
        GtkWindow *window;
        GtkWidget *play, *stop, *next;
        GtkWidget *love, *ban, *dloadbutton;
        GtkWidget *playlist, *artist, *track, *album;
        GtkWidget *radiomenu, *actionsmenu, *settings, *dload;
        GtkWidget *album_cover;
        GtkWidget *progressbar;
        gboolean is_fullscreen;
        gboolean is_hidden;
} lastfm_mainwin;

typedef enum {
        LASTFM_UI_STATE_DISCONNECTED,
        LASTFM_UI_STATE_STOPPED,
        LASTFM_UI_STATE_PLAYING,
        LASTFM_UI_STATE_CONNECTING
} lastfm_ui_state;

lastfm_mainwin *lastfm_mainwin_create(void);
void mainwin_show_window(lastfm_mainwin *w, gboolean show);
void mainwin_set_ui_state(lastfm_mainwin *w, lastfm_ui_state s,
                          const lastfm_track *t);
void mainwin_show_progress(lastfm_mainwin *w, guint length, guint played);
void mainwin_set_album_cover(lastfm_mainwin *w, const guchar *data, int size);
void mainwin_run_app(void);
void mainwin_quit_app(void);

#endif
