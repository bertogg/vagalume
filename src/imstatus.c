/*
 * imstatus.c -- set IM status to current track
 * Copyright (C) 2008 Tim Wegener <twegener@fastmail.fm>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 *
 * todo: perhaps this should be incorporated into controller.c instead
 */

#include <glib.h>
#include <dbus/dbus-glib.h>
#include "imstatus.h"

typedef enum
{
        PURPLE_STATUS_UNSET = 0,
        PURPLE_STATUS_OFFLINE,
        PURPLE_STATUS_AVAILABLE,
        PURPLE_STATUS_UNAVAILABLE,
        PURPLE_STATUS_INVISIBLE,
        PURPLE_STATUS_AWAY,
        PURPLE_STATUS_EXTENDED_AWAY,
        PURPLE_STATUS_MOBILE,
        PURPLE_STATUS_TUNE,
        PURPLE_STATUS_NUM_PRIMITIVES
} PurpleStatusPrimitive;

static gboolean do_pidgin = TRUE;
static gboolean do_gajim = TRUE;
static gboolean do_gossip = TRUE;
static gboolean do_telepathy = TRUE;

void
im_set_cfg(gboolean pidgin, gboolean gajim, gboolean gossip,
           gboolean telepathy)
{
        do_pidgin = pidgin;
        do_gajim = gajim;
        do_gossip = gossip;
        do_telepathy = telepathy;
}

static int
check_result(gboolean code, GError *error)
{
        if (!code) {
                if (error->domain == DBUS_GERROR &&
                    error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
                        g_printerr("Caught remote method exception %s: %s",
                                   dbus_g_error_get_name (error),
                                   error->message);
                } else {
                        g_printerr("Error: (%d) %s\n",
                                   error->code, error->message);
                }
                g_error_free(error);
                return(1);
        }
        return(0);
}

void
gajim_set_status(const char *message)
{
        DBusGConnection *connection;
        DBusGProxy *proxy;
        GError *error;
        gboolean result;
        char *status;
        gboolean change_status_result;

        g_debug("gajim_set_status");

        g_type_init();

        error = NULL;
        connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if (connection == NULL) {
                g_printerr("Failed to open connection to bus: %s\n",
                           error->message);
                g_error_free(error);
        }

        proxy = dbus_g_proxy_new_for_name(connection,
                                          "org.gajim.dbus",
                                          "/org/gajim/dbus/RemoteObject",
                                          "org.gajim.dbus.RemoteInterface");

        g_return_if_fail(proxy != NULL);

        result = dbus_g_proxy_call (proxy, "get_status", &error,
                                    G_TYPE_STRING, "",
                                    G_TYPE_INVALID,
                                    G_TYPE_STRING, &status,
                                    G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        /* todo: abort if not online or chat? or dnd? */

        g_debug("status: %s", status);

        /* gajim-remote help says: status, message, account */
        /* gajim/src/common/connection.py says: show, msg, auto=False */
        result = dbus_g_proxy_call(proxy, "change_status", &error,
                                   G_TYPE_STRING, status,
                                   G_TYPE_STRING, message,
                                   G_TYPE_BOOLEAN, FALSE,
                                   G_TYPE_INVALID,
                                   G_TYPE_BOOLEAN, &change_status_result,
                                   G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        g_debug("change_status_result: %d", result);

        g_free(status);
        g_object_unref(proxy);
}

void
gossip_set_status(const char *message)
{
        DBusGConnection *connection;
        DBusGProxy *proxy;
        GError *error;
        gboolean result;
        char **chats;
        char *id;
        char *state_str;
        char *status;

        g_debug("gossip_set_status");

        g_type_init();

        error = NULL;
        connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if (connection == NULL) {
                g_printerr("Failed to open connection to bus: %s\n",
                           error->message);
                g_error_free(error);
        }

        proxy = dbus_g_proxy_new_for_name(connection,
                                          "org.gnome.Gossip",
                                          "/org/gnome/Gossip",
                                          "org.gnome.Gossip");

        error = NULL;

        g_debug("GetOpenChats");

        result = dbus_g_proxy_call(proxy, "GetOpenChats", &error,
                                   G_TYPE_INVALID,
                                   G_TYPE_STRV, &chats,
                                   G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        id = *chats;

        g_debug("id: %s", id);
        g_debug("GetPresence");

        result = dbus_g_proxy_call(proxy, "GetPresence",
                                   &error,
                                   G_TYPE_STRING, id,
                                   G_TYPE_INVALID,
                                   G_TYPE_STRING, &state_str,
                                   G_TYPE_STRING, &status,
                                   G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        g_debug("state_str: %s", state_str);
        g_debug("status: %s", status);

        /* todo: check this */
/*   g_free(chats); */
        g_strfreev(chats);
        g_free(status);

        /* todo: abort if not online or chat? or dnd? */

        g_debug("message: %s", message);
        g_debug("SetPresence");

        result = dbus_g_proxy_call(proxy, "SetPresence",
                                   &error,
                                   G_TYPE_STRING, state_str,
                                   G_TYPE_STRING, message,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        g_free(state_str);
        g_return_if_fail(!check_result(result, error));

        g_object_unref(proxy);
}

void
pidgin_set_status(const char *message)
{
        DBusGConnection *connection;
        DBusGProxy *proxy;
        GError *error;
        gboolean result;
        unsigned int current;
        int status;

        g_type_init();

        error = NULL;
        connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if (connection == NULL) {
                g_printerr("Failed to open connection to bus: %s\n",
                           error->message);
                g_error_free(error);
        }

        proxy = dbus_g_proxy_new_for_name(connection,
                                          "im.pidgin.purple.PurpleService",
                                          "/im/pidgin/purple/PurpleObject",
                                          "im.pidgin.purple.PurpleInterface");

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusGetCurrent",
                                   &error,
                                   G_TYPE_INVALID,
                                   G_TYPE_INT, &current,
                                   G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        g_debug("Current: %d", current);

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusGetType",
                                   &error,
                                   G_TYPE_INT, current,
                                   G_TYPE_INVALID,
                                   G_TYPE_INT, &status,
                                   G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        /* todo: abort if not online */

        g_debug("Type: %d", status);
        g_debug("message: %s", message);

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusSetMessage",
                                   &error,
                                   G_TYPE_INT, current,
                                   G_TYPE_STRING, message,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusActivate",
                                   &error,
                                   G_TYPE_INT, current,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        g_object_unref(proxy);
}


void
telepathy_set_status(const char *message)
{
        DBusGConnection *connection;
        DBusGProxy *proxy;
        GError *error;
        gboolean result;
        unsigned int presence;

        g_debug("telepathy_set_status");

        g_type_init();

        error = NULL;
        connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if (connection == NULL) {
                g_printerr("Failed to open connection to bus: %s\n",
                           error->message);
                g_error_free(error);
        }

        proxy = dbus_g_proxy_new_for_name(connection,
                                          "org.freedesktop.Telepathy.MissionControl",
                                          "/org/freedesktop/Telepathy/MissionControl",
                                          "org.freedesktop.Telepathy.MissionControl");

        g_return_if_fail(proxy != NULL);

        result = dbus_g_proxy_call(proxy, "GetPresence", &error,
                                   G_TYPE_INVALID,
                                   G_TYPE_UINT, &presence,
                                   G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        /* todo: abort if not online or chat? or dnd? */

        g_debug("presence: %d", presence);

        result = dbus_g_proxy_call(proxy, "SetPresence", &error,
                                   G_TYPE_UINT, presence,
                                   G_TYPE_STRING, message,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        g_return_if_fail(!check_result(result, error));

        g_object_unref(proxy);
}


void
im_set_status(const lastfm_track *track)
{
        /* todo: support customizable message format */
        char *message;
        message = g_strdup_printf("♫ %s - %s ♫", track->artist, track->title);
        if (do_pidgin) pidgin_set_status(message);
        if (do_gajim) gajim_set_status(message);
        if (do_gossip) gossip_set_status(message);
        if (do_telepathy) telepathy_set_status(message);
        g_free(message);
}

void
im_clear_status(void)
{
        if (do_pidgin) pidgin_set_status("");
        if (do_gajim) gajim_set_status("");
        if (do_gossip) gossip_set_status("");
        if (do_telepathy) telepathy_set_status("");
}
