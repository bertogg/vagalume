/* impidgin.c -- Set status message in Pidgin via dbus
 * Copyright (C) 2008 Tim Wegener <twegener@fastmail.fm>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include <glib.h>
#include <dbus/dbus-glib.h>

#include "imstatus/impidgin.h"

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

/* todo: factor this out */
static int
check_result(gboolean code, GError *error)
{
        if (!code) {
                if (error->domain == DBUS_GERROR &&
                    error->code == DBUS_GERROR_REMOTE_EXCEPTION)
                        g_printerr("Caught remote method exception %s: %s",
                                   dbus_g_error_get_name (error),
                                   error->message);
                else
                        g_printerr("Error: (%d) %s\n",
                                   error->code, error->message);
                g_error_free(error);
                return(1);
        }
        return(0);
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
