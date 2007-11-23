/*
 * globaldefs.h -- Some global definitions
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3
 */

#ifndef GLOBALDEFS_H
#define GLOBALDEFS_H

#include "config.h"

#define APP_NAME "Vagalume"
#define APP_NAME_LC "vagalume"
#define APP_VERSION VERSION
#define APP_OS KERNEL_NAME
#define APP_OS_LC KERNEL_NAME_LC

#define APP_FULLNAME APP_NAME " " APP_VERSION " (" APP_OS "/" ARCH ")"

#define APP_DBUS_SERVICE "com.igalia." APP_NAME_LC
#define APP_DBUS_OBJECT "/com/igalia/" APP_NAME_LC
#define APP_DBUS_IFACE APP_DBUS_SERVICE
#define APP_DBUS_METHOD_PLAYURL "PlayUrl"

/* Client ID and version provided by Last.fm - don't change */
#define LASTFM_APP_ID "vag"
#define LASTFM_APP_VERSION "0.1"

#ifndef VAGALUME_DATA_DIR
#define VAGALUME_DATA_DIR "/usr/share/vagalume"
#endif

#endif
