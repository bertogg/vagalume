/*
 * vgl-main-menu-gtk.c -- Main menu (GTK+ version)
 *
 * Copyright (C) 2010, 2011 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include "vgl-main-menu.h"
#include "controller.h"
#include "uimisc.h"
#include "compat.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

struct _VglMainMenu {
#ifdef USE_HILDON_WINDOW
        GtkMenu parent;
#else
        GtkMenuBar parent;
#endif
        GtkWidget *play, *stop;
        GtkWidget *radiomenu, *actionsmenu, *settings, *love, *dload;
        GtkWidget *addtopls, *bmkmenu, *bmkartist, *bmktrack, *bmkradio;
};

struct _VglMainMenuClass {
#ifdef USE_HILDON_WINDOW
        GtkMenuClass parent_class;
#else
        GtkMenuBarClass parent_class;
#endif
};

#ifdef USE_HILDON_WINDOW
        G_DEFINE_TYPE (VglMainMenu, vgl_main_menu, GTK_TYPE_MENU);
#else
        G_DEFINE_TYPE (VglMainMenu, vgl_main_menu, GTK_TYPE_MENU_BAR);
#endif

static void
menu_enable_all_items                   (GtkWidget *menu,
                                         gboolean   enable)
{
        g_return_if_fail (GTK_IS_MENU (menu));
        GList *l;
        GList *items = gtk_container_get_children (GTK_CONTAINER (menu));
        for (l = items; l != NULL; l = g_list_next (l)) {
                GtkWidget *item, *submenu;
                item = GTK_WIDGET (l->data);
                submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (item));
                if (submenu != NULL) {
                        menu_enable_all_items (submenu, enable);
                } else {
                        gtk_widget_set_sensitive (item, enable);
                }
        }
        if (items != NULL) g_list_free (items);
}

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
                gtk_widget_show (menu->play);
                gtk_widget_hide (menu->stop);
                menu_enable_all_items (menu->actionsmenu, FALSE);
                menu_enable_all_items (menu->radiomenu, TRUE);
                menu_enable_all_items (menu->bmkmenu, TRUE);
                gtk_widget_set_sensitive (menu->play, TRUE);
                gtk_widget_set_sensitive (menu->settings, TRUE);
                gtk_widget_set_sensitive (menu->bmkartist, FALSE);
                gtk_widget_set_sensitive (menu->bmktrack, FALSE);
                break;
        case VGL_MAIN_WINDOW_STATE_PLAYING:
                gtk_widget_hide (menu->play);
                gtk_widget_show (menu->stop);
                menu_enable_all_items (menu->actionsmenu, TRUE);
                menu_enable_all_items (menu->radiomenu, TRUE);
                menu_enable_all_items (menu->bmkmenu, TRUE);
                gtk_widget_set_sensitive (menu->play, FALSE);
                gtk_widget_set_sensitive (menu->love, TRUE);
                gtk_widget_set_sensitive (menu->settings, TRUE);
                gtk_widget_set_sensitive (menu->bmkartist, t->artistid > 0);
                gtk_widget_set_sensitive (menu->bmktrack, t->id > 0);
                gtk_widget_set_sensitive (menu->dload,
                                          t->free_track_url != NULL);
                break;
        case VGL_MAIN_WINDOW_STATE_CONNECTING:
                gtk_widget_hide (menu->play);
                gtk_widget_show (menu->stop);
                menu_enable_all_items (menu->actionsmenu, FALSE);
                menu_enable_all_items (menu->radiomenu, FALSE);
                menu_enable_all_items (menu->bmkmenu, FALSE);
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
        g_return_if_fail (menu != NULL);
        gtk_widget_set_sensitive (menu->love, FALSE);
}

void
vgl_main_menu_set_track_as_added_to_playlist
                                        (VglMainMenu *menu,
                                         gboolean     added)
{
        g_return_if_fail (menu != NULL);
        gtk_widget_set_sensitive (menu->addtopls, !added);
}

static void
love_track_selected                     (GtkWidget *widget,
                                         gpointer   data)
{
        controller_love_track (TRUE);
}

static void
ban_track_selected                      (GtkWidget *widget,
                                         gpointer   data)
{
        controller_ban_track (TRUE);
}

static void
download_track_selected                 (GtkWidget *widget,
                                         gpointer   data)
{
        controller_download_track (FALSE);
}

static void
add_bookmark_selected                   (GtkWidget *widget,
                                         gpointer   data)
{
        BookmarkType type = GPOINTER_TO_INT (data);
        controller_add_bookmark (type);
}

static void
radio_selected                          (GtkWidget *widget,
                                         gpointer   data)
{
        LastfmRadio type = GPOINTER_TO_INT (data);
        controller_play_radio (type);
}

static void
others_radio_selected                   (GtkWidget *widget,
                                         gpointer   data)
{
        LastfmRadio type = GPOINTER_TO_INT (data);
        controller_play_others_radio (type);
}

VglMainMenu *
vgl_main_menu_new                       (GtkAccelGroup *accel)
{
        VglMainMenu *menu;
        GtkMenuShell *shell;
        GtkMenuItem *lastfm, *radio, *actions, *bookmarks, *help;
        GtkMenuItem *user, *others;
        GtkWidget *group, *globaltag, *similarartist, *urlradio;
        GtkMenuShell *lastfmsub, *radiosub, *actionssub, *bmksub, *helpsub;
        GtkMenuShell *usersub, *othersub;
        GtkWidget *settings, *import, *quit;
        GtkWidget *play, *stop, *skip, *separ1, *separ2, *separ3;
        GtkWidget *stopafter, *love, *ban, *tag, *dorecomm, *addtopls, *dload;
        GtkWidget *library, *neigh, *loved, *mix, *playlist, *recomm, *usertag;
        GtkWidget *library2, *neigh2, *loved2, *mix2, *playlist2, *recomm2, *usertag2;
        GtkWidget *managebmk, *addbmk, *bmkartist, *bmktrack, *bmkradio;
        GtkWidget *about;

        menu = g_object_new (VGL_TYPE_MAIN_MENU, NULL);
        shell = GTK_MENU_SHELL (menu);

        /* Last.fm */
        lastfm = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(_("_Last.fm")));
        lastfmsub = GTK_MENU_SHELL (gtk_menu_new ());
        settings = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES,
                                                       NULL);
        import = gtk_menu_item_new_with_mnemonic(_("_Import servers file..."));
        quit = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, accel);
        gtk_menu_shell_append (shell, GTK_WIDGET (lastfm));
        gtk_menu_item_set_submenu (lastfm, GTK_WIDGET (lastfmsub));
        gtk_menu_shell_append (lastfmsub, settings);
        gtk_menu_shell_append (lastfmsub, import);
#ifndef USE_HILDON_WINDOW
        separ1 = gtk_separator_menu_item_new();
        gtk_menu_shell_append(lastfmsub, separ1);
        gtk_menu_shell_append(lastfmsub, quit);
#endif
        g_signal_connect(G_OBJECT(settings), "activate",
                         G_CALLBACK(controller_open_usercfg), NULL);
        g_signal_connect(G_OBJECT(import), "activate",
                         G_CALLBACK(controller_import_servers_file), NULL);
        g_signal_connect(G_OBJECT(quit), "activate",
                         G_CALLBACK(controller_quit_app), NULL);

        /* Radio */
        radio = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(_("Play _Radio")));
        radiosub = GTK_MENU_SHELL(gtk_menu_new());
        user = GTK_MENU_ITEM(gtk_menu_item_new_with_label(_("My radios")));
        others = GTK_MENU_ITEM(gtk_menu_item_new_with_label(
                                       _("Others' radios")));
        group = gtk_menu_item_new_with_label(_("Group radio..."));
        globaltag = gtk_menu_item_new_with_label(_("Music tagged..."));
        similarartist = gtk_menu_item_new_with_label(_("Artists similar to..."));
        urlradio = gtk_menu_item_new_with_label(_("Enter URL..."));
        gtk_menu_shell_append (shell, GTK_WIDGET (radio));
        gtk_menu_item_set_submenu(radio, GTK_WIDGET(radiosub));
        gtk_menu_shell_append(radiosub, GTK_WIDGET(user));
        gtk_menu_shell_append(radiosub, GTK_WIDGET(others));
        gtk_menu_shell_append(radiosub, group);
        gtk_menu_shell_append(radiosub, globaltag);
        gtk_menu_shell_append(radiosub, similarartist);
        gtk_menu_shell_append(radiosub, urlradio);
        g_signal_connect(G_OBJECT(group), "activate",
                         G_CALLBACK(controller_play_group_radio), NULL);
        g_signal_connect(G_OBJECT(globaltag), "activate",
                         G_CALLBACK(controller_play_globaltag_radio), NULL);
        g_signal_connect(G_OBJECT(similarartist), "activate",
                         G_CALLBACK(controller_play_similarartist_radio),NULL);
        g_signal_connect(G_OBJECT(urlradio), "activate",
                         G_CALLBACK(controller_play_radio_ask_url), NULL);

        /* Radio -> My radios */
        usersub = GTK_MENU_SHELL(gtk_menu_new());
        gtk_menu_item_set_submenu(user, GTK_WIDGET(usersub));
        library = gtk_menu_item_new_with_label(_("My library"));
        neigh = gtk_menu_item_new_with_label(_("My neighbours"));
        loved = gtk_menu_item_new_with_label(_("My loved tracks"));
        mix = gtk_menu_item_new_with_label(_("My mix"));
        playlist = gtk_menu_item_new_with_label(_("My playlist"));
        recomm = gtk_menu_item_new_with_label(_("My recommendations"));
        usertag = gtk_menu_item_new_with_label(_("My music tagged..."));
        gtk_menu_shell_append(usersub, library);
        gtk_menu_shell_append(usersub, neigh);
        gtk_menu_shell_append(usersub, loved);
        gtk_menu_shell_append(usersub, mix);
        gtk_menu_shell_append(usersub, playlist);
        gtk_menu_shell_append(usersub, recomm);
        gtk_menu_shell_append(usersub, usertag);
        g_signal_connect(G_OBJECT(library), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_LIBRARY_RADIO));
        g_signal_connect(G_OBJECT(neigh), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_NEIGHBOURS_RADIO));
        g_signal_connect(G_OBJECT(loved), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_LOVEDTRACKS_RADIO));
        g_signal_connect(G_OBJECT(mix), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_MIX_RADIO));
        g_signal_connect(G_OBJECT(playlist), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_USERPLAYLIST_RADIO));
        g_signal_connect(G_OBJECT(recomm), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_RECOMMENDED_RADIO));
        g_signal_connect(G_OBJECT(usertag), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_USERTAG_RADIO));

        /* Radio -> Others' radios */
        othersub = GTK_MENU_SHELL(gtk_menu_new());
        gtk_menu_item_set_submenu(others, GTK_WIDGET(othersub));
        library2 = gtk_menu_item_new_with_label(_("Library..."));
        neigh2 = gtk_menu_item_new_with_label(_("Neighbours..."));
        loved2 = gtk_menu_item_new_with_label(_("Loved tracks..."));
        mix2 = gtk_menu_item_new_with_label(_("Mix..."));
        playlist2 = gtk_menu_item_new_with_label(_("Playlist..."));
        recomm2 = gtk_menu_item_new_with_label(_("Recommendations..."));
        usertag2 = gtk_menu_item_new_with_label(_("Music tagged..."));
        gtk_menu_shell_append(othersub, library2);
        gtk_menu_shell_append(othersub, neigh2);
        gtk_menu_shell_append(othersub, loved2);
        gtk_menu_shell_append(othersub, mix2);
        gtk_menu_shell_append(othersub, playlist2);
        gtk_menu_shell_append(othersub, recomm2);
        gtk_menu_shell_append(othersub, usertag2);
        g_signal_connect(G_OBJECT(library2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_LIBRARY_RADIO));
        g_signal_connect(G_OBJECT(neigh2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_NEIGHBOURS_RADIO));
        g_signal_connect(G_OBJECT(loved2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_LOVEDTRACKS_RADIO));
        g_signal_connect(G_OBJECT(mix2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_MIX_RADIO));
        g_signal_connect(G_OBJECT(playlist2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_USERPLAYLIST_RADIO));
        g_signal_connect(G_OBJECT(recomm2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_RECOMMENDED_RADIO));
        g_signal_connect(G_OBJECT(usertag2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_USERTAG_RADIO));

        /* Actions */
        actions = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(_("_Actions")));
        actionssub = GTK_MENU_SHELL(gtk_menu_new());
        play = gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_PLAY, NULL);
        stop = gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_STOP, NULL);
        skip = gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_NEXT, NULL);
        separ1 = gtk_separator_menu_item_new();
        separ2 = gtk_separator_menu_item_new();
        separ3 = gtk_separator_menu_item_new();
        stopafter = gtk_menu_item_new_with_label (_("Stop after..."));
        love = ui_menu_item_create_from_icon (LOVE_ITEM_ICON_NAME,
                                              LOVE_ITEM_STRING);
        ban = ui_menu_item_create_from_icon (BAN_ITEM_ICON_NAME,
                                             BAN_ITEM_STRING);
        addtopls = ui_menu_item_create_from_icon (ADD_TO_PLS_ITEM_ICON_NAME,
                                                  ADD_TO_PLS_ITEM_STRING);
        dload = ui_menu_item_create_from_icon ("document-save",
                                               _("Download this track"));
        tag = ui_menu_item_create_from_icon (TAG_ITEM_ICON_NAME,
                                             TAG_ITEM_STRING);
        dorecomm = ui_menu_item_create_from_icon (RECOMMEND_ITEM_ICON_NAME,
                                                  RECOMMEND_ITEM_STRING);
        gtk_menu_shell_append (shell, GTK_WIDGET (actions));
        gtk_menu_item_set_submenu(actions, GTK_WIDGET(actionssub));
        gtk_menu_shell_append(actionssub, play);
        gtk_menu_shell_append(actionssub, stop);
        gtk_menu_shell_append(actionssub, skip);
        gtk_menu_shell_append(actionssub, separ1);
        gtk_menu_shell_append(actionssub, dorecomm);
        gtk_menu_shell_append(actionssub, tag);
        gtk_menu_shell_append(actionssub, addtopls);
        gtk_menu_shell_append(actionssub, separ2);
        gtk_menu_shell_append(actionssub, love);
        gtk_menu_shell_append(actionssub, ban);
        gtk_menu_shell_append(actionssub, separ3);
        gtk_menu_shell_append(actionssub, dload);
        gtk_menu_shell_append(actionssub, stopafter);
        g_signal_connect(G_OBJECT(stopafter), "activate",
                         G_CALLBACK(controller_set_stop_after), NULL);
        g_signal_connect(G_OBJECT(play), "activate",
                         G_CALLBACK(controller_start_playing), NULL);
        g_signal_connect(G_OBJECT(stop), "activate",
                         G_CALLBACK(controller_stop_playing), NULL);
        g_signal_connect(G_OBJECT(skip), "activate",
                         G_CALLBACK(controller_skip_track), NULL);
        g_signal_connect(G_OBJECT(love), "activate",
                         G_CALLBACK(love_track_selected), NULL);
        g_signal_connect(G_OBJECT(ban), "activate",
                         G_CALLBACK(ban_track_selected), NULL);
        g_signal_connect(G_OBJECT(tag), "activate",
                         G_CALLBACK(controller_tag_track), NULL);
        g_signal_connect(G_OBJECT(dorecomm), "activate",
                         G_CALLBACK(controller_recomm_track), NULL);
        g_signal_connect(G_OBJECT(addtopls), "activate",
                         G_CALLBACK(controller_add_to_playlist), NULL);
        g_signal_connect(G_OBJECT(dload), "activate",
                         G_CALLBACK(download_track_selected), NULL);

        /* Bookmarks */
        bookmarks = GTK_MENU_ITEM(
                gtk_menu_item_new_with_mnemonic(_("_Bookmarks")));
        bmksub = GTK_MENU_SHELL(gtk_menu_new());
        addbmk = ui_menu_item_create_from_icon ("gtk-new",
                                                _("Add bookmark..."));
        managebmk = ui_menu_item_create_from_icon ("gtk-edit",
                                                   _("Manage bookmarks..."));
        bmkartist = gtk_menu_item_new_with_label(_("Bookmark this artist..."));
        bmktrack = gtk_menu_item_new_with_label(_("Bookmark this track..."));
        bmkradio = gtk_menu_item_new_with_label(_("Bookmark this radio..."));
        separ1 = gtk_separator_menu_item_new();
        gtk_menu_shell_append (shell, GTK_WIDGET (bookmarks));
        gtk_menu_item_set_submenu(bookmarks, GTK_WIDGET(bmksub));
        gtk_menu_shell_append(bmksub, addbmk);
        gtk_menu_shell_append(bmksub, managebmk);
        gtk_menu_shell_append(bmksub, separ1);
        gtk_menu_shell_append(bmksub, bmkartist);
        gtk_menu_shell_append(bmksub, bmktrack);
        gtk_menu_shell_append(bmksub, bmkradio);
        g_signal_connect(G_OBJECT(managebmk), "activate",
                         G_CALLBACK(controller_manage_bookmarks), NULL);
        g_signal_connect(G_OBJECT(addbmk), "activate",
                         G_CALLBACK(add_bookmark_selected),
                         GINT_TO_POINTER(BOOKMARK_TYPE_EMPTY));
        g_signal_connect(G_OBJECT(bmkartist), "activate",
                         G_CALLBACK(add_bookmark_selected),
                         GINT_TO_POINTER(BOOKMARK_TYPE_ARTIST));
        g_signal_connect(G_OBJECT(bmktrack), "activate",
                         G_CALLBACK(add_bookmark_selected),
                         GINT_TO_POINTER(BOOKMARK_TYPE_TRACK));
        g_signal_connect(G_OBJECT(bmkradio), "activate",
                         G_CALLBACK(add_bookmark_selected),
                         GINT_TO_POINTER(BOOKMARK_TYPE_CURRENT_RADIO));

        /* Help */
        help = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(_("_Help")));
        helpsub = GTK_MENU_SHELL(gtk_menu_new());
        about = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, accel);
        gtk_menu_shell_append (shell, GTK_WIDGET (help));
        gtk_menu_item_set_submenu(help, GTK_WIDGET(helpsub));
        gtk_menu_shell_append(helpsub, about);
        g_signal_connect(G_OBJECT(about), "activate",
                         G_CALLBACK(controller_show_about), NULL);
#ifdef USE_HILDON_WINDOW
        gtk_menu_shell_append (shell, quit);
#endif

        /* Keyboard shortcuts */
        gtk_widget_add_accelerator(play, "activate", accel, GDK_KEY_space,
                                   0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(stop, "activate", accel, GDK_KEY_space,
                                   0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(skip, "activate", accel, GDK_KEY_Right,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(ban, "activate", accel, GDK_KEY_b,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(love, "activate", accel, GDK_KEY_l,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(dorecomm, "activate", accel, GDK_KEY_r,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(tag, "activate", accel, GDK_KEY_t,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(addtopls, "activate", accel, GDK_KEY_a,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(settings, "activate", accel, GDK_KEY_p,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(quit, "activate", accel, GDK_KEY_q,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

        menu->play = play;
        menu->stop = stop;
        menu->actionsmenu = GTK_WIDGET(actionssub);
        menu->radiomenu = GTK_WIDGET(radiosub);
        menu->settings = settings;
        menu->dload = dload;
        menu->love = love;
        menu->addtopls = addtopls;
        menu->bmkmenu = GTK_WIDGET (bmksub);
        menu->bmkartist = bmkartist;
        menu->bmktrack = bmktrack;
        menu->bmkradio = bmkradio;

        return menu;
}

static void
vgl_main_menu_class_init                (VglMainMenuClass *klass)
{
}

static void
vgl_main_menu_init                      (VglMainMenu *self)
{
}
