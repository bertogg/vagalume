/*
 * uimisc.h -- Misc UI-related functions
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#ifndef UIMISC_H
#define UIMISC_H

#include <gtk/gtk.h>
#include "userconfig.h"
#include "playlist.h"

char *tagwin_get_tags(GtkWindow *parent, const char *user, GList *usertags,
                      const lastfm_track *track, request_type *type);
void ui_info_banner(GtkWindow *parent, const char *text);
void ui_info_dialog(GtkWindow *parent, const char *text);
void ui_warning_dialog(GtkWindow *parent, const char *text);
void ui_error_dialog(GtkWindow *parent, const char *text);
char *ui_input_dialog(GtkWindow *parent, const char *title,
                      const char *text, const char *value);
char *ui_input_dialog_with_list(GtkWindow *parent, const char *title,
                                const char *text, GList *elems,
                                const char *value);
gboolean ui_usercfg_dialog(GtkWindow *parent, lastfm_usercfg **cfg);
gboolean ui_confirm_dialog(GtkWindow *parent, const char *text);
void flush_ui_events(void);

#endif
