/*
 * dbus.h -- D-BUS interface
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VAGALUME_DBUS_H
#define VAGALUME_DBUS_H

#include "config.h"
#include "globaldefs.h"
#include "controller.h"

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
#define APP_DBUS_METHOD_VOLUMEUP "VolumeUp"
#define APP_DBUS_METHOD_VOLUMEDOWN "VolumeDown"
#define APP_DBUS_METHOD_SETVOLUME "SetVolume"
#define APP_DBUS_METHOD_TOPAPP "top_application"
#define APP_DBUS_METHOD_REQUEST_STATUS "request_status"

#define SB_PLUGIN_DBUS_SERVICE "com.igalia.vagalume_sb_plugin"
#define SB_PLUGIN_DBUS_OBJECT "/com/igalia/vagalume_sb_plugin"
#define SB_PLUGIN_DBUS_IFACE SB_PLUGIN_DBUS_SERVICE
#define SB_PLUGIN_DBUS_METHOD_NOTIFY "notify"
#define SB_PLUGIN_DBUS_METHOD_NOTIFY_PLAYING "playing"
#define SB_PLUGIN_DBUS_METHOD_NOTIFY_STOPPED "stopped"
#define SB_PLUGIN_DBUS_METHOD_NOTIFY_STARTED "started"
#define SB_PLUGIN_DBUS_METHOD_NOTIFY_CLOSING "closing"

#ifdef HAVE_DBUS_SUPPORT

const char *lastfm_dbus_init(void);
void lastfm_dbus_close(void);
void lastfm_dbus_notify_playback(lastfm_track *track);
void lastfm_dbus_notify_started(void);
void lastfm_dbus_notify_closing(void);

#else

const char *lastfm_dbus_init(void) { return NULL; }
void lastfm_dbus_close(void) { }
void lastfm_dbus_notify_playback(lastfm_track *track) { }
void lastfm_dbus_notify_started() { }
void lastfm_dbus_notify_closing() { }

#endif /* HAVE_DBUS_SUPPORT */

#endif
