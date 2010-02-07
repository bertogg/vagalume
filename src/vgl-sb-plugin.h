/*
 * vgl-sb-plugin.h -- Status bar plugin for Vagalume
 *
 * Copyright (C) 2007-2008, 2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *          Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_SB_PLUGIN_H
#define VGL_SB_PLUGIN_H

#include "vgl-sb-plugin.bp.h"

struct _VglSbPlugin
{
  StatusbarItem parent;
  VglSbPluginPrivate *priv;
};

struct _VglSbPluginClass
{
  StatusbarItemClass parent_class;
};

#endif /* !VGL_SB_PLUGIN_H */
