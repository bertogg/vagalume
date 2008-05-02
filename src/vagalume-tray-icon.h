/*
 * vagalume-tray-icon.h -- Freedesktop tray icon
 * Copyright (C) 2008 Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */
#ifndef _VAGALUME_TRAY_ICON_H
#define _VAGALUME_TRAY_ICON_H

#include <gtk/gtk.h>

#include "playlist.h"

G_BEGIN_DECLS

#define    VAGALUME_TRAY_ICON_TYPE           (vagalume_tray_icon_get_type())
#define    VAGALUME_TRAY_ICON(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj,   VAGALUME_TRAY_ICON_TYPE, VagalumeTrayIcon))
#define    VAGALUME_TRAY_ICON_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST(klass,    VAGALUME_TRAY_ICON_TYPE, VagalumeTrayIconClass))
#define    VAGALUME_IS_TRAY_ICON(obj)           (G_TYPE_CHECK_INSTANCE_TYPE(obj,   VAGALUME_TRAY_ICON_TYPE))
#define    VAGALUME_IS_TRAY_ICON_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),  VAGALUME_TRAY_ICON_TYPE))
#define    VAGALUME_TRAY_ICON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VAGALUME_TRAY_ICON_TYPE, VagalumeTrayIconClass))

typedef struct _VagalumeTrayIcon VagalumeTrayIcon;
typedef struct _VagalumeTrayIconClass VagalumeTrayIconClass;

struct _VagalumeTrayIcon
{
	GObject parent_instance;
};

struct _VagalumeTrayIconClass
{
	GObjectClass parent_class;
};


GType vagalume_tray_icon_get_type(void) G_GNUC_CONST;


/* Public methods */

VagalumeTrayIcon *vagalume_tray_icon_create (void);
void vagalume_tray_icon_notify_playback (VagalumeTrayIcon *vti, lastfm_track *track);
void vagalume_tray_icon_show_notifications (VagalumeTrayIcon *vti, gboolean show_notifications);

G_END_DECLS

#endif /* _VAGALUME_TRAY_ICON_H */
