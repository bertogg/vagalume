/*
 * controller.c -- Where the control of the program is
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#include <gtk/gtk.h>
#include <string.h>
#include <time.h>

#ifdef MAEMO
#include <libosso.h>
#endif

#include "controller.h"
#include "protocol.h"
#include "scrobbler.h"
#include "playlist.h"
#include "mainwin.h"
#include "audio.h"
#include "userconfig.h"
#include "uimisc.h"
#include "http.h"
#include "globaldefs.h"

static lastfm_session *session = NULL;
static lastfm_pls *playlist = NULL;
static rsp_session *rsp_sess = NULL;
static lastfm_mainwin *mainwin = NULL;
static lastfm_usercfg *usercfg = NULL;
static lastfm_track *nowplaying = NULL;
static time_t nowplaying_since = 0;

typedef struct {
        lastfm_track *track;
        time_t start;
} rsp_data;

static void
show_dialog(const char *text, GtkMessageType type)
{
        g_return_if_fail(mainwin != NULL);
        ui_info_dialog(GTK_WINDOW(mainwin->window), text, type);
}

static gboolean
controller_show_progress(gpointer data)
{
        lastfm_track *tr = (lastfm_track *) data;
        g_return_val_if_fail(mainwin != NULL && tr != NULL, FALSE);
        if (nowplaying && session && tr->id == nowplaying->id) {
                guint played = time(NULL) - nowplaying_since;
                guint length = nowplaying->duration/1000;
                mainwin_show_progress(mainwin, length, played);
                return TRUE;
        } else {
                lastfm_track_destroy(tr);
                return FALSE;
        }
}

static gpointer
scrobble_track_thread(gpointer data)
{
        rsp_data *d = (rsp_data *) data;
        rsp_session *s = NULL;
        g_return_val_if_fail(d != NULL && d->track != NULL && d->start > 0,
                             FALSE);
        gdk_threads_enter();
        s = rsp_session_copy(rsp_sess);
        gdk_threads_leave();
        if (s != NULL) {
                rsp_scrobble(s, d->track, d->start);
                rsp_session_destroy(s);
        }
        lastfm_track_destroy(d->track);
        g_free(d);
        return NULL;
}

static void
controller_scrobble_track(void)
{
        g_return_if_fail(nowplaying != NULL && nowplaying_since > 0);
        if (rsp_sess != NULL && nowplaying->duration > 30000) {
                time_t played = time(NULL) - nowplaying_since;
                if (played > 240 || played > (nowplaying->duration/2000)) {
                        rsp_data *d = g_new0(rsp_data, 1);
                        d->track = lastfm_track_copy(nowplaying);
                        d->start = nowplaying_since;
                        g_thread_create(scrobble_track_thread,d,FALSE,NULL);
                }
        }
}

static gpointer
set_nowplaying_thread(gpointer data)
{
        rsp_data *d = (rsp_data *) data;
        g_return_val_if_fail(d != NULL && d->track != NULL, FALSE);
        rsp_session *s = NULL;
        gboolean set_np = FALSE;
        g_usleep(10 * G_USEC_PER_SEC);
        gdk_threads_enter();
        if (nowplaying && nowplaying->id == d->track->id && rsp_sess) {
                s = rsp_session_copy(rsp_sess);
                set_np = TRUE;
        }
        gdk_threads_leave();
        if (set_np) {
                rsp_set_nowplaying(s, d->track);
                rsp_session_destroy(s);
        }
        lastfm_track_destroy(d->track);
        g_free(d);
        return NULL;
}

static void
controller_set_nowplaying(lastfm_track *track)
{
        if (nowplaying != NULL) {
                lastfm_track_destroy(nowplaying);
        }
        nowplaying = track;
        nowplaying_since = time(NULL);
        if (track != NULL) {
                rsp_data *d = g_new0(rsp_data, 1);
                d->track = lastfm_track_copy(track);
                d->start = 0;
                g_thread_create(set_nowplaying_thread,d,FALSE,NULL);
        }
}

static gpointer
rsp_session_init_thread(gpointer data)
{
        g_return_val_if_fail(usercfg != NULL && rsp_sess == NULL, NULL);
        gdk_threads_enter();
        char *username = g_strdup(usercfg->username);
        char *password = g_strdup(usercfg->password);
        gdk_threads_leave();
        if (username && password) {
                rsp_session *s = rsp_session_new(username, password, NULL);
                gdk_threads_enter();
                rsp_session_destroy(rsp_sess);
                rsp_sess = s;
                gdk_threads_leave();
        }
        g_free(username);
        g_free(password);
        return NULL;
}

void controller_show_warning(const char *text)
{
        show_dialog(text, GTK_MESSAGE_WARNING);
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
                mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_CONNECTING);
                flush_ui_events();
                session = lastfm_session_new(usercfg->username,
                                             usercfg->password,
                                             &err);
        }
        if (session == NULL || session->id == NULL) {
                mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_DISCONNECTED);
                if (usercfg == NULL) {
                        show_dialog("You need to enter your Last.fm\n"
                                    "username and password to be able\n"
                                    "to use this program.",
                                    GTK_MESSAGE_WARNING);
                } else if (err == LASTFM_ERR_LOGIN) {
                        show_dialog("Unable to login to Last.fm\n"
                                    "Check username and password",
                                    GTK_MESSAGE_WARNING);
                } else {
                        show_dialog("Network connection error",
                                    GTK_MESSAGE_WARNING);
                }
        } else {
                retvalue = TRUE;
                if (rsp_sess == NULL) {
                        g_thread_create(rsp_session_init_thread, NULL,
                                        FALSE, NULL);
                }
        }
        return retvalue;
}

static void
ui_update_track_info(lastfm_track *track)
{
        g_return_if_fail(playlist != NULL && track != NULL && mainwin != NULL);
        const char *pls = playlist->title;
        const char *artist = track->artist;
        const char *title = track->title;
        const char *album = track->album;
        mainwin_update_track_info(mainwin, pls, artist, title, album);
}

void
controller_stop_playing(void)
{
        g_return_if_fail(mainwin != NULL);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_STOPPED);
        lastfm_audio_stop();
        if (nowplaying != NULL) {
                controller_scrobble_track();
                controller_set_nowplaying(NULL);
        }
}

static gpointer
start_playing_get_pls_thread(gpointer data)
{
        lastfm_session *s = (lastfm_session *) data;
        g_return_val_if_fail(s != NULL, NULL);
        lastfm_pls *pls = lastfm_request_playlist(s);
        gdk_threads_enter();
        if (pls == NULL) {
                controller_stop_playing();
                show_dialog("No more content to play", GTK_MESSAGE_INFO);
        } else {
                lastfm_pls_merge(playlist, pls);
                lastfm_pls_destroy(pls);
                controller_start_playing();
        }
        gdk_threads_leave();
        lastfm_session_destroy(s);
        return NULL;
}

void
controller_start_playing(void)
{
        lastfm_track *track = NULL;
        g_return_if_fail(mainwin != NULL && playlist != NULL);
        if (!check_session()) return;
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_CONNECTING);
        flush_ui_events();
        if (lastfm_pls_size(playlist) == 0) {
                lastfm_session *s = lastfm_session_copy(session);
                g_thread_create(start_playing_get_pls_thread,s,FALSE,NULL);
                return;
        }
        track = lastfm_pls_get_track(playlist);
        controller_set_nowplaying(track);
        lastfm_audio_play(track->stream_url);
        ui_update_track_info(track);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_PLAYING);
        track = lastfm_track_copy(track);
        controller_show_progress(track);
        g_timeout_add(1000, controller_show_progress, track);
}

void
controller_skip_track(void)
{
        g_return_if_fail(mainwin != NULL);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_CONNECTING);
        lastfm_audio_stop();
        if (nowplaying != NULL) {
                controller_scrobble_track();
                controller_set_nowplaying(NULL);
        }
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
                lastfm_pls_clear(playlist);
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
controller_play_radio_ask_url(void)
{
        g_return_if_fail(mainwin != NULL);
        char *url = NULL;
        url = ui_input_dialog(GTK_WINDOW(mainwin->window),
                              "Enter radio URL",
                              "Enter the URL of the Last.fm radio",
                              "lastfm://");
        if (url != NULL) {
                if (!strncmp(url, "lastfm://", 9)) {
                        controller_play_radio_by_url(url);
                } else {
                        show_dialog("Last.fm radio URLs must start with "
                                    "lastfm://", GTK_MESSAGE_INFO);
                }
        }
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
        usercfg = read_usercfg();
        playlist = lastfm_pls_new(NULL);

#ifdef MAEMO
        if (!osso_initialize(APP_NAME_LC, APP_VERSION, FALSE, NULL)) {
                show_dialog("Unable to initialize OSSO context",
                            GTK_MESSAGE_ERROR);
                return;
        }
#endif
        if (!lastfm_audio_init()) {
                show_dialog("Error initializing audio system",
                            GTK_MESSAGE_ERROR);
                return;
        } else if (radio_url) {
                controller_play_radio_by_url(radio_url);
        }

        gtk_main();
}
