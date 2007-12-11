/*
 * connection.c -- Internet connection handling
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#include "connection.h"
#include "controller.h"
#include <conicconnection.h>
#include <conicconnectionevent.h>

static ConIcConnection *con_ic_connection = NULL;
static gboolean is_online = FALSE;

/*
 * Data passed to the ConIc signal handler when the user tries to
 * go online
 */
typedef struct {
        connection_go_online_cb cb;
        gpointer cbdata;
} connection_go_online_handler_data;

/**
 * Handler called each time the device goes online or offline. This
 * will update the is_online variable and disconnect the player if
 * needed
 */
static void
con_ic_status_handler(ConIcConnection *conn, ConIcConnectionEvent *event,
                      gpointer data)
{
        ConIcConnectionStatus status;
        status = con_ic_connection_event_get_status(event);
 	is_online = (status == CON_IC_STATUS_CONNECTED);
        if (!is_online) {
                gdk_threads_enter();
                controller_disconnect();
                gdk_threads_leave();
        }
}

/**
 * This function initializes the connection manager
 * @return An error message, or NULL if everything went OK
 */
const char *
connection_init(void)
{
	con_ic_connection = con_ic_connection_new();
	if (con_ic_connection == NULL) {
                return "Error initializing internet connection manager";
        }
        g_signal_connect (con_ic_connection, "connection-event",
                          G_CALLBACK (con_ic_status_handler), NULL);
        return NULL;
}

/**
 * Handler called after the user tries to connect the device.
 * This handler will be removed here so it won't be called again
 * @param conn The connection
 * @param event The connection event
 * @param userdata The callback to call from here
 */
static void
connection_go_online_handler(ConIcConnection *conn,
                             ConIcConnectionEvent *event, gpointer userdata)
{
        connection_go_online_handler_data *data;
        gulong id;
        data = (connection_go_online_handler_data *) userdata;
        id = g_signal_handler_find(conn, G_SIGNAL_MATCH_DATA, 0, 0,
                                   NULL, NULL, data);
        g_signal_handler_disconnect(conn, id);
        (*(data->cb))(data->cbdata);
        g_slice_free(con_ic_cb_handler_data, data);
}

/**
 * Connects the device if it's disconnected and then call the callback
 * @param cb The callback
 * @param cbdata The callback's data
 */
void
connection_go_online(connection_go_online_cb cb, gpointer cbdata)
{
        if (is_online) {
                (*cb)(cbdata);
                return;
        }
        connection_go_online_handler_data *data;
        data = g_slice_new(connection_go_online_handler_data);
        data->cb = cb;
        data->cbdata = cbdata;
        g_signal_connect(con_ic_connection, "connection-event",
                         G_CALLBACK(connection_go_online_handler), data);
        con_ic_connection_connect(con_ic_connection,
                                  CON_IC_CONNECT_FLAG_NONE);
}
