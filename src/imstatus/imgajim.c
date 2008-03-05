/* imgajim.c -- Set status message in gajim via dbus
 * Copyright (C) 2008 Tim Wegener <twegener@fastmail.fm>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include <glib.h>
#include <dbus/dbus-glib.h>

#include "imstatus/imgajim.h"

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
