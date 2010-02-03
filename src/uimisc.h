/*
 * uimisc.h -- Misc UI-related functions
 *
 * Copyright (C) 2007-2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef UIMISC_H
#define UIMISC_H

#include <gtk/gtk.h>
#include "userconfig.h"
#include "playlist.h"
#include "controller.h"
#include "lastfm-ws.h"

#define RECOMMEND_ITEM_STRING _("_Recommend to...")
#define TAG_ITEM_STRING _("_Tags...")
#define ADD_TO_PLS_ITEM_STRING _("_Add to playlist")
#define LOVE_ITEM_STRING _("_Love this track")
#define BAN_ITEM_STRING _("_Ban this track")

#define RECOMMEND_ITEM_ICON_NAME "mail-message-new"
#define TAG_ITEM_ICON_NAME "accessories-text-editor"
#define ADD_TO_PLS_ITEM_ICON_NAME "list-add"
#define LOVE_ITEM_ICON_NAME "emblem-favorite"
#define BAN_ITEM_ICON_NAME "process-stop"

G_BEGIN_DECLS

GtkWidget *
compat_gtk_button_new                   (void);

gboolean
tagwin_run                              (GtkWindow             *parent,
                                         const char            *user,
                                         char                 **newtags,
                                         const GList           *usertags,
                                         LastfmWsSession       *ws_session,
                                         LastfmTrack           *track,
                                         LastfmTrackComponent  *type);

gboolean
recommwin_run                           (GtkWindow             *parent,
                                         char                 **user,
                                         char                 **message,
                                         const GList           *friends,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent  *type);

void
ui_info_banner                          (GtkWindow  *parent,
                                         const char *text);

void
ui_info_dialog                          (GtkWindow  *parent,
                                         const char *text);

void
ui_warning_dialog                       (GtkWindow  *parent,
                                         const char *text);

void
ui_error_dialog                         (GtkWindow  *parent,
                                         const char *text);

void
ui_about_dialog                         (GtkWindow *parent);

char *
ui_input_dialog                         (GtkWindow  *parent,
                                         const char *title,
                                         const char *text,
                                         const char *value);

gboolean
ui_stop_after_dialog                    (GtkWindow     *parent,
                                         StopAfterType *stopafter,
                                         int           *minutes);

char *
ui_input_dialog_with_list               (GtkWindow   *parent,
                                         const char  *title,
                                         const char  *text,
                                         const GList *elems,
                                         const char  *value);

char *
ui_select_servers_file                  (GtkWindow *parent);

void
ui_usertag_dialog                       (GtkWindow    *parent,
                                         char        **user,
                                         char        **tag,
                                         const GList  *userlist);

gboolean
ui_edit_bookmark_dialog                 (GtkWindow  *parent,
                                         char      **name,
                                         char      **url,
                                         gboolean    add);

gboolean
ui_usercfg_window                       (GtkWindow   *parent,
                                         VglUserCfg **cfg);

gboolean
ui_confirm_dialog                       (GtkWindow  *parent,
                                         const char *text);

GtkWidget *
ui_menu_item_create_from_icon           (const gchar *icon_name,
                                         const gchar *label);

/* Private functions (shaerd between uimisc implementations) */

GtkDialog *
ui_base_dialog                          (GtkWindow  *parent,
                                         const char *title);

char *
ui_select_download_dir                  (GtkWindow  *parent,
                                         const char *curdir);

GtkTreeModel *
ui_create_options_list                  (const GList *elems);

typedef enum {
        TAGCOMBO_STATE_NULL = 0,
        TAGCOMBO_STATE_LOADING,
        TAGCOMBO_STATE_READY
} TagComboState;

G_END_DECLS

#endif
