/*
 * globaldefs.h -- Some global definitions
 *
 * Copyright (C) 2007-2010 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef GLOBALDEFS_H
#define GLOBALDEFS_H

#include "config.h"

#define APP_NAME "Vagalume"
#define APP_NAME_LC "vagalume"
#define APP_VERSION VERSION
#define APP_OS KERNEL_NAME
#define APP_OS_LC KERNEL_NAME_LC
#define SB_PLUGIN_NAME "Vagalume Status Bar Plugin"
#define SB_PLUGIN_NAME_LC "vgl-sb-plugin"
#define SB_PLUGIN_VERSION VERSION

#define APP_FULLNAME APP_NAME " " APP_VERSION " (" APP_OS "/" ARCH ")"

/* Client ID and version provided by Last.fm - don't change */
#define LASTFM_APP_ID "vag"
#define LASTFM_APP_VERSION "0.1"

#define APP_ICON PIXMAP_DIR "/vagalume.xpm"
#define APP_ICON_BIG ICON48_DIR "/vagalume.png"

#define THEME_ICONS_DIR APP_DATA_DIR "/icons"

#ifdef MAEMO5
#    define FINGER_SIZE (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH)
#endif

#endif
