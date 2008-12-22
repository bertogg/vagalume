/*
 * uimisc.h -- Misc UI-related functions
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
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

GtkWidget *compat_gtk_button_new(void);
gboolean tagwin_run(GtkWindow *parent, const char *user, char **newtags,
                    const GList *usertags, LastfmTrack *track,
                    request_type *type);
gboolean recommwin_run(GtkWindow *parent, char **user, char **message,
                       const GList *friends, const LastfmTrack *track,
                       request_type *type);
void ui_info_banner(GtkWindow *parent, const char *text);
void ui_info_dialog(GtkWindow *parent, const char *text);
void ui_warning_dialog(GtkWindow *parent, const char *text);
void ui_error_dialog(GtkWindow *parent, const char *text);
void ui_about_dialog (GtkWindow *parent);
char *ui_input_dialog(GtkWindow *parent, const char *title,
                      const char *text, const char *value);
gboolean ui_stop_after_dialog (GtkWindow *parent, StopAfterType *stopafter,
                               int *minutes);
char *ui_input_dialog_with_list(GtkWindow *parent, const char *title,
                                const char *text, const GList *elems,
                                const char *value);
void ui_usertag_dialog(GtkWindow *parent, char **user, char **tag,
                       const GList *userlist);
gboolean ui_edit_bookmark_dialog(GtkWindow *parent, char **name,
                                 char **url, gboolean add);
gboolean ui_usercfg_window(GtkWindow *parent, VglUserCfg **cfg);
gboolean ui_confirm_dialog(GtkWindow *parent, const char *text);
GtkWidget *ui_menu_item_create_from_icon(const gchar *icon_name,
                                         const gchar *label);
void flush_ui_events(void);

#endif
