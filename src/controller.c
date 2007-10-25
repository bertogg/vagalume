
#include <gtk/gtk.h>

#include "controller.h"
#include "protocol.h"
#include "playlist.h"
#include "mainwin.h"
#include "audio.h"
#include "userconfig.h"
#include "uimisc.h"
#include "http.h"

static lastfm_session *session = NULL;
static lastfm_mainwin *mainwin = NULL;
static lastfm_usercfg *usercfg = NULL;

static void
show_dialog(const char *text, GtkMessageType type)
{
        g_return_if_fail(mainwin != NULL);
        ui_info_dialog(GTK_WINDOW(mainwin->window), text, type);
}

void
controller_open_usercfg(void)
{
        g_return_if_fail(mainwin != NULL);
        gboolean changed;
        changed = ui_usercfg_dialog(GTK_WINDOW(mainwin->window), &usercfg);
        if (usercfg != NULL) {
                write_usercfg(usercfg);
        }
        if (changed && session != NULL) {
                lastfm_session_destroy(session);
                session = NULL;
                controller_stop_playing();
                mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_DISCONNECTED);
        }
}

static gboolean
check_usercfg(void)
{
        if (usercfg == NULL) usercfg = read_usercfg();
        if (usercfg == NULL) controller_open_usercfg();
        return (usercfg != NULL);
}

static gboolean
check_session(void)
{
        if (session != NULL) return TRUE;
        lastfm_err err = LASTFM_ERR_NONE;
        gboolean retvalue = FALSE;
        check_usercfg();
        if (usercfg != NULL) {
                session = lastfm_session_new(usercfg->username,
                                             usercfg->password,
                                             &err);
        }
        if (session == NULL || session->id == NULL) {
                if (err == LASTFM_ERR_LOGIN) {
                        show_dialog("Unable to login to Last.fm\n"
                                    "Check user and password",
                                    GTK_MESSAGE_WARNING);
                } else {
                        show_dialog("Network connection error",
                                    GTK_MESSAGE_WARNING);
                }
        } else {
                retvalue = TRUE;
        }
        return retvalue;
}

static void
ui_update_track_info(lastfm_track *track)
{
        g_return_if_fail(session != NULL && track != NULL && mainwin != NULL);
        const char *playlist = session->playlist->title;
        const char *artist = track->artist;
        const char *title = track->title;
        const char *album = track->album;
        mainwin_update_track_info(mainwin, playlist, artist, title, album);
}

void
controller_stop_playing(void)
{
        g_return_if_fail(mainwin != NULL);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_STOPPED);
        lastfm_audio_stop();
}

void
controller_start_playing(void)
{
        g_return_if_fail(mainwin != NULL);
        if (!check_session()) return;
        if (lastfm_pls_size(session->playlist) == 0) {
                if (!lastfm_request_playlist(session)) {
                        controller_stop_playing();
                        show_dialog("No more content to play",
                                    GTK_MESSAGE_INFO);
                        return;
                }
        }
        lastfm_track *track = lastfm_pls_get_track(session->playlist);
        lastfm_audio_play(track->stream_url);
        ui_update_track_info(track);
        lastfm_track_destroy(track);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_PLAYING);
}

void
controller_skip_track(void)
{
        g_return_if_fail(mainwin != NULL);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_CONNECTING);
        lastfm_audio_stop();
        while (gtk_events_pending()) gtk_main_iteration();
        controller_start_playing();
}

void
controller_play_radio_by_url(const char *url)
{
        if (!check_session()) return;
        if (url == NULL) {
                g_critical("Attempted to play a NULL radio URL");
                controller_stop_playing();
        } else if (lastfm_set_radio(session, url)) {
                controller_skip_track();
        } else {
                controller_stop_playing();
                show_dialog("Invalid radio URL", GTK_MESSAGE_INFO);
        }
}

void
controller_play_radio(lastfm_radio type)
{
        if (!check_session()) return;
        char *url;
        if (type == LASTFM_RECOMMENDED_RADIO) {
                url = lastfm_recommended_radio_url(
                        usercfg->username, 100);
        } else {
                url = lastfm_radio_url(type, usercfg->username);
        }
        controller_play_radio_by_url(url);
        g_free(url);
}

void
controller_quit_app(void)
{
        lastfm_audio_clear();
        lastfm_session_destroy(session);
        gtk_main_quit();
}

void
controller_run_app(lastfm_mainwin *win, const char *radio_url)
{
        g_return_if_fail(win != NULL && mainwin == NULL);
        mainwin = win;
        gtk_widget_show_all(mainwin->window);

        http_init();
        if (!lastfm_audio_init()) {
                show_dialog("Error initializing audio system",
                            GTK_MESSAGE_ERROR);
                return;
        } else if (radio_url) {
                controller_play_radio_by_url(radio_url);
        }

        gtk_main();
}
