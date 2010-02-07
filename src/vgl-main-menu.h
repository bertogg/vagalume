/*
 * vgl-main-menu.h -- Main menu
 *
 * Copyright (C) 2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef VGL_MAIN_MENU_H
#define VGL_MAIN_MENU_H

#include "vgl-main-window.h"
#include "vgl-main-menu.bp.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

VglMainMenu *
vgl_main_menu_new                       (GtkAccelGroup *accel);

void
vgl_main_menu_set_track_as_loved        (VglMainMenu *menu);

void
vgl_main_menu_set_track_as_added_to_playlist
                                        (VglMainMenu *menu,
                                         gboolean     added);

void
vgl_main_menu_set_state                 (VglMainMenu        *menu,
                                         VglMainWindowState  state,
                                         const LastfmTrack  *t,
                                         const char         *radio_url);

G_END_DECLS

#endif                                  /* VGL_MAIN_MENU_H */
