/*
 * imstatus.c -- set IM status to current track
 *
 * Copyright (C) 2008 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *          Tim Wegener <twegener@fastmail.fm>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "globaldefs.h"

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <gdk/gdk.h>
#include <string.h>
#include "imstatus.h"
#include "util.h"
#include "playlist.h"
#include "userconfig.h"

static char *saved_pidgin_status = NULL;
static char *saved_telepathy_status = NULL;
static char *saved_gajim_status = NULL;
static char *saved_gossip_status = NULL;

static gboolean im_pidgin = FALSE;
static gboolean im_telepathy = FALSE;
static gboolean im_gajim = FALSE;
static gboolean im_gossip = FALSE;

static GString *im_status_template = NULL;

static gboolean
error_happened                          (gboolean  code,
                                         GError   *error)
{
        g_return_val_if_fail(code || error != NULL, TRUE);
        gboolean retval = FALSE;
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
                retval = TRUE;
        }
        if (error != NULL) g_error_free(error);
        return retval;
}

static DBusGProxy *
get_dbus_proxy                          (const char *dest,
                                         const char *objpath,
                                         const char *iface)
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

static gboolean
gajim_set_status                        (const char *message)
{
        g_return_val_if_fail(message != NULL, FALSE);
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        char *status = NULL;
        gboolean change_status_result;
        gboolean retval = FALSE;

        proxy = get_dbus_proxy("org.gajim.dbus",
                               "/org/gajim/dbus/RemoteObject",
                               "org.gajim.dbus.RemoteInterface");

        if (proxy == NULL) goto cleanup;

        result = dbus_g_proxy_call (proxy, "get_status", &error,
                                    G_TYPE_STRING, "",
                                    G_TYPE_INVALID,
                                    G_TYPE_STRING, &status,
                                    G_TYPE_INVALID);
        if (error_happened(result, error)) goto cleanup;

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
        if (error_happened(result, error)) goto cleanup;

        retval = TRUE;
cleanup:
        g_free(status);
        if (proxy != NULL) g_object_unref(proxy);
        return retval;
}

static gboolean
gossip_set_status                       (const char *message)
{
        g_return_val_if_fail(message != NULL, FALSE);
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        char **chats = NULL;
        char *state_str = NULL;
        char *status = NULL;
        const char *id;
        gboolean retval = FALSE;

        proxy = get_dbus_proxy("org.gnome.Gossip",
                               "/org/gnome/Gossip",
                               "org.gnome.Gossip");

        if (proxy == NULL) goto cleanup;

        result = dbus_g_proxy_call(proxy, "GetOpenChats", &error,
                                   G_TYPE_INVALID,
                                   G_TYPE_STRV, &chats,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) goto cleanup;

        id = *chats;

        result = dbus_g_proxy_call(proxy, "GetPresence",
                                   &error,
                                   G_TYPE_STRING, id,
                                   G_TYPE_INVALID,
                                   G_TYPE_STRING, &state_str,
                                   G_TYPE_STRING, &status,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) goto cleanup;

        if (saved_gossip_status == NULL) {
                saved_gossip_status = status;
                status = NULL;
        }

        /* todo: abort if not online or chat? or dnd? */

        g_debug("Gossip: setting status %s", message);

        result = dbus_g_proxy_call(proxy, "SetPresence",
                                   &error,
                                   G_TYPE_STRING, state_str,
                                   G_TYPE_STRING, message,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) goto cleanup;

        retval = TRUE;
cleanup:
        g_free(state_str);
        g_free(status);
        g_strfreev(chats);
        if (proxy != NULL) g_object_unref(proxy);
        return retval;
}

static gboolean
pidgin_set_status                       (const char *message)
{
        g_return_val_if_fail(message != NULL, FALSE);
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        unsigned int current;
        int status;
        gboolean retval = FALSE;

        proxy = get_dbus_proxy("im.pidgin.purple.PurpleService",
                               "/im/pidgin/purple/PurpleObject",
                               "im.pidgin.purple.PurpleInterface");

        if (proxy == NULL) goto cleanup;

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusGetCurrent",
                                   &error,
                                   G_TYPE_INVALID,
                                   G_TYPE_INT, &current,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) goto cleanup;

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusGetType",
                                   &error,
                                   G_TYPE_INT, current,
                                   G_TYPE_INVALID,
                                   G_TYPE_INT, &status,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) goto cleanup;

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
        if (error_happened(result, error)) goto cleanup;

        result = dbus_g_proxy_call(proxy, "PurpleSavedstatusActivate",
                                   &error,
                                   G_TYPE_INT, current,
                                   G_TYPE_INVALID,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) goto cleanup;

        retval = TRUE;
cleanup:
        if (proxy != NULL) g_object_unref(proxy);
        return retval;
}


static gboolean
telepathy_set_status                    (const char *message)
{
        g_return_val_if_fail(message != NULL, FALSE);
        DBusGProxy *proxy;
        GError *error = NULL;
        gboolean result;
        unsigned int presence;
        gboolean retval = FALSE;

        proxy = get_dbus_proxy("org.freedesktop.Telepathy.MissionControl",
                               "/org/freedesktop/Telepathy/MissionControl",
                               "org.freedesktop.Telepathy.MissionControl");

        if (proxy == NULL) goto cleanup;

        result = dbus_g_proxy_call(proxy, "GetPresence", &error,
                                   G_TYPE_INVALID,
                                   G_TYPE_UINT, &presence,
                                   G_TYPE_INVALID);
        if (error_happened(result, error)) goto cleanup;

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
        if (error_happened(result, error)) goto cleanup;

        retval = TRUE;
cleanup:
        if (proxy != NULL) g_object_unref(proxy);
        return retval;
}

static gboolean
im_set_status_idle                      (gpointer data)
{
        LastfmTrack *track = (LastfmTrack *) data;
        GString *msg;

        g_return_val_if_fail (data != NULL, FALSE);

        msg = g_string_sized_new (200);
        g_string_assign (msg, im_status_template->str);

        /* Modify the template */
        string_replace_gstr(msg, "{artist}", track->artist);
        string_replace_gstr(msg, "{title}", track->title);
        string_replace_gstr(msg, "{station}", track->pls_title);
        string_replace_gstr(msg, "{version}", APP_VERSION);

        /* Set the status */
        gdk_threads_enter();
        if (im_pidgin) pidgin_set_status (msg->str);
        if (im_gajim) gajim_set_status (msg->str);
        if (im_gossip) gossip_set_status (msg->str);
        if (im_telepathy) telepathy_set_status (msg->str);
        gdk_threads_leave();

        /* Cleanup */
        g_string_free (msg, TRUE);
        lastfm_track_unref (track);

        return FALSE;
}

static gboolean
im_clear_status_idle                    (gpointer data)
{
        gdk_threads_enter();
        if (saved_pidgin_status != NULL) {
                if (pidgin_set_status(saved_pidgin_status)) {
                        g_free(saved_pidgin_status);
                        saved_pidgin_status = NULL;
                }
        }
        if (saved_gajim_status != NULL) {
                if (gajim_set_status(saved_gajim_status)) {
                        g_free(saved_gajim_status);
                        saved_gajim_status = NULL;
                }
        }
        if (saved_gossip_status != NULL) {
                if (gossip_set_status(saved_gossip_status)) {
                        g_free(saved_gossip_status);
                        saved_gossip_status = NULL;
                }
        }
        if (saved_telepathy_status != NULL) {
                if (telepathy_set_status(saved_telepathy_status)) {
                        g_free(saved_telepathy_status);
                        saved_telepathy_status = NULL;
                }
        }
        gdk_threads_leave();
        return FALSE;
}

static void
usercfg_changed_cb                      (VglController *ctrl,
                                         VglUserCfg    *cfg)
{
        if (im_pidgin    != cfg->im_pidgin    ||
            im_telepathy != cfg->im_telepathy ||
            im_gajim     != cfg->im_gajim     ||
            im_gossip    != cfg->im_gossip    ||
            !g_str_equal (im_status_template->str,
                          cfg->imstatus_template)) {

                LastfmTrack *np = controller_get_current_track ();
                im_pidgin    = cfg->im_pidgin;
                im_telepathy = cfg->im_telepathy;
                im_gajim     = cfg->im_gajim;
                im_gossip    = cfg->im_gossip;
                g_string_assign (im_status_template, cfg->imstatus_template);

                g_idle_add (im_clear_status_idle, NULL);
                if (np != NULL) {
                        g_idle_add (im_set_status_idle, lastfm_track_ref (np));
                }
        }
}

static void
player_stopped_cb                       (VglController *ctrl,
                                         gpointer       data)
{
        g_idle_add (im_clear_status_idle, NULL);
}

static void
track_started_cb                        (VglController *ctrl,
                                         LastfmTrack   *track,
                                         gpointer       data)
{
        g_idle_add (im_set_status_idle, lastfm_track_ref (track));
}

static void
controller_destroyed_cb                 (gpointer  data,
                                         GObject  *controller)
{
        g_string_free (im_status_template, TRUE);
        im_status_template = NULL;
}

void
im_status_init                          (VglController *controller)
{
        g_return_if_fail (VGL_IS_CONTROLLER (controller));
        im_status_template = g_string_sized_new (80);
        g_signal_connect (controller, "usercfg-changed",
                          G_CALLBACK (usercfg_changed_cb), NULL);
        g_signal_connect (controller, "player-stopped",
                          G_CALLBACK (player_stopped_cb), NULL);
        g_signal_connect (controller, "track-started",
                          G_CALLBACK (track_started_cb), NULL);
        g_object_weak_ref (G_OBJECT (controller),
                           controller_destroyed_cb, NULL);
}
