/*
 * compat.h -- Implementations of some common functions for old systems
 *
 * Copyright (C) 2009, 2011 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_COMPAT_H
#define VGL_COMPAT_H

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifndef GDK_KEY_space
#        define GDK_KEY_space GDK_space
#        define GDK_KEY_Right GDK_Right
#        define GDK_KEY_KP_Add GDK_KP_Add
#        define GDK_KEY_KP_Subtract GDK_KP_Subtract
#        define GDK_KEY_plus GDK_plus
#        define GDK_KEY_minus GDK_minus
#        define GDK_KEY_F6 GDK_F6
#        define GDK_KEY_F7 GDK_F7
#        define GDK_KEY_F8 GDK_F8
#        define GDK_KEY_b GDK_b
#        define GDK_KEY_l GDK_l
#        define GDK_KEY_r GDK_r
#        define GDK_KEY_t GDK_t
#        define GDK_KEY_a GDK_a
#        define GDK_KEY_p GDK_p
#        define GDK_KEY_q GDK_q
#endif /* GDK_KEY_space */

#ifndef HAVE_GDK_THREADS_ADD_TIMEOUT_SECONDS
#        define gdk_threads_add_timeout_seconds(A,B,C) \
                gdk_threads_add_timeout ((A)*1000,B,C)
#endif

#ifndef HAVE_GDK_THREADS_ADD_API
G_BEGIN_DECLS

guint
gdk_threads_add_idle_full               (gint           priority,
		                         GSourceFunc    function,
		                         gpointer       data,
		                         GDestroyNotify notify);

guint
gdk_threads_add_idle                    (GSourceFunc function,
		                         gpointer    data);

guint
gdk_threads_add_timeout_full            (gint           priority,
                                         guint          interval,
                                         GSourceFunc    function,
                                         gpointer       data,
                                         GDestroyNotify notify);

guint
gdk_threads_add_timeout                 (guint       interval,
                                         GSourceFunc function,
                                         gpointer    data);

G_END_DECLS
#endif /* HAVE_GDK_THREADS_ADD_API */

#ifndef HAVE_GTK_DIALOG_GET_CONTENT_AREA

#     define gtk_dialog_get_content_area(X) (((GtkDialog *) (X))->vbox)
#     define gtk_dialog_get_action_area(X)  (((GtkDialog *) (X))->action_area)

#endif /* HAVE_GTK_DIALOG_GET_CONTENT_AREA */

#ifndef HAVE_GTK_TOOLTIP
G_BEGIN_DECLS

void
gtk_widget_set_tooltip_text             (GtkWidget   *widget,
                                         const gchar *text);

G_END_DECLS
#endif /* HAVE_GTK_TOOLTIP */

#if !GTK_CHECK_VERSION(2,16,0)
#        define gtk_status_icon_set_tooltip_text(X,Y) \
                gtk_status_icon_set_tooltip(X,Y)
#endif /* !GTK_CHECK_VERSION(2,16,0) */

#ifdef GTK_WIDGET_IS_SENSITIVE
#        define gtk_widget_is_sensitive(X) GTK_WIDGET_IS_SENSITIVE(X)
#endif /* GTK_WIDGET_IS_SENSITIVE */

#ifndef GTK_TYPE_PROGRESS
#        define gtk_progress_set_text_alignment(X,Y,Z)
#endif /* GTK_TYPE_PROGRESS */

#if !GTK_CHECK_VERSION(3,0,0)
#        define gtk_progress_bar_set_show_text(X,Y)
#endif /* !GTK_CHECK_VERSION(3,0,0) */

#ifndef HAVE_GSTRCMP0
G_BEGIN_DECLS

int
g_strcmp0                               (const char *str1,
                                         const char *str2);

G_END_DECLS
#endif

#ifndef GTK_TYPE_COMBO_BOX_TEXT
#        define GtkComboBoxText GtkComboBox
#        define GTK_COMBO_BOX_TEXT(X) GTK_COMBO_BOX(X)
#        define gtk_combo_box_text_new gtk_combo_box_new_text
#        define gtk_combo_box_text_append_text(X,Y) \
                gtk_combo_box_append_text(X,Y)
#        define gtk_combo_box_text_get_active_text(X) \
                gtk_combo_box_get_active_text(X)
#        define gtk_combo_box_text_new_with_entry() \
                g_object_new(GTK_TYPE_COMBO_BOX_ENTRY, "text-column", 0, NULL)
#endif /* GTK_TYPE_COMBO_BOX_TEXT */

#endif /* VGL_COMPAT_H */
