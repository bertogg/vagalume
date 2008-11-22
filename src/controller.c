/*
 * controller.c -- Where the control of the program is
 * Copyright (C) 2007, 2008 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib.h>
#include <glib/gstdio.h>
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
#include "imstatus.h"
#include "vgl-bookmark-mgr.h"
#include "vgl-bookmark-window.h"

#ifdef HAVE_TRAY_ICON
#include "vgl-tray-icon.h"
#endif

#ifdef MAEMO
#include <libosso.h>
#endif

G_DEFINE_TYPE (VglController, vgl_controller, G_TYPE_OBJECT);

static VglController *vgl_controller = NULL;

enum {
        CONNECTED,
        DISCONNECTED,
        TRACK_STARTED,
        TRACK_STOPPED,
        PLAYER_STOPPED,
        USERCFG_CHANGED,
        N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static LastfmSession *session = NULL;
static LastfmPls *playlist = NULL;
static RspSession *rsp_sess = NULL;
static VglMainWindow *mainwin = NULL;
static VglUserCfg *usercfg = NULL;
static GList *friends = NULL;
static GList *usertags = NULL;
static LastfmTrack *nowplaying = NULL;
static time_t nowplaying_since = 0;
static char *current_radio_url = NULL;
static RspRating nowplaying_rating = RSP_RATING_NONE;
static gboolean showing_cover = FALSE;
static gboolean stop_after_this_track = FALSE;
static gboolean shutting_down = FALSE;

#ifdef HAVE_TRAY_ICON
static VglTrayIcon *tray_icon = NULL;
#endif

typedef struct {
        LastfmTrack *track;
        RspRating rating;
        time_t start;
} rsp_data;

typedef struct {
        LastfmTrack *track;
        char *taglist;                /* comma-separated list of tags */
        request_type type;
} tag_data;

typedef struct {
        LastfmTrack *track;
        char *rcpt;                  /* Recipient of the recommendation */
        char *text;                  /* text of the recommendation */
        request_type type;
} recomm_data;

typedef struct {
        LastfmTrack *track;
        char *dstpath;
} DownloadData;

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
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        ui_error_dialog(vgl_main_window_get_window(mainwin, TRUE), text);
}

/**
 * Show a warning dialog with an OK button
 *
 * @param text The text to show
 */
void
controller_show_warning(const char *text)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        ui_warning_dialog(vgl_main_window_get_window(mainwin, TRUE), text);
}

/**
 * Show an info dialog with an OK button
 *
 * @param text The text to show
 */
void
controller_show_info(const char *text)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        ui_info_dialog(vgl_main_window_get_window(mainwin, TRUE), text);
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
        ui_info_banner(vgl_main_window_get_window(mainwin, FALSE), text);
#else
        vgl_main_window_show_status_msg(mainwin, text);
#endif
}

/**
 * Show the about dialog
 */
void
controller_show_about (void)
{
        ui_about_dialog (vgl_main_window_get_window (mainwin, FALSE));
}

/**
 * Show an OK/cancel dialog to request confirmation from the user
 *
 * @param text The text to show
 * @return TRUE if the user selects OK, cancel otherwise
 */
gboolean
controller_confirm_dialog(const char *text, gboolean show_always)
{
        g_return_val_if_fail(VGL_IS_MAIN_WINDOW(mainwin), FALSE);

        if (!show_always && usercfg->disable_confirm_dialogs)
                return TRUE;

        return ui_confirm_dialog(vgl_main_window_get_window(mainwin, FALSE),
                                 text);
}

/**
 * Show/Hide the program main window
 *
 * @param show Whether to show or hide it
 */
void
controller_show_mainwin(gboolean show)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        vgl_main_window_show(mainwin, show);
}

/**
 * Iconify/deiconify the program main window
 *
 * @param iconify Whether to iconify or not (deiconify)
 */
void
controller_toggle_mainwin_visibility (void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        vgl_main_window_toggle_visibility(mainwin);
}

/**
 * Calculate the amount of time that a track has been playing and
 * updates the UI (progressbars, etc). to reflect that. To be called
 * using g_timeout_add()
 *
 * @param data A pointer to the LastfmTrack being played
 * @return TRUE if the track hasn't fininished yet, FALSE otherwise
 */
static gboolean
controller_show_progress(gpointer data)
{
        LastfmTrack *tr = (LastfmTrack *) data;
        g_return_val_if_fail(VGL_IS_MAIN_WINDOW(mainwin) && tr, FALSE);
        if (nowplaying != NULL && tr->id == nowplaying->id) {
                guint played = lastfm_audio_get_running_time();
                if (played != -1) {
                        guint length = nowplaying->duration/1000;
                        vgl_main_window_show_progress(mainwin,length,played);
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
LastfmTrack *
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
set_rsp_session(const char *user, RspSession *sess)
{
        g_return_if_fail(user != NULL && usercfg != NULL);
        if (!strcmp(user, usercfg->username)) {
                if (rsp_sess != NULL) rsp_session_destroy(rsp_sess);
                rsp_sess = sess;
        }
}

/**
 * Renews the RSP session. Must be called from a thread.
 */
static void
renew_rsp_session (void)
{
        char *user, *pass;
        RspSession *session;

        g_return_if_fail (usercfg != NULL);

        gdk_threads_enter ();
        user = g_strdup (usercfg->username);
        pass = g_strdup (usercfg->password);
        gdk_threads_leave ();

        g_debug ("Renewing RSP session ...");
        session = rsp_session_new (user, pass, NULL);
        if (session != NULL) {
                gdk_threads_enter ();
                set_rsp_session (user, session);
                gdk_threads_leave ();
        }

        g_free (user);
        g_free (pass);
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
        RspSession *s = NULL;
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
                RspResponse ret;
                ret = rsp_scrobble (s, d->track, d->start, d->rating);
                if (ret == RSP_RESPONSE_BADSESSION) {
                        rsp_session_destroy (s);
                        renew_rsp_session ();
                        gdk_threads_enter ();
                        s = rsp_session_copy (rsp_sess);
                        gdk_threads_leave ();
                        rsp_scrobble (s, d->track, d->start, d->rating);
                }
                rsp_session_destroy (s);
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
        g_slice_free(rsp_data, d);
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
                rsp_data *d = g_slice_new0(rsp_data);
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
        RspSession *s = NULL;
        gboolean set_np = FALSE;
        g_usleep(10 * G_USEC_PER_SEC);
        gdk_threads_enter();
        if (nowplaying && nowplaying->id == d->track->id && rsp_sess) {
                s = rsp_session_copy(rsp_sess);
                set_np = TRUE;
        }
        gdk_threads_leave();
        if (set_np) {
                RspResponse ret = rsp_set_nowplaying (s, d->track);
                if (ret == RSP_RESPONSE_BADSESSION) {
                        rsp_session_destroy (s);
                        renew_rsp_session ();
                        gdk_threads_enter ();
                        s = rsp_session_copy (rsp_sess);
                        gdk_threads_leave ();
                        rsp_set_nowplaying (s, d->track);
                }
                rsp_session_destroy (s);
        }
        lastfm_track_unref(d->track);
        g_slice_free(rsp_data, d);
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
controller_set_nowplaying(LastfmTrack *track)
{
        g_return_if_fail(usercfg != NULL);
        if (nowplaying != NULL) {
                lastfm_track_unref(nowplaying);
        }
        nowplaying = track;
        nowplaying_since = 0;
        nowplaying_rating = RSP_RATING_NONE;
        if (track != NULL && usercfg->enable_scrobbling) {
                rsp_data *d = g_slice_new0(rsp_data);
                d->track = lastfm_track_ref(track);
                d->start = 0;
                g_thread_create(set_nowplaying_thread,d,FALSE,NULL);
        }
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
        RspSession *sess = NULL;
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
        if (nowplaying != NULL) {
                im_set_status(usercfg, nowplaying);
        }
        g_signal_emit (vgl_controller, signals[USERCFG_CHANGED], 0, usercfg);
#ifdef HAVE_TRAY_ICON
        if (tray_icon != NULL) {
                vgl_tray_icon_show_notifications (
                        tray_icon, usercfg->show_notifications);
        }
#endif
}

/**
 * Open the user settings dialog and save the new settings to the
 * configuration file. If the username or password have been modified,
 * close the current session.
 */
void
controller_open_usercfg(void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        gboolean userchanged = FALSE;
        gboolean pwchanged = FALSE;
        gboolean changed;
        char *olduser = usercfg != NULL ? g_strdup(usercfg->username) :
                                          g_strdup("");
        char *oldpw = usercfg != NULL ? g_strdup(usercfg->password) :
                                        g_strdup("");

        changed = ui_usercfg_window(
                vgl_main_window_get_window(mainwin, FALSE), &usercfg);

        if (changed && usercfg != NULL) {
                vgl_user_cfg_write(usercfg);
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
        if (usercfg == NULL) usercfg = vgl_user_cfg_read();
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
        LastfmErr err = LASTFM_ERR_NONE;
        LastfmSession *s;
        data = (check_session_thread_data *) userdata;
        s = lastfm_session_new(data->user, data->pass, &err);
        if (s == NULL || s->id == NULL) {
                gdk_threads_enter();
                controller_disconnect();
                if (err == LASTFM_ERR_LOGIN) {
                        controller_show_warning(_("Unable to login to Last.fm\n"
                                                  "Check username and password"));
                } else {
                        controller_show_warning(_("Network connection error"));
                }
                gdk_threads_leave();
        } else {
                gdk_threads_enter();
                session = s;
                g_signal_emit (vgl_controller, signals[CONNECTED], 0);
                vgl_main_window_set_state (mainwin,
                                           VGL_MAIN_WINDOW_STATE_STOPPED,
                                           NULL, NULL);
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
        g_slice_free(check_session_thread_data, data);
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
                        data = g_slice_new(check_session_thread_data);
                        data->user = g_strdup(usercfg->username);
                        data->pass = g_strdup(usercfg->password);
                        data->success_cb = success_cb;
                        data->failure_cb = failure_cb;
                        data->cbdata = cbdata;
                        connection_go_online(check_session_conn_cb, data);
                        vgl_main_window_set_state (
                                mainwin, VGL_MAIN_WINDOW_STATE_CONNECTING,
                                NULL, NULL);
                } else {
                        controller_disconnect();
                        controller_show_warning(_("You need to enter your "
                                                  "Last.fm\nusername and "
                                                  "password to be able\n"
                                                  "to use this program."));
                        if (failure_cb != NULL) (*failure_cb)(cbdata);
                }
        }
}

/**
 * Set the album cover image. This must be done in a thread to avoid
 * freezing the UI.
 * @param data A pointer to the LastfmTrack
 * @return NULL (not used)
 */
static gpointer
set_album_cover_thread(gpointer data)
{
        LastfmTrack *t = (LastfmTrack *) data;
        g_return_val_if_fail(t != NULL && t->image_url != NULL, NULL);
        lastfm_get_track_cover_image(t);
        if (t->image_data == NULL) g_warning("Error getting cover image");
        gdk_threads_enter();
        if (mainwin && nowplaying == t) {
                vgl_main_window_set_album_cover(mainwin, t->image_data,
                                        t->image_data_size);
        }
        gdk_threads_leave();
        lastfm_track_unref(t);
        return NULL;
}

/**
 * Get a new playlist and append it to the end of the current
 * one. Show an error if no playlist is found, start playing
 * otherwise. This can take a bit, so it is done in a separate thread
 * to avoid freezing the UI.
 *
 * @param data A copy of the LastfmSession used to get the new
 *             playlist.
 * @return NULL (not used)
 */
static gpointer
start_playing_get_pls_thread(gpointer data)
{
        LastfmSession *s = (LastfmSession *) data;
        g_return_val_if_fail(s != NULL && usercfg != NULL, NULL);
        LastfmPls *pls = lastfm_request_playlist(s, usercfg->discovery_mode);
        gdk_threads_enter();
        if (pls == NULL) {
                controller_stop_playing();
                controller_show_info(_("No more content to play"));
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
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        if (showing_cover || nowplaying == NULL ||
            vgl_main_window_is_hidden(mainwin)) return;
        showing_cover = TRUE;
        if (nowplaying->image_url != NULL) {
                g_thread_create(set_album_cover_thread,
                                lastfm_track_ref(nowplaying), FALSE, NULL);
        } else {
                vgl_main_window_set_album_cover(mainwin, NULL, 0);
        }
}

/**
 * Callback to be called by the audio component when it actually
 * starts playing
 */
static void
controller_audio_started_cb(void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin) && nowplaying);
        LastfmTrack *track;
        nowplaying_since = time(NULL);
        vgl_main_window_set_state (mainwin, VGL_MAIN_WINDOW_STATE_PLAYING,
                                   nowplaying, current_radio_url);
        track = lastfm_track_ref(nowplaying);
        controller_show_progress(track);
        im_set_status(usercfg, track);
        showing_cover = FALSE;
        controller_show_cover();
        g_timeout_add_seconds (1, controller_show_progress, track);
        g_signal_emit (vgl_controller, signals[TRACK_STARTED], 0, nowplaying);
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
        LastfmTrack *track = NULL;
        g_return_if_fail(mainwin && playlist && nowplaying == NULL);
        vgl_main_window_set_state (mainwin, VGL_MAIN_WINDOW_STATE_CONNECTING,
                                   NULL, NULL);
        if (lastfm_pls_size(playlist) == 0) {
                LastfmSession *s = lastfm_session_copy(session);
                g_thread_create(start_playing_get_pls_thread,s,FALSE,NULL);
                return;
        }
        track = lastfm_pls_get_track(playlist);
        controller_set_nowplaying(track);

        if (usercfg->autodl_free_tracks) {
                controller_download_track (TRUE);
        }

        /* Notify the playback status */
        lastfm_dbus_notify_playback(track);

#ifdef HAVE_TRAY_ICON
        if (tray_icon) {
                vgl_tray_icon_notify_playback (tray_icon, track);
        }
#endif

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
        if (stop_after_this_track) {
                controller_stop_playing();
        } else {
                check_session_cb cb;
                cb = (check_session_cb) controller_start_playing_cb;
                check_session(cb, NULL, NULL);
        }
}

/**
 * Stop the track being played (if any) and scrobbles it. To be called
 * only from controller_stop_playing(), controller_skip_track() and
 * controller_play_radio_by_url()
 */
static void
finish_playing_track(void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        if (nowplaying != NULL) {
                controller_scrobble_track();
                controller_set_nowplaying(NULL);
        }
        lastfm_audio_stop();
        g_signal_emit (vgl_controller, signals[TRACK_STOPPED], 0);
}

/**
 * Stop the track being played, updating the UI as well
 */
void
controller_stop_playing(void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        VglMainWindowState new_state = session != NULL ?
                VGL_MAIN_WINDOW_STATE_STOPPED :
                VGL_MAIN_WINDOW_STATE_DISCONNECTED;

        /* Updating the window title just before destroying the window
         * causes a crash, at least in the Moblin platform */
        if (!shutting_down)
                vgl_main_window_set_state (mainwin, new_state, NULL, NULL);

        finish_playing_track();
        stop_after_this_track = FALSE;
        im_clear_status();

        /* Notify the playback status */
        lastfm_dbus_notify_playback(NULL);

#ifdef HAVE_TRAY_ICON
        if (tray_icon) {
                vgl_tray_icon_notify_playback (tray_icon, NULL);
        }
#endif
        g_signal_emit (vgl_controller, signals[PLAYER_STOPPED], 0);
}

/**
 * Stop the track being played and immediately play the next one.
 */
void
controller_skip_track(void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
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
        lastfm_pls_clear(playlist);
        controller_stop_playing();
        g_signal_emit (vgl_controller, signals[DISCONNECTED], 0);
}

/**
 * Download a file in background. Must be called using g_thread_create()
 */
static gpointer
download_track_thread (gpointer userdata)
{
        DownloadData *d = (DownloadData *) userdata;
        gboolean success;
        char *text;

        success = http_download_file (d->track->free_track_url, d->dstpath,
                                      NULL, NULL);
        if (success) {
                text = g_strdup_printf (_("Downloaded %s - %s"),
                                        d->track->artist, d->track->title);
        } else {
                g_unlink (d->dstpath);
                text = g_strdup_printf (_("Error downloading %s - %s"),
                                        d->track->artist, d->track->title);
        }

        d->track->dl_in_progress = FALSE;

        gdk_threads_enter ();
        controller_show_banner (text);
        gdk_threads_leave ();

        g_free (text);
        lastfm_track_unref (d->track);
        g_free (d->dstpath);
        g_slice_free (DownloadData, d);

        return NULL;
}

/**
 * Callback executed when dlwin_download_file() finishes
 */
static void
download_track_dlwin_cb (gboolean success, gpointer userdata)
{
        LastfmTrack *track = (LastfmTrack *) userdata;
        track->dl_in_progress = FALSE;
        lastfm_track_unref (track);
}

/**
 * Download the current track (if it's free)
 *
 * @param background If TRUE, start immediately in the background,
 *                   without using a download window
 */
void
controller_download_track (gboolean background)
{
        g_return_if_fail(nowplaying && nowplaying->free_track_url && usercfg);
        LastfmTrack *t = lastfm_track_ref(nowplaying);

        if (t->dl_in_progress) {
                const char *msg = _("Track already being downloaded");
                if (!background) {
                        controller_show_info (msg);
                }
        } else if (background ||
                   controller_confirm_dialog (_("Download this track?"),
                                              FALSE)) {
                char *filename, *dstpath;
                gboolean download = TRUE;
                filename = g_strconcat(t->artist, " - ", t->title, ".mp3",
                                       NULL);
                dstpath = g_strconcat(usercfg->download_dir, "/", filename,
                                       NULL);
                if (file_exists(dstpath)) {
                        if (background) {
                                download = FALSE;
                        } else {
                                download = controller_confirm_dialog (
                                        _("File exists. " "Overwrite?"), TRUE);
                        }
                }
                if (download) {
                        g_debug ("Downloading %s to file %s",
                                 t->free_track_url, dstpath);
                        t->dl_in_progress = TRUE;
                        if (background) {
                                char *banner;
                                DownloadData *dldata;
                                dldata = g_slice_new (DownloadData);
                                dldata->track = lastfm_track_ref (t);
                                dldata->dstpath = g_strdup (dstpath);
                                g_thread_create (download_track_thread,
                                                 dldata, FALSE, NULL);
                                banner = g_strdup_printf (
                                        _("Downloading %s - %s"),
                                        t->artist, t->title);
                                controller_show_banner (banner);
                                g_free (banner);
                        } else {
                                dlwin_download_file (t->free_track_url,
                                                     filename, dstpath,
                                                     download_track_dlwin_cb,
                                                     lastfm_track_ref (t));
                        }
                }
                g_free(filename);
                g_free(dstpath);
        }
        lastfm_track_unref(t);
}

/**
 * Opens the bookmarks window
 */
void
controller_manage_bookmarks(void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        vgl_bookmark_window_show(vgl_main_window_get_window(mainwin, FALSE));
}

/*
 * Bookmark the current track
 */
void controller_add_bookmark (BookmarkType type)
{
        g_return_if_fail (VGL_IS_MAIN_WINDOW (mainwin));
        g_return_if_fail (nowplaying != NULL || (type == BOOKMARK_TYPE_EMPTY));
        char *name, *url;
        const char *banner;
        VglBookmarkMgr *mgr = vgl_bookmark_mgr_get_instance();
        if (type == BOOKMARK_TYPE_ARTIST) {
                g_return_if_fail(nowplaying->artistid != 0);
                name = g_strdup(nowplaying->artist);
                url = g_strdup_printf("lastfm://play/artists/%u",
                                      nowplaying->artistid);
                banner = _("Artist added to bookmarks");
        } else if (type == BOOKMARK_TYPE_TRACK) {
                g_return_if_fail(nowplaying->id != 0);
                name = g_strdup_printf("%s - '%s'", nowplaying->artist,
                                       nowplaying->title);
                url = g_strdup_printf("lastfm://play/tracks/%u",
                                      nowplaying->id);
                banner = _("Track added to bookmarks");
        } else if (type == BOOKMARK_TYPE_CURRENT_RADIO) {
                g_return_if_fail (nowplaying->pls_title != NULL &&
                                  current_radio_url != NULL);
                name = g_strdup (nowplaying->pls_title);
                url = g_strdup (current_radio_url);
                banner = _("Current radio added to bookmarks");
        } else if (type == BOOKMARK_TYPE_EMPTY) {
                name = url = NULL;
                banner = _("Bookmark added");
        } else {
                g_critical("Bookmark request not supported");
                return;
        }
        if (ui_edit_bookmark_dialog(vgl_main_window_get_window(mainwin, FALSE),
                                    &name, &url, TRUE)) {
                vgl_bookmark_mgr_add_bookmark(mgr, name, url);
                vgl_bookmark_mgr_save_to_disk (mgr, TRUE);
                controller_show_banner(banner);
        }
        g_free(name);
        g_free(url);
}

/**
 * Stops playing a song after a given timeout. This function is called
 * from controller_set_stop_after().
 */
static gboolean
stop_after_timeout (gpointer data)
{
        guint *source_id = (guint *) data;
        controller_stop_playing();
        *source_id = 0;
        return FALSE;
}

/**
 * Asks the user whether to stop playback after this track of a given
 * time.
 */
void
controller_set_stop_after (void)
{
        /* If source_id != 0, playback will stop at source_stop_at (approx) */
        static guint source_id = 0;
        static time_t source_stop_at;
        int minutes;
        StopAfterType stopafter;

        /* Get current values */
        if (stop_after_this_track) {
                stopafter = STOP_AFTER_THIS_TRACK;
        } else if (source_id != 0) {
                stopafter = STOP_AFTER_N_MINUTES;
                minutes = (source_stop_at - time (NULL) + 59) / 60;
                if (minutes <= 0) minutes = 1;
        } else {
                stopafter = STOP_AFTER_DONT_STOP;
        }

        if (ui_stop_after_dialog (
                    vgl_main_window_get_window (mainwin, FALSE),
                    &stopafter, &minutes)) {
                /* Clear previous values */
                if (source_id != 0) {
                        g_source_remove (source_id);
                        source_id = 0;
                }
                stop_after_this_track = FALSE;

                /* Set new values */
                if (stopafter == STOP_AFTER_N_MINUTES) {
                        g_return_if_fail (minutes > 0);
                        source_id = g_timeout_add_seconds
                                (minutes * 60, stop_after_timeout, &source_id);
                        source_stop_at = time (NULL) + minutes * 60;
                } else if (stopafter == STOP_AFTER_THIS_TRACK) {
                        stop_after_this_track = TRUE;
                } else if (stopafter != STOP_AFTER_DONT_STOP) {
                        g_return_if_reached ();
                }
        }
}

/**
 * Mark the currently playing track as a loved track. This will be
 * used when scrobbling it.
 * @param interactive If called interactively
 */
void
controller_love_track(gboolean interactive)
{
        g_return_if_fail(nowplaying != NULL && VGL_IS_MAIN_WINDOW(mainwin));
        if (!interactive ||
            controller_confirm_dialog(_("Really mark track as loved?"),FALSE)) {
                nowplaying_rating = RSP_RATING_LOVE;
                vgl_main_window_set_track_as_loved(mainwin);
                if (interactive) {
                        controller_show_banner(_("Marking track as loved"));
                }
        }
}

/**
 * Ban this track, marking it as banned and skipping it
 * @param interactive If called interactively
 */
void
controller_ban_track(gboolean interactive)
{
        g_return_if_fail(nowplaying != NULL);
        if (!interactive ||
            controller_confirm_dialog(_("Really ban this track?"), FALSE)) {
                nowplaying_rating = RSP_RATING_BAN;
                controller_skip_track();
                if (interactive) {
                        controller_show_banner(_("Banning track"));
                }
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
        g_slice_free(tag_data, d);
        gdk_threads_enter();
        if (mainwin) {
                controller_show_banner(tagged ? _("Tags set correctly") :
                                       _("Error tagging"));
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
        LastfmTrack *track = lastfm_track_ref(nowplaying);
        gboolean accept;
        if (track->album[0] == '\0' && type == REQUEST_ALBUM) {
                type = REQUEST_ARTIST;
        }
        accept = tagwin_run(vgl_main_window_get_window(mainwin, FALSE),
                            usercfg->username, &tags,
                            usertags, track, &type);
        if (accept) {
                tag_data *d = g_slice_new0(tag_data);
                if (tags == NULL) {
                        tags = g_strdup("");
                }
                d->track = track;
                d->taglist = tags;
                d->type = type;
                g_thread_create(tag_track_thread,d,FALSE,NULL);
        } else {
                if (accept) {
                        controller_show_info(_("You must type a list of tags"));
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
        if (mainwin) {
                controller_show_banner(retval ?
                                       _("Recommendation sent") :
                                       _("Error sending recommendation"));
        }
        gdk_threads_leave();
        lastfm_track_unref(d->track);
        g_free(d->rcpt);
        g_free(d->text);
        g_slice_free(recomm_data, d);
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
        LastfmTrack *track = lastfm_track_ref(nowplaying);
        gboolean accept;
        if (track->album[0] == '\0' && type == REQUEST_ALBUM) {
                type = REQUEST_ARTIST;
        }

        accept = recommwin_run(vgl_main_window_get_window(mainwin, FALSE),
                               &rcpt, &body, friends, track, &type);
        if (accept && rcpt && body && rcpt[0] && body[0]) {
                g_strstrip(rcpt);
                recomm_data *d = g_slice_new0(recomm_data);
                d->track = track;
                d->rcpt = rcpt;
                d->text = body;
                d->type = type;
                g_thread_create(recomm_track_thread,d,FALSE,NULL);
        } else {
                if (accept) {
                        controller_show_info(_("You must type a user name\n"
                                               "and a recommendation message."));
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
 * @param data Pointer to the LastfmTrack to add. This data must be
 *             freed here
 * @return NULL (this value is not used)
 */
gpointer
add_to_playlist_thread(gpointer data)
{
        LastfmTrack *t = (LastfmTrack *) data;
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
        if (mainwin) {
                controller_show_banner(retval ?
                                       _("Track added to playlist") :
                                       _("Error adding track to playlist"));
                vgl_main_window_set_track_as_added_to_playlist(mainwin,
                                                               retval);
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
        if (controller_confirm_dialog(
                    _("Really add this track to the playlist?"), FALSE)) {
                LastfmTrack *track = lastfm_track_ref(nowplaying);
                vgl_main_window_set_track_as_added_to_playlist(mainwin, TRUE);
                g_thread_create(add_to_playlist_thread,track,FALSE,NULL);
        }
}

/**
 * Start playing a radio by its URL, stopping the current track if
 * necessary
 *
 * This is a thread created from controller_play_radio_by_url_cb()
 *
 * @param data The URL of the radio to be played (freed by this function)
 * @return NULL (not used)
 */
static gpointer
controller_play_radio_by_url_thread(gpointer data)
{
        char *url = (char *) data;
        LastfmSession *sess;
        gdk_threads_enter();
        if (!VGL_IS_MAIN_WINDOW(mainwin) || session == NULL) {
                g_critical("Main window destroyed or session not found");
                gdk_threads_leave();
                return NULL;
        }
        sess = lastfm_session_copy(session);
        if (url == NULL) {
                g_critical("Attempted to play a NULL radio URL");
                controller_stop_playing();
        } else if (lastfm_radio_url_is_custom(url)) {
                LastfmPls *pls;
                gdk_threads_leave();
                pls = lastfm_request_custom_playlist(sess, url);
                gdk_threads_enter();
                if (pls != NULL) {
                        lastfm_pls_destroy(playlist);
                        playlist = pls;
                        g_free (current_radio_url);
                        current_radio_url = g_strdup (url);
                        controller_skip_track();
                } else {
                        controller_stop_playing();
                        controller_show_info(_("Invalid radio URL"));
                }
        } else {
                gboolean radio_set;
                gdk_threads_leave();
                radio_set = lastfm_set_radio(sess, url);
                gdk_threads_enter();
                if (radio_set) {
                        g_free (current_radio_url);
                        current_radio_url = g_strdup (url);
                        lastfm_pls_clear(playlist);
                        controller_skip_track();
                } else {
                        controller_stop_playing();
                        controller_show_info(_("Invalid radio URL. Either\n"
                                               "this radio doesn't exist\n"
                                               "or it is only available\n"
                                               "for Last.fm subscribers"));
                }
        }
        gdk_threads_leave();
        g_free(url);
        lastfm_session_destroy(sess);
        return NULL;
}

/**
 * Start playing a radio by its URL, stopping the current track if
 * necessary
 *
 * This is the success callback of controller_play_radio_by_url().
 * This just creates a thread, the actual code is in
 * controller_play_radio_by_url_thread()
 *
 * @param url The URL of the radio to be played (freed by this function)
 */
static void
controller_play_radio_by_url_cb(char *url)
{
        g_thread_create(controller_play_radio_by_url_thread, url, FALSE, NULL);
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
        finish_playing_track();
        vgl_main_window_set_state (mainwin, VGL_MAIN_WINDOW_STATE_CONNECTING,
                                   NULL, NULL);
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
                tag = ui_input_dialog_with_list(
                        vgl_main_window_get_window(mainwin, TRUE),
                        _("Enter tag"), _("Enter one of your tags"),
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
        char *user = ui_input_dialog_with_list(vgl_main_window_get_window(mainwin,
                                                                  TRUE),
                                               _("Enter user name"),
                                               _("Play this user's radio"),
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
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        static char *previous = NULL;
        char *url = NULL;
        char *group = ui_input_dialog(
                vgl_main_window_get_window(mainwin, TRUE),
                _("Enter group"), _("Enter group name"), previous);
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
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        static char *previous = NULL;
        char *url = NULL;
        char *tag;
        tag = ui_input_dialog_with_list(
                vgl_main_window_get_window(mainwin, TRUE),
                _("Enter tag"), _("Enter a global tag"), usertags, previous);
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
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        static char *previous = NULL;
        char *url = NULL;
        char *artist = ui_input_dialog(
                vgl_main_window_get_window(mainwin, TRUE),
                _("Enter artist"), _("Enter an artist's name"), previous);
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
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        static char *previous = NULL;
        char *url = NULL;
        url = ui_input_dialog(vgl_main_window_get_window(mainwin, TRUE),
                              _("Enter radio URL"),
                              _("Enter the URL of the Last.fm radio"),
                              previous ? previous : "lastfm://");
        if (url != NULL) {
                if (!strncmp(url, "lastfm://", 9)) {
                        controller_play_radio_by_url(url);
                        /* Store the new value for later use */
                        g_free(previous);
                        previous = url;
                } else {
                        controller_show_info(_("Last.fm radio URLs must start "
                                               "with lastfm://"));
                        g_free(url);
                }
        }
}

/**
 * Increase (or decrease) the audio volume
 * @param inc Level of increase
 * @return The new level
 */
int
controller_increase_volume(int inc)
{
        int newvol;
        if (inc != 0) {
                newvol = lastfm_audio_increase_volume(inc);
        } else {
                newvol = lastfm_audio_get_volume();
        }
        return newvol;
}

/**
 * Set the audio volume level
 * @param vol New volume level
 */
void
controller_set_volume(int vol)
{
        lastfm_audio_set_volume (vol);
}

/**
 * Close the application
 */
void
controller_quit_app(void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        shutting_down = TRUE;
        controller_stop_playing();
        vgl_main_window_destroy(mainwin);
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
controller_run_app (const char *radio_url)
{
        g_return_if_fail (mainwin == NULL);
        const char *errmsg = NULL;
#ifdef MAEMO
        osso_context_t *osso_context = NULL;
#endif
        DbusInitReturnCode dbuscode = lastfm_dbus_init ();
        if (dbuscode == DBUS_INIT_ERROR) {
                errmsg = _("Unable to initialize DBUS");
        } else if (dbuscode == DBUS_INIT_ALREADY_RUNNING) {
                if (radio_url) {
                        g_debug ("Playing radio URL %s in running instance",
                                 radio_url);
                        lastfm_dbus_play_radio_url (radio_url);
                }
                return;
        }

        vgl_controller = g_object_new (VGL_TYPE_CONTROLLER, NULL);
        g_object_add_weak_pointer (G_OBJECT (vgl_controller),
                                   (gpointer) &vgl_controller);

        mainwin = VGL_MAIN_WINDOW (vgl_main_window_new ());
        g_object_add_weak_pointer (G_OBJECT (mainwin), (gpointer) &mainwin);
        vgl_main_window_show(mainwin, TRUE);
        vgl_main_window_set_state (mainwin, VGL_MAIN_WINDOW_STATE_DISCONNECTED,
                                   NULL, NULL);

        http_init();
        check_usercfg(FALSE);
        playlist = lastfm_pls_new();

        if (!errmsg && !lastfm_audio_init()) {
                controller_show_error(_("Error initializing audio system"));
                return;
        }

#ifdef MAEMO
        /* Initialize osso context */
        if (!errmsg) {
                osso_context = osso_initialize (APP_NAME_LC,
                                                APP_VERSION, FALSE, NULL);
                if (!osso_context) {
                        errmsg = _("Unable to initialize OSSO context");
                }
        }
#endif
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

#ifdef HAVE_TRAY_ICON
        /* Init Freedesktop tray icon */
        tray_icon = vgl_tray_icon_create ();
        g_object_add_weak_pointer (G_OBJECT (tray_icon), (gpointer) &tray_icon);
        vgl_tray_icon_notify_playback (tray_icon, NULL);
        if (usercfg != NULL) {
                vgl_tray_icon_show_notifications (
                        tray_icon, usercfg->show_notifications);
        }
#endif

        vgl_main_window_run_app();

        /* --- From here onwards the app shuts down --- */

        lastfm_dbus_notify_closing();

#ifdef HAVE_TRAY_ICON
        g_object_unref(tray_icon);
#endif
        lastfm_session_destroy(session);
        session = NULL;
        rsp_session_destroy(rsp_sess);
        rsp_sess = NULL;
        lastfm_pls_destroy(playlist);
        playlist = NULL;
        if (usercfg != NULL) {
                set_user_tag_list(usercfg->username, NULL);
                set_friend_list(usercfg->username, NULL);
                vgl_user_cfg_destroy(usercfg);
                usercfg = NULL;
        }
        lastfm_audio_clear();
        lastfm_dbus_close();
        vgl_bookmark_mgr_save_to_disk (vgl_bookmark_mgr_get_instance (), FALSE);

#ifdef MAEMO
        /* Cleanup OSSO context */
        osso_deinitialize(osso_context);
#endif

        g_object_unref (vgl_controller);
}

/**
 * Close the application (either to systray or shut it down)
 */
void
controller_close_mainwin(void)
{
#ifdef HAVE_TRAY_ICON
        if (usercfg->close_to_systray)
                controller_show_mainwin(FALSE);
        else
#endif
                controller_quit_app();
}

static void
vgl_controller_class_init (VglControllerClass *klass)
{
        signals[CONNECTED] =
                g_signal_new ("connected",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        signals[DISCONNECTED] =
                g_signal_new ("disconnected",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        signals[TRACK_STARTED] =
                g_signal_new ("track-started",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);

        signals[TRACK_STOPPED] =
                g_signal_new ("track-stopped",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        signals[PLAYER_STOPPED] =
                g_signal_new ("player-stopped",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        signals[USERCFG_CHANGED] =
                g_signal_new ("usercfg-changed",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
vgl_controller_init (VglController *self)
{
}
