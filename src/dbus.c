/*
 * dbus.c -- D-BUS interface
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#include "dbus.h"
#include "controller.h"

#include <gtk/gtk.h>
#include <strings.h>

#ifdef MAEMO
#include <libosso.h>

static osso_context_t *context;
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
        gdk_threads_enter();
        controller_love_track();
        gdk_threads_leave();
        return FALSE;
}

static gboolean
bantrack_handler_idle(gpointer data)
{
        gdk_threads_enter();
        controller_ban_track();
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
closeapp_handler_idle(gpointer data)
{
        gdk_threads_enter();
        controller_quit_app();
        gdk_threads_leave();
        return FALSE;
}

#ifdef MAEMO
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
                g_idle_add(lovetrack_handler_idle, NULL);
        } else if (!strcasecmp(method, APP_DBUS_METHOD_BANTRACK)) {
                g_idle_add(bantrack_handler_idle, NULL);
        } else if (!strcasecmp(method, APP_DBUS_METHOD_SHOWWINDOW)) {
                g_idle_add(showwindow_handler_idle, GINT_TO_POINTER(TRUE));
        } else if (!strcasecmp(method, APP_DBUS_METHOD_HIDEWINDOW)) {
                g_idle_add(showwindow_handler_idle, GINT_TO_POINTER(FALSE));
        } else if (!strcasecmp(method, APP_DBUS_METHOD_CLOSEAPP)) {
                g_idle_add(closeapp_handler_idle, NULL);
        } else if (!strcasecmp(method, APP_DBUS_METHOD_TOPAPP)) {
                g_idle_add(showwindow_handler_idle, GINT_TO_POINTER(TRUE));
        }
        return OSSO_OK;
}

const char *
lastfm_dbus_init(void)
{
        osso_return_t result;
        context = osso_initialize(APP_NAME_LC, APP_VERSION, FALSE, NULL);
        if (!context) {
                return "Unable to initialize OSSO context";
        }
        result = osso_rpc_set_cb_f(context, APP_DBUS_SERVICE, APP_DBUS_OBJECT,
                                   APP_DBUS_IFACE, dbus_req_handler, NULL);
        if (result != OSSO_OK) {
                return "Unable to set D-BUS callback";
        }
        return NULL;
}

void
lastfm_dbus_close(void)
{
        osso_deinitialize(context);
}
#endif /* MAEMO */
