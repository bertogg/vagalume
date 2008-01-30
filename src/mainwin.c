/*
 * mainwin.c -- Functions to control the main program window
 * Copyright (C) 2007, 2008 Alberto Garcia <agarcia@igalia.com>
 * Copyright (C) 2008 Felipe Erias Morandeira <femorandeira@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#include "config.h"

#include <gdk/gdkkeysyms.h>
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

#ifdef MAEMO
static const int album_cover_size = 200;
static const int cover_frame_size = 230;
static const int primary_button_size = 110;
static const int secondary_button_size = 63;
#else
static const int album_cover_size = 110;
static const int cover_frame_size = 130;
static const int primary_button_size = 60;
static const int secondary_button_size = 35;
#endif

static const char *cover_background = APP_DATA_DIR "/cover.png";

static const char *authors[] = {
        "Alberto Garcia Gonzalez\n<agarcia@igalia.com>\n",
        "Mario Sanchez Prada\n<msanchez@igalia.com>",
        NULL
};
static const char *artists[] = {
        "Felipe Erias Morandeira\n<femorandeira@igalia.com>",
        NULL
};

static const char *appdescr = "A Last.fm client for Gnome and Maemo";
static const char *copyright = "(c) 2007, 2008 Alberto Garcia Gonzalez";
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
"http://www.gnu.org/licenses/\n";

void
mainwin_run_app(void)
{
        gtk_main();
}

void
mainwin_quit_app(void)
{
        gtk_main_quit();
}

void
mainwin_show_window(lastfm_mainwin *w, gboolean show)
{
        g_return_if_fail(w != NULL && GTK_IS_WINDOW(w->window));
        if (show) {
                gtk_window_present(w->window);
        } else {
                gtk_widget_hide(GTK_WIDGET(w->window));
        }
}

void
mainwin_set_album_cover(lastfm_mainwin *w, const guchar *data, int size)
{
        g_return_if_fail(w != NULL);
        GdkPixbufLoader *ldr = NULL;
        GdkPixbuf *pixbuf = NULL;
        if (data != NULL) {
                g_return_if_fail(size > 0);
                GError *err = NULL;
                ldr = gdk_pixbuf_loader_new();
                gdk_pixbuf_loader_set_size(ldr, album_cover_size,
                                           album_cover_size);
                gdk_pixbuf_loader_write(ldr, data, size, NULL);
                gdk_pixbuf_loader_close(ldr, &err);
                if (err != NULL) {
                        g_warning("Error loading image: %s",
                                  err->message ? err->message : "unknown");
                        g_error_free(err);
                        g_object_unref(G_OBJECT(ldr));
                        ldr = NULL;
                } else {
                        pixbuf = gdk_pixbuf_loader_get_pixbuf(ldr);;
                }
        }
        gtk_image_set_from_pixbuf(GTK_IMAGE(w->album_cover), pixbuf);
        gtk_widget_set_sensitive(w->album_cover, TRUE);
        if (ldr != NULL) g_object_unref(G_OBJECT(ldr));
}

static void
mainwin_update_track_info(lastfm_mainwin *w, const lastfm_track *t)
{
        g_return_if_fail(w != NULL && t != NULL);
        char *text;
        char *markup;

        markup = g_markup_escape_text(t->artist, -1);
        text = g_strconcat("<b>Artist:</b>\n", markup, "", NULL);
        gtk_label_set_markup(GTK_LABEL(w->artist), text);
        g_free(text);
        g_free(markup);

        markup = g_markup_escape_text(t->title, -1);
        text = g_strconcat("<b>Title:</b>\n", markup, "", NULL);
        gtk_label_set_markup(GTK_LABEL(w->track), text);
        g_free(text);
        g_free(markup);

        if (t->album[0] != '\0') {
                markup = g_markup_escape_text(t->album, -1);
                text = g_strconcat("<b>Album:</b>\n", markup, "", NULL);
                gtk_label_set_markup(GTK_LABEL(w->album), text);
                g_free(text);
                g_free(markup);
        } else {
                gtk_label_set_text(GTK_LABEL(w->album), "");
        }

        markup = g_markup_escape_text(t->pls_title, -1);
        text = g_strconcat("<b>Listening to <i>", markup, "</i></b>", NULL);
        gtk_label_set_markup(GTK_LABEL(w->playlist), text);
        g_free(text);
        g_free(markup);

        text = g_strconcat(t->artist, " - ", t->title, NULL);
        gtk_window_set_title(w->window, text);
        g_free(text);
}

static void
set_progress_bar_text(lastfm_mainwin *w, const char *text)
{
        g_return_if_fail(w != NULL);
        if (w->showing_msg) {
                /* If showing a status message, save text for later */
                g_string_assign(w->progressbar_text, text);
        } else {
                gtk_progress_bar_set_text(w->progressbar, text);
        }
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
        gtk_progress_bar_set_fraction(w->progressbar, fraction);
        set_progress_bar_text(w, count);
}

typedef struct {
        lastfm_mainwin *win;
        guint msgid;
} remove_status_msg_data;

static gboolean
remove_status_msg(gpointer data)
{
        g_return_val_if_fail(data != NULL, FALSE);
        remove_status_msg_data *d = (remove_status_msg_data *) data;
        if (d->win->lastmsg_id == d->msgid) {
                d->win->showing_msg = FALSE;
                /* Restore saved text */
                gtk_progress_bar_set_text(d->win->progressbar,
                                          d->win->progressbar_text->str);
        }
        g_slice_free(remove_status_msg_data, d);
        return FALSE;
}

void
mainwin_show_status_msg(lastfm_mainwin *w, const char *text)
{
        g_return_if_fail(w != NULL && text != NULL);
        remove_status_msg_data *d;
        w->lastmsg_id++;
        if (!(w->showing_msg)) {
                const char *old;
                w->showing_msg = TRUE;
                /* Save current text for later */
                old = gtk_progress_bar_get_text(w->progressbar);
                g_string_assign(w->progressbar_text, old ? old : " ");
        }
        gtk_progress_bar_set_text(w->progressbar, text);
        d = g_slice_new(remove_status_msg_data);
        d->win = w;
        d->msgid = w->lastmsg_id;
        g_timeout_add(1500, remove_status_msg, d);
}

void
mainwin_set_track_as_loved(lastfm_mainwin *w)
{
        g_return_if_fail(w != NULL);
        gtk_widget_set_sensitive (w->lovebutton, FALSE);
        gtk_widget_set_sensitive (w->love, FALSE);
}

void
mainwin_set_ui_state(lastfm_mainwin *w, lastfm_ui_state state,
                     const lastfm_track *t)
{
        g_return_if_fail(w != NULL);
        gboolean dim_labels = FALSE;
        switch (state) {
        case LASTFM_UI_STATE_DISCONNECTED:
        case LASTFM_UI_STATE_STOPPED:
                dim_labels = FALSE;
                set_progress_bar_text(w, "Stopped");
                gtk_label_set_text(GTK_LABEL(w->playlist), "Stopped");
                gtk_label_set_text(GTK_LABEL(w->artist), NULL);
                gtk_label_set_text(GTK_LABEL(w->track), NULL);
                gtk_label_set_text(GTK_LABEL(w->album), NULL);
                gtk_widget_show (w->play);
                gtk_widget_hide (w->stop);
                gtk_widget_set_sensitive (w->play, TRUE);
                gtk_widget_set_sensitive (w->next, FALSE);
                gtk_widget_set_sensitive (w->lovebutton, FALSE);
                gtk_widget_set_sensitive (w->banbutton, FALSE);
                gtk_widget_set_sensitive (w->recommendbutton, FALSE);
                gtk_widget_set_sensitive (w->dloadbutton, FALSE);
                gtk_widget_set_sensitive (w->tagbutton, FALSE);
                gtk_widget_set_sensitive (w->addplbutton, FALSE);
                gtk_widget_set_sensitive (w->radiomenu, TRUE);
                gtk_widget_set_sensitive (w->actionsmenu, FALSE);
                gtk_widget_set_sensitive (w->settings, TRUE);
                gtk_window_set_title(w->window, APP_NAME);
                mainwin_set_album_cover(w, NULL, 0);
                break;
        case LASTFM_UI_STATE_PLAYING:
                if (t == NULL) {
                        g_warning("Set playing state with t == NULL");
                        break;
                }
                mainwin_update_track_info(w, t);
                dim_labels = FALSE;
                set_progress_bar_text(w, "Playing...");
                gtk_widget_hide (w->play);
                gtk_widget_show (w->stop);
                gtk_widget_set_sensitive (w->stop, TRUE);
                gtk_widget_set_sensitive (w->next, TRUE);
                gtk_widget_set_sensitive (w->lovebutton, TRUE);
                gtk_widget_set_sensitive (w->banbutton, TRUE);
                gtk_widget_set_sensitive (w->recommendbutton, TRUE);
                gtk_widget_set_sensitive (w->dloadbutton,
                                          t->free_track_url != NULL);
                gtk_widget_set_sensitive (w->tagbutton, TRUE);
                gtk_widget_set_sensitive (w->addplbutton, TRUE);
                gtk_widget_set_sensitive (w->radiomenu, TRUE);
                gtk_widget_set_sensitive (w->actionsmenu, TRUE);
                gtk_widget_set_sensitive (w->love, TRUE);
                gtk_widget_set_sensitive (w->settings, TRUE);
                gtk_widget_set_sensitive (w->dload, t->free_track_url != NULL);
                break;
        case LASTFM_UI_STATE_CONNECTING:
                dim_labels = TRUE;
                set_progress_bar_text(w, "Connecting...");
                gtk_label_set_text(GTK_LABEL(w->playlist), "Connecting...");
                gtk_widget_hide (w->play);
                gtk_widget_show (w->stop);
                gtk_widget_set_sensitive (w->stop, FALSE);
                gtk_widget_set_sensitive (w->next, FALSE);
                gtk_widget_set_sensitive (w->lovebutton, FALSE);
                gtk_widget_set_sensitive (w->recommendbutton, FALSE);
                gtk_widget_set_sensitive (w->banbutton, FALSE);
                gtk_widget_set_sensitive (w->dloadbutton, FALSE);
                gtk_widget_set_sensitive (w->tagbutton, FALSE);
                gtk_widget_set_sensitive (w->addplbutton, FALSE);
                gtk_widget_set_sensitive (w->radiomenu, FALSE);
                gtk_widget_set_sensitive (w->actionsmenu, FALSE);
                gtk_widget_set_sensitive (w->settings, FALSE);
                gtk_window_set_title(w->window, APP_NAME);
                gtk_widget_set_sensitive(w->album_cover, FALSE);
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
        gtk_progress_bar_set_fraction(w->progressbar, 0);
}

static gboolean
window_state_cb(GtkWidget *widget, GdkEventWindowState *event,
                lastfm_mainwin *win)
{
        GdkWindowState st = event->new_window_state;
        win->is_fullscreen = (st & GDK_WINDOW_STATE_FULLSCREEN);
        win->is_hidden =
                st & (GDK_WINDOW_STATE_ICONIFIED|GDK_WINDOW_STATE_WITHDRAWN);
        if (!win->is_hidden) {
                controller_show_cover();
        }
        return FALSE;
}

#ifdef MAEMO
static gboolean
key_press_cb(GtkWidget *widget, GdkEventKey *event, lastfm_mainwin *win)
{
        switch (event->keyval) {
        case GDK_F6:
                if (win->is_fullscreen) {
                        gtk_window_unfullscreen(win->window);
                } else {
                        gtk_window_fullscreen(win->window);
                }
                break;
        case GDK_F7:
                controller_increase_volume(5);
                break;
        case GDK_F8:
                controller_increase_volume(-5);
                break;
        }
        return FALSE;
}
#endif /* MAEMO */

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
delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
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
        controller_tag_track();
}

static void
recomm_track_selected(GtkWidget *widget, gpointer data)
{
        controller_recomm_track();
}

static void
add_to_playlist_selected(GtkWidget *widget, gpointer data)
{
        controller_add_to_playlist();
}

static void
download_track_selected(GtkWidget *widget, gpointer data)
{
        controller_download_track();
}

static void
show_about_dialog(GtkWidget *widget, gpointer data)
{
        GtkWindow *win = GTK_WINDOW(data);
        GdkPixbuf *logo = gdk_pixbuf_new_from_file(APP_ICON_BIG, NULL);
        gtk_show_about_dialog(win, "name", APP_NAME, "authors", authors,
                              "comments", appdescr, "copyright", copyright,
                              "license", license, "version", APP_VERSION,
                              "website", website, "artists", artists,
                              "logo", logo, NULL);
        g_object_unref(G_OBJECT(logo));
}

static void
open_user_settings(GtkWidget *widget, gpointer data)
{
        controller_open_usercfg();
}

static gboolean
image_button_event_received(GtkWidget *widget, GdkEventButton *event,
                            gpointer data)
{
        GtkWidget *image = gtk_image_new_from_pixbuf(GDK_PIXBUF(data));
        gtk_button_set_image(GTK_BUTTON(widget), image);
        if (event->type == GDK_BUTTON_RELEASE) {
                gpointer mouse_over;
                mouse_over = g_object_get_data(G_OBJECT(widget), "mouse_over");
                /* If the mouse is over the button, emit a "clicked" event */
                if (mouse_over) gtk_button_clicked(GTK_BUTTON(widget));
        }
        return FALSE;
}

static gboolean
image_button_set_mouse_over(GtkWidget *widget, GdkEventCrossing *event,
                            gpointer data)
{
        /* Store whether the mouse is over the button */
        g_object_set_data_full(G_OBJECT(widget), "mouse_over", data, NULL);
        /* Stop this event from being propagated */
        return TRUE;
}

GtkWidget *
image_button_new(char *img)
{
        g_return_val_if_fail(img != NULL, NULL);
        GtkWidget *button;
        /* TODO: Not very important, but these pixbufs should be
         * destroyed along with the button */
        GdkPixbuf *normal, *pressed;
        char *normalstr, *pressedstr;
        GtkWidget *image;

        /* Load the images */
        normalstr = g_strdup_printf("%s/%s.png", APP_DATA_DIR, img);
        pressedstr = g_strdup_printf("%s/%s_pressed.png", APP_DATA_DIR, img);
        normal = gdk_pixbuf_new_from_file(normalstr, NULL);
        pressed = gdk_pixbuf_new_from_file(pressedstr, NULL);
        g_free(normalstr);
        g_free(pressedstr);

        button = compat_gtk_button_new();
        image = gtk_image_new_from_pixbuf(GDK_PIXBUF(normal));
        gtk_button_set_image(GTK_BUTTON(button), image);
        gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
        g_object_set(button, "can-focus", FALSE, NULL);

        g_signal_connect(button, "button-press-event",
                         G_CALLBACK(image_button_event_received), pressed);
        g_signal_connect(button, "button-release-event",
                         G_CALLBACK(image_button_event_received), normal);
        /* Store when the mouse enters or leaves the button */
        g_signal_connect(button, "enter-notify-event",
                         G_CALLBACK(image_button_set_mouse_over),
                         GINT_TO_POINTER(1));
        g_signal_connect(button, "leave-notify-event",
                         G_CALLBACK(image_button_set_mouse_over), NULL);
        return button;
}

static GtkWidget *
create_main_menu(lastfm_mainwin *w, GtkAccelGroup *accel)
{
        GtkMenuItem *lastfm, *radio, *actions, *help;
        GtkMenuItem *user, *others;
        GtkWidget *group, *globaltag, *similarartist, *urlradio;
        GtkMenuShell *lastfmsub, *radiosub, *actionssub, *helpsub;
        GtkMenuShell *usersub, *othersub;
        GtkWidget *settings, *quit;
        GtkWidget *love, *ban, *tag, *dorecomm, *addtopls, *dload;
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
        gtk_widget_add_accelerator(quit, "activate", accel, GDK_q,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
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
        actions = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("_Actions"));
        actionssub = GTK_MENU_SHELL(gtk_menu_new());
        love = gtk_menu_item_new_with_label("Love this track");
        ban = gtk_menu_item_new_with_label("Ban this track");
        addtopls = gtk_menu_item_new_with_label("Add to playlist");
        dload = gtk_menu_item_new_with_label("Download this track");
        tag = gtk_menu_item_new_with_label("Tag...");
        dorecomm = gtk_menu_item_new_with_label("Recommend...");
        gtk_menu_shell_append(bar, GTK_WIDGET(actions));
        gtk_menu_item_set_submenu(actions, GTK_WIDGET(actionssub));
        gtk_menu_shell_append(actionssub, love);
        gtk_menu_shell_append(actionssub, ban);
        gtk_menu_shell_append(actionssub, addtopls);
        gtk_menu_shell_append(actionssub, dload);
        gtk_menu_shell_append(actionssub, tag);
        gtk_menu_shell_append(actionssub, dorecomm);
        g_signal_connect(G_OBJECT(love), "activate",
                         G_CALLBACK(love_track_selected), NULL);
        g_signal_connect(G_OBJECT(ban), "activate",
                         G_CALLBACK(ban_track_selected), NULL);
        g_signal_connect(G_OBJECT(tag), "activate",
                         G_CALLBACK(tag_track_selected), NULL);
        g_signal_connect(G_OBJECT(dorecomm), "activate",
                         G_CALLBACK(recomm_track_selected), NULL);
        g_signal_connect(G_OBJECT(addtopls), "activate",
                         G_CALLBACK(add_to_playlist_selected), NULL);
        g_signal_connect(G_OBJECT(dload), "activate",
                         G_CALLBACK(download_track_selected), NULL);

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

        /* Keyboard shortcuts */
        gtk_widget_add_accelerator(ban, "activate", accel, GDK_b,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(love, "activate", accel, GDK_l,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(dorecomm, "activate", accel, GDK_r,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(tag, "activate", accel, GDK_t,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(addtopls, "activate", accel, GDK_p,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

        w->radiomenu = GTK_WIDGET(radio);
        w->actionsmenu = GTK_WIDGET(actions);
        w->settings = GTK_WIDGET(settings);
        w->dload = GTK_WIDGET(dload);
        w->love = GTK_WIDGET(love);
        return GTK_WIDGET(bar);
}

void
lastfm_mainwin_destroy(lastfm_mainwin *w)
{
        g_string_free(w->progressbar_text, TRUE);
        g_free(w);
}

lastfm_mainwin *
lastfm_mainwin_create(void)
{
        lastfm_mainwin *w = g_new0(lastfm_mainwin, 1);
        GtkBox *vbox, *centralbox;
        GtkBox *image_box_holder, *image_box_holder2, *labelbox;
        GtkBox *buttonshbox, *secondary_bbox, *secondary_bar_vbox;
        GtkWidget *menu;
        GtkWidget* image_box;
        GtkRcStyle* image_box_style;
        char *image_box_bg;
        GtkAccelGroup *accel = gtk_accel_group_new();
        w->progressbar_text = g_string_sized_new(30);
        g_string_assign(w->progressbar_text, " ");
        /* Window */
#ifdef MAEMO
        w->window = GTK_WINDOW(hildon_window_new());
#else
        w->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
        gtk_window_set_default_size(w->window, 500, -1);
#endif
        gtk_window_add_accel_group(w->window, accel);
        gtk_window_set_icon_from_file(w->window, APP_ICON, NULL);
        gtk_container_set_border_width(GTK_CONTAINER(w->window), 2);
        /* Boxes */
        vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
        centralbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        image_box_holder = GTK_BOX(gtk_hbox_new(FALSE, 0));
        image_box_holder2 = GTK_BOX(gtk_vbox_new(FALSE, 5));
        labelbox = GTK_BOX(gtk_vbox_new(TRUE, 5));
        buttonshbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
        secondary_bbox = GTK_BOX(gtk_hbox_new(TRUE, 5));
        secondary_bar_vbox = GTK_BOX(gtk_vbox_new(FALSE, 2));
        /* Buttons */
        w->play = image_button_new("play");
        w->stop = image_button_new("stop");
        w->next = image_button_new("next");
        w->lovebutton = image_button_new("love");
        w->banbutton = image_button_new("ban");
        w->recommendbutton = image_button_new("recommend");
        w->dloadbutton = image_button_new("dload");
        w->tagbutton = image_button_new("tag");
        w->addplbutton = image_button_new("addplist");
        /* Text labels */
        w->playlist = gtk_label_new(NULL);
        w->artist = gtk_label_new(NULL);
        w->track = gtk_label_new(NULL);
        w->album = gtk_label_new(NULL);
        gtk_label_set_ellipsize(GTK_LABEL(w->playlist), PANGO_ELLIPSIZE_END);
        gtk_label_set_ellipsize(GTK_LABEL(w->artist), PANGO_ELLIPSIZE_END);
        gtk_label_set_ellipsize(GTK_LABEL(w->track), PANGO_ELLIPSIZE_END);
        gtk_label_set_ellipsize(GTK_LABEL(w->album), PANGO_ELLIPSIZE_END);
        /* Cover image */
        image_box = gtk_event_box_new();
        image_box_style = gtk_rc_style_new();
        image_box_bg = g_strdup(cover_background);
        image_box_style->bg_pixmap_name[GTK_STATE_NORMAL] = image_box_bg;
        image_box_style->bg_pixmap_name[GTK_STATE_ACTIVE] = image_box_bg;
        image_box_style->bg_pixmap_name[GTK_STATE_PRELIGHT] = image_box_bg;
        image_box_style->bg_pixmap_name[GTK_STATE_SELECTED] = image_box_bg;
        image_box_style->bg_pixmap_name[GTK_STATE_INSENSITIVE] = image_box_bg;
        gtk_widget_modify_style(image_box, image_box_style);
        gtk_widget_set_size_request(GTK_WIDGET(image_box),
                                    cover_frame_size, cover_frame_size);
        w->album_cover = gtk_image_new();
        gtk_container_add(GTK_CONTAINER(image_box), w->album_cover);
        /* Menu */
        menu = create_main_menu(w, accel);
        /* Progress bar */
        w->progressbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
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

        gtk_box_pack_start(buttonshbox, w->play, FALSE, FALSE, 0);
        gtk_box_pack_start(buttonshbox, w->stop, FALSE, FALSE, 0);
        gtk_box_pack_start(buttonshbox, w->next, FALSE, FALSE, 0);

        gtk_box_pack_start(secondary_bbox, w->lovebutton, TRUE, FALSE, 0);
        gtk_box_pack_start(secondary_bbox, w->banbutton, TRUE, FALSE, 0);
        gtk_box_pack_start(secondary_bbox, w->recommendbutton, TRUE, FALSE, 0);
        gtk_box_pack_start(secondary_bbox, w->dloadbutton, TRUE, FALSE, 0);
        gtk_box_pack_start(secondary_bbox, w->tagbutton, TRUE, FALSE, 0);
        gtk_box_pack_start(secondary_bbox, w->addplbutton, TRUE, FALSE, 0);

        gtk_box_pack_start(secondary_bar_vbox,
                           GTK_WIDGET(secondary_bbox), FALSE, FALSE, 0);
        gtk_box_pack_start(secondary_bar_vbox,
                           GTK_WIDGET(w->progressbar), FALSE, FALSE, 0);
        gtk_box_pack_start(buttonshbox,
                           GTK_WIDGET(secondary_bar_vbox), TRUE, TRUE, 0);

        gtk_box_pack_start(labelbox, w->artist, TRUE, TRUE, 0);
        gtk_box_pack_start(labelbox, w->track, TRUE, TRUE, 0);
        gtk_box_pack_start(labelbox, w->album, TRUE, TRUE, 0);

        gtk_box_pack_start(image_box_holder, image_box, FALSE, FALSE, 0);
        gtk_box_pack_start(image_box_holder2,
                           GTK_WIDGET(image_box_holder), TRUE, FALSE, 0);

        gtk_box_pack_start(centralbox,
                           GTK_WIDGET(image_box_holder2), FALSE, FALSE, 0);
        gtk_box_pack_start(centralbox, GTK_WIDGET(labelbox), TRUE, TRUE, 0);

        gtk_container_add(GTK_CONTAINER(w->window), GTK_WIDGET(vbox));

#ifdef MAEMO
        hildon_window_set_menu(HILDON_WINDOW(w->window), GTK_MENU(menu));
#else
        gtk_box_pack_start(vbox, menu, FALSE, FALSE, 0);
#endif
        gtk_box_pack_start(vbox, w->playlist, FALSE, FALSE, 0);
        gtk_box_pack_start(vbox, gtk_hseparator_new(), FALSE, FALSE, 0);
        gtk_box_pack_start(vbox, GTK_WIDGET(centralbox), TRUE, TRUE, 0);
        gtk_box_pack_start(vbox, GTK_WIDGET(buttonshbox), FALSE, FALSE, 0);

        /* Signals */
        g_signal_connect(G_OBJECT(w->play), "clicked",
                         G_CALLBACK(play_clicked), NULL);
        g_signal_connect(G_OBJECT(w->next), "clicked",
                         G_CALLBACK(next_clicked), NULL);
        g_signal_connect(G_OBJECT(w->stop), "clicked",
                         G_CALLBACK(stop_clicked), NULL);
        g_signal_connect(G_OBJECT(w->lovebutton), "clicked",
                         G_CALLBACK(love_track_selected), NULL);
        g_signal_connect(G_OBJECT(w->banbutton), "clicked",
                         G_CALLBACK(ban_track_selected), NULL);
        g_signal_connect(G_OBJECT(w->recommendbutton), "clicked",
                         G_CALLBACK(recomm_track_selected), NULL);
        g_signal_connect(G_OBJECT(w->dloadbutton), "clicked",
                         G_CALLBACK(download_track_selected), NULL);
        g_signal_connect(G_OBJECT(w->tagbutton), "clicked",
                         G_CALLBACK(tag_track_selected), NULL);
        g_signal_connect(G_OBJECT(w->addplbutton), "clicked",
                         G_CALLBACK(add_to_playlist_selected), NULL);
        g_signal_connect(G_OBJECT(w->window), "destroy",
                         G_CALLBACK(close_app), NULL);
        g_signal_connect(G_OBJECT(w->window), "delete-event",
                         G_CALLBACK(delete_event), NULL);
#ifdef MAEMO
        g_signal_connect(G_OBJECT(w->window), "key_press_event",
                         G_CALLBACK(key_press_cb), w);
#endif
        g_signal_connect(G_OBJECT(w->window), "window_state_event",
                         G_CALLBACK(window_state_cb), w);
        /* Shortcuts */
#ifndef MAEMO
        gtk_widget_add_accelerator(w->play, "clicked", accel, GDK_space,
                                   0, 0);
        gtk_widget_add_accelerator(w->stop, "clicked", accel, GDK_space,
                                   0, 0);
        gtk_widget_add_accelerator(w->next, "clicked", accel, GDK_Right,
                                   GDK_CONTROL_MASK, 0);
#endif
        /* Initial state */
        gtk_widget_show_all(GTK_WIDGET(vbox));
        mainwin_set_ui_state(w, LASTFM_UI_STATE_DISCONNECTED, NULL);
        return w;
}
