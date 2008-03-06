/*
 * imstatus.c -- set IM status to current track
 * Copyright (C) 2008 Tim Wegener <twegener@fastmail.fm>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
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

static gboolean
error_happened(gboolean code, GError *error)
{
        g_return_val_if_fail(code || error != NULL, TRUE);
        if (!code) {
                if (error->domain == DBUS_GERROR &&
                    error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
                        g_warning("Caught remote method exception %s: %s",
                                  dbus_g_error_get_name (error),
                                  error->message);
                } else {
                        g_warning("Error: (%d) %s",
                                  error->code, error->message);
                }
                g_error_free(error);
                return TRUE;
        }
        return FALSE;
}

static void
gajim_set_status(const char *message)
{
        g_return_if_fail(message != NULL);
        DBusGConnection *connection;
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        char *status;
        gboolean change_status_result;

        g_debug("gajim_set_status");

        connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if (connection == NULL) {
                g_warning("Failed to open connection to bus: %s",
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
        if (error_happened(result, error)) return;

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
        if (error_happened(result, error)) return;

        g_debug("change_status_result: %d", result);

        g_free(status);
        g_object_unref(proxy);
}

static void
gossip_set_status(const char *message)
{
        g_return_if_fail(message != NULL);
        DBusGConnection *connection;
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        char **chats;
        char *id;
        char *state_str;
        char *status;

        g_debug("gossip_set_status");

        connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if (connection == NULL) {
                g_warning("Failed to open connection to bus: %s",
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
        if (error_happened(result, error)) return;

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
        if (error_happened(result, error)) return;

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
        if (error_happened(result, error)) return;

        g_object_unref(proxy);
}

static void
pidgin_set_status(const char *message)
{
        g_return_if_fail(message != NULL);
        DBusGConnection *connection;
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        unsigned int current;
        int status;

        connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if (connection == NULL) {
                g_warning("Failed to open connection to bus: %s",
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
        if (error_happened(result, error)) return;

        g_debug("Current: %d", current);

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusGetType",
                                   &error,
                                   G_TYPE_INT, current,
                                   G_TYPE_INVALID,
                                   G_TYPE_INT, &status,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        /* todo: abort if not online */

        g_debug("Type: %d", status);
        g_debug("message: %s", message);

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusSetMessage",
                                   &error,
                                   G_TYPE_INT, current,
                                   G_TYPE_STRING, message,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusActivate",
                                   &error,
                                   G_TYPE_INT, current,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        g_object_unref(proxy);
}


static void
telepathy_set_status(const char *message)
{
        g_return_if_fail(message != NULL);
        DBusGConnection *connection;
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        unsigned int presence;

        g_debug("telepathy_set_status");

        connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if (connection == NULL) {
                g_warning("Failed to open connection to bus: %s",
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
        if (error_happened(result, error)) return;

        /* todo: abort if not online or chat? or dnd? */

        g_debug("presence: %d", presence);

        result = dbus_g_proxy_call(proxy, "SetPresence", &error,
                                   G_TYPE_UINT, presence,
                                   G_TYPE_STRING, message,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        g_object_unref(proxy);
}


void
im_set_status(const lastfm_usercfg *cfg, const lastfm_track *track)
{
        g_return_if_fail(cfg != NULL && track != NULL);
        /* todo: support customizable message format */
        char *message;
        message = g_strdup_printf("\342\231\253 %s - %s \342\231\253",
                                  track->artist, track->title);
        if (cfg->im_pidgin) pidgin_set_status(message);
        if (cfg->im_gajim) gajim_set_status(message);
        if (cfg->im_gossip) gossip_set_status(message);
        if (cfg->im_telepathy) telepathy_set_status(message);
        g_free(message);
}

void
im_clear_status(const lastfm_usercfg *cfg)
{
        g_return_if_fail(cfg != NULL);
        if (cfg->im_pidgin) pidgin_set_status("");
        if (cfg->im_gajim) gajim_set_status("");
        if (cfg->im_gossip) gossip_set_status("");
        if (cfg->im_telepathy) telepathy_set_status("");
}
