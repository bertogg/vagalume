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
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VGL_TYPE_MAIN_MENU              (vgl_main_menu_get_type())
#define VGL_MAIN_MENU(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                         VGL_TYPE_MAIN_MENU, VglMainMenu))
#define VGL_MAIN_MENU_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass),  \
                                         VGL_TYPE_MAIN_MENU, VglMainMenuClass))
#define VGL_IS_MAIN_MENU(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                         VGL_TYPE_MAIN_MENU))
#define VGL_IS_MAIN_MENU_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass),  \
                                         VGL_TYPE_MAIN_MENU))
#define VGL_MAIN_MENU_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj),  \
                                         VGL_TYPE_MAIN_MENU, VglMainMenuClass))

typedef struct _VglMainMenu             VglMainMenu;
typedef struct _VglMainMenuClass        VglMainMenuClass;

GType
vgl_main_menu_get_type                  (void) G_GNUC_CONST;

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
