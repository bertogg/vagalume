/*
 * controller.c -- Where the control of the program is
 *
 * Copyright (C) 2007-2011, 2013 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "marshal.h"
#include "connection.h"
#include "controller.h"
#include "scrobbler.h"
#include "protocol.h"
#include "playlist.h"
#include "audio.h"
#include "uimisc.h"
#include "dlwin.h"
#include "http.h"
#include "globaldefs.h"
#include "util.h"
#include "vgl-bookmark-mgr.h"
#include "vgl-bookmark-window.h"
#include "vgl-server.h"
#include "lastfm-ws.h"
#include "compat.h"

#ifdef SET_IM_STATUS
#include "imstatus.h"
#endif

#ifdef HAVE_DBUS_SUPPORT
#include "dbus.h"
#endif

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
        PLAYBACK_PROGRESS,
        N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static LastfmWsSession *session = NULL;
static LastfmPls *playlist = NULL;
static VglMainWindow *mainwin = NULL;
static VglUserCfg *usercfg = NULL;
static GList *friends = NULL;
static GList *usertags = NULL;
static LastfmTrack *nowplaying = NULL;
static char *current_radio_url = NULL;
static RspRating nowplaying_rating = RSP_RATING_NONE;
static gboolean showing_cover = FALSE;
static gboolean stop_after_this_track = FALSE;
static gboolean shutting_down = FALSE;

typedef struct {
        LastfmWsSession *session;
        LastfmTrack *track;
        char *taglist;                /* comma-separated list of tags */
        LastfmTrackComponent type;
} TagData;

typedef struct {
        LastfmWsSession *session;
        LastfmTrack *track;
        char *rcpt;                  /* Recipient of the recommendation */
        char *text;                  /* text of the recommendation */
        LastfmTrackComponent type;
} RecommData;

typedef struct {
        LastfmTrack *track;
        char *dstpath;
} DownloadData;

typedef struct {
        LastfmWsSession *session;
        gboolean discovery;
        gboolean lowbitrate;
} GetPlaylistData;

typedef struct {
        LastfmWsSession *session;
        LastfmTrack *track;
} AddToPlaylistData;

typedef struct {
        LastfmWsSession *session;
        char *url;
        LastfmErrorCode error_code;
} PlayRadioByUrlData;

/*
 * Callback called after check_session() in all cases.
 * data is user-provided data, and must be freed by the caller
 * Different callbacks must be supplied for success and failure
 */
typedef void (*check_session_cb)(gpointer data);

typedef struct {
        char *user;
        char *pass;
        VglServer *srv;
        LastfmWsSession *session;
        LastfmErr err;
        check_session_cb success_cb;
        check_session_cb failure_cb;
        gpointer cbdata;
} CheckSessionData;

/**
 * Show an error dialog with an OK button
 *
 * @param text The text to show
 */
void
controller_show_error                   (const char *text)
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
controller_show_warning                 (const char *text)
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
controller_show_info                    (const char *text)
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
controller_show_banner                  (const char *text)
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
controller_show_about                   (void)
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
controller_confirm_dialog               (const char *text,
                                         gboolean    show_always)
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
controller_show_mainwin                 (gboolean show)
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
controller_toggle_mainwin_visibility    (void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        vgl_main_window_toggle_visibility(mainwin);
}

/**
 * Calculate the amount of time that a track has been playing and emit
 * the 'playback-progress' signal. To be called using g_timeout_add()
 *
 * @param data A pointer to the LastfmTrack being played
 * @return TRUE if the track hasn't fininished yet, FALSE otherwise
 */
static gboolean
controller_update_progress              (gpointer data)
{
        LastfmTrack *tr = (LastfmTrack *) data;
        g_return_val_if_fail(VGL_IS_MAIN_WINDOW(mainwin) && tr, FALSE);
        if (nowplaying != NULL && tr->id == nowplaying->id) {
                guint played = lastfm_audio_get_running_time();
                if (played != -1) {
                        guint length = nowplaying->duration/1000;
                        g_signal_emit (vgl_controller,
                                       signals[PLAYBACK_PROGRESS], 0,
                                       played, length);
                }
                return TRUE;
        } else {
                vgl_object_unref(tr);
                return FALSE;
        }
}

/**
 * Gets the list of friends
 * @return The list
 */
const GList *
controller_get_friend_list              (void)
{
        return friends;
}

/**
 * Gets the track that is currently being played
 * @return The track
 */
LastfmTrack *
controller_get_current_track            (void)
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
set_friend_list                         (const char *user,
                                         GList      *list)
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
set_user_tag_list                       (const char *user,
                                         GList      *list)
{
        g_return_if_fail(user != NULL && usercfg != NULL);
        if (!strcmp(user, usercfg->username)) {
                g_list_foreach(usertags, (GFunc) g_free, NULL);
                g_list_free(usertags);
                usertags = list;
        }
}

/**
 * Set a track as "Now Playing"
 *
 * @param track The track to be set as Now Playing
 */
static void
controller_set_nowplaying               (LastfmTrack *track)
{
        if (nowplaying != NULL) {
                vgl_object_unref (nowplaying);
        }
        nowplaying = track;
        nowplaying_rating = RSP_RATING_NONE;
}

/**
 * Initializes an RSP session and get list of friends. This must be
 * called only from check_session_thread() !!
 *
 * @param data Not used
 * @return NULL (not used)
 */
static void
get_user_extradata                      (void)
{
        g_return_if_fail (usercfg != NULL);
        gboolean finished = FALSE;
        gboolean friends_ok = FALSE;
        gboolean usertags_ok = FALSE;
        GList *friends = NULL;
        GList *usertags = NULL;
        gdk_threads_enter();
        char *user = g_strdup(usercfg->username);
        char *pass = g_strdup(usercfg->password);
        VglServer *srv = vgl_object_ref (usercfg->server);
        gdk_threads_leave();
        if (!user || !pass || user[0] == '\0' || pass[0] == '\0') {
                finished = TRUE;
        }
        while (!finished) {
                if (!friends_ok) {
                        friends_ok = lastfm_ws_get_friends (srv, user,
                                                            &friends);
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
                        usertags_ok = lastfm_ws_get_user_tags (srv, user,
                                                               &usertags);
                        if (usertags_ok) {
                                g_debug("Tag list ready");
                                gdk_threads_enter();
                                set_user_tag_list(user, usertags);
                                gdk_threads_leave();
                        } else {
                                g_warning("Error getting tag list");
                        }
                }
                if (friends_ok && usertags_ok) {
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
        vgl_object_unref(srv);
}

/**
 * Apply all user cfg settings
 */
static void
apply_usercfg                           (void)
{
        g_return_if_fail(usercfg != NULL);
        if (usercfg->use_proxy) {
                http_set_proxy (usercfg->http_proxy,
                                usercfg->use_system_proxy);
        } else {
                http_set_proxy (NULL, FALSE);
        }
        g_signal_emit (vgl_controller, signals[USERCFG_CHANGED], 0, usercfg);
}

/**
 * Open the user settings dialog and save the new settings to the
 * configuration file. If the username or password have been modified,
 * close the current session.
 */
void
controller_open_usercfg                 (void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        gboolean userchanged = FALSE;
        gboolean pwchanged = FALSE;
        gboolean srvchanged = FALSE;
        gboolean bitratechanged = FALSE;
        gboolean changed;
        char *olduser = usercfg != NULL ? g_strdup(usercfg->username) :
                                          g_strdup("");
        char *oldpw = usercfg != NULL ? g_strdup(usercfg->password) :
                                        g_strdup("");
        VglServer *oldsrv = usercfg ? vgl_object_ref (usercfg->server) : NULL;
        gboolean oldbitrate = usercfg ? usercfg->low_bitrate : FALSE;

        changed = ui_usercfg_window(
                vgl_main_window_get_window(mainwin, FALSE), &usercfg);

        if (changed && usercfg != NULL) {
                vgl_user_cfg_write(usercfg);
                userchanged = strcmp(olduser, usercfg->username);
                pwchanged = strcmp(oldpw, usercfg->password);
                srvchanged = oldsrv != usercfg->server;
                bitratechanged = oldbitrate != usercfg->low_bitrate;
                apply_usercfg();
        }
        if (userchanged || pwchanged || srvchanged || bitratechanged) {
                if (userchanged || srvchanged) {
                        set_friend_list(usercfg->username, NULL);
                        set_user_tag_list(usercfg->username, NULL);
                }
                controller_disconnect();
        }
        g_free(olduser);
        g_free(oldpw);
        if (oldsrv) {
                vgl_object_unref (oldsrv);
        }
}

/**
 * Import an XML file with server data and merge it with the current
 * file
 */
void
controller_import_servers_file          (void)
{
        char *filename;
        g_return_if_fail (VGL_IS_MAIN_WINDOW (mainwin));
        filename = ui_select_servers_file (
                vgl_main_window_get_window (mainwin, FALSE));
        if (filename != NULL) {
                gboolean imported = vgl_server_import_file (filename);
                if (imported && usercfg != NULL) {
                        VglServer *srv;
                        controller_disconnect ();
                        srv = vgl_server_list_find_by_name (
                                usercfg->server->name);
                        g_return_if_fail (srv != NULL);
                        vgl_object_ref (srv);
                        vgl_object_unref (usercfg->server);
                        usercfg->server = srv;
                }
                if (imported) {
                        ui_info_dialog (
                                vgl_main_window_get_window (mainwin, TRUE),
                                _("Server info imported correctly"));
                } else {
                        ui_warning_dialog (
                                vgl_main_window_get_window (mainwin, TRUE),
                                _("No server info found in that file"));
                }
                g_free (filename);
        }
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
check_usercfg                           (gboolean ask)
{
        if (usercfg == NULL) {
                usercfg = vgl_user_cfg_read();
                if (usercfg == NULL && ask) {
                        controller_open_usercfg();
                }
                if (usercfg != NULL) {
                        apply_usercfg();
                }
        }
        return (usercfg != NULL);
}

/**
 * Proceed after creating (or trying to create) a new Last.fm session.
 *
 * @param userdata A pointer to a CheckSessionData struct
 * @return FALSE (to remove the idle handler)
 */
static gboolean
check_session_idle                      (gpointer userdata)
{
        CheckSessionData *data = userdata;
        const gboolean success = data->session != NULL;

        if (!success) {
                controller_disconnect();
                if (data->err == LASTFM_ERR_LOGIN) {
                        char *text = g_strdup_printf (
                                _("Unable to login to %s\n"
                                  "Check username and password"),
                                data->srv->name);
                        controller_show_warning (text);
                        g_free (text);
                } else {
                        controller_show_warning(_("Network connection error"));
                }
        } else {
                if (session) {
                        vgl_object_unref (session);
                }
                session = data->session;
                g_signal_emit (vgl_controller, signals[CONNECTED], 0,
                               data->session);
                vgl_main_window_set_state (mainwin,
                                           VGL_MAIN_WINDOW_STATE_STOPPED,
                                           NULL, NULL);
        }

        /* Call the callback */
        if (success && data->success_cb != NULL) {
                (*(data->success_cb)) (data->cbdata);
        } else if (!success && data->failure_cb != NULL) {
                (*(data->failure_cb)) (data->cbdata);
        }

        /* Free memory */
        g_free (data->user);
        g_free (data->pass);
        vgl_object_unref (data->srv);
        g_slice_free (CheckSessionData, data);

        return FALSE;
}

/**
 * Create a new Last.fm session. This is done in a thread to avoid
 * freezing the UI.
 *
 * @param userdata A pointer to a CheckSessionData struct
 * @return NULL (not used)
 */
static gpointer
check_session_thread                    (gpointer userdata)
{
        gboolean success;
        CheckSessionData *data = userdata;
        g_return_val_if_fail (data != NULL, NULL);

        data->session = lastfm_ws_get_session (data->srv, data->user,
                                               data->pass, &(data->err));
        success = (data->session != NULL);

        gdk_threads_add_idle (check_session_idle, data);

        if (success) {
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
check_session_conn_cb                   (gpointer data)
{
        g_thread_create(check_session_thread, data, FALSE, NULL);
}

/**
 * Check if there's a Last.fm session opened. If not, try to create
 * one.
 * The actual connection is performed in check_session_thread() to
 * avoid freezing the UI
 *
 * @param success_cb A callback in case this function succeeds
 * @param failure_cb A callback in case this function fails
 * @param cbdata Parameter that will be passed to both callbacks,
 *               must be freed by the caller (if necessary)
 */
static void
check_session                           (check_session_cb success_cb,
                                         check_session_cb failure_cb,
                                         gpointer         cbdata)
{
        /* Then the actual streaming session */
        if (session != NULL) {
                if (success_cb != NULL) (*success_cb)(cbdata);
        } else {
                check_usercfg (TRUE);
                if (usercfg != NULL) {
                        CheckSessionData *data;
                        data = g_slice_new (CheckSessionData);
                        data->user = g_strdup(usercfg->username);
                        data->pass = g_strdup(usercfg->password);
                        data->srv = vgl_object_ref(usercfg->server);
                        data->session = NULL;
                        data->err = LASTFM_ERR_NONE;
                        data->success_cb = success_cb;
                        data->failure_cb = failure_cb;
                        data->cbdata = cbdata;
                        connection_go_online(check_session_conn_cb, data);
                        vgl_main_window_set_state (
                                mainwin, VGL_MAIN_WINDOW_STATE_CONNECTING,
                                NULL, NULL);
                } else {
                        controller_disconnect();
                        controller_show_warning(_("You need to enter your\n"
                                                  "username and password\n"
                                                  "to be able "
                                                  "to use this program."));
                        if (failure_cb != NULL) (*failure_cb)(cbdata);
                }
        }
}

/**
 * Set the album cover image.
 * @param data A pointer to the LastfmTrack
 * @return FALSE (to remove the idle handler)
 */
static gboolean
set_album_cover_idle                    (gpointer data)
{
        LastfmTrack *track = data;
        if (mainwin && nowplaying == track) {
                vgl_main_window_set_album_cover (mainwin,
                                                 track->image_data,
                                                 track->image_data_size);
        }
        return FALSE;
}

/**
 * Retrieve the album cover image. This must be done in a thread to
 * avoid freezing the UI.
 * @param data A pointer to the LastfmTrack
 * @return NULL (not used)
 */
static gpointer
set_album_cover_thread                  (gpointer data)
{
        LastfmTrack *track = data;
        g_return_val_if_fail (track != NULL && track->image_url != NULL, NULL);

        lastfm_get_track_cover_image (track);
        if (track->image_data == NULL) {
                g_warning ("Error getting cover image");
        }

        gdk_threads_add_idle_full (G_PRIORITY_DEFAULT, set_album_cover_idle,
                                   track, vgl_object_unref);
        return NULL;
}

/**
 * Get a new playlist and append it to the end of the current
 * one. Show an error if no playlist is found, start playing
 * otherwise.
 *
 * @param data A pointer to the new playlist, or NULL
 * @return FALSE (to remove the idle handler)
 */
static gboolean
start_playing_get_pls_idle              (gpointer data)
{
        LastfmPls *pls = data;
        if (pls == NULL) {
                controller_stop_playing ();
                controller_show_info (_("No more content to play"));
        } else {
                lastfm_pls_merge (playlist, pls);
                lastfm_pls_destroy (pls);
                controller_start_playing ();
        }
        return FALSE;
}

/**
 * Get a new playlist. This can take a bit, so it is done in a
 * separate thread to avoid freezing the UI.
 *
 * @param data A pointer to the LastfmWsSession used to get the new
 *             playlist.
 * @return NULL (not used)
 */
static gpointer
start_playing_get_pls_thread            (gpointer data)
{
        GetPlaylistData *d = data;
        LastfmPls *pls;
        g_return_val_if_fail (d != NULL && d->session != NULL, NULL);

        pls = lastfm_ws_radio_get_playlist (d->session, d->discovery,
                                            d->lowbitrate, TRUE);
        gdk_threads_add_idle (start_playing_get_pls_idle, pls);

        vgl_object_unref (d->session);
        g_slice_free (GetPlaylistData, d);

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
controller_show_cover                   (void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        if (showing_cover || nowplaying == NULL ||
            vgl_main_window_is_hidden(mainwin)) return;
        showing_cover = TRUE;
        if (nowplaying->image_url != NULL && nowplaying->image_data == NULL) {
                g_thread_create (set_album_cover_thread,
                                 vgl_object_ref (nowplaying), FALSE, NULL);
        } else {
                vgl_main_window_set_album_cover (mainwin,
                                                 nowplaying->image_data,
                                                 nowplaying->image_data_size);
        }
}

/**
 * Callback to be called by the audio component when it actually
 * starts playing
 */
static void
controller_audio_started_cb             (void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin) && nowplaying);
        LastfmTrack *track;
        vgl_main_window_set_state (mainwin, VGL_MAIN_WINDOW_STATE_PLAYING,
                                   nowplaying, current_radio_url);
        track = vgl_object_ref (nowplaying);
        controller_update_progress (track);
        showing_cover = FALSE;
        controller_show_cover();
        gdk_threads_add_timeout_seconds (1, controller_update_progress, track);
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
controller_start_playing_cb             (gpointer userdata)
{
        LastfmTrack *track = NULL;
        LastfmSession *v1session;
        g_return_if_fail(mainwin && playlist && nowplaying == NULL);
        vgl_main_window_set_state (mainwin, VGL_MAIN_WINDOW_STATE_CONNECTING,
                                   NULL, NULL);
        if (lastfm_pls_size(playlist) == 0) {
                GetPlaylistData *data = g_slice_new (GetPlaylistData);
                data->session = vgl_object_ref (session);
                data->discovery = usercfg->discovery_mode;
                data->lowbitrate = usercfg->low_bitrate;
                g_thread_create (start_playing_get_pls_thread,
                                 data, FALSE, NULL);
                return;
        }
        track = lastfm_pls_get_track(playlist);
        controller_set_nowplaying(track);

        if (usercfg->autodl_free_tracks) {
                controller_download_track (TRUE);
        }

        v1session = lastfm_ws_session_get_v1_session (session);

        lastfm_audio_play(track->stream_url,
                          (GCallback) controller_audio_started_cb,
                          v1session ? v1session->id : NULL);
}

/**
 * Play the next track from the playlist, getting a new playlist if
 * necessary, see start_playing_get_pls_thread().
 * This only calls check_session() the actual code is in
 * controller_start_playing_cb()
 */
void
controller_start_playing                (void)
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
finish_playing_track                    (void)
{
        if (nowplaying != NULL) {
                RspRating rating = nowplaying_rating;
                controller_set_nowplaying(NULL);
                lastfm_audio_stop ();
                g_signal_emit (vgl_controller, signals[TRACK_STOPPED], 0,
                               rating);
        }
}

/**
 * Stop the track being played, updating the UI as well
 */
void
controller_stop_playing                 (void)
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

        g_signal_emit (vgl_controller, signals[PLAYER_STOPPED], 0);
}

/**
 * Stop the track being played and immediately play the next one.
 */
void
controller_skip_track                   (void)
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
controller_disconnect                   (void)
{
        if (session != NULL) {
                vgl_object_unref (session);
                session = NULL;
        }
        lastfm_pls_clear(playlist);
        controller_stop_playing();
        g_signal_emit (vgl_controller, signals[DISCONNECTED], 0);
}

/**
 * Idle handler to call controller_show_banner()
 *
 * @param data The banner text
 * @return FALSE (to remove the idle handler)
 */
static gboolean
show_banner_idle                        (gpointer data)
{
        if (mainwin) {
                controller_show_banner ((const char *) data);
        }
        return FALSE;
}

/**
 * Download a file in background. Must be called using g_thread_create()
 */
static gpointer
download_track_thread                   (gpointer userdata)
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
        gdk_threads_add_idle_full (G_PRIORITY_DEFAULT, show_banner_idle,
                                   text, g_free);

        d->track->dl_in_progress = FALSE;

        vgl_object_unref (d->track);
        g_free (d->dstpath);
        g_slice_free (DownloadData, d);

        return NULL;
}

/**
 * Callback executed when dlwin_download_file() finishes
 */
static void
download_track_dlwin_cb                 (gboolean success,
                                         gpointer userdata)
{
        LastfmTrack *track = (LastfmTrack *) userdata;
        track->dl_in_progress = FALSE;
        vgl_object_unref (track);
}

/**
 * Download the current track (if it's free)
 *
 * @param background If TRUE, start immediately in the background,
 *                   without using a download window
 */
void
controller_download_track               (gboolean background)
{
        g_return_if_fail(nowplaying && nowplaying->free_track_url && usercfg);
        LastfmTrack *t = vgl_object_ref (nowplaying);

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
                                dldata->track = vgl_object_ref (t);
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
                                                     vgl_object_ref (t));
                        }
                }
                g_free(filename);
                g_free(dstpath);
        }
        vgl_object_unref(t);
}

/**
 * Opens the bookmarks window
 */
void
controller_manage_bookmarks             (void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        vgl_bookmark_window_show(vgl_main_window_get_window(mainwin, FALSE));
}

/*
 * Bookmark the current track
 */
void controller_add_bookmark            (BookmarkType type)
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
stop_after_timeout                      (gpointer data)
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
controller_set_stop_after               (void)
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
                        source_id = gdk_threads_add_timeout_seconds
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
controller_love_track                   (gboolean interactive)
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
controller_ban_track                    (gboolean interactive)
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
 * @param data Pointer to TagData, with info about the track to be
 *             tagged. This data must be freed here
 * @return NULL (this value is not used)
 */
static gpointer
tag_track_thread                        (gpointer data)
{
        TagData *d = (TagData *) data;
        const char *text;
        gboolean tagged;
        GSList *list = NULL;
        char **tags;
        int i;

        g_return_val_if_fail (d && d->track && d->taglist, NULL);

        tags = g_strsplit(d->taglist, ",", 0);
        for (i = 0; tags[i] != NULL; i++) {
                list = g_slist_append(list, g_strstrip(tags[i]));
        }
        tagged = lastfm_ws_tag_track (d->session, d->track, d->type, list);

        /* Cleanup */
        g_strfreev(tags);
        g_slist_free(list);
        vgl_object_unref(d->session);
        vgl_object_unref(d->track);
        g_free(d->taglist);
        g_slice_free(TagData, d);

        text = tagged ? _("Tags set correctly") : _("Error tagging");
        gdk_threads_add_idle (show_banner_idle, (char *) text);

        return NULL;
}

/**
 * Ask the user a list of tags and tag the current artist, track or
 * album (yes, the name of the function is misleading but I can't
 * think of a better one)
 * @param type The type of tag (artist, track, album)
 */
void
controller_tag_track                    (void)
{
        /* Keep this static to remember the previous value */
        static LastfmTrackComponent type = LASTFM_TRACK_COMPONENT_ARTIST;
        char *tags = NULL;
        LastfmTrack *track;
        LastfmWsSession *sess;
        gboolean accept;

        g_return_if_fail (mainwin && usercfg && nowplaying);

        track = vgl_object_ref (nowplaying);
        sess = vgl_object_ref (session);
        if (track->album[0] == '\0' && type == LASTFM_TRACK_COMPONENT_ALBUM) {
                type = LASTFM_TRACK_COMPONENT_ARTIST;
        }
        accept = tagwin_run(vgl_main_window_get_window(mainwin, FALSE),
                            usercfg->username, &tags,
                            usertags, sess, track, &type);
        if (accept) {
                TagData *d = g_slice_new (TagData);
                if (tags == NULL) {
                        tags = g_strdup("");
                }
                d->session = sess;
                d->track = track;
                d->taglist = tags;
                d->type = type;
                g_thread_create(tag_track_thread,d,FALSE,NULL);
        } else {
                if (accept) {
                        controller_show_info(_("You must type a list of tags"));
                }
                vgl_object_unref(sess);
                vgl_object_unref(track);
        }
}

/**
 * Recommend a track. This can take some seconds, so it must be called
 * using g_thread_create() to avoid freezing the UI.
 *
 * @param data Pointer to RecommData, with info about the track to be
 *             recommended. This data must be freed here
 * @return NULL (this value is not used)
 */
static gpointer
recomm_track_thread                     (gpointer data)
{
        RecommData *d = (RecommData *) data;
        const char *text;
        gboolean retval;

        g_return_val_if_fail (d && d->session && d->track &&
                              d->rcpt && d->text, NULL);

        retval = lastfm_ws_share_track (d->session, d->track, d->text,
                                        d->type, d->rcpt);

        text = retval ?
                _("Recommendation sent") :
                _("Error sending recommendation");
        gdk_threads_add_idle (show_banner_idle, (char *) text);

        vgl_object_unref(d->track);
        vgl_object_unref(d->session);
        g_free(d->rcpt);
        g_free(d->text);
        g_slice_free(RecommData, d);

        return NULL;
}

/**
 * Ask the user a recipient and a message and recommend the current
 * artist, track or album (chosen by the user)
 */
void
controller_recomm_track                 (void)
{
        g_return_if_fail (nowplaying && session);
        char *rcpt = NULL;
        char *body = NULL;
        /* Keep this static to remember the previous value */
        static LastfmTrackComponent type = LASTFM_TRACK_COMPONENT_TRACK;
        LastfmTrack *track = vgl_object_ref(nowplaying);
        LastfmWsSession *sess = vgl_object_ref (session);
        gboolean accept;
        if (track->album[0] == '\0' && type == LASTFM_TRACK_COMPONENT_ALBUM) {
                type = LASTFM_TRACK_COMPONENT_ARTIST;
        }

        accept = recommwin_run(vgl_main_window_get_window(mainwin, FALSE),
                               &rcpt, &body, friends, track, &type);
        if (accept && rcpt && body && rcpt[0] && body[0]) {
                g_strstrip(rcpt);
                RecommData *d = g_slice_new (RecommData);
                d->session = sess;
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
                vgl_object_unref(track);
                vgl_object_unref(sess);
                g_free(rcpt);
                g_free(body);
        }
}

/**
 * Idle handler to update the UI after adding a track to the user's
 * playlist.
 *
 * @param data Boolean to tell whether the track was added or not
 * @return FALSE (to remove the idle handler)
 */
static gboolean
add_to_playlist_idle                    (gpointer data)
{
        if (mainwin) {
                gboolean added = GPOINTER_TO_INT (data);
                controller_show_banner (added ?
                                        _("Track added to playlist") :
                                        _("Error adding track to playlist"));
                vgl_main_window_set_track_as_added_to_playlist (mainwin,
                                                                added);
        }
        return FALSE;
}

/**
 * Add a track to the user's playlist. This can take some seconds, so
 * it must be called using g_thread_create() to avoid freezing the UI.
 *
 * @param data Pointer to an AddToPlaylistData structure.
 * @return NULL (this value is not used)
 */
static gpointer
add_to_playlist_thread                  (gpointer data)
{
        AddToPlaylistData *d = (AddToPlaylistData *) data;
        gboolean added;

        g_return_val_if_fail (d && d->session && d->track, NULL);

        added = lastfm_ws_add_to_playlist (d->session, d->track);
        gdk_threads_add_idle (add_to_playlist_idle, GINT_TO_POINTER (added));

        vgl_object_unref (d->session);
        vgl_object_unref (d->track);
        g_slice_free (AddToPlaylistData, d);

        return NULL;
}

/**
 * Add the now-playing track to the user's playlist, the one in the
 * Last.fm website: http://www.last.fm/user/USERNAME/playlist/
 */
void
controller_add_to_playlist              (void)
{
        LastfmTrack *track;
        LastfmWsSession *sess;

        g_return_if_fail (session != NULL && nowplaying != NULL);

        track = vgl_object_ref (nowplaying);
        sess = vgl_object_ref (session);

        if (controller_confirm_dialog(
                    _("Really add this track to the playlist?"), FALSE)) {
                AddToPlaylistData *data = g_slice_new (AddToPlaylistData);
                data->session = session;
                data->track = track;
                vgl_main_window_set_track_as_added_to_playlist (mainwin, TRUE);
                g_thread_create (add_to_playlist_thread, data, FALSE, NULL);
        } else {
                vgl_object_unref (track);
                vgl_object_unref (sess);
        }
}

/**
 * Idle handler to start/stop playback after a new radio has been set
 *
 * @param data Pointer to a PlayRadioByUrlData struct
 * @return FALSE (to remove the idle handler)
 */
static gboolean
controller_play_radio_by_url_idle       (gpointer data)
{
        PlayRadioByUrlData *d = data;
        const char *error_msg = NULL;

        switch (d->error_code) {
        case LASTFM_OK:
                g_free (current_radio_url);
                current_radio_url = d->url;
                lastfm_pls_clear (playlist);
                controller_skip_track ();
                break;
        case LASTFM_GEO_RESTRICTED:
                error_msg = _("This radio is not available\n"
                              "in your country.");
                break;
        default:
                error_msg = _("Invalid radio URL. Either\n"
                              "this radio doesn't exist\n"
                              "or it is only available\n"
                              "for Last.fm subscribers");
                break;
        }

        if (error_msg != NULL) {
                controller_stop_playing ();
                controller_show_info (error_msg);
                g_free (d->url);
        }

        vgl_object_unref (d->session);
        g_slice_free (PlayRadioByUrlData, d);
        return FALSE;
}

/**
 * Start playing a radio by its URL, stopping the current track if
 * necessary
 *
 * This is a thread created from controller_play_radio_by_url_cb()
 *
 * @param data Pointer to a PlayRadioByUrlData struct
 * @return NULL (not used)
 */
static gpointer
controller_play_radio_by_url_thread     (gpointer data)
{
        PlayRadioByUrlData *d = data;
        d->error_code = lastfm_ws_radio_tune (d->session, d->url,
                                              get_language_code ());
        gdk_threads_add_idle (controller_play_radio_by_url_idle, d);
        return NULL;
}

/**
 * Start playing a radio by its URL, stopping the current track if
 * necessary
 *
 * This is the success callback of controller_play_radio_by_url().
 *
 * @param url The URL of the radio to be played
 */
static void
controller_play_radio_by_url_cb         (char *url)
{
        if (!VGL_IS_MAIN_WINDOW (mainwin) || session == NULL) {
                g_critical ("Main window destroyed or session not found");
                g_free (url);
        } else if (url == NULL) {
                g_critical ("Attempted to play a NULL radio URL");
                controller_stop_playing ();
        } else {
                PlayRadioByUrlData *data = g_slice_new (PlayRadioByUrlData);
                data->session = vgl_object_ref (session);
                data->url = url;
                g_thread_create (controller_play_radio_by_url_thread,
                                 data, FALSE, NULL);
        }
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
controller_play_radio_by_url            (const char *url)
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
controller_play_radio_cb                (gpointer userdata)
{
        LastfmRadio type = GPOINTER_TO_INT (userdata);
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
controller_play_radio                   (LastfmRadio type)
{
        check_session_cb cb;
        cb = (check_session_cb) controller_play_radio_cb;
        check_session(cb, NULL, GINT_TO_POINTER(type));
}

/* controller_play_others_radio() is not needed in Maemo 5 */
#ifndef MAEMO5
/**
 * Start playing other user's radio by its type. It will pop up a
 * dialog to ask the user whose radio is going to be played
 *
 * This is a success callback of controller_play_radio()
 *
 * @param userdata The radio type (passed as a gpointer)
 */
static void
controller_play_others_radio_cb         (gpointer userdata)
{
        LastfmRadio type = GPOINTER_TO_INT (userdata);
        static char *previous = NULL;
        char *url = NULL;
        char *user = ui_input_dialog_with_list(vgl_main_window_get_window(mainwin,
                                                                  TRUE),
                                               _("Enter user name"),
                                               _("Play this user's radio"),
                                               friends,
                                               previous);
        if (user != NULL) {
                if (type == LASTFM_RECOMMENDED_RADIO) {
                        url = lastfm_recommended_radio_url (user, 100);
                } else {
                        url = lastfm_radio_url (type, user);
                }
                controller_play_radio_by_url(url);
                g_free(url);
                /* Store the new value for later use */
                g_free(previous);
                previous = user;
        }
}

/**
 * Start playing other user's tag radio. It will pop up a dialog to
 * ask the user and tag to select the radio to be played.
 *
 * This is a success callback of controller_play_radio()
 *
 * @param userdata Not used
 **/
static void
controller_play_usertag_radio_cb        (gpointer userdata)
{
        static char *previoususer = NULL;
        static char *previoustag = NULL;
        char *user = g_strdup (previoususer);
        char *tag = g_strdup (previoustag);
        ui_usertag_dialog (vgl_main_window_get_window (mainwin, TRUE),
                           &user, &tag, friends);
        if (user != NULL && tag != NULL) {
                char *url = lastfm_usertag_radio_url (user, tag);
                controller_play_radio_by_url (url);
                g_free (url);
                /* Store new values for later use */
                g_free (previoususer);
                g_free (previoustag);
                previoususer = user;
                previoustag = tag;
        } else {
                g_free (user);
                g_free (tag);
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
controller_play_others_radio            (LastfmRadio type)
{
        check_session_cb cb;
        if (type == LASTFM_USERTAG_RADIO) {
                cb = (check_session_cb) controller_play_usertag_radio_cb;
        } else {
                cb = (check_session_cb) controller_play_others_radio_cb;
        }
        check_session(cb, NULL, GINT_TO_POINTER(type));
}
#else /* ifndef MAEMO5 */

/**
 * Start playing other user's radio by its type and user name.
 *
 * @param username User name
 * @param type Radio type
 */
void
controller_play_others_radio_by_user    (const char  *username,
                                         LastfmRadio  type)
{
        char *url = NULL;
        g_return_if_fail (username != NULL && mainwin != NULL);

        if (type == LASTFM_USERTAG_RADIO) {
                static char *previous = NULL;
                char *tag = ui_input_dialog(
                        vgl_main_window_get_window (mainwin, TRUE),
                        _("Enter tag"), _("Enter a tag"), previous);
                if (tag != NULL) {
                        url = lastfm_usertag_radio_url (username, tag);
                        g_free (previous);
                        previous = tag;
                }
        } else {
                if (type == LASTFM_RECOMMENDED_RADIO) {
                        url = lastfm_recommended_radio_url (username, 100);
                } else {
                        url = lastfm_radio_url (type, username);
                }
        }
        if (url != NULL) {
                controller_play_radio_by_url (url);
                g_free (url);
        }
}
#endif /* MAEMO5 */

/**
 * Open a dialog asking for a group and play its radio
 */
void
controller_play_group_radio             (void)
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
controller_play_globaltag_radio         (void)
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
controller_play_similarartist_radio     (void)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(mainwin));
        static char *previous = NULL;
        char *url = NULL;
        char *artist = ui_input_dialog(
                vgl_main_window_get_window(mainwin, TRUE),
                _("Enter artist"), _("Enter an artist's name"),
                nowplaying ? nowplaying->artist : previous);
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
controller_play_radio_ask_url           (void)
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
controller_increase_volume              (int inc)
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
controller_set_volume                   (int vol)
{
        lastfm_audio_set_volume (vol);
}

/**
 * Close the application
 */
void
controller_quit_app                     (void)
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
controller_run_app                      (const char *radio_url)
{
        g_return_if_fail (mainwin == NULL);
        const char *errmsg = NULL;
#ifdef MAEMO
        osso_context_t *osso_context = NULL;
#endif

        vgl_controller = g_object_new (VGL_TYPE_CONTROLLER, NULL);
        g_object_add_weak_pointer (G_OBJECT (vgl_controller),
                                   (gpointer) &vgl_controller);

#ifdef HAVE_DBUS_SUPPORT
        switch (lastfm_dbus_init (vgl_controller)) {
        case DBUS_INIT_ERROR:
                errmsg = _("Unable to initialize DBUS");
                break;
        case DBUS_INIT_ALREADY_RUNNING:
                if (radio_url) {
                        g_debug ("Playing radio URL %s in running instance",
                                 radio_url);
                        lastfm_dbus_play_radio_url (radio_url);
                }
                return;
        default:
                break;
        }
#endif
#ifdef SET_IM_STATUS
        im_status_init (vgl_controller);
#endif

        mainwin = VGL_MAIN_WINDOW (vgl_main_window_new (vgl_controller));
        g_object_add_weak_pointer (G_OBJECT (mainwin), (gpointer) &mainwin);
        vgl_main_window_show(mainwin, TRUE);
        vgl_main_window_set_state (mainwin, VGL_MAIN_WINDOW_STATE_DISCONNECTED,
                                   NULL, NULL);

#ifdef HAVE_TRAY_ICON
        /* Init Freedesktop tray icon */
        vgl_tray_icon_create (vgl_controller);
#endif

        http_init();
        playlist = lastfm_pls_new();
        rsp_init (vgl_controller);

        if (!errmsg && !vgl_server_list_init()) {
                errmsg = _("Error reading the server list file");
        }

        if (!errmsg) {
                check_usercfg(FALSE);
        }

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

#ifdef HAVE_DBUS_SUPPORT
        lastfm_dbus_notify_started();
#endif

        vgl_main_window_run_app();

        /* --- From here onwards the app shuts down --- */

        if (session) {
                vgl_object_unref (session);
                session = NULL;
        }
        lastfm_pls_destroy(playlist);
        playlist = NULL;
        if (usercfg != NULL) {
                set_user_tag_list(usercfg->username, NULL);
                set_friend_list(usercfg->username, NULL);
                vgl_user_cfg_destroy(usercfg);
                usercfg = NULL;
        }
        lastfm_audio_clear();
        vgl_server_list_finalize ();
        vgl_bookmark_mgr_save_to_disk (vgl_bookmark_mgr_get_instance (), FALSE);

        g_object_unref (vgl_controller);

#ifdef MAEMO
        /* Cleanup OSSO context */
        osso_deinitialize(osso_context);
#endif
}

/**
 * Close the application (either to systray or shut it down)
 */
void
controller_close_mainwin                (void)
{
#ifdef HAVE_TRAY_ICON
        if (usercfg->close_to_systray)
                controller_show_mainwin(FALSE);
        else
#endif
                controller_quit_app();
}

static void
vgl_controller_class_init               (VglControllerClass *klass)
{
        signals[CONNECTED] =
                g_signal_new ("connected",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);

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
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1, G_TYPE_INT);

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

        signals[PLAYBACK_PROGRESS] =
                g_signal_new ("playback-progress",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              vgl_marshal_VOID__UINT_UINT,
                              G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
}

static void
vgl_controller_init                     (VglController *self)
{
}
