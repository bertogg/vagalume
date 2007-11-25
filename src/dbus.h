/*
 * dbus.h -- D-BUS interface
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#ifndef VAGALUME_DBUS_H
#define VAGALUME_DBUS_H

#include "config.h"
#include "globaldefs.h"

#define APP_DBUS_SERVICE "com.igalia." APP_NAME_LC
#define APP_DBUS_OBJECT "/com/igalia/" APP_NAME_LC
#define APP_DBUS_IFACE APP_DBUS_SERVICE
#define APP_DBUS_METHOD_PLAYURL "PlayUrl"
#define APP_DBUS_METHOD_PLAY "Play"
#define APP_DBUS_METHOD_STOP "Stop"
#define APP_DBUS_METHOD_SKIP "Skip"
#define APP_DBUS_METHOD_LOVETRACK "LoveTrack"
#define APP_DBUS_METHOD_BANTRACK "BanTrack"
#define APP_DBUS_METHOD_SHOWWINDOW "ShowWindow"
#define APP_DBUS_METHOD_HIDEWINDOW "HideWindow"
#define APP_DBUS_METHOD_CLOSEAPP "CloseApp"
#define APP_DBUS_METHOD_TOPAPP "top_application"

#ifdef HAVE_DBUS_SUPPORT

const char *lastfm_dbus_init(void);
void lastfm_dbus_close(void);

#else

const char *lastfm_dbus_init() { return NULL; }
void lastfm_dbus_close(void) { }

#endif

#endif
