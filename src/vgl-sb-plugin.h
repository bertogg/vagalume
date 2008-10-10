/*
 * vgl-sb-plugin.h -- Status bar plugin for Vagalume
 * Copyright (C) 2008 Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_SB_PLUGIN_H
#define VGL_SB_PLUGIN_H

#include <glib.h>

#define VGL_SB_PLUGIN_TYPE            (vgl_sb_plugin_get_type ())
#define VGL_SB_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VGL_SB_PLUGIN_TYPE, VglSbPlugin))
#define VGL_SB_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  VGL_SB_PLUGIN_TYPE, VglSbPluginClass))
#define VGL_IS_SB_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VGL_SB_PLUGIN_TYPE))
#define VGL_IS_SB_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  VGL_SB_PLUGIN_TYPE))
#define VGL_SB_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  VGL_SB_PLUGIN_TYPE, VglSbPluginClass))

typedef struct _VglSbPlugin        VglSbPlugin;
typedef struct _VglSbPluginClass   VglSbPluginClass;
typedef struct _VglSbPluginPrivate VglSbPluginPrivate;

struct _VglSbPlugin
{
  StatusbarItem parent;
};

struct _VglSbPluginClass
{
  StatusbarItemClass parent_class;
  VglSbPluginPrivate *priv;
};

GType vgl_sb_plugin_get_type(void);

#endif /* !VGL_SB_PLUGIN_H */
