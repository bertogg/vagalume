/*
 * vgl-main-window.c -- Main program window
 * Copyright (C) 2007, 2008 Alberto Garcia <agarcia@igalia.com>
 * Copyright (C) 2008 Felipe Erias Morandeira <femorandeira@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>

#include "controller.h"
#include "vgl-main-window.h"
#include "radio.h"
#include "uimisc.h"
#include "xmlrpc.h"
#include "util.h"

#ifdef MAEMO
#        define PARENT_CLASS_TYPE HILDON_TYPE_WINDOW
#        define ALBUM_COVER_SIZE 200
#        define COVER_FRAME_SIZE 230
#        define BIGBUTTON_IMG_SIZE 64
#        define SMALLBUTTON_IMG_SIZE 50
#else
#        define PARENT_CLASS_TYPE GTK_TYPE_WINDOW
#        define ALBUM_COVER_SIZE 110
#        define COVER_FRAME_SIZE 130
#        define BIGBUTTON_IMG_SIZE 48
#        define SMALLBUTTON_IMG_SIZE 24
#endif

#define BIGBUTTON_SIZE ((COVER_FRAME_SIZE - 5) / 2)

#define VGL_MAIN_WINDOW_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), VGL_TYPE_MAIN_WINDOW, \
                                      VglMainWindowPrivate))
G_DEFINE_TYPE (VglMainWindow, vgl_main_window, PARENT_CLASS_TYPE);

typedef struct _VglMainWindowPrivate VglMainWindowPrivate;
struct _VglMainWindowPrivate {
        GtkWidget *play, *stop;
        GtkWidget *playbutton, *stopbutton, *skipbutton;
        GtkWidget *lovebutton, *banbutton, *recommendbutton;
        GtkWidget *dloadbutton, *tagbutton, *addplbutton;
        GtkWidget *playlist, *artist, *track, *album;
        GtkWidget *radiomenu, *actionsmenu, *settings, *love, *dload;
        GtkWidget *addtopls, *stopafter;
        GtkWidget *album_cover;
        GtkProgressBar *progressbar;
        GString *progressbar_text;
        gboolean is_fullscreen;
        gboolean is_hidden;
        gboolean showing_msg;
        guint lastmsg_id;
        int x_pos, y_pos;
};

typedef struct {
        const char *icon_name;
        const int icon_size;
        const int button_width;
        const int button_height;
        const char *tooltip;
} button_data;

static const button_data play_button = {
        "media-playback-start", BIGBUTTON_IMG_SIZE, BIGBUTTON_SIZE, -1,
        N_("Start playing")
};
static const button_data stop_button = {
        "media-playback-stop", BIGBUTTON_IMG_SIZE, BIGBUTTON_SIZE, -1,
        N_("Stop playing")
};
static const button_data skip_button = {
        "media-skip-forward", BIGBUTTON_IMG_SIZE, BIGBUTTON_SIZE, -1,
        N_("Skip this track")
};
static const button_data love_button = {
        "emblem-favorite", SMALLBUTTON_IMG_SIZE, -1, -1,
        N_("Mark as loved")
};
static const button_data ban_button = {
        "process-stop", SMALLBUTTON_IMG_SIZE, -1, -1,
        N_("Ban this track")
};
static const button_data recommend_button = {
        "mail-message-new", SMALLBUTTON_IMG_SIZE, -1, -1,
        N_("Recommend this track")
};
static const button_data dload_button = {
        "document-save", SMALLBUTTON_IMG_SIZE, -1, -1,
        N_("Download this track")
};
static const button_data tag_button = {
        "accessories-text-editor", SMALLBUTTON_IMG_SIZE, -1, -1,
        N_("Edit tags for this track")
};
static const button_data addpl_button = {
        "list-add", SMALLBUTTON_IMG_SIZE, -1, -1,
        N_("Add this track to playlist")
};

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

static const char *appdescr = N_("A Last.fm client for Gnome and Maemo");
static const char *copyright = "(c) 2007, 2008 Alberto Garcia Gonzalez";
static const char *website = "http://vagalume.igalia.com/";
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
static const char *translators_tpl =
"%s (de)\n* Stephan Reichholf <stephan@reichholf.net>\n"
         "* Oskar Welzl <mail@welzl.info>\n\n"
"%s (es)\n* Oscar A. Mata T. <omata_mac@yahoo.com>\n\n"
"%s (fi)\n* Janne Mäkinen <janne.makinen@surffi.fi>\n\n"
"%s (gl)\n* Ignacio Casal Quinteiro <nacho.resa@gmail.com>\n"
         "* Amador Loureiro Blanco <dorfun@adorfunteca.org>\n\n"
"%s (it)\n* Andrea Grandi <a.grandi@gmail.com>\n\n"
"%s (lv)\n* Pēteris Caune <cuu508@gmail.com>\n\n"
"%s (pt)\n* Marcos Garcia <marcosgg@gmail.com>\n\n"
"%s (pt_BR)\n* Rodrigo Flores <rodrigomarquesflores@gmail.com>";

void
vgl_main_window_run_app(void)
{
        gtk_main();
}

void
vgl_main_window_destroy(VglMainWindow *w)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        while (gtk_events_pending()) gtk_main_iteration();
        gtk_widget_destroy(GTK_WIDGET(w));
        gtk_main_quit();
}

GtkWindow *
vgl_main_window_get_window(VglMainWindow *w, gboolean get_if_hidden)
{
        g_return_val_if_fail(VGL_IS_MAIN_WINDOW(w), NULL);
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        if (!priv->is_hidden || get_if_hidden) {
                return GTK_WINDOW(w);
        } else {
                return NULL;
        }
}

void
vgl_main_window_show(VglMainWindow *w, gboolean show)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        GtkWindow *win = GTK_WINDOW(w);
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        if (show) {
                /* Move window to its right place (not needed in Maemo) */
                gtk_window_move (win, priv->x_pos, priv->y_pos);
                gtk_window_present (win);
        } else {
                /* Save position before hiding window (not needed in Maemo) */
                gtk_window_get_position(win, &(priv->x_pos), &(priv->y_pos));
                gtk_widget_hide (GTK_WIDGET(w));
        }
}

void
vgl_main_window_toggle_visibility(VglMainWindow *w)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        vgl_main_window_show(w, priv->is_hidden);
}

gboolean
vgl_main_window_is_hidden(VglMainWindow *w)
{
        g_return_val_if_fail(VGL_IS_MAIN_WINDOW(w), TRUE);
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        return priv->is_hidden;
}

void
vgl_main_window_set_album_cover(VglMainWindow *w, const char *data, int size)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        GdkPixbuf *pixbuf = get_pixbuf_from_image(data, size, ALBUM_COVER_SIZE);
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->album_cover), pixbuf);
        gtk_widget_set_sensitive(priv->album_cover, TRUE);
        if (pixbuf != NULL) g_object_unref(pixbuf);
}

static void
vgl_main_window_update_track_info(VglMainWindow *w, const lastfm_track *t)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w) && t != NULL);
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        GString *text = g_string_sized_new(50);
        char *markup;

        markup = g_markup_escape_text(t->artist, -1);
        g_string_assign(text, _("<b>Artist:</b>\n"));
        g_string_append(text, markup);
        gtk_label_set_markup(GTK_LABEL(priv->artist), text->str);
        g_free(markup);

        markup = g_markup_escape_text(t->title, -1);
        g_string_assign(text, _("<b>Title:</b>\n"));
        g_string_append(text, markup);
        gtk_label_set_markup(GTK_LABEL(priv->track), text->str);
        g_free(markup);

        if (t->album[0] != '\0') {
                markup = g_markup_escape_text(t->album, -1);
                g_string_assign(text, _("<b>Album:</b>\n"));
                g_string_append(text, markup);
                gtk_label_set_markup(GTK_LABEL(priv->album), text->str);
                g_free(markup);
        } else {
                gtk_label_set_text(GTK_LABEL(priv->album), "");
        }

        markup = g_markup_escape_text(t->pls_title, -1);
        g_string_assign(text, _("<b>Listening to <i>"));
        g_string_append(text, markup);
        g_string_append(text, "</i></b>");
        gtk_label_set_markup(GTK_LABEL(priv->playlist), text->str);
        g_free(markup);

        g_string_assign(text, t->artist);
        g_string_append(text, " - ");
        g_string_append(text, t->title);
        gtk_window_set_title(GTK_WINDOW(w), text->str);

        g_string_free(text, TRUE);
}

static void
set_progress_bar_text(VglMainWindow *w, const char *text)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        if (priv->showing_msg) {
                /* If showing a status message, save text for later */
                g_string_assign(priv->progressbar_text, text);
        } else {
                gtk_progress_bar_set_text(priv->progressbar, text);
        }
}

void
vgl_main_window_show_progress(VglMainWindow *w, guint length, guint played)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
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
        gtk_progress_bar_set_fraction(priv->progressbar, fraction);
        set_progress_bar_text(w, count);
}

typedef struct {
        VglMainWindow *win;
        guint msgid;
} remove_status_msg_data;

static gboolean
remove_status_msg(gpointer data)
{
        g_return_val_if_fail(data != NULL, FALSE);
        remove_status_msg_data *d = (remove_status_msg_data *) data;
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(d->win);
        if (priv->lastmsg_id == d->msgid) {
                priv->showing_msg = FALSE;
                /* Restore saved text */
                gtk_progress_bar_set_text(priv->progressbar,
                                          priv->progressbar_text->str);
        }
        g_slice_free(remove_status_msg_data, d);
        return FALSE;
}

void
vgl_main_window_show_status_msg(VglMainWindow *w, const char *text)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w) && text != NULL);
        remove_status_msg_data *d;
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        priv->lastmsg_id++;
        if (!(priv->showing_msg)) {
                const char *old;
                priv->showing_msg = TRUE;
                /* Save current text for later */
                old = gtk_progress_bar_get_text(priv->progressbar);
                g_string_assign(priv->progressbar_text, old ? old : " ");
        }
        gtk_progress_bar_set_text(priv->progressbar, text);
        d = g_slice_new(remove_status_msg_data);
        d->win = w;
        d->msgid = priv->lastmsg_id;
        g_timeout_add(1500, remove_status_msg, d);
}

void
vgl_main_window_set_track_as_loved(VglMainWindow *w)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        gtk_widget_set_sensitive (priv->lovebutton, FALSE);
        gtk_widget_set_sensitive (priv->love, FALSE);
}

void
vgl_main_window_set_track_as_added_to_playlist(VglMainWindow *w,gboolean added)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        gtk_widget_set_sensitive (priv->addplbutton, !added);
        gtk_widget_set_sensitive (priv->addtopls, !added);
}

static void
menu_enable_all_items(GtkWidget *menu, gboolean enable)
{
        g_return_if_fail(GTK_IS_MENU(menu));
        GList *l;
        GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
        for (l = items; l != NULL; l = g_list_next(l)) {
                GtkWidget *item, *submenu;
                item = GTK_WIDGET(l->data);
                submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
                if (submenu != NULL) {
                        menu_enable_all_items(submenu, enable);
                } else {
                        gtk_widget_set_sensitive(item, enable);
                }
        }
        if (items != NULL) g_list_free(items);
}

void
vgl_main_window_set_state(VglMainWindow *w, VglMainWindowState state,
                          const lastfm_track *t)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        gboolean dim_labels = FALSE;
        switch (state) {
        case VGL_MAIN_WINDOW_STATE_DISCONNECTED:
        case VGL_MAIN_WINDOW_STATE_STOPPED:
                dim_labels = FALSE;
                set_progress_bar_text(w, _("Stopped"));
                gtk_label_set_text(GTK_LABEL(priv->playlist), _("Stopped"));
                gtk_label_set_text(GTK_LABEL(priv->artist), NULL);
                gtk_label_set_text(GTK_LABEL(priv->track), NULL);
                gtk_label_set_text(GTK_LABEL(priv->album), NULL);
                gtk_widget_show (priv->play);
                gtk_widget_hide (priv->stop);
                gtk_widget_show (priv->playbutton);
                gtk_widget_hide (priv->stopbutton);
                menu_enable_all_items (priv->actionsmenu, FALSE);
                menu_enable_all_items (priv->radiomenu, TRUE);
                gtk_widget_set_sensitive (priv->play, TRUE);
                gtk_widget_set_sensitive (priv->playbutton, TRUE);
                gtk_widget_set_sensitive (priv->skipbutton, FALSE);
                gtk_widget_set_sensitive (priv->lovebutton, FALSE);
                gtk_widget_set_sensitive (priv->banbutton, FALSE);
                gtk_widget_set_sensitive (priv->recommendbutton, FALSE);
                gtk_widget_set_sensitive (priv->dloadbutton, FALSE);
                gtk_widget_set_sensitive (priv->tagbutton, FALSE);
                gtk_widget_set_sensitive (priv->addplbutton, FALSE);
                gtk_widget_set_sensitive (priv->settings, TRUE);
                gtk_window_set_title(GTK_WINDOW(w), APP_NAME);
                vgl_main_window_set_album_cover(w, NULL, 0);
                break;
        case VGL_MAIN_WINDOW_STATE_PLAYING:
                if (t == NULL) {
                        g_warning("Set playing state with t == NULL");
                        break;
                }
                vgl_main_window_update_track_info(w, t);
                dim_labels = FALSE;
                set_progress_bar_text(w, _("Playing..."));
                gtk_widget_hide (priv->play);
                gtk_widget_hide (priv->playbutton);
                gtk_widget_show (priv->stop);
                gtk_widget_show (priv->stopbutton);
                menu_enable_all_items (priv->actionsmenu, TRUE);
                menu_enable_all_items (priv->radiomenu, TRUE);
                gtk_widget_set_sensitive (priv->play, FALSE);
                gtk_widget_set_sensitive (priv->stopbutton, TRUE);
                gtk_widget_set_sensitive (priv->skipbutton, TRUE);
                gtk_widget_set_sensitive (priv->lovebutton, TRUE);
                gtk_widget_set_sensitive (priv->banbutton, TRUE);
                gtk_widget_set_sensitive (priv->recommendbutton, TRUE);
                gtk_widget_set_sensitive (priv->dloadbutton,
                                          t->free_track_url != NULL);
                gtk_widget_set_sensitive (priv->tagbutton, TRUE);
                gtk_widget_set_sensitive (priv->addplbutton, TRUE);
                gtk_widget_set_sensitive (priv->love, TRUE);
                gtk_widget_set_sensitive (priv->settings, TRUE);
                gtk_widget_set_sensitive (priv->dload, t->free_track_url != NULL);
                break;
        case VGL_MAIN_WINDOW_STATE_CONNECTING:
                dim_labels = TRUE;
                set_progress_bar_text(w, _("Connecting..."));
                gtk_label_set_text(GTK_LABEL(priv->playlist), _("Connecting..."));
                gtk_widget_hide (priv->play);
                gtk_widget_hide (priv->playbutton);
                gtk_widget_show (priv->stop);
                gtk_widget_show (priv->stopbutton);
                menu_enable_all_items (priv->actionsmenu, FALSE);
                menu_enable_all_items (priv->radiomenu, FALSE);
                gtk_widget_set_sensitive (priv->stopbutton, FALSE);
                gtk_widget_set_sensitive (priv->skipbutton, FALSE);
                gtk_widget_set_sensitive (priv->lovebutton, FALSE);
                gtk_widget_set_sensitive (priv->recommendbutton, FALSE);
                gtk_widget_set_sensitive (priv->banbutton, FALSE);
                gtk_widget_set_sensitive (priv->dloadbutton, FALSE);
                gtk_widget_set_sensitive (priv->tagbutton, FALSE);
                gtk_widget_set_sensitive (priv->addplbutton, FALSE);
                gtk_widget_set_sensitive (priv->settings, FALSE);
                gtk_window_set_title(GTK_WINDOW(w), APP_NAME);
                gtk_widget_set_sensitive(priv->album_cover, FALSE);
                gtk_check_menu_item_set_active(
                        GTK_CHECK_MENU_ITEM(priv->stopafter), FALSE);
                break;
        default:
                g_critical("Unknown ui state received: %d", state);
                break;
        }
        if (state == VGL_MAIN_WINDOW_STATE_DISCONNECTED) {
                gtk_label_set_text(GTK_LABEL(priv->playlist), _("Disconnected"));
        }
        gtk_widget_set_sensitive (priv->artist, !dim_labels);
        gtk_widget_set_sensitive (priv->track, !dim_labels);
        gtk_widget_set_sensitive (priv->album, !dim_labels);
        gtk_progress_bar_set_fraction(priv->progressbar, 0);
}

static gboolean
window_state_cb(GtkWidget *widget, GdkEventWindowState *event,
                VglMainWindow *win)
{
        GdkWindowState st = event->new_window_state;
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(win);
        /* Save the old position if the window has been minimized */
        if (st == GDK_WINDOW_STATE_ICONIFIED && event->changed_mask == st) {
                gtk_window_get_position(
                        GTK_WINDOW(win), &(priv->x_pos), &(priv->y_pos));
        }
        priv->is_fullscreen = (st & GDK_WINDOW_STATE_FULLSCREEN);
        priv->is_hidden =
                st & (GDK_WINDOW_STATE_ICONIFIED|GDK_WINDOW_STATE_WITHDRAWN);
        if (!priv->is_hidden) {
                controller_show_cover();
        }
        return FALSE;
}

#ifdef MAEMO
static void
is_topmost_cb(GObject *obj, GParamSpec *arg, VglMainWindow *win)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(win));
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(win);
        HildonWindow *hildonwin = HILDON_WINDOW(win);
        priv->is_hidden = !hildon_window_get_is_topmost(hildonwin);
        if (!priv->is_hidden) {
                controller_show_cover();
        }
}

static gboolean
key_press_cb(GtkWidget *widget, GdkEventKey *event, VglMainWindow *win)
{
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(win);
        int volchange = 0;
        switch (event->keyval) {
        case GDK_F6:
                if (priv->is_fullscreen) {
                        gtk_window_unfullscreen(GTK_WINDOW(win));
                } else {
                        gtk_window_fullscreen(GTK_WINDOW(win));
                }
                break;
        case GDK_F7:
                volchange = 5;
                break;
        case GDK_F8:
                volchange = -5;
                break;
        }
        if (volchange != 0) {
                int newvol = controller_increase_volume(volchange);
                char *text = g_strdup_printf(_("Volume: %d/100"), newvol);
                controller_show_banner(text);
                g_free(text);
        }
        return FALSE;
}
#endif /* MAEMO */

static void
play_selected(GtkWidget *widget, gpointer data)
{
        controller_start_playing();
}

static void
stop_selected(GtkWidget *widget, gpointer data)
{
        controller_stop_playing();
}

static void
skip_selected(GtkWidget *widget, gpointer data)
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
        controller_close_mainwin();
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
stop_after_selected(GtkWidget *widget, gpointer data)
{
        GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM(widget);
        gboolean stop = gtk_check_menu_item_get_active(item);
        controller_set_stop_after(stop);
}

static void
love_track_selected(GtkWidget *widget, gpointer data)
{
        controller_love_track(TRUE);
}

static void
ban_track_selected(GtkWidget *widget, gpointer data)
{
        controller_ban_track(TRUE);
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
manage_bookmarks_selected(GtkWidget *widget, gpointer data)
{
        controller_manage_bookmarks();
}

static void
show_about_dialog(GtkWidget *widget, gpointer data)
{
        GtkWindow *win = GTK_WINDOW(data);
        GdkPixbuf *logo = gdk_pixbuf_new_from_file(APP_ICON_BIG, NULL);
        char *translators = g_strdup_printf(translators_tpl, _("German"),
                                            _("Spanish"), _("Finnish"),
                                            _("Galician"), _("Italian"),
                                            _("Latvian"), _("Portuguese"),
                                            _("Portuguese"));
        gtk_show_about_dialog(win, "name", APP_NAME, "authors", authors,
                              "comments", _(appdescr), "copyright", copyright,
                              "license", license, "version", APP_VERSION,
                              "website", website, "artists", artists,
                              "translator-credits", translators,
                              "logo", logo, NULL);
        g_object_unref(G_OBJECT(logo));
        g_free(translators);
}

static void
open_user_settings(GtkWidget *widget, gpointer data)
{
        controller_open_usercfg();
}

static void
apply_icon_theme(GtkIconTheme *icon_theme, GObject* button)
{
        g_return_if_fail(button != NULL);
        const button_data *data;
        GdkPixbuf *pixbuf = NULL;
        GtkWidget *image;

        data = (const button_data *) g_object_get_data(button, "button_data");

        g_return_if_fail(data && data->icon_name && data->icon_size > 0);

        pixbuf = gtk_icon_theme_load_icon(icon_theme, data->icon_name,
                                          data->icon_size, 0, NULL);

        if (pixbuf != NULL) {
                image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_button_set_image(GTK_BUTTON(button), image);
                g_object_unref (pixbuf);
        }
}

static GtkWidget *
image_button_new(const button_data *data)
{
        g_return_val_if_fail(data->icon_name && data->icon_size > 0, NULL);
        GtkWidget *button = compat_gtk_button_new();
        GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
        static GtkTooltips *tooltips = NULL;

        g_object_set_data(G_OBJECT(button), "button_data", (gpointer)data);

        apply_icon_theme(icon_theme, G_OBJECT(button));

        g_signal_connect(G_OBJECT(icon_theme), "changed",
                         G_CALLBACK(apply_icon_theme), button);

        gtk_widget_set_size_request(button, data->button_width,
                                    data->button_height);

        if (data->tooltip != NULL) {
                if (tooltips == NULL) tooltips = gtk_tooltips_new();
                gtk_tooltips_set_tip(tooltips, button,
                                     _(data->tooltip), _(data->tooltip));
        }

        return button;
}

static GtkWidget *
create_main_menu(VglMainWindow *w, GtkAccelGroup *accel)
{
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(w);
        GtkMenuItem *lastfm, *radio, *actions, *bookmarks, *help;
        GtkMenuItem *user, *others;
        GtkWidget *group, *globaltag, *similarartist, *urlradio;
        GtkMenuShell *lastfmsub, *radiosub, *actionssub, *bmksub, *helpsub;
        GtkMenuShell *usersub, *othersub;
        GtkWidget *settings, *quit;
        GtkWidget *play, *stop, *skip, *separ1, *separ2;
        GtkWidget *stopafter, *love, *ban, *tag, *dorecomm, *addtopls, *dload;
        GtkWidget *personal, *neigh, *loved, *playlist, *recomm, *usertag;
        GtkWidget *personal2, *neigh2, *loved2, *playlist2;
        GtkWidget *managebmk;
        GtkWidget *about;
#ifdef MAEMO
        GtkMenuShell *bar = GTK_MENU_SHELL(gtk_menu_new());
#else
        GtkMenuShell *bar = GTK_MENU_SHELL(gtk_menu_bar_new());
#endif

        /* Last.fm */
        lastfm = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(_("_Last.fm")));
        lastfmsub = GTK_MENU_SHELL(gtk_menu_new());
        settings = gtk_menu_item_new_with_label(_("Settings..."));
        quit = gtk_menu_item_new_with_label(_("Quit"));
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
        radio = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(_("Play _Radio")));
        radiosub = GTK_MENU_SHELL(gtk_menu_new());
        user = GTK_MENU_ITEM(gtk_menu_item_new_with_label(_("My radios")));
        others = GTK_MENU_ITEM(gtk_menu_item_new_with_label(
                                       _("Others' radios")));
        group = gtk_menu_item_new_with_label(_("Group radio..."));
        globaltag = gtk_menu_item_new_with_label(_("Music tagged..."));
        similarartist = gtk_menu_item_new_with_label(_("Artists similar to..."));
        urlradio = gtk_menu_item_new_with_label(_("Enter URL..."));
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
        personal = gtk_menu_item_new_with_label(_("My personal radio"));
        neigh = gtk_menu_item_new_with_label(_("My neighbours"));
        loved = gtk_menu_item_new_with_label(_("My loved tracks"));
        playlist = gtk_menu_item_new_with_label(_("My playlist"));
        recomm = gtk_menu_item_new_with_label(_("My recommendations"));
        usertag = gtk_menu_item_new_with_label(_("My music tagged..."));
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
        personal2 = gtk_menu_item_new_with_label(_("Personal..."));
        neigh2 = gtk_menu_item_new_with_label(_("Neighbours..."));
        loved2 = gtk_menu_item_new_with_label(_("Loved tracks..."));
        playlist2 = gtk_menu_item_new_with_label(_("Playlist..."));
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
        actions = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(_("_Actions")));
        actionssub = GTK_MENU_SHELL(gtk_menu_new());
        play = gtk_menu_item_new_with_label(_("Play"));
        stop = gtk_menu_item_new_with_label(_("Stop"));
        skip = gtk_menu_item_new_with_label(_("Skip"));
        separ1 = gtk_separator_menu_item_new();
        separ2 = gtk_separator_menu_item_new();
        stopafter =
                gtk_check_menu_item_new_with_label(_("Stop after this track"));
        love = gtk_menu_item_new_with_label(_("Love this track"));
        ban = gtk_menu_item_new_with_label(_("Ban this track"));
        addtopls = gtk_menu_item_new_with_label(_("Add to playlist"));
        dload = gtk_menu_item_new_with_label(_("Download this track"));
        tag = gtk_menu_item_new_with_label(_("Tag..."));
        dorecomm = gtk_menu_item_new_with_label(_("Recommend..."));
        gtk_menu_shell_append(bar, GTK_WIDGET(actions));
        gtk_menu_item_set_submenu(actions, GTK_WIDGET(actionssub));
        gtk_menu_shell_append(actionssub, play);
        gtk_menu_shell_append(actionssub, stop);
        gtk_menu_shell_append(actionssub, skip);
        gtk_menu_shell_append(actionssub, separ1);
        gtk_menu_shell_append(actionssub, love);
        gtk_menu_shell_append(actionssub, ban);
        gtk_menu_shell_append(actionssub, addtopls);
        gtk_menu_shell_append(actionssub, dload);
        gtk_menu_shell_append(actionssub, tag);
        gtk_menu_shell_append(actionssub, dorecomm);
        gtk_menu_shell_append(actionssub, separ2);
        gtk_menu_shell_append(actionssub, stopafter);
        g_signal_connect(G_OBJECT(stopafter), "toggled",
                         G_CALLBACK(stop_after_selected), NULL);
        g_signal_connect(G_OBJECT(play), "activate",
                         G_CALLBACK(play_selected), NULL);
        g_signal_connect(G_OBJECT(stop), "activate",
                         G_CALLBACK(stop_selected), NULL);
        g_signal_connect(G_OBJECT(skip), "activate",
                         G_CALLBACK(skip_selected), NULL);
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

        /* Bookmarks */
        bookmarks = GTK_MENU_ITEM(
                gtk_menu_item_new_with_mnemonic(_("_Bookmarks")));
        bmksub = GTK_MENU_SHELL(gtk_menu_new());
        managebmk = gtk_menu_item_new_with_label(_("Manage bookmarks..."));
        gtk_menu_shell_append(bar, GTK_WIDGET(bookmarks));
        gtk_menu_item_set_submenu(bookmarks, GTK_WIDGET(bmksub));
        gtk_menu_shell_append(bmksub, managebmk);
        g_signal_connect(G_OBJECT(managebmk), "activate",
                         G_CALLBACK(manage_bookmarks_selected), NULL);

        /* Help */
        help = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(_("_Help")));
        helpsub = GTK_MENU_SHELL(gtk_menu_new());
        about = gtk_menu_item_new_with_label(_("About..."));
        gtk_menu_shell_append(bar, GTK_WIDGET(help));
        gtk_menu_item_set_submenu(help, GTK_WIDGET(helpsub));
        gtk_menu_shell_append(helpsub, about);
        g_signal_connect(G_OBJECT(about), "activate",
                         G_CALLBACK(show_about_dialog), w);
#ifdef MAEMO
        gtk_menu_shell_append(bar, quit);
#endif

        /* Keyboard shortcuts */
        gtk_widget_add_accelerator(play, "activate", accel, GDK_space,
                                   0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(stop, "activate", accel, GDK_space,
                                   0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(skip, "activate", accel, GDK_Right,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
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
        gtk_widget_add_accelerator(quit, "activate", accel, GDK_q,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

        priv->play = play;
        priv->stop = stop;
        priv->actionsmenu = GTK_WIDGET(actionssub);
        priv->radiomenu = GTK_WIDGET(radiosub);
        priv->settings = settings;
        priv->dload = dload;
        priv->love = love;
        priv->addtopls = addtopls;
        priv->stopafter = stopafter;
        return GTK_WIDGET(bar);
}

static void
vgl_main_window_init(VglMainWindow *self)
{
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(self);
        GtkWindow *win = GTK_WINDOW(self);
        GtkBox *vbox, *centralbox;
        GtkBox *image_box_holder, *image_box_holder2, *labelbox;
        GtkBox *buttonshbox, *secondary_bbox, *secondary_bar_vbox;
        GtkWidget *menu;
        GtkWidget* image_box;
        GtkRcStyle* image_box_style;
        char *image_box_bg;
        GtkAccelGroup *accel = gtk_accel_group_new();
        priv->progressbar_text = g_string_sized_new(30);
        g_string_assign(priv->progressbar_text, " ");
        /* Window */
#ifndef MAEMO
        gtk_window_set_default_size(win, 500, -1);
#endif
        gtk_window_add_accel_group(win, accel);
        gtk_window_set_icon_from_file(win, APP_ICON, NULL);
        gtk_container_set_border_width(GTK_CONTAINER(win), 2);
        /* Boxes */
        vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
        centralbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        image_box_holder = GTK_BOX(gtk_hbox_new(FALSE, 0));
        image_box_holder2 = GTK_BOX(gtk_vbox_new(FALSE, 5));
        labelbox = GTK_BOX(gtk_vbox_new(TRUE, 5));
        buttonshbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        secondary_bbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        secondary_bar_vbox = GTK_BOX(gtk_vbox_new(FALSE, 2));
        /* Buttons */
        priv->playbutton = image_button_new(&play_button);
        priv->stopbutton = image_button_new(&stop_button);
        priv->skipbutton = image_button_new(&skip_button);
        priv->lovebutton = image_button_new(&love_button);
        priv->banbutton = image_button_new(&ban_button);
        priv->recommendbutton = image_button_new(&recommend_button);
        priv->dloadbutton = image_button_new(&dload_button);
        priv->tagbutton = image_button_new(&tag_button);
        priv->addplbutton = image_button_new(&addpl_button);
        /* Text labels */
        priv->playlist = gtk_label_new(NULL);
        priv->artist = gtk_label_new(NULL);
        priv->track = gtk_label_new(NULL);
        priv->album = gtk_label_new(NULL);
        gtk_label_set_ellipsize(GTK_LABEL(priv->playlist),PANGO_ELLIPSIZE_END);
        gtk_label_set_ellipsize(GTK_LABEL(priv->artist), PANGO_ELLIPSIZE_END);
        gtk_label_set_ellipsize(GTK_LABEL(priv->track), PANGO_ELLIPSIZE_END);
        gtk_label_set_ellipsize(GTK_LABEL(priv->album), PANGO_ELLIPSIZE_END);
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
                                    COVER_FRAME_SIZE, COVER_FRAME_SIZE);
        priv->album_cover = gtk_image_new();
        gtk_container_add(GTK_CONTAINER(image_box), priv->album_cover);
        /* Menu */
        menu = create_main_menu(self, accel);
        /* Progress bar */
        priv->progressbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
        gtk_progress_set_text_alignment(GTK_PROGRESS(priv->progressbar),
                                        0.5, 0.5);
        /* Layout */
        gtk_misc_set_alignment(GTK_MISC(priv->playlist), 0, 0.5);
        gtk_misc_set_alignment(GTK_MISC(priv->artist), 0, 0.5);
        gtk_misc_set_alignment(GTK_MISC(priv->track), 0, 0.5);
        gtk_misc_set_alignment(GTK_MISC(priv->album), 0, 0.5);
        gtk_misc_set_padding(GTK_MISC(priv->playlist), 10, 0);
        gtk_misc_set_padding(GTK_MISC(priv->artist), 10, 0);
        gtk_misc_set_padding(GTK_MISC(priv->track), 10, 0);
        gtk_misc_set_padding(GTK_MISC(priv->album), 10, 0);

        gtk_box_pack_start(buttonshbox, priv->playbutton, FALSE, FALSE, 0);
        gtk_box_pack_start(buttonshbox, priv->stopbutton, FALSE, FALSE, 0);
        gtk_box_pack_start(buttonshbox, priv->skipbutton, FALSE, FALSE, 0);

        gtk_box_pack_start(secondary_bbox, priv->lovebutton, TRUE, TRUE, 0);
        gtk_box_pack_start(secondary_bbox, priv->recommendbutton,
                           TRUE, TRUE, 0);
        gtk_box_pack_start(secondary_bbox, priv->tagbutton, TRUE, TRUE, 0);
        gtk_box_pack_start(secondary_bbox, priv->addplbutton, TRUE, TRUE, 0);
        gtk_box_pack_start(secondary_bbox, priv->dloadbutton, TRUE, TRUE, 0);
        gtk_box_pack_start(secondary_bbox, priv->banbutton, TRUE, TRUE, 0);

        gtk_box_pack_start(secondary_bar_vbox,
                           GTK_WIDGET(secondary_bbox), TRUE, TRUE, 0);
        gtk_box_pack_start(secondary_bar_vbox,
                           GTK_WIDGET(priv->progressbar), FALSE, FALSE, 0);
        gtk_box_pack_start(buttonshbox,
                           GTK_WIDGET(secondary_bar_vbox), TRUE, TRUE, 0);

        gtk_box_pack_start(labelbox, priv->artist, TRUE, TRUE, 0);
        gtk_box_pack_start(labelbox, priv->track, TRUE, TRUE, 0);
        gtk_box_pack_start(labelbox, priv->album, TRUE, TRUE, 0);

        gtk_box_pack_start(image_box_holder, image_box, FALSE, FALSE, 0);
        gtk_box_pack_start(image_box_holder2,
                           GTK_WIDGET(image_box_holder), TRUE, FALSE, 0);

        gtk_box_pack_start(centralbox,
                           GTK_WIDGET(image_box_holder2), FALSE, FALSE, 0);
        gtk_box_pack_start(centralbox, GTK_WIDGET(labelbox), TRUE, TRUE, 0);

        gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));

#ifdef MAEMO
        hildon_window_set_menu(HILDON_WINDOW(win), GTK_MENU(menu));
#else
        gtk_box_pack_start(vbox, menu, FALSE, FALSE, 0);
#endif
        gtk_box_pack_start(vbox, priv->playlist, FALSE, FALSE, 0);
        gtk_box_pack_start(vbox, gtk_hseparator_new(), FALSE, FALSE, 0);
        gtk_box_pack_start(vbox, GTK_WIDGET(centralbox), TRUE, TRUE, 0);
        gtk_box_pack_start(vbox, GTK_WIDGET(buttonshbox), TRUE, TRUE, 0);

        /* Signals */
        g_signal_connect(G_OBJECT(priv->playbutton), "clicked",
                         G_CALLBACK(play_selected), NULL);
        g_signal_connect(G_OBJECT(priv->skipbutton), "clicked",
                         G_CALLBACK(skip_selected), NULL);
        g_signal_connect(G_OBJECT(priv->stopbutton), "clicked",
                         G_CALLBACK(stop_selected), NULL);
        g_signal_connect(G_OBJECT(priv->lovebutton), "clicked",
                         G_CALLBACK(love_track_selected), NULL);
        g_signal_connect(G_OBJECT(priv->banbutton), "clicked",
                         G_CALLBACK(ban_track_selected), NULL);
        g_signal_connect(G_OBJECT(priv->recommendbutton), "clicked",
                         G_CALLBACK(recomm_track_selected), NULL);
        g_signal_connect(G_OBJECT(priv->dloadbutton), "clicked",
                         G_CALLBACK(download_track_selected), NULL);
        g_signal_connect(G_OBJECT(priv->tagbutton), "clicked",
                         G_CALLBACK(tag_track_selected), NULL);
        g_signal_connect(G_OBJECT(priv->addplbutton), "clicked",
                         G_CALLBACK(add_to_playlist_selected), NULL);
        g_signal_connect(G_OBJECT(win), "destroy",
                         G_CALLBACK(close_app), NULL);
        g_signal_connect(G_OBJECT(win), "delete-event",
                         G_CALLBACK(delete_event), NULL);
#ifdef MAEMO
        g_signal_connect(G_OBJECT(win), "notify::is-topmost",
                         G_CALLBACK(is_topmost_cb), self);
        g_signal_connect(G_OBJECT(win), "key_press_event",
                         G_CALLBACK(key_press_cb), self);
#endif
        g_signal_connect(G_OBJECT(win), "window_state_event",
                         G_CALLBACK(window_state_cb), self);
        /* Initial state */
        gtk_widget_show_all(GTK_WIDGET(vbox));
        vgl_main_window_set_state(self, VGL_MAIN_WINDOW_STATE_DISCONNECTED,
                                  NULL);
}

static void
vgl_main_window_finalize(GObject *object)
{
        VglMainWindow *win = VGL_MAIN_WINDOW(object);
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(win);
        g_debug("Destroying main window ...");
        g_string_free(priv->progressbar_text, TRUE);
        g_signal_handlers_destroy(object);
        G_OBJECT_CLASS(vgl_main_window_parent_class)->finalize(object);
}

static void
vgl_main_window_class_init(VglMainWindowClass *klass)
{
        GObjectClass *obj_class = (GObjectClass *)klass;
        obj_class->finalize = vgl_main_window_finalize;
        g_type_class_add_private(obj_class, sizeof(VglMainWindowPrivate));
}

GtkWidget *
vgl_main_window_new(void)
{
        GtkWidget *win;
        win = g_object_new(VGL_TYPE_MAIN_WINDOW, NULL);
        GTK_WINDOW(win)->type = GTK_WINDOW_TOPLEVEL;
        return win;
}