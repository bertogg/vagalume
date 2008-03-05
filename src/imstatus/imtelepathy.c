/* imtelepathy.c -- Set status message in telepathy via dbus
 * Copyright (C) 2008 Tim Wegener <twegener@fastmail.fm>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include <glib.h>
#include <dbus/dbus-glib.h>

#include "imstatus/imtelepathy.h"

/* todo: factor this out */
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
