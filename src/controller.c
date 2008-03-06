/*
 * controller.c -- Where the control of the program is
 * Copyright (C) 2007, 2008 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "config.h"

#include <glib.h>
#include <string.h>
#include <time.h>

#include "connection.h"
#include "controller.h"
#include "metadata.h"
#include "scrobbler.h"
#include "playlist.h"
#include "audio.h"
#include "uimisc.h"
#include "dlwin.h"
#include "http.h"
#include "globaldefs.h"
#include "dbus.h"
#include "util.h"
#include "imstatus/imstatus.h"

static lastfm_session *session = NULL;
static lastfm_pls *playlist = NULL;
static rsp_session *rsp_sess = NULL;
static lastfm_mainwin *mainwin = NULL;
static lastfm_usercfg *usercfg = NULL;
static GList *friends = NULL;
static GList *usertags = NULL;
static lastfm_track *nowplaying = NULL;
static time_t nowplaying_since = 0;
static rsp_rating nowplaying_rating = RSP_RATING_NONE;
static gboolean showing_cover = FALSE;
static gboolean stopping_after_track = FALSE;

typedef struct {
        lastfm_track *track;
        rsp_rating rating;
        time_t start;
} rsp_data;

typedef struct {
        lastfm_track *track;
        char *taglist;                /* comma-separated list of tags */
        request_type type;
} tag_data;

typedef struct {
        lastfm_track *track;
        char *rcpt;                  /* Recipient of the recommendation */
        char *text;                  /* text of the recommendation */
        request_type type;
} recomm_data;

typedef struct {
        guint track_id;              /* Id of the track */
        char *image_url;             /* URL of the album cover */
} getcover_data;

/*
 * Callback called after check_session() in all cases.
 * data is user-provided data, and must be freed by the caller
 * Different callbacks must be supplied for success and failure
 */
typedef void (*check_session_cb)(gpointer data);

typedef struct {
        char *user;
        char *pass;
        check_session_cb success_cb;
        check_session_cb failure_cb;
        gpointer cbdata;
} check_session_thread_data;

/**
 * Show an error dialog with an OK button
 *
 * @param text The text to show
 */
void
controller_show_error(const char *text)
{
        g_return_if_fail(mainwin != NULL);
        ui_error_dialog(mainwin->window, text);
}

/**
 * Show a warning dialog with an OK button
 *
 * @param text The text to show
 */
void
controller_show_warning(const char *text)
{
        g_return_if_fail(mainwin != NULL);
        ui_warning_dialog(mainwin->window, text);
}

/**
 * Show an info dialog with an OK button
 *
 * @param text The text to show
 */
void
controller_show_info(const char *text)
{
        g_return_if_fail(mainwin != NULL);
        ui_info_dialog(mainwin->window, text);
}

/**
 * Show an info banner (with no buttons if possible)
 *
 * @param text The text to show
 */
void
controller_show_banner(const char *text)
{
#ifdef MAEMO
        ui_info_banner(mainwin->window, text);
#else
        mainwin_show_status_msg(mainwin, text);
#endif
}

/**
 * Show an OK/cancel dialog to request confirmation from the user
 *
 * @param text The text to show
 * @return TRUE if the user selects OK, cancel otherwise
 */
gboolean
controller_confirm_dialog(const char *text)
{
        g_return_val_if_fail(mainwin != NULL, FALSE);

#ifdef MAEMO
        return ui_confirm_dialog(NULL, text);
#else
        return ui_confirm_dialog(mainwin->window, text);
#endif
}

/**
 * Show/Hide the program main window
 *
 * @param show Whether to show or hide it
 */
void
controller_show_mainwin(gboolean show)
{
        g_return_if_fail(mainwin != NULL);
        mainwin_show_window(mainwin, show);
}

/**
 * Calculate the amount of time that a track has been playing and
 * updates the UI (progressbars, etc). to reflect that. To be called
 * using g_timeout_add()
 *
 * @param data A pointer to the lastfm_track being played
 * @return TRUE if the track hasn't fininished yet, FALSE otherwise
 */
static gboolean
controller_show_progress(gpointer data)
{
        lastfm_track *tr = (lastfm_track *) data;
        g_return_val_if_fail(mainwin != NULL && tr != NULL, FALSE);
        if (nowplaying != NULL && tr->id == nowplaying->id) {
                guint played = lastfm_audio_get_running_time();
                if (played != -1) {
                        guint length = nowplaying->duration/1000;
                        mainwin_show_progress(mainwin, length, played);
                }
                return TRUE;
        } else {
                lastfm_track_unref(tr);
                return FALSE;
        }
}

/**
 * Gets the track that is currently being played
 * @return The track
 */
lastfm_track *
controller_get_current_track(void)
{
  return nowplaying;
}

/**
 * Set the list of friends, deleting the previous one.
 * @param user The user ID. If it's different from the active user,
 *             no changes will be made.
 * @param list The list of friends
 */
static void
set_friend_list(const char *user, GList *list)
{
        g_return_if_fail(user != NULL && usercfg != NULL);
        if (!strcmp(user, usercfg->username)) {
                g_list_foreach(friends, (GFunc) g_free, NULL);
                g_list_free(friends);
                friends = list;
        }
}

/**
 * Set the list of user tags, deleting the previous one.
 * @param user The user ID. If it's different from the active user,
 *             no changes will be made.
 * @param list The list of tags
 */
static void
set_user_tag_list(const char *user, GList *list)
{
        g_return_if_fail(user != NULL && usercfg != NULL);
        if (!strcmp(user, usercfg->username)) {
                g_list_foreach(usertags, (GFunc) g_free, NULL);
                g_list_free(usertags);
                usertags = list;
        }
}

/**
 * Set a new RSP session, deleting the previous one.
 * @param user The user ID. If it's different from the active user,
 *             no changes will be made.
 * @param sess The new session (can be NULL)
 */
static void
set_rsp_session(const char *user, rsp_session *sess)
{
        g_return_if_fail(user != NULL && usercfg != NULL);
        if (!strcmp(user, usercfg->username)) {
                if (rsp_sess != NULL) rsp_session_destroy(rsp_sess);
                rsp_sess = sess;
        }
}

/**
 * Scrobble a track using the Audioscrobbler Realtime Submission
 * Protocol. This can take some seconds, so it must be called using
 * g_thread_create() to avoid freezing the UI.
 *
 * @param data Pointer to rsp_data, with info about the track to be
 *             scrobbled. This data must be freed here
 * @return NULL (this value is not used)
 */
static gpointer
scrobble_track_thread(gpointer data)
{
        rsp_data *d = (rsp_data *) data;
        rsp_session *s = NULL;
        g_return_val_if_fail(d != NULL && d->track != NULL && d->start > 0,
                             NULL);
        char *user = NULL, *pass = NULL;
        gdk_threads_enter();
        s = rsp_session_copy(rsp_sess);
        if (usercfg != NULL) {
                user = g_strdup(usercfg->username);
                pass = g_strdup(usercfg->password);
        }
        gdk_threads_leave();
        if (s != NULL) {
                rsp_scrobble(s, d->track, d->start, d->rating);
                rsp_session_destroy(s);
        }
        /* This love_ban_track() call won't be needed anymore with
         * Lastfm's new protocol v1.2 */
        if (user && pass && d->rating == RSP_RATING_LOVE) {
                love_ban_track(user, pass, d->track, TRUE);
        } else if (user && pass && d->rating == RSP_RATING_BAN) {
                love_ban_track(user, pass, d->track, FALSE);
        }
        g_free(user);
        g_free(pass);
        lastfm_track_unref(d->track);
        g_free(d);
        return NULL;
}

/**
 * Scrobble the track that is currently playing. Only tracks that have
 * been playing for more than half its time (or 240 seconds) can be
 * scrobbled, see http://www.audioscrobbler.net/development/protocol/
 *
 * This function only does checks and prepares the data, the process
 * itself is done in another thread, see scrobble_track_thread()
 */
static void
controller_scrobble_track(void)
{
        g_return_if_fail(nowplaying != NULL && usercfg != NULL);
        if (usercfg->enable_scrobbling && nowplaying_since > 0 &&
            rsp_sess != NULL && nowplaying->duration > 30000) {
                int played = time(NULL) - nowplaying_since;
                /* If a track is unrated and hasn't been played for
                   enough time, scrobble it as skipped */
                if (nowplaying_rating == RSP_RATING_NONE &&
                    played < nowplaying->duration/2000 && played < 240) {
                        nowplaying_rating = RSP_RATING_SKIP;
                }
                rsp_data *d = g_new0(rsp_data, 1);
                d->track = lastfm_track_ref(nowplaying);
                d->start = nowplaying_since;
                d->rating = nowplaying_rating;
                g_thread_create(scrobble_track_thread,d,FALSE,NULL);
        }
}

/**
 * Set the "Now Playing" information using the Audioscrobbler Realtime
 * Submission Protocol. This can take some seconds, so it must be
 * called using g_thread_create() to avoid freezing the UI.
 *
 * @param data Pointer to rsp_data, with info about the track to be
 *             processed. This data must be freed here
 * @return NULL (this value is not used)
 */
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
        lastfm_track_unref(d->track);
        g_free(d);
        return NULL;
}

/**
 * Set a track as "Now Playing", eventually setting it in the Last.fm
 * server using the Audioscrobbler RSP (Realtime Submission Protocol).
 *
 * The actual RSP code is run in another thread to avoid freezing the
 * UI, see set_nowplaying_thread()
 *
 * @param track The track to be set as Now Playing
 */
static void
controller_set_nowplaying(lastfm_track *track)
{
        g_return_if_fail(usercfg != NULL);
        if (nowplaying != NULL) {
                lastfm_track_unref(nowplaying);
        }
        nowplaying = track;
        if (track == NULL) {
                im_clear_status();
        } else {
                im_set_status(track);
        }
        nowplaying_since = 0;
        nowplaying_rating = RSP_RATING_NONE;
        if (track != NULL && usercfg->enable_scrobbling) {
                rsp_data *d = g_new0(rsp_data, 1);
                d->track = lastfm_track_ref(track);
                d->start = 0;
                g_thread_create(set_nowplaying_thread,d,FALSE,NULL);
        }

        /* Notify the playback status */
        lastfm_dbus_notify_playback(track);
}

/**
 * Initializes an RSP session and get list of friends. This must be
 * called only from check_session_thread() !!
 *
 * @param data Not used
 * @return NULL (not used)
 */
static void
get_user_extradata(void)
{
        g_return_if_fail(usercfg != NULL && rsp_sess == NULL);
        gboolean finished = FALSE;
        gboolean rsp_ok = FALSE;
        gboolean friends_ok = FALSE;
        gboolean usertags_ok = FALSE;
        rsp_session *sess = NULL;
        GList *friends = NULL;
        GList *usertags = NULL;
        gdk_threads_enter();
        char *user = g_strdup(usercfg->username);
        char *pass = g_strdup(usercfg->password);
        gdk_threads_leave();
        if (!user || !pass || user[0] == '\0' || pass[0] == '\0') {
                finished = TRUE;
        }
        while (!finished) {
                if (!rsp_ok) {
                        sess = rsp_session_new(user, pass, NULL);
                        if (sess != NULL) {
                                g_debug("RSP session ready");
                                rsp_ok = TRUE;
                                gdk_threads_enter();
                                set_rsp_session(user, sess);
                                gdk_threads_leave();
                        } else {
                                g_warning("Error creating RSP session");
                        }
                }
                if (!friends_ok) {
                        friends_ok = lastfm_get_friends(user, &friends);
                        if (friends_ok) {
                                g_debug("Friend list ready");
                                gdk_threads_enter();
                                set_friend_list(user, friends);
                                gdk_threads_leave();
                        } else {
                                g_warning("Error getting friend list");
                        }
                }
                if (!usertags_ok) {
                        usertags_ok = lastfm_get_user_tags(user, &usertags);
                        if (usertags_ok) {
                                g_debug("Tag list ready");
                                gdk_threads_enter();
                                set_user_tag_list(user, usertags);
                                gdk_threads_leave();
                        } else {
                                g_warning("Error getting tag list");
                        }
                }
                if (rsp_ok && friends_ok && usertags_ok) {
                        finished = TRUE;
                } else {
                        gdk_threads_enter();
                        if (!usercfg || strcmp(usercfg->username, user)) {
                                finished = TRUE;
                        }
                        gdk_threads_leave();
                        if (!finished) g_usleep(G_USEC_PER_SEC * 60);
                }
        }
        g_free(user);
        g_free(pass);
}

/**
 * Apply all user cfg settings
 */
static void
apply_usercfg(void)
{
        g_return_if_fail(usercfg != NULL);
        if (usercfg->use_proxy) {
                http_set_proxy(usercfg->http_proxy);
        } else {
                http_set_proxy(NULL);
        }
        im_clear_status();
        im_set_cfg(usercfg->im_pidgin,
                   usercfg->im_gajim,
                   usercfg->im_gossip,
                   usercfg->im_telepathy);
        if (nowplaying != NULL) {
                im_set_status(nowplaying);
        }
}

/**
 * Open the user settings dialog and save the new settings to the
 * configuration file. If the username or password have been modified,
 * close the current session.
 */
void
controller_open_usercfg(void)
{
        g_return_if_fail(mainwin != NULL);
        gboolean userchanged = FALSE;
        gboolean pwchanged = FALSE;
        gboolean changed;
        char *olduser = usercfg != NULL ? g_strdup(usercfg->username) :
                                          g_strdup("");
        char *oldpw = usercfg != NULL ? g_strdup(usercfg->password) :
                                        g_strdup("");
        changed = ui_usercfg_dialog(mainwin->window, &usercfg);
        if (changed && usercfg != NULL) {
                write_usercfg(usercfg);
                userchanged = strcmp(olduser, usercfg->username);
                pwchanged = strcmp(oldpw, usercfg->password);
                apply_usercfg();
        }
        if (userchanged || pwchanged) {
                if (userchanged) {
                        set_friend_list(usercfg->username, NULL);
                        set_user_tag_list(usercfg->username, NULL);
                }
                controller_disconnect();
        }
        g_free(olduser);
        g_free(oldpw);
}

/**
 * Check if the user settings exist (whether they are valid or
 * not). If they don't exist, read the config file or open the
 * settings dialog (if ask == TRUE). This should only return FALSE
 * the first time the user runs the program.
 *
 * @param ask If TRUE, open the settings dialog if necessary
 * @return TRUE if the settings exist, FALSE otherwise
 */
static gboolean
check_usercfg(gboolean ask)
{
        if (usercfg == NULL) usercfg = read_usercfg();
        if (usercfg == NULL && ask) controller_open_usercfg();
        if (usercfg != NULL) apply_usercfg();
        return (usercfg != NULL);
}

/**
 * Check if there's a Last.fm session opened. If not, try to create
 * one (and an RSP session as well).
 * This is done in a thread to avoid freezing the UI. After this, the
 * callback supplied to check_session() will be called, see below for
 * details
 *
 * @param userdata A check_session_thread_data, containing the
 *                 callback and other info necessary for creating
 *                 a new session.
 * @return NULL (not used)
 */
static gpointer
check_session_thread(gpointer userdata)
{
        g_return_val_if_fail(userdata != NULL, NULL);
        check_session_thread_data *data;
        gboolean connected = FALSE;
        lastfm_err err = LASTFM_ERR_NONE;
        lastfm_session *s;
        data = (check_session_thread_data *) userdata;
        s = lastfm_session_new(data->user, data->pass, &err);
        if (s == NULL || s->id == NULL) {
                gdk_threads_enter();
                mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_DISCONNECTED,
                                     NULL);
                if (err == LASTFM_ERR_LOGIN) {
                        controller_show_warning("Unable to login to Last.fm\n"
                                                "Check username and password");
                } else {
                        controller_show_warning("Network connection error");
                }
                gdk_threads_leave();
        } else {
                gdk_threads_enter();
                session = s;
                mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_STOPPED, NULL);
                gdk_threads_leave();
                connected = TRUE;
        }
        /* Call the callback */
        gdk_threads_enter();
        if (connected && data->success_cb != NULL) {
                (*(data->success_cb))(data->cbdata);
        } else if (!connected && data->failure_cb != NULL) {
                (*(data->failure_cb))(data->cbdata);
        }
        gdk_threads_leave();
        /* Free memory */
        g_free(data->user);
        g_free(data->pass);
        g_free(data);
        if (connected) {
                get_user_extradata();
        }
        return NULL;
}

/**
 * This is the callback of connection_go_online(). It'll just create
 * a thread and let the process continue
 * @param data A pointer with data to pass to the thread
 */
static void
check_session_conn_cb(gpointer data)
{
        g_thread_create(check_session_thread, data, FALSE, NULL);
}

/**
 * Check if there's a Last.fm session opened. If not, try to create
 * one (and an RSP session as well).
 * The actual connection is performed in check_session_thread() to
 * avoid freezing the UI
 *
 * @param success_cb A callback in case this function succeeds
 * @param failure_cb A callback in case this function fails
 * @param cbdata Parameter that will be passed to both callbacks,
 *               must be freed by the caller (if necessary)
 */
static void
check_session(check_session_cb success_cb, check_session_cb failure_cb,
              gpointer cbdata)
{
        if (session != NULL) {
                if (success_cb != NULL) (*success_cb)(cbdata);
        } else {
                check_usercfg(TRUE);
                if (usercfg != NULL) {
                        check_session_thread_data *data;
                        data = g_new(check_session_thread_data, 1);
                        data->user = g_strdup(usercfg->username);
                        data->pass = g_strdup(usercfg->password);
                        data->success_cb = success_cb;
                        data->failure_cb = failure_cb;
                        data->cbdata = cbdata;
                        connection_go_online(check_session_conn_cb, data);
                        mainwin_set_ui_state(mainwin,
                                             LASTFM_UI_STATE_CONNECTING,
                                             NULL);
                } else {
                        mainwin_set_ui_state(mainwin,
                                             LASTFM_UI_STATE_DISCONNECTED,
                                             NULL);
                        controller_show_warning("You need to enter your "
                                                "Last.fm\nusername and "
                                                "password to be able\n"
                                                "to use this program.");
                        if (failure_cb != NULL) (*failure_cb)(cbdata);
                }
        }
}

/**
 * Set the album cover image. This must be done in a thread to avoid
 * freezing the UI.
 * @param data A pointer to getcover_data
 * @return NULL (not used)
 */
static gpointer
set_album_cover_thread(gpointer data)
{
        getcover_data *d = (getcover_data *) data;
        g_return_val_if_fail(d != NULL && d->image_url != NULL, NULL);
        char *buffer = NULL;
        size_t bufsize = 0;
        http_get_buffer(d->image_url, &buffer, &bufsize);
        if (buffer == NULL) g_warning("Error getting cover image");
        gdk_threads_enter();
        if (mainwin && nowplaying && nowplaying->id == d->track_id) {
                mainwin_set_album_cover(mainwin, (guchar *) buffer, bufsize);
        }
        gdk_threads_leave();
        g_free(buffer);
        g_free(d->image_url);
        g_free(d);
        return NULL;
}

/**
 * Get a new playlist and append it to the end of the current
 * one. Show an error if no playlist is found, start playing
 * otherwise. This can take a bit, so it is done in a separate thread
 * to avoid freezing the UI.
 *
 * @param data A copy of the lastfm_session used to get the new
 *             playlist.
 * @return NULL (not used)
 */
static gpointer
start_playing_get_pls_thread(gpointer data)
{
        lastfm_session *s = (lastfm_session *) data;
        g_return_val_if_fail(s != NULL && usercfg != NULL, NULL);
        lastfm_pls *pls = lastfm_request_playlist(s, usercfg->discovery_mode);
        gdk_threads_enter();
        if (pls == NULL) {
                controller_stop_playing();
                controller_show_info("No more content to play");
        } else {
                lastfm_pls_merge(playlist, pls);
                lastfm_pls_destroy(pls);
                controller_start_playing();
        }
        gdk_threads_leave();
        lastfm_session_destroy(s);
        return NULL;
}

/**
 * Download the cover of the track being played and show it in the
 * main window. This will create a thread to avoid freezing the UI.
 * If the window is not visible or if the cover is already displayed
 * (or about to be displayed) this will do nothing so this function
 * can be called several times.
 */
void
controller_show_cover(void)
{
        g_return_if_fail(mainwin != NULL);
        if (showing_cover || nowplaying == NULL || mainwin->is_hidden) return;
        showing_cover = TRUE;
        if (nowplaying->image_url != NULL) {
                getcover_data *d = g_new(getcover_data, 1);
                d->track_id = nowplaying->id;
                d->image_url = g_strdup(nowplaying->image_url);
                g_thread_create(set_album_cover_thread, d, FALSE, NULL);
        } else {
                mainwin_set_album_cover(mainwin, NULL, 0);
        }
}

/**
 * Callback to be called by the audio component when it actually
 * starts playing
 */
static void
controller_audio_started_cb(void)
{
        g_return_if_fail(mainwin != NULL && nowplaying != NULL);
        lastfm_track *track;
        nowplaying_since = time(NULL);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_PLAYING, nowplaying);
        track = lastfm_track_ref(nowplaying);
        controller_show_progress(track);
        showing_cover = FALSE;
        controller_show_cover();
        g_timeout_add(1000, controller_show_progress, track);
}

/**
 * Play the next track from the playlist, getting a new playlist if
 * necessary, see start_playing_get_pls_thread().
 *
 * This is the success callback of controller_start_playing()
 *
 * @param userdata Not used
 */
static void
controller_start_playing_cb(gpointer userdata)
{
        lastfm_track *track = NULL;
        g_return_if_fail(mainwin && playlist && nowplaying == NULL);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_CONNECTING, NULL);
        if (lastfm_pls_size(playlist) == 0) {
                lastfm_session *s = lastfm_session_copy(session);
                g_thread_create(start_playing_get_pls_thread,s,FALSE,NULL);
                return;
        }
        track = lastfm_pls_get_track(playlist);
        controller_set_nowplaying(track);
        if (track->custom_pls) {
                lastfm_audio_play(track->stream_url,
                                  (GCallback) controller_audio_started_cb,
                                  session->id);
        } else {
                lastfm_audio_play(track->stream_url,
                                  (GCallback) controller_audio_started_cb,
                                  NULL);
        }
}

/**
 * Play the next track from the playlist, getting a new playlist if
 * necessary, see start_playing_get_pls_thread().
 * This only calls check_session() the actual code is in
 * controller_start_playing_cb()
 */
void
controller_start_playing(void)
{
        if (stopping_after_track) {
                controller_stop_playing();
        } else {
                check_session_cb cb;
                cb = (check_session_cb) controller_start_playing_cb;
                check_session(cb, NULL, NULL);
        }
}

/**
 * Stop the track being played (if any) and scrobbles it. To be called
 * only from controller_stop_playing() and controller_skip_track()
 */
static void
finish_playing_track(void)
{
        g_return_if_fail(mainwin != NULL);
        if (nowplaying != NULL) {
                controller_scrobble_track();
                controller_set_nowplaying(NULL);
        }
        lastfm_audio_stop();
}

/**
 * Stop the track being played, updating the UI as well
 */
void
controller_stop_playing(void)
{
        g_return_if_fail(mainwin != NULL);
        lastfm_ui_state new_state = session != NULL ?
                LASTFM_UI_STATE_STOPPED :
                LASTFM_UI_STATE_DISCONNECTED;
        mainwin_set_ui_state(mainwin, new_state, NULL);
        finish_playing_track();
        stopping_after_track = FALSE;

        /* Notify the playback status */
        lastfm_dbus_notify_playback(NULL);
}

/**
 * Stop the track being played and immediately play the next one.
 */
void
controller_skip_track(void)
{
        g_return_if_fail(mainwin != NULL);
        finish_playing_track();
        controller_start_playing();
}

/**
 * Stop the track being played and disconnect from Last.fm, clearing
 * the active session (if any)
 */
void
controller_disconnect(void)
{
        if (session != NULL) {
                lastfm_session_destroy(session);
                session = NULL;
        }
        if (usercfg != NULL) {
                set_rsp_session(usercfg->username, NULL);
        }
        controller_stop_playing();
}

/**
 * Download the currently play track (if it's free)
 */
void
controller_download_track(void)
{
        g_return_if_fail(nowplaying && nowplaying->free_track_url && usercfg);
        lastfm_track *t = lastfm_track_ref(nowplaying);
        if (controller_confirm_dialog("Download this track?")) {
                char *filename, *dstpath;
                gboolean download = TRUE;
                filename = g_strconcat(t->artist, " - ", t->title, ".mp3",
                                       NULL);
                dstpath = g_strconcat(usercfg->download_dir, "/", filename,
                                       NULL);
                if (file_exists(dstpath)) {
                        download = controller_confirm_dialog("File exists. "
                                                             "Overwrite?");
                }
                if (download) {
                        dlwin_download_file(t->free_track_url, filename,
                                            dstpath);
                }
                g_free(filename);
                g_free(dstpath);
        } else {
                lastfm_track_unref(t);
        }
}

/**
 * Tells Vagalume to stop (or not) after the current track
 * @param stop Whether to stop or not
 */
void
controller_set_stop_after(gboolean stop)
{
	stopping_after_track = stop;
}

/**
 * Mark the currently playing track as a loved track. This will be
 * used when scrobbling it.
 */
void
controller_love_track(void)
{
        g_return_if_fail(nowplaying != NULL && mainwin != NULL);
        if (controller_confirm_dialog("Really mark track as loved?")) {
                nowplaying_rating = RSP_RATING_LOVE;
                controller_show_banner("Marking track as loved");
                mainwin_set_track_as_loved(mainwin);
        }
}

/**
 * Ban this track, marking it as banned and skipping it
 */
void
controller_ban_track(void)
{
        g_return_if_fail(nowplaying != NULL);
        if (controller_confirm_dialog("Really ban this track?")) {
                nowplaying_rating = RSP_RATING_BAN;
                controller_show_banner("Banning track");
                controller_skip_track();
        }
}

/**
 * Tag a track. This can take some seconds, so it must be called using
 * g_thread_create() to avoid freezing the UI.
 *
 * @param data Pointer to tag_data, with info about the track to be
 *             tagged. This data must be freed here
 * @return NULL (this value is not used)
 */
static gpointer
tag_track_thread(gpointer data)
{
        tag_data *d = (tag_data *) data;
        g_return_val_if_fail(d && d->track && d->taglist, NULL);
        gboolean tagged = FALSE;
        char *user = NULL, *pass = NULL;
        gdk_threads_enter();
        if (usercfg != NULL) {
                user = g_strdup(usercfg->username);
                pass = g_strdup(usercfg->password);
        }
        gdk_threads_leave();
        if (user != NULL && pass != NULL) {
                GSList *list = NULL;
                char **tags = g_strsplit(d->taglist, ",", 0);
                int i;
                for (i = 0; tags[i] != NULL; i++) {
                        list = g_slist_append(list, g_strstrip(tags[i]));
                }
                tagged = tag_track(user, pass, d->track, d->type, list);
                g_strfreev(tags);
                g_slist_free(list);
                g_free(user);
                g_free(pass);
        }
        lastfm_track_unref(d->track);
        g_free(d->taglist);
        g_free(d);
        gdk_threads_enter();
        if (mainwin && mainwin->window) {
                controller_show_banner(tagged ? "Tags set correctly" :
                                       "Error tagging");
        }
        gdk_threads_leave();
        return NULL;
}

/**
 * Ask the user a list of tags and tag the current artist, track or
 * album (yes, the name of the function is misleading but I can't
 * think of a better one)
 * @param type The type of tag (artist, track, album)
 */
void
controller_tag_track()
{
        g_return_if_fail(mainwin && usercfg && nowplaying);
        /* Keep this static to remember the previous value */
        static request_type type = REQUEST_ARTIST;
        char *tags = NULL;
        lastfm_track *track = lastfm_track_ref(nowplaying);
        gboolean accept;
        if (track->album[0] == '\0' && type == REQUEST_ALBUM) {
                type = REQUEST_ARTIST;
        }
        accept = tagwin_run(mainwin->window, usercfg->username, &tags,
                            usertags, track, &type);
        if (accept && tags != NULL && tags[0] != '\0') {
                tag_data *d = g_new0(tag_data, 1);
                d->track = track;
                d->taglist = tags;
                d->type = type;
                g_thread_create(tag_track_thread,d,FALSE,NULL);
        } else {
                if (accept) {
                        controller_show_info("You must type a list of tags");
                }
                lastfm_track_unref(track);
        }
}

/**
 * Recommend a track. This can take some seconds, so it must be called
 * using g_thread_create() to avoid freezing the UI.
 *
 * @param data Pointer to recomm_data, with info about the track to be
 *             recommended. This data must be freed here
 * @return NULL (this value is not used)
 */
static gpointer
recomm_track_thread(gpointer data)
{
        recomm_data *d = (recomm_data *) data;
        g_return_val_if_fail(d && d->track && d->rcpt && d->text, NULL);
        gboolean retval = FALSE;
        char *user = NULL, *pass = NULL;
        gdk_threads_enter();
        if (usercfg != NULL) {
                user = g_strdup(usercfg->username);
                pass = g_strdup(usercfg->password);
        }
        gdk_threads_leave();
        if (user != NULL && pass != NULL) {
                retval = recommend_track(user, pass, d->track, d->text,
                                         d->type, d->rcpt);
                g_free(user);
                g_free(pass);
        }
        gdk_threads_enter();
        if (mainwin && mainwin->window) {
                controller_show_banner(retval ?
                                       "Recommendation sent" :
                                       "Error sending recommendation");
        }
        gdk_threads_leave();
        lastfm_track_unref(d->track);
        g_free(d->rcpt);
        g_free(d->text);
        g_free(d);
        return NULL;
}

/**
 * Ask the user a recipient and a message and recommend the current
 * artist, track or album (chosen by the user)
 */
void
controller_recomm_track(void)
{
        g_return_if_fail(usercfg != NULL && nowplaying != NULL);
        char *rcpt = NULL;
        char *body = NULL;
        /* Keep this static to remember the previous value */
        static request_type type = REQUEST_TRACK;
        lastfm_track *track = lastfm_track_ref(nowplaying);
        gboolean accept;
        if (track->album[0] == '\0' && type == REQUEST_ALBUM) {
                type = REQUEST_ARTIST;
        }
        accept = recommwin_run(mainwin->window, &rcpt, &body, friends,
                               track, &type);
        if (accept && rcpt && body && rcpt[0] && body[0]) {
                g_strstrip(rcpt);
                recomm_data *d = g_new0(recomm_data, 1);
                d->track = track;
                d->rcpt = rcpt;
                d->text = body;
                d->type = type;
                g_thread_create(recomm_track_thread,d,FALSE,NULL);
        } else {
                if (accept) {
                        controller_show_info("You must type a user name\n"
                                             "and a recommendation message.");
                }
                lastfm_track_unref(track);
                g_free(rcpt);
                g_free(body);
        }
}

/**
 * Add a track to the user's playlist. This can take some seconds, so
 * it must be called using g_thread_create() to avoid freezing the UI.
 *
 * @param data Pointer to the lastfm_track to add. This data must be
 *             freed here
 * @return NULL (this value is not used)
 */
gpointer
add_to_playlist_thread(gpointer data)
{
        lastfm_track *t = (lastfm_track *) data;
        g_return_val_if_fail(t != NULL, NULL);
        gboolean retval = FALSE;
        char *user = NULL, *pass = NULL;
        gdk_threads_enter();
        if (usercfg != NULL) {
                user = g_strdup(usercfg->username);
                pass = g_strdup(usercfg->password);
        }
        gdk_threads_leave();
        if (user != NULL && pass != NULL) {
                retval = add_to_playlist(user, pass, t);
                g_free(user);
                g_free(pass);
        }
        gdk_threads_enter();
        if (mainwin && mainwin->window) {
                controller_show_banner(retval ?
                                       "Track added to playlist" :
                                       "Error adding track to playlist");
        }
        gdk_threads_leave();
        lastfm_track_unref(t);
        return NULL;
}

/**
 * Add the now-playing track to the user's playlist, the one in the
 * Last.fm website: http://www.last.fm/user/USERNAME/playlist/
 */
void
controller_add_to_playlist(void)
{
        g_return_if_fail(usercfg != NULL && nowplaying != NULL);
        if (ui_confirm_dialog(mainwin->window,
                              "Really add this track to the playlist?")) {
                lastfm_track *track = lastfm_track_ref(nowplaying);
                g_thread_create(add_to_playlist_thread,track,FALSE,NULL);
        }
}

/**
 * Start playing a radio by its URL, stopping the current track if
 * necessary
 *
 * This is the success callback of controller_play_radio_by_url()
 *
 * @param url The URL of the radio to be played (freed by this function)
 */
static void
controller_play_radio_by_url_cb(char *url)
{
        if (url == NULL) {
                g_critical("Attempted to play a NULL radio URL");
                controller_stop_playing();
        } else if (lastfm_radio_url_is_custom(url)) {
                lastfm_pls *pls = lastfm_request_custom_playlist(session, url);
                if (pls != NULL) {
                        lastfm_pls_destroy(playlist);
                        playlist = pls;
                        controller_skip_track();
                } else {
                        controller_stop_playing();
                        controller_show_info("Invalid radio URL");
                }
        } else if (lastfm_set_radio(session, url)) {
                lastfm_pls_clear(playlist);
                controller_skip_track();
        } else {
                controller_stop_playing();
                controller_show_info("Invalid radio URL. Either\n"
                                     "this radio doesn't exist\n"
                                     "or it is only available\n"
                                     "for Last.fm subscribers");
        }
        g_free(url);
}

/**
 * Start playing a radio by its URL, stopping the current track if
 * necessary
 * This only calls check_session() the actual code is in
 * controller_play_radio_by_url_cb()
 *
 * @param url The URL of the radio to be played
 */
void
controller_play_radio_by_url(const char *url)
{
        check_session_cb cb;
        cb = (check_session_cb) controller_play_radio_by_url_cb;
        check_session(cb, (check_session_cb) g_free, g_strdup(url));
}

/**
 * Start playing a radio by its type. In all cases it will be the
 * radio of the user running the application, not someone else's radio
 *
 * This is the success callback of controller_play_radio()
 *
 * @param userdata The radio type (passed as a gpointer)
 */
static void
controller_play_radio_cb(gpointer userdata)
{
        lastfm_radio type = GPOINTER_TO_INT(userdata);
        char *url = NULL;
        if (type == LASTFM_RECOMMENDED_RADIO) {
                url = lastfm_recommended_radio_url(
                        usercfg->username, 100);
        } else if (type == LASTFM_USERTAG_RADIO) {
                static char *previous = NULL;
                char *tag;
                tag = ui_input_dialog_with_list(mainwin->window, "Enter tag",
                                                "Enter one of your tags",
                                                usertags, previous);
                if (tag != NULL) {
                        url = lastfm_usertag_radio_url(usercfg->username, tag);
                        /* Store the new value for later use */
                        g_free(previous);
                        previous = tag;
                }
        } else {
                url = lastfm_radio_url(type, usercfg->username);
        }
        if (url != NULL) {
                controller_play_radio_by_url(url);
                g_free(url);
        }
}

/**
 * Start playing a radio by its type. In all cases it will be the
 * radio of the user running the application, not someone else's radio
 *
 * This only calls check_session() the actual code is in
 * controller_play_radio_cb()
 *
 * @param type Radio type
 */
void
controller_play_radio(lastfm_radio type)
{
        check_session_cb cb;
        cb = (check_session_cb) controller_play_radio_cb;
        check_session(cb, NULL, GINT_TO_POINTER(type));
}

/**
 * Start playing other user's radio by its type. It will pop up a
 * dialog to ask the user whose radio is going to be played
 *
 * This is the success callback of controller_play_radio()
 *
 * @param userdata The radio type (passed as a gpointer)
 */
static void
controller_play_others_radio_cb(gpointer userdata)
{
        lastfm_radio type = GPOINTER_TO_INT(userdata);
        static char *previous = NULL;
        char *url = NULL;
        char *user = ui_input_dialog_with_list(mainwin->window,
                                               "Enter user name",
                                               "Play this user's radio",
                                               friends,
                                               previous);
        if (user != NULL) {
                url = lastfm_radio_url(type, user);
                controller_play_radio_by_url(url);
                g_free(url);
                /* Store the new value for later use */
                g_free(previous);
                previous = user;
        }
}

/**
 * Start playing other user's radio by its type. It will pop up a
 * dialog to ask the user whose radio is going to be played
 *
 * This only calls check_session() the actual code is in
 * controller_play_others_radio_cb()
 *
 * @param type Radio type
 */
void
controller_play_others_radio(lastfm_radio type)
{
        check_session_cb cb;
        cb = (check_session_cb) controller_play_others_radio_cb;
        check_session(cb, NULL, GINT_TO_POINTER(type));
}

/**
 * Open a dialog asking for a group and play its radio
 */
void
controller_play_group_radio(void)
{
        g_return_if_fail(mainwin != NULL);
        static char *previous = NULL;
        char *url = NULL;
        char *group = ui_input_dialog(mainwin->window, "Enter group",
                                      "Enter group name", previous);
        if (group != NULL) {
                url = lastfm_radio_url(LASTFM_GROUP_RADIO, group);
                controller_play_radio_by_url(url);
                g_free(url);
                /* Store the new value for later use */
                g_free(previous);
                previous = group;
        }
}

/**
 * Open a dialog asking for a global tag and play its radio
 */
void
controller_play_globaltag_radio(void)
{
        g_return_if_fail(mainwin != NULL);
        static char *previous = NULL;
        char *url = NULL;
        char *tag;
        tag = ui_input_dialog_with_list(mainwin->window, "Enter tag",
                                        "Enter a global tag",
                                        usertags, previous);
        if (tag != NULL) {
                url = lastfm_radio_url(LASTFM_GLOBALTAG_RADIO, tag);
                controller_play_radio_by_url(url);
                g_free(url);
                /* Store the new value for later use */
                g_free(previous);
                previous = tag;
        }
}


/**
 * Open a dialog asking for an artist and play its similar artists'
 * radio
 */
void
controller_play_similarartist_radio(void)
{
        g_return_if_fail(mainwin != NULL);
        static char *previous = NULL;
        char *url = NULL;
        char *artist = ui_input_dialog(mainwin->window, "Enter artist",
                                       "Enter an artist's name", previous);
        if (artist != NULL) {
                url = lastfm_radio_url(LASTFM_SIMILAR_ARTIST_RADIO, artist);
                controller_play_radio_by_url(url);
                g_free(url);
                /* Store the new value for later use */
                g_free(previous);
                previous = artist;
        }
}

/**
 * Open a dialog asking for a radio URL and play it
 */
void
controller_play_radio_ask_url(void)
{
        g_return_if_fail(mainwin != NULL);
        static char *previous = NULL;
        char *url = NULL;
        url = ui_input_dialog(mainwin->window, "Enter radio URL",
                              "Enter the URL of the Last.fm radio",
                              previous ? previous : "lastfm://");
        if (url != NULL) {
                if (!strncmp(url, "lastfm://", 9)) {
                        controller_play_radio_by_url(url);
                        /* Store the new value for later use */
                        g_free(previous);
                        previous = url;
                } else {
                        controller_show_info("Last.fm radio URLs must start "
                                             "with lastfm://");
                        g_free(url);
                }
        }
}

/**
 * Increase (or decrease) the audio volume
 * @param inc Level of increase
 */
void
controller_increase_volume(int inc)
{
        if (inc != 0) {
                char *text;
                int newvol = lastfm_audio_increase_volume(inc);
                text = g_strdup_printf("Volume: %d/100", newvol);
                controller_show_banner(text);
                g_free(text);
        }
}

/**
 * Close the application
 */
void
controller_quit_app(void)
{
        controller_stop_playing();
        mainwin_quit_app();
}

/**
 * Start running the application, initializing all of its
 * subcomponents and finally letting the main window take control.
 *
 * @param win The program's main window.
 * @param radio_url If not NULL, this radio will be played
 *                  automatically from the start. Tipically set in the
 *                  command line
 */
void
controller_run_app(lastfm_mainwin *win, const char *radio_url)
{
        g_return_if_fail(win != NULL && mainwin == NULL);
        const char *errmsg;
        mainwin = win;
        mainwin_show_window(mainwin, TRUE);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_DISCONNECTED, NULL);

        http_init();
        check_usercfg(FALSE);
        playlist = lastfm_pls_new();

        if (!lastfm_audio_init()) {
                controller_show_error("Error initializing audio system");
                return;
        }
        errmsg = lastfm_dbus_init();
        if (!errmsg) {
                errmsg = connection_init();
        }
        if (errmsg) {
                controller_show_error(errmsg);
                return;
        }
        if (radio_url) {
                controller_play_radio_by_url(radio_url);
        }

        lastfm_dbus_notify_started();

        mainwin_run_app();

        lastfm_dbus_notify_closing();

        lastfm_session_destroy(session);
        session = NULL;
        rsp_session_destroy(rsp_sess);
        rsp_sess = NULL;
        lastfm_pls_destroy(playlist);
        playlist = NULL;
        lastfm_mainwin_destroy(mainwin);
        mainwin = NULL;
        if (usercfg != NULL) {
                set_user_tag_list(usercfg->username, NULL);
                set_friend_list(usercfg->username, NULL);
                lastfm_usercfg_destroy(usercfg);
                usercfg = NULL;
        }
        lastfm_audio_clear();
        lastfm_dbus_close();
}
