
#ifndef MAINWIN_H
#define MAINWIN_H

typedef struct {
        GtkWidget *window;
        GtkWidget *play, *stop, *next;
        GtkWidget *hbox;
        GtkWidget *vbox;
        GtkWidget *menubar;
        GtkWidget *playlist, *artist, *track, *album;
} lastfm_mainwin;

typedef enum {
        LASTFM_UI_STATE_STOPPED,
        LASTFM_UI_STATE_PLAYING,
        LASTFM_UI_STATE_CONNECTING
} lastfm_ui_state;

lastfm_mainwin *lastfm_mainwin_create(void);
void mainwin_update_track_info(lastfm_mainwin *w, const char *playlist,
                               const char *artist, const char *track,
                               const char *album);
void mainwin_set_ui_state(lastfm_mainwin *w, lastfm_ui_state s);

#endif
