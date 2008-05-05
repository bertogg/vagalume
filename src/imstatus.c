/*
 * imstatus.c -- set IM status to current track
 * Copyright (C) 2008 Tim Wegener <twegener@fastmail.fm>
 * Copyright (C) 2008 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <string.h>
#include "imstatus.h"
#include "util.h"

static char *saved_pidgin_status = NULL;
static char *saved_telepathy_status = NULL;
static char *saved_gajim_status = NULL;
static char *saved_gossip_status = NULL;

typedef struct {
        gboolean im_pidgin, im_gajim, im_gossip, im_telepathy;
        GString *msg;
        lastfm_track *track;
} ImStatusData;

static GStaticMutex imstatus_mutex = G_STATIC_MUTEX_INIT;

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

static DBusGProxy *
get_dbus_proxy(const char *dest, const char *objpath, const char *iface)
{
        g_return_val_if_fail (dest && objpath && iface, NULL);
        DBusGConnection *connection;
        DBusGProxy *proxy = NULL;
        GError *error = NULL;

        connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if (connection != NULL) {
                proxy = dbus_g_proxy_new_for_name(connection,
                                                  dest, objpath, iface);
        } else {
                g_warning("Failed to open connection to bus: %s",
                          (error && error->message) ?
                          error->message : "(unknown error)");
        }

        if (error != NULL) g_error_free(error);
        if (connection != NULL) dbus_g_connection_unref(connection);

        return proxy;
}

static void
gajim_set_status(const char *message)
{
        g_return_if_fail(message != NULL);
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        char *status;
        gboolean change_status_result;

        proxy = get_dbus_proxy("org.gajim.dbus",
                               "/org/gajim/dbus/RemoteObject",
                               "org.gajim.dbus.RemoteInterface");

        if (proxy == NULL) return;

        result = dbus_g_proxy_call (proxy, "get_status", &error,
                                    G_TYPE_STRING, "",
                                    G_TYPE_INVALID,
                                    G_TYPE_STRING, &status,
                                    G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        /* todo: abort if not online or chat? or dnd? */

        if (saved_gajim_status == NULL) {
                dbus_g_proxy_call(proxy, "get_status_message",
                                  &error,
                                  G_TYPE_STRING, "",
                                  G_TYPE_INVALID,
                                  G_TYPE_STRING, &saved_gajim_status,
                                  G_TYPE_INVALID);
        }

        g_debug("Gajim: setting status %s", message);

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

        g_free(status);
        g_object_unref(proxy);
}

static void
gossip_set_status(const char *message)
{
        g_return_if_fail(message != NULL);
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        char **chats;
        char *id;
        char *state_str;
        char *status;

        proxy = get_dbus_proxy("org.gnome.Gossip",
                               "/org/gnome/Gossip",
                               "org.gnome.Gossip");

        if (proxy == NULL) return;

        result = dbus_g_proxy_call(proxy, "GetOpenChats", &error,
                                   G_TYPE_INVALID,
                                   G_TYPE_STRV, &chats,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        id = *chats;

        result = dbus_g_proxy_call(proxy, "GetPresence",
                                   &error,
                                   G_TYPE_STRING, id,
                                   G_TYPE_INVALID,
                                   G_TYPE_STRING, &state_str,
                                   G_TYPE_STRING, &status,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        g_strfreev(chats);

        if (saved_gossip_status == NULL) {
                saved_gossip_status = status;
        } else {
                g_free(status);
        }

        /* todo: abort if not online or chat? or dnd? */

        g_debug("Gossip: setting status %s", message);

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
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        unsigned int current;
        int status;

        proxy = get_dbus_proxy("im.pidgin.purple.PurpleService",
                               "/im/pidgin/purple/PurpleObject",
                               "im.pidgin.purple.PurpleInterface");

        if (proxy == NULL) return;

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusGetCurrent",
                                   &error,
                                   G_TYPE_INVALID,
                                   G_TYPE_INT, &current,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusGetType",
                                   &error,
                                   G_TYPE_INT, current,
                                   G_TYPE_INVALID,
                                   G_TYPE_INT, &status,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        /* todo: abort if not online */

        if (saved_pidgin_status == NULL) {
                dbus_g_proxy_call(proxy, "PurpleSavedstatusGetMessage",
                                  &error,
                                  G_TYPE_INT, current,
                                  G_TYPE_INVALID,
                                  G_TYPE_STRING, &saved_pidgin_status,
                                  G_TYPE_INVALID);
        }

        g_debug("Pidgin: setting status %s", message);

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
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        unsigned int presence;

        proxy = get_dbus_proxy("org.freedesktop.Telepathy.MissionControl",
                               "/org/freedesktop/Telepathy/MissionControl",
                               "org.freedesktop.Telepathy.MissionControl");

        if (proxy == NULL) return;

        result = dbus_g_proxy_call(proxy, "GetPresence", &error,
                                   G_TYPE_INVALID,
                                   G_TYPE_UINT, &presence,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        if (saved_telepathy_status == NULL) {
                dbus_g_proxy_call(proxy, "GetPresenceMessage", &error,
                                  G_TYPE_INVALID,
                                  G_TYPE_STRING, &saved_telepathy_status,
                                  G_TYPE_INVALID);
        }

        /* todo: abort if not online or chat? or dnd? */

        g_debug("Telepathy: setting status %s", message);

        result = dbus_g_proxy_call(proxy, "SetPresence", &error,
                                   G_TYPE_UINT, presence,
                                   G_TYPE_STRING, message,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) return;

        g_object_unref(proxy);
}

static gboolean
im_set_status_idle(gpointer data)
{
        g_return_val_if_fail(data != NULL, FALSE);
        ImStatusData *d = (ImStatusData *) data;

        g_static_mutex_lock(&imstatus_mutex);

        /* Modify the template */
        string_replace_gstr(d->msg, "{artist}", d->track->artist);
        string_replace_gstr(d->msg, "{title}", d->track->title);

        /* Set the status */
        if (d->im_pidgin) pidgin_set_status(d->msg->str);
        if (d->im_gajim) gajim_set_status(d->msg->str);
        if (d->im_gossip) gossip_set_status(d->msg->str);
        if (d->im_telepathy) telepathy_set_status(d->msg->str);

        /* Cleanup */
        g_string_free(d->msg, TRUE);
        lastfm_track_unref(d->track);
        g_slice_free(ImStatusData, d);

        g_static_mutex_unlock(&imstatus_mutex);

        return FALSE;
}

static gboolean
im_clear_status_idle(gpointer data)
{
        g_return_val_if_fail(data != NULL, FALSE);
        ImStatusData *d = (ImStatusData *) data;
        g_static_mutex_lock(&imstatus_mutex);
        if (d->im_pidgin && saved_pidgin_status != NULL) {
                pidgin_set_status(saved_pidgin_status);
                g_free(saved_pidgin_status);
                saved_pidgin_status = NULL;
        }
        if (d->im_gajim && saved_gajim_status != NULL) {
                gajim_set_status(saved_gajim_status);
                g_free(saved_gajim_status);
                saved_gajim_status = NULL;
        }
        if (d->im_gossip && saved_gossip_status != NULL) {
                gossip_set_status(saved_gossip_status);
                g_free(saved_gossip_status);
                saved_gossip_status = NULL;
        }
        if (d->im_telepathy && saved_telepathy_status != NULL) {
                telepathy_set_status(saved_telepathy_status);
                g_free(saved_telepathy_status);
                saved_telepathy_status = NULL;
        }
        g_slice_free(ImStatusData, d);
        g_static_mutex_unlock(&imstatus_mutex);
        return FALSE;
}

void
im_set_status(const lastfm_usercfg *cfg, lastfm_track *track)
{
        g_return_if_fail(cfg != NULL && track != NULL);
        ImStatusData *data = g_slice_new(ImStatusData);
        data->im_pidgin = cfg->im_pidgin;
        data->im_gossip = cfg->im_gossip;
        data->im_telepathy = cfg->im_telepathy;
        data->im_gajim = cfg->im_gajim;
        data->msg = g_string_new(cfg->imstatus_template);
        data->track = lastfm_track_ref(track);
        g_idle_add(im_set_status_idle,data);

}

void
im_clear_status(const lastfm_usercfg *cfg)
{
        g_return_if_fail(cfg != NULL);
        ImStatusData *data = g_slice_new0(ImStatusData);
        data->im_pidgin = cfg->im_pidgin;
        data->im_gossip = cfg->im_gossip;
        data->im_telepathy = cfg->im_telepathy;
        data->im_gajim = cfg->im_gajim;
        g_idle_add(im_clear_status_idle,data);
}
