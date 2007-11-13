/*
 * mainwin.c -- Functions to control the main program window
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <stdio.h>

#if defined(MAEMO2) || defined(MAEMO3)
#include <hildon-widgets/hildon-program.h>
#elif defined(MAEMO4)
#include <hildon/hildon-program.h>
#endif

#include "controller.h"
#include "mainwin.h"
#include "radio.h"
#include "uimisc.h"
#include "globaldefs.h"
#include "xmlrpc.h"

static const char *authors[] = {
        "Alberto Garcia Gonzalez\n<agarcia@igalia.com>",
        NULL
};

static const char *appdescr = "A Last.fm client for Gnome and Maemo";
static const char *copyright = "(c) 2007 Alberto Garcia Gonzalez";
static const char *website = "http://people.igalia.com/berto/";
static const char *license =
"Vagalume is free software: you can redistribute\n"
"it and/or modify it under the terms of the GNU\n"
"General Public License version 3 as published by\n"
"the Free Software Foundation.\n"
"\n"
"Vagalume is distributed in the hope that it will\n"
"be useful, but WITHOUT ANY WARRANTY; without even\n"
"the implied warranty of MERCHANTABILITY or FITNESS\n"
"FOR A PARTICULAR PURPOSE. See the GNU General\n"
"Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General\n"
"Public License along with Vagalume. If not, see\n"
"http://www.gnu.org/licenses/.\n";

void
mainwin_update_track_info(lastfm_mainwin *w, const char *playlist,
                          const char *artist, const char *track,
                          const char *album)
{
        g_return_if_fail(w != NULL && playlist != NULL);
        char *text;
        text = g_strconcat("Artist: ", artist, NULL);
        gtk_label_set_text(GTK_LABEL(w->artist), text);
        g_free(text);
        text = g_strconcat("Track: ", track, NULL);
        gtk_label_set_text(GTK_LABEL(w->track), text);
        g_free(text);
        text = g_strconcat("Album: ", album, NULL);
        gtk_label_set_text(GTK_LABEL(w->album), text);
        g_free(text);
        text = g_strconcat("Listening to ", playlist, NULL);
        gtk_label_set_text(GTK_LABEL(w->playlist), text);
        g_free(text);
        text = g_strconcat(artist, " - ", track, NULL);
        gtk_window_set_title(w->window, text);
        g_free(text);
}

void
mainwin_show_progress(lastfm_mainwin *w, guint length, guint played)
{
        g_return_if_fail(w != NULL && w->progressbar != NULL);
        const int bufsize = 16;
        char count[bufsize];
        gdouble fraction = 0;
        if (length != 0) {
                if (played > length) played = length;
                fraction = (gdouble)played / length;
                snprintf(count, bufsize, "%u:%02u / %u:%02u", played/60,
                         played%60, length/60, length%60);
        } else {
                snprintf(count, bufsize, "%u:%02u / ??:??",
                         played/60, played%60);
        }
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->progressbar),
                                      fraction);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w->progressbar), count);
}

void
mainwin_set_ui_state(lastfm_mainwin *w, lastfm_ui_state state)
{
        g_return_if_fail(w != NULL);
        gboolean dim_labels = FALSE;
        switch (state) {
        case LASTFM_UI_STATE_DISCONNECTED:
        case LASTFM_UI_STATE_STOPPED:
                dim_labels = FALSE;
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w->progressbar),
                                          "Stopped");
                gtk_label_set_text(GTK_LABEL(w->playlist), "Stopped");
                gtk_label_set_text(GTK_LABEL(w->artist), NULL);
                gtk_label_set_text(GTK_LABEL(w->track), NULL);
                gtk_label_set_text(GTK_LABEL(w->album), NULL);
                gtk_widget_set_sensitive (w->play, TRUE);
                gtk_widget_set_sensitive (w->stop, FALSE);
                gtk_widget_set_sensitive (w->next, FALSE);
                gtk_widget_set_sensitive (w->radiomenu, TRUE);
                gtk_widget_set_sensitive (w->ratemenu, FALSE);
                gtk_widget_set_sensitive (w->settings, TRUE);
                gtk_window_set_title(w->window, APP_NAME);
                break;
        case LASTFM_UI_STATE_PLAYING:
                dim_labels = FALSE;
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w->progressbar),
                                          "Playing...");
                gtk_widget_set_sensitive (w->play, FALSE);
                gtk_widget_set_sensitive (w->stop, TRUE);
                gtk_widget_set_sensitive (w->next, TRUE);
                gtk_widget_set_sensitive (w->radiomenu, TRUE);
                gtk_widget_set_sensitive (w->ratemenu, TRUE);
                gtk_widget_set_sensitive (w->settings, TRUE);
                break;
        case LASTFM_UI_STATE_CONNECTING:
                dim_labels = TRUE;
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w->progressbar),
                                          "Connecting...");
                gtk_label_set_text(GTK_LABEL(w->playlist), "Connecting...");
                gtk_widget_set_sensitive (w->play, FALSE);
                gtk_widget_set_sensitive (w->stop, FALSE);
                gtk_widget_set_sensitive (w->next, FALSE);
                gtk_widget_set_sensitive (w->radiomenu, FALSE);
                gtk_widget_set_sensitive (w->ratemenu, FALSE);
                gtk_widget_set_sensitive (w->settings, FALSE);
                gtk_window_set_title(w->window, APP_NAME);
                break;
        default:
                g_critical("Unknown ui state received: %d", state);
                break;
        }
        if (state == LASTFM_UI_STATE_DISCONNECTED) {
                gtk_label_set_text(GTK_LABEL(w->playlist), "Disconnected");
        }
        gtk_widget_set_sensitive (w->artist, !dim_labels);
        gtk_widget_set_sensitive (w->track, !dim_labels);
        gtk_widget_set_sensitive (w->album, !dim_labels);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->progressbar), 0);
}

static void
play_clicked(GtkWidget *widget, gpointer data)
{
        controller_start_playing();
}

static void
stop_clicked(GtkWidget *widget, gpointer data)
{
        controller_stop_playing();
}

static void
next_clicked(GtkWidget *widget, gpointer data)
{
        controller_skip_track();
}

static void
close_app(GtkWidget *widget, gpointer data)
{
        controller_quit_app();
}

static gboolean
delete_event(GtkWidget *widget, gpointer data)
{
        controller_quit_app();
        return TRUE;
}

static void
radio_selected(GtkWidget *widget, gpointer data)
{
        lastfm_radio type = GPOINTER_TO_INT(data);
        controller_play_radio(type);
}

static void
others_radio_selected(GtkWidget *widget, gpointer data)
{
        lastfm_radio type = GPOINTER_TO_INT(data);
        controller_play_others_radio(type);
}

static void
group_radio_selected(GtkWidget *widget, gpointer data)
{
        controller_play_group_radio();
}

static void
globaltag_radio_selected(GtkWidget *widget, gpointer data)
{
        controller_play_globaltag_radio();
}

static void
similarartist_radio_selected(GtkWidget *widget, gpointer data)
{
        controller_play_similarartist_radio();
}

static void
url_radio_selected(GtkWidget *widget, gpointer data)
{
        controller_play_radio_ask_url();
}

static void
love_track_selected(GtkWidget *widget, gpointer data)
{
        controller_love_track();
}

static void
ban_track_selected(GtkWidget *widget, gpointer data)
{
        controller_ban_track();
}

static void
tag_track_selected(GtkWidget *widget, gpointer data)
{
        request_type type = GPOINTER_TO_INT(data);
        controller_tag_track(type);
}

static void
recomm_track_selected(GtkWidget *widget, gpointer data)
{
        request_type type = GPOINTER_TO_INT(data);
        controller_recomm_track(type);
}

static void
add_to_playlist_selected(GtkWidget *widget, gpointer data)
{
        controller_add_to_playlist();
}

static void
show_about_dialog(GtkWidget *widget, gpointer data)
{
        GtkWindow *win = GTK_WINDOW(data);
        gtk_show_about_dialog(win, "name", APP_NAME, "authors", authors,
                              "comments", appdescr, "copyright", copyright,
                              "license", license, "version", APP_VERSION,
                              "website", website, NULL);
}

static void
open_user_settings(GtkWidget *widget, gpointer data)
{
        controller_open_usercfg();
}

static GtkWidget *
create_main_menu(lastfm_mainwin *w)
{
        GtkMenuItem *lastfm, *radio, *rate, *help;
        GtkMenuItem *user, *others;
        GtkWidget *group, *globaltag, *similarartist, *urlradio;
        GtkMenuShell *lastfmsub, *radiosub, *ratesub, *helpsub;
        GtkMenuShell *usersub, *othersub;
        GtkWidget *settings, *quit;
        GtkWidget *love, *ban, *tag, *dorecomm, *addtopls;
        GtkMenuShell *tagsub, *dorecommsub;
        GtkWidget *tagartist, *tagtrack, *tagalbum;
        GtkWidget *recommartist, *recommtrack, *recommalbum;
        GtkWidget *personal, *neigh, *loved, *playlist, *recomm, *usertag;
        GtkWidget *personal2, *neigh2, *loved2, *playlist2;
        GtkWidget *about;
#ifdef MAEMO
        GtkMenuShell *bar = GTK_MENU_SHELL(gtk_menu_new());
#else
        GtkMenuShell *bar = GTK_MENU_SHELL(gtk_menu_bar_new());
#endif

        /* Last.fm */
        lastfm = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("_Last.fm"));
        lastfmsub = GTK_MENU_SHELL(gtk_menu_new());
        settings = gtk_menu_item_new_with_label("Settings...");
        quit = gtk_menu_item_new_with_label("Quit");
        gtk_menu_shell_append(bar, GTK_WIDGET(lastfm));
        gtk_menu_item_set_submenu(lastfm, GTK_WIDGET(lastfmsub));
        gtk_menu_shell_append(lastfmsub, settings);
#ifndef MAEMO
        gtk_menu_shell_append(lastfmsub, quit);
#endif
        g_signal_connect(G_OBJECT(settings), "activate",
                         G_CALLBACK(open_user_settings), NULL);
        g_signal_connect(G_OBJECT(quit), "activate",
                         G_CALLBACK(close_app), NULL);

        /* Radio */
        radio = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("Play _Radio"));
        radiosub = GTK_MENU_SHELL(gtk_menu_new());
        user = GTK_MENU_ITEM(gtk_menu_item_new_with_label("My radios"));
        others = GTK_MENU_ITEM(gtk_menu_item_new_with_label(
                                      "Others' radios"));
        group = gtk_menu_item_new_with_label("Group radio...");
        globaltag = gtk_menu_item_new_with_label("Music tagged...");
        similarartist = gtk_menu_item_new_with_label("Artists similar to...");
        urlradio = gtk_menu_item_new_with_label("Enter URL...");
        gtk_menu_shell_append(bar, GTK_WIDGET(radio));
        gtk_menu_item_set_submenu(radio, GTK_WIDGET(radiosub));
        gtk_menu_shell_append(radiosub, GTK_WIDGET(user));
        gtk_menu_shell_append(radiosub, GTK_WIDGET(others));
        gtk_menu_shell_append(radiosub, group);
        gtk_menu_shell_append(radiosub, globaltag);
        gtk_menu_shell_append(radiosub, similarartist);
        gtk_menu_shell_append(radiosub, urlradio);
        g_signal_connect(G_OBJECT(group), "activate",
                         G_CALLBACK(group_radio_selected), NULL);
        g_signal_connect(G_OBJECT(globaltag), "activate",
                         G_CALLBACK(globaltag_radio_selected), NULL);
        g_signal_connect(G_OBJECT(similarartist), "activate",
                         G_CALLBACK(similarartist_radio_selected), NULL);
        g_signal_connect(G_OBJECT(urlradio), "activate",
                         G_CALLBACK(url_radio_selected), NULL);

        /* Radio -> My radios */
        usersub = GTK_MENU_SHELL(gtk_menu_new());
        gtk_menu_item_set_submenu(user, GTK_WIDGET(usersub));
        personal = gtk_menu_item_new_with_label("My personal radio");
        neigh = gtk_menu_item_new_with_label("My neighbours");
        loved = gtk_menu_item_new_with_label("My loved tracks");
        playlist = gtk_menu_item_new_with_label("My playlist");
        recomm = gtk_menu_item_new_with_label("My recommendations");
        usertag = gtk_menu_item_new_with_label("My music tagged...");
        gtk_menu_shell_append(usersub, personal);
        gtk_menu_shell_append(usersub, neigh);
        gtk_menu_shell_append(usersub, loved);
        gtk_menu_shell_append(usersub, playlist);
        gtk_menu_shell_append(usersub, recomm);
        gtk_menu_shell_append(usersub, usertag);
        g_signal_connect(G_OBJECT(personal), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_PERSONAL_RADIO));
        g_signal_connect(G_OBJECT(neigh), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_NEIGHBOURS_RADIO));
        g_signal_connect(G_OBJECT(loved), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_LOVEDTRACKS_RADIO));
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
        personal2 = gtk_menu_item_new_with_label("Personal...");
        neigh2 = gtk_menu_item_new_with_label("Neighbours...");
        loved2 = gtk_menu_item_new_with_label("Loved tracks...");
        playlist2 = gtk_menu_item_new_with_label("Playlist...");
        gtk_menu_shell_append(othersub, personal2);
        gtk_menu_shell_append(othersub, neigh2);
        gtk_menu_shell_append(othersub, loved2);
        gtk_menu_shell_append(othersub, playlist2);
        g_signal_connect(G_OBJECT(personal2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_PERSONAL_RADIO));
        g_signal_connect(G_OBJECT(neigh2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_NEIGHBOURS_RADIO));
        g_signal_connect(G_OBJECT(loved2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_LOVEDTRACKS_RADIO));
        g_signal_connect(G_OBJECT(playlist2), "activate",
                         G_CALLBACK(others_radio_selected),
                         GINT_TO_POINTER(LASTFM_USERPLAYLIST_RADIO));

        /* Actions */
        rate = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("_Actions"));
        ratesub = GTK_MENU_SHELL(gtk_menu_new());
        tagsub = GTK_MENU_SHELL(gtk_menu_new());
        dorecommsub = GTK_MENU_SHELL(gtk_menu_new());
        love = gtk_menu_item_new_with_label("Love this track");
        ban = gtk_menu_item_new_with_label("Ban this track");
        addtopls = gtk_menu_item_new_with_label("Add to playlist");
        tag = gtk_menu_item_new_with_label("Tag");
        dorecomm = gtk_menu_item_new_with_label("Recommend");
        tagartist = gtk_menu_item_new_with_label("This artist...");
        tagtrack = gtk_menu_item_new_with_label("This track...");
        tagalbum = gtk_menu_item_new_with_label("This album...");
        recommartist = gtk_menu_item_new_with_label("This artist...");
        recommtrack = gtk_menu_item_new_with_label("This track...");
        recommalbum = gtk_menu_item_new_with_label("This album...");
        gtk_menu_shell_append(bar, GTK_WIDGET(rate));
        gtk_menu_item_set_submenu(rate, GTK_WIDGET(ratesub));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(tag), GTK_WIDGET(tagsub));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(dorecomm),
                                  GTK_WIDGET(dorecommsub));
        gtk_menu_shell_append(ratesub, love);
        gtk_menu_shell_append(ratesub, ban);
        gtk_menu_shell_append(ratesub, addtopls);
        gtk_menu_shell_append(ratesub, tag);
        gtk_menu_shell_append(ratesub, dorecomm);
        gtk_menu_shell_append(tagsub, tagartist);
        gtk_menu_shell_append(tagsub, tagtrack);
        gtk_menu_shell_append(tagsub, tagalbum);
        gtk_menu_shell_append(dorecommsub, recommartist);
        gtk_menu_shell_append(dorecommsub, recommtrack);
        gtk_menu_shell_append(dorecommsub, recommalbum);
        g_signal_connect(G_OBJECT(love), "activate",
                         G_CALLBACK(love_track_selected), NULL);
        g_signal_connect(G_OBJECT(ban), "activate",
                         G_CALLBACK(ban_track_selected), NULL);
        g_signal_connect(G_OBJECT(tagartist), "activate",
                         G_CALLBACK(tag_track_selected),
                         GINT_TO_POINTER(REQUEST_ARTIST));
        g_signal_connect(G_OBJECT(tagtrack), "activate",
                         G_CALLBACK(tag_track_selected),
                         GINT_TO_POINTER(REQUEST_TRACK));
        g_signal_connect(G_OBJECT(tagalbum), "activate",
                         G_CALLBACK(tag_track_selected),
                         GINT_TO_POINTER(REQUEST_ALBUM));
        g_signal_connect(G_OBJECT(recommartist), "activate",
                         G_CALLBACK(recomm_track_selected),
                         GINT_TO_POINTER(REQUEST_ARTIST));
        g_signal_connect(G_OBJECT(recommtrack), "activate",
                         G_CALLBACK(recomm_track_selected),
                         GINT_TO_POINTER(REQUEST_TRACK));
        g_signal_connect(G_OBJECT(recommalbum), "activate",
                         G_CALLBACK(recomm_track_selected),
                         GINT_TO_POINTER(REQUEST_ALBUM));
        g_signal_connect(G_OBJECT(addtopls), "activate",
                         G_CALLBACK(add_to_playlist_selected), NULL);

        /* Help */
        help = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("_Help"));
        helpsub = GTK_MENU_SHELL(gtk_menu_new());
        about = gtk_menu_item_new_with_label("About...");
        gtk_menu_shell_append(bar, GTK_WIDGET(help));
        gtk_menu_item_set_submenu(help, GTK_WIDGET(helpsub));
        gtk_menu_shell_append(helpsub, about);
        g_signal_connect(G_OBJECT(about), "activate",
                         G_CALLBACK(show_about_dialog), w->window);
#ifdef MAEMO
        gtk_menu_shell_append(bar, quit);
#endif

        w->radiomenu = GTK_WIDGET(radio);
        w->ratemenu = GTK_WIDGET(rate);
        w->settings = GTK_WIDGET(settings);
        return GTK_WIDGET(bar);
}

lastfm_mainwin *
lastfm_mainwin_create(void)
{
        lastfm_mainwin *w = g_new0(lastfm_mainwin, 1);
        GtkBox *hbox, *vbox;
        GtkWidget *menu;
        /* Window */
#ifdef MAEMO
        w->window = GTK_WINDOW(hildon_window_new());
#else
        w->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
#endif
        gtk_container_set_border_width(GTK_CONTAINER(w->window), 0);
        /* Boxes */
        hbox = GTK_BOX(gtk_hbox_new(TRUE, 5));
        vbox = GTK_BOX(gtk_vbox_new(FALSE, 0));
        /* Buttons */
        w->play = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
        w->stop = gtk_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
        w->next = gtk_button_new_from_stock(GTK_STOCK_MEDIA_NEXT);
        /* Text labels */
        w->playlist = gtk_label_new(NULL);
        w->artist = gtk_label_new(NULL);
        w->track = gtk_label_new(NULL);
        w->album = gtk_label_new(NULL);
        /* Menu */
        menu = create_main_menu(w);
        /* Progress bar */
        w->progressbar = gtk_progress_bar_new();
        gtk_progress_set_text_alignment(GTK_PROGRESS(w->progressbar),
                                        0.5, 0.5);
        /* Layout */
        gtk_misc_set_alignment(GTK_MISC(w->playlist), 0, 0.5);
        gtk_misc_set_alignment(GTK_MISC(w->artist), 0, 0.5);
        gtk_misc_set_alignment(GTK_MISC(w->track), 0, 0.5);
        gtk_misc_set_alignment(GTK_MISC(w->album), 0, 0.5);
        gtk_misc_set_padding(GTK_MISC(w->playlist), 10, 0);
        gtk_misc_set_padding(GTK_MISC(w->artist), 10, 0);
        gtk_misc_set_padding(GTK_MISC(w->track), 10, 0);
        gtk_misc_set_padding(GTK_MISC(w->album), 10, 0);
        gtk_container_add(GTK_CONTAINER(w->window), GTK_WIDGET(vbox));
        gtk_box_pack_start(hbox, w->play, TRUE, TRUE, 5);
        gtk_box_pack_start(hbox, w->stop, TRUE, TRUE, 5);
        gtk_box_pack_start(hbox, w->next, TRUE, TRUE, 5);
#ifdef MAEMO
        hildon_window_set_menu(HILDON_WINDOW(w->window), GTK_MENU(menu));
#else
        gtk_box_pack_start(vbox, menu, FALSE, FALSE, 0);
#endif
        gtk_box_pack_start(vbox, w->playlist, TRUE, TRUE, 5);
        gtk_box_pack_start(vbox, GTK_WIDGET(hbox), TRUE, TRUE, 5);
        gtk_box_pack_start(vbox, w->artist, TRUE, TRUE, 5);
        gtk_box_pack_start(vbox, w->track, TRUE, TRUE, 5);
        gtk_box_pack_start(vbox, w->album, TRUE, TRUE, 5);
        gtk_box_pack_start(vbox, w->progressbar, FALSE, FALSE, 0);
        /* Signals */
        g_signal_connect(G_OBJECT(w->play), "clicked",
                         G_CALLBACK(play_clicked), NULL);
        g_signal_connect(G_OBJECT(w->next), "clicked",
                         G_CALLBACK(next_clicked), NULL);
        g_signal_connect(G_OBJECT(w->stop), "clicked",
                         G_CALLBACK(stop_clicked), NULL);
        g_signal_connect(G_OBJECT(w->window), "destroy",
                         G_CALLBACK(close_app), NULL);
        g_signal_connect(G_OBJECT(w->window), "delete-event",
                         G_CALLBACK(delete_event), NULL);
        /* Initial state */
        mainwin_set_ui_state(w, LASTFM_UI_STATE_DISCONNECTED);
        return w;
}
