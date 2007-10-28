/*
 * mainwin.c -- Functions to control the main program window
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#include <gtk/gtk.h>

#ifdef MAEMO
#include <hildon-widgets/hildon-program.h>
#endif

#include "controller.h"
#include "mainwin.h"
#include "radio.h"
#include "uimisc.h"
#include "globaldefs.h"

static const char *authors[] = {
        "Alberto Garcia\n<agarcia@igalia.com>",
        NULL
};

static const char *appdescr = "A Last.fm player\nfor GNOME and Maemo";
static const char *copyright = "(c) 2007 Alberto Garcia Gonzalez";

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
}

void
mainwin_show_progress(lastfm_mainwin *w, guint length, guint played)
{
        g_return_if_fail(w != NULL && w->progressbar != NULL);
        char *count;
        gdouble fraction = 0;
        if (length != 0) {
                fraction = (gdouble)played / length;
                if (fraction < 0) fraction = 0;
                else if (fraction > 1) fraction = 1;
                count = g_strdup_printf("%u:%02u / %u:%02u", played/60,
                                        played%60, length/60, length%60);
        } else {
                count = g_strdup_printf("%u:%02u / ??:??",
                                        played/60, played%60);
        }
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->progressbar),
                                      fraction);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w->progressbar), count);
        g_free(count);
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
                gtk_widget_set_sensitive (w->settings, TRUE);
                break;
        case LASTFM_UI_STATE_PLAYING:
                dim_labels = FALSE;
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w->progressbar),
                                          "Playing...");
                gtk_widget_set_sensitive (w->play, FALSE);
                gtk_widget_set_sensitive (w->stop, TRUE);
                gtk_widget_set_sensitive (w->next, TRUE);
                gtk_widget_set_sensitive (w->radiomenu, TRUE);
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
                gtk_widget_set_sensitive (w->settings, FALSE);
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
url_radio_selected(GtkWidget *widget, gpointer data)
{
        controller_play_radio_ask_url();
}

static void
show_about_dialog(GtkWidget *widget, gpointer data)
{
        GtkWindow *win = GTK_WINDOW(data);
        gtk_show_about_dialog(win, "name", APP_NAME, "authors", authors,
                              "comments", appdescr, "copyright", copyright,
                              NULL);
}

static void
open_user_settings(GtkWidget *widget, gpointer data)
{
        controller_open_usercfg();
}

static GtkWidget *
create_main_menu(lastfm_mainwin *w)
{
        GtkMenuItem *lastfm, *radio, *help;
        GtkMenuShell *lastfmsub, *radiosub, *helpsub;
        GtkWidget *settings, *quit;
        GtkWidget *personal, *neigh, *loved, *playlist, *recomm, *urlradio;
        GtkWidget *about;
#ifdef MAEMO
        GtkMenuShell *bar = GTK_MENU_SHELL(gtk_menu_new());
#else
        GtkMenuShell *bar = GTK_MENU_SHELL(gtk_menu_bar_new());
#endif

        /* Last.fm */
        lastfm = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("_Last.fm"));
        lastfmsub = GTK_MENU_SHELL(gtk_menu_new());
        settings = gtk_menu_item_new_with_mnemonic("_Settings...");
        quit = gtk_menu_item_new_with_mnemonic("_Quit");
        gtk_menu_shell_append(bar, GTK_WIDGET(lastfm));
        gtk_menu_item_set_submenu(lastfm, GTK_WIDGET(lastfmsub));
        gtk_menu_shell_append(lastfmsub, settings);
        gtk_menu_shell_append(lastfmsub, quit);
        g_signal_connect(G_OBJECT(settings), "activate",
                         G_CALLBACK(open_user_settings), NULL);
        g_signal_connect(G_OBJECT(quit), "activate",
                         G_CALLBACK(close_app), NULL);

        /* Radio */
        radio = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("Play _Radio"));
        radiosub = GTK_MENU_SHELL(gtk_menu_new());
        personal = gtk_menu_item_new_with_mnemonic("_Personal");
        neigh = gtk_menu_item_new_with_mnemonic("_Neighbours");
        loved = gtk_menu_item_new_with_mnemonic("_Loved tracks");
        playlist = gtk_menu_item_new_with_mnemonic("Pl_aylist");
        recomm = gtk_menu_item_new_with_mnemonic("R_ecommendations");
        urlradio = gtk_menu_item_new_with_mnemonic("Enter _URL...");
        gtk_menu_shell_append(bar, GTK_WIDGET(radio));
        gtk_menu_item_set_submenu(radio, GTK_WIDGET(radiosub));
        gtk_menu_shell_append(radiosub, personal);
        gtk_menu_shell_append(radiosub, neigh);
        gtk_menu_shell_append(radiosub, loved);
        gtk_menu_shell_append(radiosub, playlist);
        gtk_menu_shell_append(radiosub, recomm);
        gtk_menu_shell_append(radiosub, urlradio);
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
        g_signal_connect(G_OBJECT(urlradio), "activate",
                         G_CALLBACK(url_radio_selected), NULL);

        /* Help */
        help = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("_Help"));
        helpsub = GTK_MENU_SHELL(gtk_menu_new());
        about = gtk_menu_item_new_with_mnemonic("_About...");
        gtk_menu_shell_append(bar, GTK_WIDGET(help));
        gtk_menu_item_set_submenu(help, GTK_WIDGET(helpsub));
        gtk_menu_shell_append(helpsub, about);
        g_signal_connect(G_OBJECT(about), "activate",
                         G_CALLBACK(show_about_dialog), w->window);

        w->radiomenu = GTK_WIDGET(radio);
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
        w->window = hildon_window_new();
#else
        w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
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
        gtk_box_pack_start(vbox, w->progressbar, TRUE, TRUE, 0);
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
