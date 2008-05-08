/*
 * globaldefs.h -- Some global definitions
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
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
#define SB_PLUGIN_NAME_LC "vagalume-sb-plugin"
#define SB_PLUGIN_VERSION VERSION

#define APP_FULLNAME APP_NAME " " APP_VERSION " (" APP_OS "/" ARCH ")"

/* Client ID and version provided by Last.fm - don't change */
#define LASTFM_APP_ID "vag"
#define LASTFM_APP_VERSION "0.1"

#define APP_ICON PIXMAP_DIR "/vagalume.xpm"
#define APP_ICON_BIG ICON48_DIR "/vagalume.png"

#define THEME_ICONS_DIR APP_DATA_DIR "/icons"

#endif
