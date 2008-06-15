/*
 * dbus.c -- D-BUS interface
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 * Copyright (C) 2008 Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "dbus.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <strings.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

static DBusConnection *dbus_connection = NULL;

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
        lastfm_track *current_track = NULL;

        current_track = controller_get_current_track();
        gdk_threads_enter();
        lastfm_dbus_notify_playback(current_track);
        gdk_threads_leave();

        return FALSE;
}

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

        dbus_msg = dbus_message_new_method_call (SB_PLUGIN_DBUS_SERVICE,
                                                 SB_PLUGIN_DBUS_OBJECT,
                                                 SB_PLUGIN_DBUS_IFACE,
                                                 message);
        dbus_message_set_no_reply (dbus_msg, TRUE);
        if (first_type != DBUS_TYPE_INVALID)
        {
                va_start (ap, first_type);
                dbus_message_append_args_valist (dbus_msg, first_type, ap);
                va_end (ap);
        }
        dbus_connection_send (dbus_connection, dbus_msg, 0);
        dbus_connection_flush (dbus_connection);
        dbus_message_unref (dbus_msg);
}

void
lastfm_dbus_notify_playback (lastfm_track *track)
{
        const char *param = NULL;

        if (track != NULL) {
                /* Now playing */
                param = SB_PLUGIN_DBUS_METHOD_NOTIFY_PLAYING;
                send_message(SB_PLUGIN_DBUS_METHOD_NOTIFY,
                             DBUS_TYPE_STRING, &param,
                             DBUS_TYPE_STRING, &track->artist,
                             DBUS_TYPE_STRING, &track->title,
                             DBUS_TYPE_STRING, &track->album,
                             DBUS_TYPE_INVALID);

        } else {
                /* Stopped */
                param = SB_PLUGIN_DBUS_METHOD_NOTIFY_STOPPED;
                send_message(SB_PLUGIN_DBUS_METHOD_NOTIFY,
                             DBUS_TYPE_STRING, &param,
                             DBUS_TYPE_INVALID);
        }
}

void
lastfm_dbus_notify_started(void)
{
        const char *param = SB_PLUGIN_DBUS_METHOD_NOTIFY_STARTED;

        send_message(SB_PLUGIN_DBUS_METHOD_NOTIFY,
                     DBUS_TYPE_STRING,
                     &param,
                     DBUS_TYPE_INVALID);
}

void
lastfm_dbus_notify_closing(void)
{
        const char *param = SB_PLUGIN_DBUS_METHOD_NOTIFY_CLOSING;

        send_message(SB_PLUGIN_DBUS_METHOD_NOTIFY,
                     DBUS_TYPE_STRING,
                     &param,
                     DBUS_TYPE_INVALID);
}

static DBusHandlerResult
dbus_req_handler(DBusConnection *connection, DBusMessage *message,
                 gpointer user_data)
{
        DBusMessage *reply = NULL;
        DBusHandlerResult result = DBUS_HANDLER_RESULT_HANDLED;

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
                gboolean interactive;
                dbus_message_get_args (message, NULL, DBUS_TYPE_BOOLEAN,
                                       &interactive, DBUS_TYPE_INVALID);
                g_idle_add (lovetrack_handler_idle,
                            GINT_TO_POINTER (interactive));
        } else if (dbus_message_is_method_call(message, APP_DBUS_IFACE,
                                               APP_DBUS_METHOD_BANTRACK)) {
                gboolean interactive;
                dbus_message_get_args (message, NULL, DBUS_TYPE_BOOLEAN,
                                       &interactive, DBUS_TYPE_INVALID);
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

        /* Send message reply, if handled */
        if (result == DBUS_HANDLER_RESULT_HANDLED) {
                reply = dbus_message_new_method_return (message);
                dbus_connection_send (connection, reply, NULL);
                dbus_message_unref (reply);

                g_debug("D-BUS message handled");
        }

        return result;
}

gboolean
lastfm_dbus_init(void)
{
        int result;

        g_debug("Initializing D-Bus...");

        dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);

        if (!dbus_connection) {
                g_debug("Unable to get DBUS connection");
                return FALSE;
        }

        dbus_connection_setup_with_g_main(dbus_connection, NULL);

        if (!dbus_connection_add_filter(dbus_connection, dbus_req_handler,
                                        NULL, NULL)) {
                g_debug("Unable to add a filter");
                return FALSE;
        }

        result = dbus_bus_request_name(dbus_connection, APP_DBUS_SERVICE,
                                       DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                       NULL);

        if (result == -1) {
                g_debug("Unable to request name on DBUS");
                return FALSE;
        }

        if (result == DBUS_REQUEST_NAME_REPLY_EXISTS) {
                g_debug("Another instance is running");
                return FALSE;
        }

        return TRUE;
}

void
lastfm_dbus_close(void)
{
        dbus_connection_unref (dbus_connection);
}
