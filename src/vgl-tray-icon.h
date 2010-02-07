/*
 * vgl-tray-icon.h -- Freedesktop tray icon
 *
 * Copyright (C) 2007-2008, 2010 Igalia, S.L.
 * Authors: Mario Sanchez Prada <msanchez@igalia.com>
 *          Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */
#ifndef _VGL_TRAY_ICON_H
#define _VGL_TRAY_ICON_H

#include <gtk/gtk.h>

#include "vgl-tray-icon.bp.h"
#include "playlist.h"
#include "controller.h"

G_BEGIN_DECLS

struct _VglTrayIcon
{
	GObject parent_instance;
        VglTrayIconPrivate *priv;
};

struct _VglTrayIconClass
{
	GObjectClass parent_class;
};

VglTrayIcon *
vgl_tray_icon_create                    (VglController *controller);

G_END_DECLS

#endif /* _VGL_TRAY_ICON_H */
