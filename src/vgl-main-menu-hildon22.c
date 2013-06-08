/*
 * vgl-main-menu-hildon22.c -- Main menu (Hildon 2.2 version)
 *
 * Copyright (C) 2010, 2011 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include <hildon/hildon.h>

#include "vgl-main-menu.h"
#include "controller.h"
#include "compat.h"

struct _VglMainMenu {
        HildonAppMenu parent;
        GtkWidget *settings;
        GtkWidget *playradio;
        GtkWidget *people;
        GtkWidget *stopafter;
        GtkWidget *bookmarks;
        GtkWidget *bmkartist;
        GtkWidget *bmktrack;
        GtkWidget *bmkradio;
};

struct _VglMainMenuClass {
        HildonAppMenuClass parent_class;
};

G_DEFINE_TYPE (VglMainMenu, vgl_main_menu, HILDON_TYPE_APP_MENU);

void
vgl_main_menu_set_state                 (VglMainMenu        *menu,
                                         VglMainWindowState  state,
                                         const LastfmTrack  *t,
                                         const char         *radio_url)
{
        g_return_if_fail (VGL_IS_MAIN_MENU (menu));
        switch (state) {
        case VGL_MAIN_WINDOW_STATE_DISCONNECTED:
        case VGL_MAIN_WINDOW_STATE_STOPPED:
                gtk_widget_set_sensitive (menu->stopafter, FALSE);
                gtk_widget_set_sensitive (menu->playradio, TRUE);
                gtk_widget_set_sensitive (menu->people, TRUE);
                gtk_widget_set_sensitive (menu->bookmarks, TRUE);
                gtk_widget_set_sensitive (menu->settings, TRUE);
                gtk_widget_set_sensitive (menu->bmkartist, FALSE);
                gtk_widget_set_sensitive (menu->bmktrack, FALSE);
                break;
        case VGL_MAIN_WINDOW_STATE_PLAYING:
                gtk_widget_set_sensitive (menu->stopafter, TRUE);
                gtk_widget_set_sensitive (menu->playradio, TRUE);
                gtk_widget_set_sensitive (menu->people, TRUE);
                gtk_widget_set_sensitive (menu->bookmarks, TRUE);
                gtk_widget_set_sensitive (menu->settings, TRUE);
                gtk_widget_set_sensitive (menu->bmkartist, t->artistid > 0);
                gtk_widget_set_sensitive (menu->bmktrack, t->id > 0);
                break;
        case VGL_MAIN_WINDOW_STATE_CONNECTING:
                gtk_widget_set_sensitive (menu->stopafter, FALSE);
                gtk_widget_set_sensitive (menu->playradio, FALSE);
                gtk_widget_set_sensitive (menu->people, FALSE);
                gtk_widget_set_sensitive (menu->bookmarks, FALSE);
                gtk_widget_set_sensitive (menu->settings, FALSE);
                break;
        default:
                g_critical("Unknown ui state received: %d", state);
                break;
        }
        gtk_widget_set_sensitive (menu->bmkradio, radio_url != NULL);
}

void
vgl_main_menu_set_track_as_loved        (VglMainMenu *menu)
{
}

void
vgl_main_menu_set_track_as_added_to_playlist
                                        (VglMainMenu *menu,
                                         gboolean     added)
{
}

static void
bookmark_dialog_destroyed_cb            (GtkWidget   *dialog,
                                         VglMainMenu *menu)
{
        GtkContainer *table;
        table = GTK_CONTAINER (gtk_widget_get_parent (menu->bmkartist));
        gtk_container_remove (table, menu->bmkartist);
        gtk_container_remove (table, menu->bmktrack);
        gtk_container_remove (table, menu->bmkradio);
}

static void
manage_bookmarks_cb                     (GtkWidget *button,
                                         GtkWidget *dialog)
{
        gtk_widget_destroy (dialog);
        controller_manage_bookmarks ();
}

static void
bookmarks_selected_cb                   (GtkWidget   *widget,
                                         VglMainMenu *menu)
{
        GtkWidget *dialog, *table, *button, *vbox;

        dialog = gtk_dialog_new ();
        vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_title (GTK_WINDOW (dialog), _("Bookmarks"));

        table = gtk_table_new (2, 2, TRUE);
        gtk_container_add (GTK_CONTAINER (vbox), table);

        button = gtk_button_new_from_stock (_("Manage bookmarks..."));
        hildon_gtk_widget_set_theme_size (button, FINGER_SIZE);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (manage_bookmarks_cb), dialog);

        gtk_table_attach_defaults (GTK_TABLE (table),
                                   g_object_ref (menu->bmkartist),
                                   0, 1, 0, 1);
        gtk_table_attach_defaults (GTK_TABLE (table),
                                   g_object_ref (menu->bmktrack),
                                   1, 2, 0, 1);
        gtk_table_attach_defaults (GTK_TABLE (table),
                                   g_object_ref (menu->bmkradio),
                                   0, 1, 1, 2);
        gtk_table_attach_defaults (GTK_TABLE (table), button, 1, 2, 1, 2);

        g_signal_connect (dialog, "destroy",
                          G_CALLBACK (bookmark_dialog_destroyed_cb), menu);

        gtk_widget_show_all (dialog);
}

static void
add_bookmark_selected                   (GtkWidget *widget,
                                         gpointer   data)
{
        BookmarkType type = GPOINTER_TO_INT (data);
        gtk_widget_destroy (gtk_widget_get_toplevel (widget));
        controller_add_bookmark (type);
}

static void
user_radio_selected_cb                  (HildonPickerDialog *dialog,
                                         gint                response,
                                         const char         *user)
{
        int active = -1;

        if (response == GTK_RESPONSE_OK) {
                HildonTouchSelector *sel;
                sel = hildon_picker_dialog_get_selector (dialog);
                active = hildon_touch_selector_get_active (sel, 0);
        }

        gtk_widget_destroy (GTK_WIDGET (dialog));

        switch (active) {
        case -1:
                break;
        case 0:
                controller_play_others_radio_by_user
                        (user, LASTFM_LIBRARY_RADIO);
                break;
        case 1:
                controller_play_others_radio_by_user
                        (user, LASTFM_NEIGHBOURS_RADIO);
                break;
        case 2:
                controller_play_others_radio_by_user
                        (user, LASTFM_LOVEDTRACKS_RADIO);
                break;
        case 3:
                controller_play_others_radio_by_user
                        (user, LASTFM_MIX_RADIO);
                break;
        case 4:
                controller_play_others_radio_by_user (
                        user, LASTFM_USERPLAYLIST_RADIO);
                break;
        case 5:
                controller_play_others_radio_by_user
                        (user, LASTFM_RECOMMENDED_RADIO);
                break;
        case 6:
                controller_play_others_radio_by_user
                        (user, LASTFM_USERTAG_RADIO);
                break;
        default:
                g_return_if_reached ();
        }
}

static void
user_selected_cb                        (GtkWidget           *dialog,
                                         gint                 response,
                                         HildonTouchSelector *usersel)
{
        GtkWidget *d;
        HildonTouchSelector *sel;
        char *user;
        GString *text;

        if (response != GTK_RESPONSE_OK) {
                return;
        }

        user = g_strstrip (hildon_touch_selector_get_current_text (usersel));
        text = g_string_sized_new (100);

        gtk_widget_destroy (dialog);

        d = hildon_picker_dialog_new (NULL);
        sel = HILDON_TOUCH_SELECTOR (hildon_touch_selector_new_text ());
        g_object_set (sel, "live-search", FALSE, NULL);

        gtk_window_set_title (GTK_WINDOW (d), _("Select radio"));

        g_string_printf (text, _("%s's library"), user);
        hildon_touch_selector_append_text (sel, text->str);
        g_string_printf (text, _("%s's neighbours"), user);
        hildon_touch_selector_append_text (sel, text->str);
        g_string_printf (text, _("%s's loved tracks"), user);
        hildon_touch_selector_append_text (sel, text->str);
        g_string_printf (text, _("%s's mix"), user);
        hildon_touch_selector_append_text (sel, text->str);
        g_string_printf (text, _("%s's playlist"), user);
        hildon_touch_selector_append_text (sel, text->str);
        g_string_printf (text, _("%s's recommendations"), user);
        hildon_touch_selector_append_text (sel, text->str);
        g_string_printf (text, _("%s's music tagged..."), user);
        hildon_touch_selector_append_text (sel, text->str);

        hildon_picker_dialog_set_selector (HILDON_PICKER_DIALOG (d), sel);

        g_signal_connect_data (d, "response",
                               G_CALLBACK (user_radio_selected_cb), user,
                               (GClosureNotify) g_free, 0);

        gtk_widget_show_all (d);

        g_string_free (text, TRUE);
}

static void
people_cb                               (GtkButton *button,
                                         gpointer   data)
{
        GtkWidget *d;
        HildonTouchSelector *sel;
        const GList *friends;

        friends = controller_get_friend_list ();

        d = hildon_picker_dialog_new (NULL);
        sel = HILDON_TOUCH_SELECTOR (hildon_touch_selector_entry_new_text ());

        gtk_window_set_title (GTK_WINDOW (d), _("Type any user name"));

        if (friends == NULL) {
                /* Hack for libhildon < 2.2.5 */
                hildon_touch_selector_append_text (sel, "");
        } else for (; friends != NULL; friends = friends->next) {
                hildon_touch_selector_append_text (sel, friends->data);
        }

        hildon_picker_dialog_set_selector (HILDON_PICKER_DIALOG (d), sel);

        g_signal_connect (d, "response", G_CALLBACK (user_selected_cb), sel);

        gtk_widget_show_all (d);
}

static void
radio_selected_cb                       (GtkWidget           *dialog,
                                         gint                 response,
                                         HildonTouchSelector *sel)
{
        int active = -1;

        if (response == GTK_RESPONSE_OK) {
                active = hildon_touch_selector_get_active (sel, 0);
        }

        gtk_widget_destroy (dialog);

        switch (active) {
        case -1:
                break;
        case 0:
                controller_play_globaltag_radio ();
                break;
        case 1:
                controller_play_similarartist_radio ();
                break;
        case 2:
                controller_play_group_radio ();
                break;
        case 3:
                controller_play_radio (LASTFM_LIBRARY_RADIO);
                break;
        case 4:
                controller_play_radio (LASTFM_NEIGHBOURS_RADIO);
                break;
        case 5:
                controller_play_radio (LASTFM_LOVEDTRACKS_RADIO);
                break;
        case 6:
                controller_play_radio (LASTFM_MIX_RADIO);
                break;
        case 7:
                controller_play_radio (LASTFM_USERPLAYLIST_RADIO);
                break;
        case 8:
                controller_play_radio (LASTFM_RECOMMENDED_RADIO);
                break;
        case 9:
                controller_play_radio (LASTFM_USERTAG_RADIO);
                break;
        case 10:
                controller_play_radio_ask_url ();
                break;
        default:
                g_return_if_reached ();
        }
}

static void
play_radio_cb                           (GtkButton *button,
                                         gpointer   data)
{
        GtkWidget *d;
        HildonTouchSelector *sel;

        d = hildon_picker_dialog_new (NULL);
        sel = HILDON_TOUCH_SELECTOR (hildon_touch_selector_new_text ());
        g_object_set (sel, "live-search", FALSE, NULL);

        gtk_window_set_title (GTK_WINDOW (d), _("Select radio"));

        hildon_touch_selector_append_text (sel, _("Music tagged..."));
        hildon_touch_selector_append_text (sel, _("Artists similar to..."));
        hildon_touch_selector_append_text (sel, _("Group radio..."));
        hildon_touch_selector_append_text (sel, _("My library"));
        hildon_touch_selector_append_text (sel, _("My neighbours"));
        hildon_touch_selector_append_text (sel, _("My loved tracks"));
        hildon_touch_selector_append_text (sel, _("My mix"));
        hildon_touch_selector_append_text (sel, _("My playlist"));
        hildon_touch_selector_append_text (sel, _("My recommendations"));
        hildon_touch_selector_append_text (sel, _("My music tagged..."));
        hildon_touch_selector_append_text (sel, _("Enter URL..."));

        hildon_picker_dialog_set_selector (HILDON_PICKER_DIALOG (d), sel);

        g_signal_connect (d, "response", G_CALLBACK (radio_selected_cb), sel);

        gtk_widget_show_all (d);
}

static void
vgl_main_menu_dispose                   (GObject *object)
{
        VglMainMenu *menu = VGL_MAIN_MENU (object);

        if (menu->bmkartist) {
                gtk_widget_destroy (menu->bmkartist);
                menu->bmkartist = NULL;
        }

        if (menu->bmktrack) {
                gtk_widget_destroy (menu->bmktrack);
                menu->bmktrack = NULL;
        }

        if (menu->bmkradio) {
                gtk_widget_destroy (menu->bmkradio);
                menu->bmkradio = NULL;
        }

        G_OBJECT_CLASS (vgl_main_menu_parent_class)->dispose (object);
}

static void
vgl_main_menu_class_init                (VglMainMenuClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
        gobject_class->dispose = vgl_main_menu_dispose;
}

static void
vgl_main_menu_init                      (VglMainMenu *self)
{
        GtkWidget *button;
        HildonAppMenu *menu = (HildonAppMenu *) self;

        button = gtk_button_new_from_stock (GTK_STOCK_PREFERENCES);
        hildon_app_menu_append (menu, GTK_BUTTON (button));
        g_signal_connect (button, "clicked",
                          G_CALLBACK (controller_open_usercfg), NULL);
        self->settings = button;

        button = gtk_button_new_with_label (_("Import servers file"));
        hildon_app_menu_append (menu, GTK_BUTTON (button));
        g_signal_connect (button, "clicked",
                          G_CALLBACK (controller_import_servers_file), NULL);

        button = gtk_button_new_with_label (_("Play radio"));
        hildon_app_menu_append (menu, GTK_BUTTON (button));
        g_signal_connect (button, "clicked", G_CALLBACK (play_radio_cb), NULL);
        self->playradio = button;

        button = gtk_button_new_with_label (_("People"));
        hildon_app_menu_append (menu, GTK_BUTTON (button));
        g_signal_connect (button, "clicked", G_CALLBACK (people_cb), NULL);
        self->people = button;

        button = gtk_button_new_with_label (_("Stop after..."));
        hildon_app_menu_append (menu, GTK_BUTTON (button));
        g_signal_connect (button, "clicked",
                          G_CALLBACK (controller_set_stop_after), NULL);
        self->stopafter = button;

        button = gtk_button_new_with_label (_("Bookmarks"));
        hildon_app_menu_append (menu, GTK_BUTTON (button));
        g_signal_connect (button, "clicked",
                          G_CALLBACK (bookmarks_selected_cb), self);
        self->bookmarks = button;

        button = gtk_button_new_from_stock (GTK_STOCK_ABOUT);
        hildon_app_menu_append (menu, GTK_BUTTON (button));
        g_signal_connect (button, "clicked",
                          G_CALLBACK (controller_show_about), NULL);

        button = gtk_button_new_from_stock (_("Bookmark this artist..."));
        hildon_gtk_widget_set_theme_size (button, FINGER_SIZE);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (add_bookmark_selected),
                          GINT_TO_POINTER (BOOKMARK_TYPE_ARTIST));
        self->bmkartist = button;

        button = gtk_button_new_from_stock (_("Bookmark this track..."));
        hildon_gtk_widget_set_theme_size (button, FINGER_SIZE);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (add_bookmark_selected),
                          GINT_TO_POINTER (BOOKMARK_TYPE_TRACK));
        self->bmktrack = button;

        button = gtk_button_new_from_stock (_("Bookmark this radio..."));
        hildon_gtk_widget_set_theme_size (button, FINGER_SIZE);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (add_bookmark_selected),
                          GINT_TO_POINTER (BOOKMARK_TYPE_CURRENT_RADIO));
        self->bmkradio = button;

        gtk_widget_show_all (GTK_WIDGET (menu));
}

VglMainMenu *
vgl_main_menu_new                       (GtkAccelGroup *accel)
{
        VglMainMenu *menu = g_object_new (VGL_TYPE_MAIN_MENU, NULL);
        gtk_widget_add_accelerator (menu->settings, "activate", accel,
                                    GDK_KEY_p, GDK_CONTROL_MASK, 0);
        return menu;
}
