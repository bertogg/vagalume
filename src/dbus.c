/*
 * dbus.c -- D-BUS interface
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 * Copyright (C) 2008 Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "dbus.h"
#include "controller.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <strings.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

static DBusConnection *dbus_connection = NULL;
static gboolean dbus_filter_added = FALSE;

static gboolean
playurl_handler_idle(gpointer data)
{
        g_return_val_if_fail(data != NULL, FALSE);
        char *url = (char *) data;
        gdk_threads_enter();
        controller_play_radio_by_url(url);
        gdk_threads_leave();
        g_free(url);
        return FALSE;
}

static gboolean
play_handler_idle(gpointer data)
{
        gdk_threads_enter();
        controller_start_playing();
        gdk_threads_leave();
        return FALSE;
}

static gboolean
stop_handler_idle(gpointer data)
{
        gdk_threads_enter();
        controller_stop_playing();
        gdk_threads_leave();
        return FALSE;
}

static gboolean
skip_handler_idle(gpointer data)
{
        gdk_threads_enter();
        controller_skip_track();
        gdk_threads_leave();
        return FALSE;
}

static gboolean
lovetrack_handler_idle(gpointer data)
{
        gboolean interactive = (gboolean)GPOINTER_TO_INT(data);
        gdk_threads_enter();
        controller_love_track(interactive);
        gdk_threads_leave();
        return FALSE;
}

static gboolean
bantrack_handler_idle(gpointer data)
{
        gboolean interactive = (gboolean)GPOINTER_TO_INT(data);
        gdk_threads_enter();
        controller_ban_track(interactive);
        gdk_threads_leave();
        return FALSE;
}

static gboolean
showwindow_handler_idle(gpointer data)
{
        gboolean show = GPOINTER_TO_INT(data);
        gdk_threads_enter();
        controller_show_mainwin(show);
        gdk_threads_leave();
        return FALSE;
}

static gboolean
volumechange_handler_idle(gpointer data)
{
        gint volchange = GPOINTER_TO_INT(data);
        gdk_threads_enter();
        controller_increase_volume(volchange);
        gdk_threads_leave();
        return FALSE;
}

static gboolean
volumeset_handler_idle(gpointer data)
{
        gint vol = GPOINTER_TO_INT(data);
        gdk_threads_enter();
        controller_set_volume(vol);
        gdk_threads_leave();
        return FALSE;
}

static gboolean
requeststatus_handler_idle(gpointer data)
{
        LastfmTrack *current_track = NULL;

        current_track = controller_get_current_track();
        gdk_threads_enter();
        lastfm_dbus_notify_playback(current_track);
        gdk_threads_leave();

        return FALSE;
}

#ifdef HAVE_GSD_MEDIA_PLAYER_KEYS

static gboolean
gsd_mp_keys_handler_idle(gpointer data)
{
        g_return_val_if_fail (data != NULL, FALSE);

        gchar *key_pressed = (gchar *) data;
        GCallback key_handler = NULL;

        if (g_str_equal(key_pressed, GSD_DBUS_MK_KEYPRESSED_STOP)) {
                key_handler = controller_stop_playing;
        } else if (g_str_equal(key_pressed, GSD_DBUS_MK_KEYPRESSED_PLAY) &&
                   (controller_get_current_track () == NULL)) {
                key_handler = controller_start_playing;
        } else if (g_str_equal(key_pressed, GSD_DBUS_MK_KEYPRESSED_NEXT)) {
                key_handler = controller_skip_track;
        }

        if (key_handler) {
                gdk_threads_enter();
                key_handler ();
                gdk_threads_leave();
        }

        /* Free passed memory */
        g_free (key_pressed);

        return FALSE;
}

static void
grab_media_player_keys(void)
{
        DBusMessage *dbus_msg = NULL;
        gchar *app_name = g_strdup (APP_NAME);
        guint32 unknown;

        dbus_msg = dbus_message_new_method_call(GSD_DBUS_SERVICE,
                                                GSD_DBUS_MK_OBJECT,
                                                GSD_DBUS_MK_IFACE,
                                                GSD_DBUS_MK_GRAB_KEYS);

        dbus_message_append_args (dbus_msg,
                                  DBUS_TYPE_STRING, &app_name,
                                  DBUS_TYPE_UINT32, &unknown,
                                  DBUS_TYPE_INVALID);

        dbus_connection_send (dbus_connection, dbus_msg, 0);
        dbus_connection_flush (dbus_connection);
        dbus_message_unref (dbus_msg);

        g_free (app_name);
}

static void
release_media_player_keys(void)
{
        DBusMessage *dbus_msg = NULL;
        gchar *app_name = g_strdup (APP_NAME);

        dbus_msg = dbus_message_new_method_call(GSD_DBUS_SERVICE,
                                                GSD_DBUS_MK_OBJECT,
                                                GSD_DBUS_MK_IFACE,
                                                GSD_DBUS_MK_RELEASE_KEYS);

        dbus_message_append_args (dbus_msg,
                                  DBUS_TYPE_STRING, &app_name,
                                  DBUS_TYPE_INVALID);

        dbus_connection_send (dbus_connection, dbus_msg, 0);
        dbus_connection_flush (dbus_connection);
        dbus_message_unref (dbus_msg);

        g_free (app_name);
}

#endif /* HAVE_GSD_MEDIA_PLAYER_KEYS */

static gboolean
closeapp_handler_idle(gpointer data)
{
        gdk_threads_enter();
        controller_quit_app();
        gdk_threads_leave();
        return FALSE;
}

static void
send_message(char *message, int first_type, ...)
{
        DBusMessage *dbus_msg = NULL;
        va_list ap;

        dbus_msg = dbus_message_new_signal(APP_DBUS_OBJECT,
                                           APP_DBUS_IFACE,
                                           message);

        if (first_type != DBUS_TYPE_INVALID) {
                va_start (ap, first_type);
                dbus_message_append_args_valist (dbus_msg, first_type, ap);
                va_end (ap);
        }
        dbus_connection_send (dbus_connection, dbus_msg, 0);
        dbus_connection_flush (dbus_connection);
        dbus_message_unref (dbus_msg);
}

void
lastfm_dbus_notify_playback (LastfmTrack *track)
{
        const char *param = NULL;

        if (track != NULL) {
                /* Now playing */
                param = APP_DBUS_SIGNAL_NOTIFY_PLAYING;
                send_message(APP_DBUS_SIGNAL_NOTIFY,
                             DBUS_TYPE_STRING, &param,
                             DBUS_TYPE_STRING, &track->artist,
                             DBUS_TYPE_STRING, &track->title,
                             DBUS_TYPE_STRING, &track->album,
                             DBUS_TYPE_INVALID);

        } else {
                /* Stopped */
                param = APP_DBUS_SIGNAL_NOTIFY_STOPPED;
                send_message(APP_DBUS_SIGNAL_NOTIFY,
                             DBUS_TYPE_STRING, &param,
                             DBUS_TYPE_INVALID);
        }
}

void
lastfm_dbus_notify_started(void)
{
        const char *param = APP_DBUS_SIGNAL_NOTIFY_STARTED;

        send_message(APP_DBUS_SIGNAL_NOTIFY,
                     DBUS_TYPE_STRING,
                     &param,
                     DBUS_TYPE_INVALID);
}

void
lastfm_dbus_notify_closing(void)
{
        const char *param = APP_DBUS_SIGNAL_NOTIFY_CLOSING;

        send_message(APP_DBUS_SIGNAL_NOTIFY,
                     DBUS_TYPE_STRING,
                     &param,
                     DBUS_TYPE_INVALID);
}

static gboolean
method_is_interactive (DBusMessage *message)
{
        gboolean interactive = FALSE;
        DBusMessageIter iter;

        if (dbus_message_iter_init (message, &iter) &&
            dbus_message_iter_get_arg_type (&iter) == DBUS_TYPE_BOOLEAN) {
                dbus_message_iter_get_basic (&iter, &interactive);
        }

        return interactive;
}

void
lastfm_dbus_play_radio_url (const char *url)
{
        DBusMessage *dbus_msg = NULL;
        dbus_msg = dbus_message_new_method_call (APP_DBUS_SERVICE,
                                                 APP_DBUS_OBJECT,
                                                 APP_DBUS_IFACE,
                                                 APP_DBUS_METHOD_PLAYURL);

        dbus_message_append_args (dbus_msg,
                                  DBUS_TYPE_STRING, &url,
                                  DBUS_TYPE_INVALID);

        dbus_connection_send (dbus_connection, dbus_msg, 0);
        dbus_connection_flush (dbus_connection);
        dbus_message_unref (dbus_msg);
}

static DBusHandlerResult
dbus_req_handler(DBusConnection *connection, DBusMessage *message,
                 gpointer user_data)
{
        DBusHandlerResult result = DBUS_HANDLER_RESULT_HANDLED;

        /* Check calls to Vagalume D-Bus methods */
        if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                        APP_DBUS_METHOD_PLAYURL)) {
                char *url = NULL;
                dbus_message_get_args(message, NULL,
                                      DBUS_TYPE_STRING,
                                      &url,
                                      DBUS_TYPE_INVALID);
                g_idle_add(playurl_handler_idle, g_strdup(url));
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_PLAY)) {
                g_idle_add(play_handler_idle, NULL);
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_STOP)) {
                g_idle_add(stop_handler_idle, NULL);
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_SKIP)) {
                g_idle_add(skip_handler_idle, NULL);
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_LOVETRACK)) {
                gboolean interactive = method_is_interactive (message);
                g_idle_add (lovetrack_handler_idle,
                            GINT_TO_POINTER (interactive));
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_BANTRACK)) {
                gboolean interactive = method_is_interactive (message);
                g_idle_add (bantrack_handler_idle,
                            GINT_TO_POINTER (interactive));
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_SHOWWINDOW)) {
                g_idle_add(showwindow_handler_idle, GINT_TO_POINTER(TRUE));
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_HIDEWINDOW)) {
                g_idle_add(showwindow_handler_idle, GINT_TO_POINTER(FALSE));
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                                APP_DBUS_METHOD_CLOSEAPP)) {
                g_idle_add(closeapp_handler_idle, NULL);
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_VOLUMEUP)) {
                guint32 inc;
                dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32,
                                      &inc, DBUS_TYPE_INVALID);
                if (inc == G_MAXUINT32) {
                        inc = 5;
                }
                g_idle_add (volumechange_handler_idle,
                            GINT_TO_POINTER ((gint) inc));
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_VOLUMEDOWN)) {
                guint32 inc;
                dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32,
                                      &inc, DBUS_TYPE_INVALID);
                if (inc == G_MAXUINT32) {
                        inc = 5;
                }
                g_idle_add (volumechange_handler_idle,
                            GINT_TO_POINTER ((gint) -inc));
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_SETVOLUME)) {
                guint32 vol;
                dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32,
                                      &vol, DBUS_TYPE_INVALID);
                if (vol != G_MAXUINT32) {
                        g_idle_add (volumeset_handler_idle,
                                    GINT_TO_POINTER ((gint) vol));
                } else {
                        g_debug ("No parameter received for "
                                 APP_DBUS_METHOD_SETVOLUME);
                }
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_TOPAPP)) {
                g_idle_add(showwindow_handler_idle, GINT_TO_POINTER(TRUE));
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_REQUEST_STATUS)) {
                g_idle_add(requeststatus_handler_idle, NULL);
        } else {
                result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

#ifdef HAVE_GSD_MEDIA_PLAYER_KEYS
        /* Match signals from the Gnome Settings Daemon */
        if (dbus_message_is_signal(message, GSD_DBUS_MK_IFACE,
                                   GSD_DBUS_MK_KEYPRESSED)) {
                gchar *app_name = NULL;
                gchar *key_pressed = NULL;

                dbus_message_get_args(message, NULL,
                                      DBUS_TYPE_STRING, &app_name,
                                      DBUS_TYPE_STRING, &key_pressed,
                                      DBUS_TYPE_INVALID);

                if (app_name != NULL && key_pressed != NULL &&
                    g_str_equal (app_name, APP_NAME)) {
                        g_idle_add(gsd_mp_keys_handler_idle,
                                   g_strdup (key_pressed));
                }
                result = DBUS_HANDLER_RESULT_HANDLED;
        }
#endif /* HAVE_GSD_MEDIA_PLAYER_KEYS */

        /* Send message reply, if needed */
        if (result == DBUS_HANDLER_RESULT_HANDLED &&
            !dbus_message_get_no_reply(message)) {
                DBusMessage *reply  = dbus_message_new_method_return(message);
                dbus_connection_send(connection, reply, NULL);
                dbus_message_unref(reply);
        }

        return result;
}

DbusInitReturnCode
lastfm_dbus_init(void)
{
        int result;

        g_debug("Initializing D-Bus...");

        /* Get D-Bus connection */
        dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
        if (!dbus_connection) {
                g_debug("Unable to get DBUS connection");
                return DBUS_INIT_ERROR;
        }

        dbus_connection_setup_with_g_main(dbus_connection, NULL);

        /* Add D-Bus handler */
        dbus_filter_added = dbus_connection_add_filter(dbus_connection,
                                                       dbus_req_handler,
                                                       NULL,
                                                       NULL);
        if (!dbus_filter_added) {
                g_debug("Unable to add a filter");
                return DBUS_INIT_ERROR;
        }

#ifdef HAVE_GSD_MEDIA_PLAYER_KEYS
        /* Grab media player keys for Vagalume */
        grab_media_player_keys();

        /* Match gnome-settings-daemon signals for media player keys  */
        dbus_bus_add_match (dbus_connection,
                            "type='signal',interface='" GSD_DBUS_MK_IFACE "'",
                            NULL);

        dbus_connection_flush (dbus_connection);
#endif /* HAVE_GSD_MEDIA_PLAYER_KEYS */

        /* Release name in D-Bus if already present */
        if (dbus_bus_name_has_owner(dbus_connection, APP_DBUS_SERVICE, NULL)) {
                dbus_bus_release_name(dbus_connection, APP_DBUS_SERVICE, NULL);
        }

        /* Request name in D-Bus for Vagalume */
        result = dbus_bus_request_name(dbus_connection, APP_DBUS_SERVICE,
                                       DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                       NULL);

        if (result == -1) {
                g_debug("Unable to request name on DBUS");
                return DBUS_INIT_ERROR;
        }

        if (result == DBUS_REQUEST_NAME_REPLY_EXISTS) {
                g_debug("Another instance is running");
                return DBUS_INIT_ALREADY_RUNNING;
        }

        return DBUS_INIT_OK;
}

void
lastfm_dbus_close(void)
{
        /* Remove filter, if added */
        if (dbus_filter_added) {
                dbus_connection_remove_filter (dbus_connection,
                                               dbus_req_handler,
                                               NULL);
        }

#ifdef HAVE_GSD_MEDIA_PLAYER_KEYS
        /* Remove matching rules */
        dbus_bus_remove_match (dbus_connection,
                               "type='signal',interface='"
                               GSD_DBUS_MK_IFACE "'",
                               NULL);

        /* Release media player keys for Vagalume */
        release_media_player_keys();
#endif /* HAVE_GSD_MEDIA_PLAYER_KEYS */

        /* Unref the D-bus connection */
        if (dbus_connection) {
                dbus_connection_unref (dbus_connection);
        }
}
