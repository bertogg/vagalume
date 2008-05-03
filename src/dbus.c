/*
 * dbus.c -- D-BUS interface
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 * Copyright (C) 2008 Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include "dbus.h"

#include <gtk/gtk.h>
#include <strings.h>

#ifdef MAEMO
#include <libosso.h>

static osso_context_t *context = NULL;
#endif

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
closeapp_handler_idle(gpointer data)
{
        gdk_threads_enter();
        controller_quit_app();
        gdk_threads_leave();
        return FALSE;
}

#ifdef MAEMO
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
get_bool_parameter (GArray* arguments)
{
        if (arguments->len > 0) {
                osso_rpc_t val;
                val = g_array_index(arguments, osso_rpc_t, 0);
                return val.value.b;
        }
        return FALSE;
}

static guint32
get_uint32_parameter (GArray* arguments)
{
        if (arguments->len > 0) {
                osso_rpc_t val;
                val = g_array_index(arguments, osso_rpc_t, 0);
                return val.value.u;
        }
        return G_MAXUINT32;
}

static gint
dbus_req_handler(const gchar* interface, const gchar* method,
                 GArray* arguments, gpointer data, osso_rpc_t* retval)
{
        g_debug("Received D-BUS message: %s", method);
        if (!strcasecmp(method, APP_DBUS_METHOD_PLAYURL)) {
                osso_rpc_t val = g_array_index(arguments, osso_rpc_t, 0);
                gchar *url = g_strdup(val.value.s);
                g_idle_add(playurl_handler_idle, url);
        } else if (!strcasecmp(method, APP_DBUS_METHOD_PLAY)) {
                g_idle_add(play_handler_idle, NULL);
        } else if (!strcasecmp(method, APP_DBUS_METHOD_STOP)) {
                g_idle_add(stop_handler_idle, NULL);
        } else if (!strcasecmp(method, APP_DBUS_METHOD_SKIP)) {
                g_idle_add(skip_handler_idle, NULL);
        } else if (!strcasecmp(method, APP_DBUS_METHOD_LOVETRACK)) {
                gboolean interactive = get_bool_parameter(arguments);
                g_idle_add(lovetrack_handler_idle, GINT_TO_POINTER(interactive));
        } else if (!strcasecmp(method, APP_DBUS_METHOD_BANTRACK)) {
                gboolean interactive = get_bool_parameter(arguments);
                g_idle_add(bantrack_handler_idle, GINT_TO_POINTER(interactive));
        } else if (!strcasecmp(method, APP_DBUS_METHOD_SHOWWINDOW)) {
                g_idle_add(showwindow_handler_idle, GINT_TO_POINTER(TRUE));
        } else if (!strcasecmp(method, APP_DBUS_METHOD_HIDEWINDOW)) {
                g_idle_add(showwindow_handler_idle, GINT_TO_POINTER(FALSE));
        } else if (!strcasecmp(method, APP_DBUS_METHOD_CLOSEAPP)) {
                g_idle_add(closeapp_handler_idle, NULL);
        } else if (!strcasecmp(method, APP_DBUS_METHOD_VOLUMEUP)) {
                guint32 inc = get_uint32_parameter (arguments);
                if (inc == G_MAXUINT32) {
                        inc = 5;
                }
                g_idle_add(volumechange_handler_idle, GINT_TO_POINTER((gint)inc));
        } else if (!strcasecmp(method, APP_DBUS_METHOD_VOLUMEDOWN)) {
                guint32 inc = get_uint32_parameter (arguments);
                if (inc == G_MAXUINT32) {
                        inc = 5;
                }
                g_idle_add(volumechange_handler_idle, GINT_TO_POINTER((gint)-inc));
        } else if (!strcasecmp(method, APP_DBUS_METHOD_SETVOLUME)) {
                guint32 vol = get_uint32_parameter (arguments);
                if (vol != G_MAXUINT32) {
                        g_idle_add(volumeset_handler_idle, GINT_TO_POINTER((gint)vol));
                } else {
                        g_debug ("No parameter received for " APP_DBUS_METHOD_SETVOLUME);
                }
        } else if (!strcasecmp(method, APP_DBUS_METHOD_TOPAPP)) {
                g_idle_add(showwindow_handler_idle, GINT_TO_POINTER(TRUE));
        } else if (!strcasecmp(method, APP_DBUS_METHOD_REQUEST_STATUS)) {
                g_idle_add(requeststatus_handler_idle, NULL);
        }
        return OSSO_OK;
}

static void
hw_event_handler(osso_hw_state_t *state, gpointer data) {
        /* The device is about to be shutdown */
        if (state->shutdown_ind) {
                controller_disconnect();
        }
}

const char *
lastfm_dbus_init(void)
{
        osso_return_t result;
        context = osso_initialize(APP_NAME_LC, APP_VERSION, FALSE, NULL);
        if (!context) {
                return _("Unable to initialize OSSO context");
        }
        result = osso_rpc_set_cb_f(context, APP_DBUS_SERVICE, APP_DBUS_OBJECT,
                                   APP_DBUS_IFACE, dbus_req_handler, NULL);
        if (result != OSSO_OK) {
                return _("Unable to set D-BUS callback");
        }

        result = osso_hw_set_event_cb(context, NULL, hw_event_handler, NULL);
        if (result != OSSO_OK) {
                return _("Unable to set hw callback");
        }

        return NULL;
}

void
lastfm_dbus_close(void)
{
        osso_deinitialize(context);
}

void
lastfm_dbus_notify_playback (lastfm_track *track)
{
        osso_return_t result;
        const char *notify_type = NULL;

        if (track != NULL) {

                /* Now playing */

                notify_type = SB_PLUGIN_DBUS_METHOD_NOTIFY_PLAYING;
                result = osso_rpc_async_run(context,
                                            SB_PLUGIN_DBUS_SERVICE,
                                            SB_PLUGIN_DBUS_OBJECT,
                                            SB_PLUGIN_DBUS_IFACE,
                                            SB_PLUGIN_DBUS_METHOD_NOTIFY,
                                            NULL,
                                            NULL,
                                            DBUS_TYPE_STRING, notify_type,
                                            DBUS_TYPE_STRING, track->artist,
                                            DBUS_TYPE_STRING, track->title,
                                            DBUS_TYPE_STRING, track->album,
                                            DBUS_TYPE_INVALID);

        } else {

                /* Stopped */

                notify_type = SB_PLUGIN_DBUS_METHOD_NOTIFY_STOPPED;
                result = osso_rpc_async_run(context,
                                            SB_PLUGIN_DBUS_SERVICE,
                                            SB_PLUGIN_DBUS_OBJECT,
                                            SB_PLUGIN_DBUS_IFACE,
                                            SB_PLUGIN_DBUS_METHOD_NOTIFY,
                                            NULL,
                                            NULL,
                                            DBUS_TYPE_STRING, notify_type,
                                            DBUS_TYPE_INVALID);
        }

        if (result != OSSO_OK) {
                g_warning("Error sending DBus message");
        }
}

void
lastfm_dbus_notify_started(void)
{
        osso_return_t result;

        result = osso_rpc_async_run(context,
                                    SB_PLUGIN_DBUS_SERVICE,
                                    SB_PLUGIN_DBUS_OBJECT,
                                    SB_PLUGIN_DBUS_IFACE,
                                    SB_PLUGIN_DBUS_METHOD_NOTIFY,
                                    NULL,
                                    NULL,
                                    DBUS_TYPE_STRING,
                                    SB_PLUGIN_DBUS_METHOD_NOTIFY_STARTED,
                                    DBUS_TYPE_INVALID);

        if (result != OSSO_OK) {
                g_warning("Error sending DBus message");
        }
}

void
lastfm_dbus_notify_closing(void)
{
        osso_return_t result;

        result = osso_rpc_async_run(context,
                                    SB_PLUGIN_DBUS_SERVICE,
                                    SB_PLUGIN_DBUS_OBJECT,
                                    SB_PLUGIN_DBUS_IFACE,
                                    SB_PLUGIN_DBUS_METHOD_NOTIFY,
                                    NULL,
                                    NULL,
                                    DBUS_TYPE_STRING,
                                    SB_PLUGIN_DBUS_METHOD_NOTIFY_CLOSING,
                                    DBUS_TYPE_INVALID);
        if (result != OSSO_OK) {
                g_warning("Error sending DBus message");
        }
}
#endif /* MAEMO */
