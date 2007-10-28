/*
 * mainwin.h -- Functions to control the main program window
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#ifndef MAINWIN_H
#define MAINWIN_H

typedef struct {
        GtkWidget *window;
        GtkWidget *play, *stop, *next;
        GtkWidget *playlist, *artist, *track, *album;
        GtkWidget *radiomenu, *settings;
        GtkWidget *progressbar;
} lastfm_mainwin;

typedef enum {
        LASTFM_UI_STATE_DISCONNECTED,
        LASTFM_UI_STATE_STOPPED,
        LASTFM_UI_STATE_PLAYING,
        LASTFM_UI_STATE_CONNECTING
} lastfm_ui_state;

lastfm_mainwin *lastfm_mainwin_create(void);
void mainwin_update_track_info(lastfm_mainwin *w, const char *playlist,
                               const char *artist, const char *track,
                               const char *album);
void mainwin_set_ui_state(lastfm_mainwin *w, lastfm_ui_state s);
void mainwin_show_progress(lastfm_mainwin *w, guint length, guint played);

#endif
