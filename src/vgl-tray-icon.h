/*
 * vgl-tray-icon.h -- Freedesktop tray icon
 * Copyright (C) 2008 Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */
#ifndef _VGL_TRAY_ICON_H
#define _VGL_TRAY_ICON_H

#include <gtk/gtk.h>

#include "playlist.h"

G_BEGIN_DECLS

#define    VGL_TRAY_ICON_TYPE           (vgl_tray_icon_get_type())
#define    VGL_TRAY_ICON(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj,   VGL_TRAY_ICON_TYPE, VglTrayIcon))
#define    VGL_TRAY_ICON_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST(klass,    VGL_TRAY_ICON_TYPE, VglTrayIconClass))
#define    VGL_IS_TRAY_ICON(obj)           (G_TYPE_CHECK_INSTANCE_TYPE(obj,   VGL_TRAY_ICON_TYPE))
#define    VGL_IS_TRAY_ICON_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),  VGL_TRAY_ICON_TYPE))
#define    VGL_TRAY_ICON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VGL_TRAY_ICON_TYPE, VglTrayIconClass))

typedef struct _VglTrayIcon VglTrayIcon;
typedef struct _VglTrayIconClass VglTrayIconClass;

struct _VglTrayIcon
{
	GObject parent_instance;
};

struct _VglTrayIconClass
{
	GObjectClass parent_class;
};


GType vgl_tray_icon_get_type(void) G_GNUC_CONST;


/* Public methods */

VglTrayIcon *vgl_tray_icon_create (void);
void vgl_tray_icon_notify_playback (VglTrayIcon *vti, lastfm_track *track);
void vgl_tray_icon_show_notifications (VglTrayIcon *vti, gboolean show_notifications);

G_END_DECLS

#endif /* _VGL_TRAY_ICON_H */
