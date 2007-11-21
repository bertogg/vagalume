/*
 * controller.c -- Where the control of the program is
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#ifdef MAEMO
#include <libosso.h>
#endif

#include "controller.h"
#include "scrobbler.h"
#include "playlist.h"
#include "audio.h"
#include "uimisc.h"
#include "http.h"
#include "globaldefs.h"

static lastfm_session *session = NULL;
static lastfm_pls *playlist = NULL;
static rsp_session *rsp_sess = NULL;
static lastfm_mainwin *mainwin = NULL;
static lastfm_usercfg *usercfg = NULL;
static GList *friends = NULL;
static lastfm_track *nowplaying = NULL;
static time_t nowplaying_since = 0;
static rsp_rating nowplaying_rating = RSP_RATING_NONE;

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
        ui_info_banner(mainwin->window, text);
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
        return ui_confirm_dialog(mainwin->window, text);
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
                guint played = time(NULL) - nowplaying_since;
                guint length = nowplaying->duration/1000;
                mainwin_show_progress(mainwin, length, played);
                return TRUE;
        } else {
                lastfm_track_destroy(tr);
                return FALSE;
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
        lastfm_track_destroy(d->track);
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
                time_t played = time(NULL) - nowplaying_since;
                /* If a track is unrated and hasn't been played for
                   enough time, scrobble it as skipped */
                if (nowplaying_rating == RSP_RATING_NONE &&
                    played < nowplaying->duration/2000 && played < 240) {
                        nowplaying_rating = RSP_RATING_SKIP;
                }
                rsp_data *d = g_new0(rsp_data, 1);
                d->track = lastfm_track_copy(nowplaying);
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
        lastfm_track_destroy(d->track);
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
                lastfm_track_destroy(nowplaying);
        }
        nowplaying = track;
        nowplaying_since = 0;
        nowplaying_rating = RSP_RATING_NONE;
        if (track != NULL && usercfg->enable_scrobbling) {
                rsp_data *d = g_new0(rsp_data, 1);
                d->track = lastfm_track_copy(track);
                d->start = 0;
                g_thread_create(set_nowplaying_thread,d,FALSE,NULL);
        }
}

/**
 * Initializes an RSP session and get list of friends. This is done in
 * a thread to avoid freezing the UI.
 *
 * @param data Not used
 * @return NULL (not used)
 */
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
                GList *list;
                lastfm_get_friends(username, &list);
                gdk_threads_enter();
                rsp_session_destroy(rsp_sess);
                rsp_sess = s;
                g_list_foreach(friends, (GFunc) g_free, NULL);
                g_list_free(friends);
                friends = list;
                gdk_threads_leave();
        }
        g_free(username);
        g_free(password);
        return NULL;
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
        gboolean userpwchanged = FALSE;
        gboolean changed;
        char *olduser = usercfg != NULL ? g_strdup(usercfg->username) :
                                          g_strdup("");
        char *oldpw = usercfg != NULL ? g_strdup(usercfg->password) :
                                        g_strdup("");
        changed = ui_usercfg_dialog(mainwin->window, &usercfg);
        if (changed && usercfg != NULL) {
                write_usercfg(usercfg);
                if (strcmp(olduser, usercfg->username) ||
                    strcmp(oldpw, usercfg->password)) {
                        userpwchanged = TRUE;
                }
        }
        if (session != NULL && userpwchanged) {
                lastfm_session_destroy(session);
                session = NULL;
                controller_stop_playing();
                mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_DISCONNECTED,
                                     NULL);
        }
        g_free(olduser);
        g_free(oldpw);
}

/**
 * Check if the user settings exist (whether they are valid or
 * not). If they don't exist, read the config file or open the
 * settings dialog. This should only return FALSE the first time the
 * user runs the program.
 *
 * @return TRUE if the settings exist, FALSE otherwise
 */
static gboolean
check_usercfg(void)
{
        if (usercfg == NULL) usercfg = read_usercfg();
        if (usercfg == NULL) controller_open_usercfg();
        return (usercfg != NULL);
}

/**
 * Check if there's a Last.fm session opened. If not, try to create
 * one (and an RSP session as well).
 *
 * FIXME: This is done synchronously so it will freeze the UI during
 * its processing. However this will only happen once each time the
 * program is run, so fortunately it's not that critical.
 *
 * @return TRUE if a session exists, false otherwise
 */
static gboolean
check_session(void)
{
        if (session != NULL) return TRUE;
        lastfm_err err = LASTFM_ERR_NONE;
        gboolean retvalue = FALSE;
        check_usercfg();
        if (usercfg != NULL) {
                mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_CONNECTING,
                                     NULL);
                flush_ui_events();
                session = lastfm_session_new(usercfg->username,
                                             usercfg->password,
                                             &err);
        }
        if (session == NULL || session->id == NULL) {
                mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_DISCONNECTED,
                                     NULL);
                if (usercfg == NULL) {
                        controller_show_warning("You need to enter your "
                                                "Last.fm\nusername and "
                                                "password to be able\n"
                                                "to use this program.");
                } else if (err == LASTFM_ERR_LOGIN) {
                        controller_show_warning("Unable to login to Last.fm\n"
                                                "Check username and password");
                } else {
                        controller_show_warning("Network connection error");
                }
        } else {
                mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_STOPPED, NULL);
                retvalue = TRUE;
                if (rsp_sess == NULL) {
                        g_thread_create(rsp_session_init_thread, NULL,
                                        FALSE, NULL);
                }
        }
        return retvalue;
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
        track = lastfm_track_copy(nowplaying);
        controller_show_progress(track);
        g_timeout_add(1000, controller_show_progress, track);
}

/**
 * Play the next track from the playlist, getting a new playlist if
 * necessary, see start_playing_get_pls_thread().
 */
void
controller_start_playing(void)
{
        lastfm_track *track = NULL;
        g_return_if_fail(mainwin != NULL && playlist != NULL);
        if (!check_session()) return;
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_CONNECTING, NULL);
        if (lastfm_pls_size(playlist) == 0) {
                lastfm_session *s = lastfm_session_copy(session);
                g_thread_create(start_playing_get_pls_thread,s,FALSE,NULL);
                return;
        }
        track = lastfm_pls_get_track(playlist);
        controller_set_nowplaying(track);
        if (track->image_url != NULL) {
                getcover_data *d = g_new(getcover_data, 1);
                d->track_id = track->id;
                d->image_url = g_strdup(track->image_url);
                g_thread_create(set_album_cover_thread, d, FALSE, NULL);
        } else {
                mainwin_set_album_cover(mainwin, NULL, 0);
        }
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
 * Stop the track being played (if any) and scrobbles it. To be called
 * only from controller_stop_playing() and controller_skip_track()
 */
static void
finish_playing_track(void)
{
        g_return_if_fail(mainwin != NULL);
        lastfm_audio_stop();
        if (nowplaying != NULL) {
                controller_scrobble_track();
                controller_set_nowplaying(NULL);
        }
}

/**
 * Stop the track being played, updating the UI as well
 */
void
controller_stop_playing(void)
{
        g_return_if_fail(mainwin != NULL);
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_STOPPED, NULL);
        finish_playing_track();
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
 * Download a track. This will take some time, so it must be called
 * using g_thread_create() to avoid freezing the UI.
 * @param data Pointer to the lastfm_track to be downloaded
 * @return NULL (this value is not used)
 */
static gpointer
download_track_thread(gpointer data)
{
        lastfm_track *t = (lastfm_track *) data;
        g_return_val_if_fail(t != NULL && t->free_track_url != NULL, NULL);
        char *filename = NULL;
        gdk_threads_enter();
        if (usercfg != NULL) {
                filename = g_strconcat(usercfg->download_dir, "/",
                                       t->artist, " - ", t->title, ".mp3",
                                       NULL);
        }
        gdk_threads_leave();
        if (filename != NULL) {
                g_debug("Downloading track to %s", filename);
                gdk_threads_enter();
                controller_show_banner("Downloading track");
                gdk_threads_leave();
                if (http_download_file(t->free_track_url, filename)) {
                        g_debug("Downloaded track %s", filename);
                        gdk_threads_enter();
                        controller_show_banner("Track downloaded");
                        gdk_threads_leave();
                } else {
                        g_warning("Error downloading track %s", filename);
                        gdk_threads_enter();
                        controller_show_banner("Error downloading track");
                        gdk_threads_leave();
                }
                g_free(filename);
        } else {
                g_warning("Could not find user cfg!!!");
        }
        lastfm_track_destroy(t);
        return NULL;
}

/**
 * Download the currently play track (if it's free)
 */
void
controller_download_track(void)
{
        g_return_if_fail(nowplaying && nowplaying->free_track_url);
        lastfm_track *t = lastfm_track_copy(nowplaying);
        if (controller_confirm_dialog("Download this track to\n"
                                      "your audio clips directory?")) {
                g_thread_create(download_track_thread, t, FALSE, NULL);
        } else {
                lastfm_track_destroy(t);
        }
}

/**
 * Mark the currently playing track as a loved track. This will be
 * used when scrobbling it.
 */
void
controller_love_track(void)
{
        g_return_if_fail(nowplaying != NULL);
        if (controller_confirm_dialog("Really mark track as loved?")) {
                nowplaying_rating = RSP_RATING_LOVE;
                controller_show_banner("Marking track as loved");
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
        lastfm_track_destroy(d->track);
        g_free(d->taglist);
        g_free(d);
        gdk_threads_enter();
        if (mainwin && mainwin->window) {
                controller_show_banner(tagged ? "Tag set correctly" :
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
controller_tag_track(request_type type)
{
        g_return_if_fail(usercfg != NULL && nowplaying != NULL);
        char *tags = NULL;
        const char *text;
        if (type == REQUEST_ARTIST) {
                text = "Enter a comma-separated list\nof tags for this artist";
        } else if (type == REQUEST_TRACK) {
                text = "Enter a comma-separated list\nof tags for this track";
        } else {
                text = "Enter a comma-separated list\nof tags for this album";
        }
        tags = ui_input_dialog(mainwin->window, "Enter tags", text, NULL);
        if (tags != NULL) {
                tag_data *d = g_new0(tag_data, 1);
                d->track = lastfm_track_copy(nowplaying);
                d->taglist = tags;
                d->type = type;
                g_thread_create(tag_track_thread,d,FALSE,NULL);
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
        lastfm_track_destroy(d->track);
        g_free(d->rcpt);
        g_free(d->text);
        g_free(d);
        return NULL;
}

/**
 * Ask the user a recipient and recommend the current artist, track or
 * album album (yes, the name of the function is misleading but I
 * can't think of a better one)
 *
 * @param type The type of recommendation (artist, track, album)
 */
void
controller_recomm_track(request_type type)
{
        g_return_if_fail(usercfg != NULL && nowplaying != NULL);
        char *rcpt = NULL;
        char *body = NULL;;
        const char *text = "Recommend to this user...";
        rcpt = ui_input_dialog_with_list(mainwin->window, "Recommendation",
                                         text, friends, NULL);
        if (rcpt != NULL) {
                body = ui_input_dialog(mainwin->window, "Recommendation",
                                       "Recommendation message", NULL);
        }
        if (body != NULL) {
                g_strstrip(rcpt);
                recomm_data *d = g_new0(recomm_data, 1);
                d->track = lastfm_track_copy(nowplaying);
                d->rcpt = rcpt;
                d->text = body;
                d->type = type;
                g_thread_create(recomm_track_thread,d,FALSE,NULL);
        } else {
                controller_show_info("Recommendation cancelled");
                g_free(rcpt);
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
        lastfm_track_destroy(t);
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
                lastfm_track *track = lastfm_track_copy(nowplaying);
                g_thread_create(add_to_playlist_thread,track,FALSE,NULL);
        }
}

/**
 * Start playing a radio by its URL, stopping the current track if
 * necessary
 *
 * @param url The URL of the radio to be played
 */
void
controller_play_radio_by_url(const char *url)
{
        if (!check_session()) return;
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
                controller_show_info("Invalid radio URL");
        }
}

/**
 * Start playing a radio by its type. In all cases it will be the
 * radio of the user running the application, not someone else's radio
 * @param type Radio type
 */
void
controller_play_radio(lastfm_radio type)
{
        if (!check_session()) return;
        char *url = NULL;
        if (type == LASTFM_RECOMMENDED_RADIO) {
                url = lastfm_recommended_radio_url(
                        usercfg->username, 100);
        } else if (type == LASTFM_USERTAG_RADIO) {
                static char *previous = NULL;
                char *tag = ui_input_dialog(mainwin->window, "Enter tag",
                                            "Enter one of your tags",
                                            previous);
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
 * Start playing other user's radio by its type. It will pop up a
 * dialog to ask the user whose radio is going to be played
 * @param type Radio type
 */
void
controller_play_others_radio(lastfm_radio type)
{
        if (!check_session()) return;
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
        char *tag = ui_input_dialog(mainwin->window, "Enter tag",
                                    "Enter a global tag", previous);
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
 * Close the application
 */
void
controller_quit_app(void)
{
        controller_stop_playing();
        lastfm_audio_clear();
        lastfm_session_destroy(session);
        session = NULL;
        gtk_main_quit();
}

/*
 * This enables the hardware fullscreen key. Only makes sense for
 * Maemo really
 */
#ifdef MAEMO
static gboolean
window_state_cb(GtkWidget *widget, GdkEventWindowState *event,
                lastfm_mainwin *win)
{
        win->is_fullscreen = (event->new_window_state &
                              GDK_WINDOW_STATE_FULLSCREEN);
        return FALSE;
}

static gboolean
key_press_cb(GtkWidget *widget, GdkEventKey *event, lastfm_mainwin *win)
{
        gboolean volume_changed = FALSE;
        int newvol = 0;
        switch (event->keyval) {
        case GDK_F6:
                if (win->is_fullscreen) {
                        gtk_window_unfullscreen(win->window);
                } else {
                        gtk_window_fullscreen(win->window);
                }
                break;
        case GDK_F7:
                newvol = lastfm_audio_increase_volume(5);
                volume_changed = TRUE;
                break;
        case GDK_F8:
                newvol = lastfm_audio_increase_volume(-5);
                volume_changed = TRUE;
                break;
        }

        if (volume_changed) {
                char *text = g_strdup_printf("Volume: %d/100", newvol);
                controller_show_banner(text);
                g_free(text);
        }
        return FALSE;
}

static gpointer
playurl_handler_thread(gpointer data)
{
        g_return_val_if_fail(data != NULL, NULL);
        char *url = (char *) data;
        gdk_threads_enter();
        controller_play_radio_by_url(url);
        gdk_threads_leave();
        g_free(url);
        return NULL;
}

static gint
dbus_req_handler(const gchar* interface, const gchar* method,
                 GArray* arguments, gpointer data, osso_rpc_t* retval)
{
        if (!strcasecmp(method, APP_DBUS_METHOD_PLAYURL)) {
                osso_rpc_t val = g_array_index(arguments, osso_rpc_t, 0);
                gchar *url = g_strdup(val.value.s);
                g_thread_create(playurl_handler_thread, url, FALSE, NULL);
        }
        return OSSO_OK;
}
#endif /* MAEMO */

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
        mainwin = win;
        gtk_widget_show_all(GTK_WIDGET(mainwin->window));
        mainwin_set_ui_state(mainwin, LASTFM_UI_STATE_DISCONNECTED, NULL);

        http_init();
        usercfg = read_usercfg();
        playlist = lastfm_pls_new();

        if (!lastfm_audio_init()) {
                controller_show_error("Error initializing audio system");
                return;
        }
#ifdef MAEMO
        osso_context_t *context;
        osso_return_t result;
        context = osso_initialize(APP_NAME_LC, APP_VERSION, FALSE, NULL);
        if (!context) {
                controller_show_error("Unable to initialize OSSO context");
                return;
        }
        result = osso_rpc_set_cb_f(context, APP_DBUS_SERVICE, APP_DBUS_OBJECT,
                                   APP_DBUS_IFACE, dbus_req_handler, NULL);
        if (result != OSSO_OK) {
                controller_show_error("Unable to set D-BUS callback");
                return;
        }
        g_signal_connect(G_OBJECT(mainwin->window), "key_press_event",
                         G_CALLBACK(key_press_cb), mainwin);
        g_signal_connect(G_OBJECT(mainwin->window), "window_state_event",
                         G_CALLBACK(window_state_cb), mainwin);
#endif
        if (radio_url) {
                controller_play_radio_by_url(radio_url);
        }

        gtk_main();
        mainwin = NULL;
#ifdef MAEMO
        osso_deinitialize(context);
#endif
}
