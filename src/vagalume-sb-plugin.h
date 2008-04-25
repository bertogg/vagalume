/*
 * vagalume-sb-plugin.h -- Status bar plugin for Vagalume
 * Copyright (C) 2008 Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VAGALUME_SB_PLUGIN_H
#define VAGALUME_SB_PLUGIN_H

#include <glib.h>

#define VAGALUME_SB_PLUGIN_TYPE            (vagalume_sb_plugin_get_type ())
#define VAGALUME_SB_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VAGALUME_SB_PLUGIN_TYPE, VagalumeSbPlugin))
#define VAGALUME_SB_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  VAGALUME_SB_PLUGIN_TYPE, VagalumeSbPluginClass))
#define VAGALUME_IS_SB_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VAGALUME_SB_PLUGIN_TYPE))
#define VAGALUME_IS_SB_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  VAGALUME_SB_PLUGIN_TYPE))
#define VAGALUME_SB_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  VAGALUME_SB_PLUGIN_TYPE, VagalumeSbPluginClass))

typedef struct _VagalumeSbPlugin      VagalumeSbPlugin;
typedef struct _VagalumeSbPluginClass VagalumeSbPluginClass;

struct _VagalumeSbPlugin
{
  StatusbarItem parent;
};

struct _VagalumeSbPluginClass
{
  StatusbarItemClass parent_class;
};

GType vagalume_sb_plugin_get_type(void);

#endif /* !VAGALUME_SB_PLUGIN_H */
