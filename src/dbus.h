/*
 * dbus.h -- D-BUS interface
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VAGALUME_DBUS_H
#define VAGALUME_DBUS_H

#include "globaldefs.h"
#include "playlist.h"

#define APP_DBUS_SERVICE "com.igalia." APP_NAME_LC
#define APP_DBUS_OBJECT "/com/igalia/" APP_NAME_LC
#define APP_DBUS_IFACE APP_DBUS_SERVICE

/* D-Bus methods */
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

/* D-Bus signals */
#define APP_DBUS_SIGNAL_NOTIFY "notify"
#define APP_DBUS_SIGNAL_NOTIFY_PLAYING "playing"
#define APP_DBUS_SIGNAL_NOTIFY_STOPPED "stopped"
#define APP_DBUS_SIGNAL_NOTIFY_STARTED "started"
#define APP_DBUS_SIGNAL_NOTIFY_CLOSING "closing"

#ifdef HAVE_GSD_MEDIA_PLAYER_KEYS

/* Gnome Settings Daemon - Media Player Keys D-Bus interface */
#define GSD_DBUS_SERVICE "org.gnome.SettingsDaemon"
#define GSD_DBUS_MK_OBJECT "/org/gnome/SettingsDaemon/MediaKeys"
#define GSD_DBUS_MK_IFACE GSD_DBUS_SERVICE ".MediaKeys"

#define GSD_DBUS_MK_GRAB_KEYS "GrabMediaPlayerKeys"
#define GSD_DBUS_MK_RELEASE_KEYS "ReleaseMediaPlayerKeys"
#define GSD_DBUS_MK_KEYPRESSED "MediaPlayerKeyPressed"
#define GSD_DBUS_MK_KEYPRESSED_STOP "Stop"
#define GSD_DBUS_MK_KEYPRESSED_PLAY "Play"
#define GSD_DBUS_MK_KEYPRESSED_PREVIOUS "Previous"
#define GSD_DBUS_MK_KEYPRESSED_NEXT "Next"

#endif /* HAVE_GSD_MEDIA_PLAYER_KEYS */

typedef enum {
        DBUS_INIT_OK,
        DBUS_INIT_ERROR,
        DBUS_INIT_ALREADY_RUNNING
} DbusInitReturnCode;

#ifdef HAVE_DBUS_SUPPORT

DbusInitReturnCode lastfm_dbus_init(void);
void lastfm_dbus_close(void);
void lastfm_dbus_notify_playback(LastfmTrack *track);
void lastfm_dbus_notify_started(void);
void lastfm_dbus_notify_closing(void);
void lastfm_dbus_play_radio_url(const char *url);

#else

DbusInitReturnCode lastfm_dbus_init(void) { return DBUS_INIT_OK; }
void lastfm_dbus_close(void) { }
void lastfm_dbus_notify_playback(LastfmTrack *track) { }
void lastfm_dbus_notify_started(void) { }
void lastfm_dbus_notify_closing(void) { }
void lastfm_dbus_play_radio_url(const char *url) { }

#endif /* HAVE_DBUS_SUPPORT */

#endif
