/*
 * vgl-main-window.c -- Main program window
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
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
#include "util.h"
#include "compat.h"

#if defined(SCR_RESOLUTION_800X480)
#        define ALBUM_COVER_SIZE 200
#        define COVER_FRAME_SIZE 230
#        define BIGBUTTON_IMG_SIZE 64
#        define SMALLBUTTON_IMG_SIZE 50
#elif defined(SCR_RESOLUTION_1024X600)
#        define ALBUM_COVER_SIZE 230
#        define COVER_FRAME_SIZE 250
#        define BIGBUTTON_IMG_SIZE 98
#        define SMALLBUTTON_IMG_SIZE 82
#else
#        define ALBUM_COVER_SIZE 110
#        define COVER_FRAME_SIZE 130
#        define BIGBUTTON_IMG_SIZE 48
#        define SMALLBUTTON_IMG_SIZE 24
#endif

#define BIGBUTTON_SIZE ((COVER_FRAME_SIZE - 5) / 2)

#ifdef USE_HILDON_WINDOW
#        define PARENT_CLASS_TYPE HILDON_TYPE_WINDOW
#else
#        define PARENT_CLASS_TYPE GTK_TYPE_WINDOW
#endif

#define VGL_MAIN_WINDOW_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), VGL_TYPE_MAIN_WINDOW, \
                                      VglMainWindowPrivate))
G_DEFINE_TYPE (VglMainWindow, vgl_main_window, PARENT_CLASS_TYPE);

struct _VglMainWindowPrivate {
        GtkWidget *play, *stop;
        GtkWidget *playbutton, *stopbutton, *skipbutton;
        GtkWidget *lovebutton, *banbutton, *recommendbutton;
        GtkWidget *dloadbutton, *tagbutton, *addplbutton;
        GtkWidget *playlist, *artist, *track, *album;
        GtkWidget *radiomenu, *actionsmenu, *settings, *love, *dload;
        GtkWidget *addtopls, *bmkmenu, *bmkartist, *bmktrack, *bmkradio;
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

static const char cover_background[] = APP_DATA_DIR "/cover.png";

void
vgl_main_window_run_app                 (void)
{
        gtk_main();
}

void
vgl_main_window_destroy                 (VglMainWindow *w)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        while (gtk_events_pending()) gtk_main_iteration();
        gtk_widget_destroy(GTK_WIDGET(w));
}

GtkWindow *
vgl_main_window_get_window              (VglMainWindow *w,
                                         gboolean       get_if_hidden)
{
        g_return_val_if_fail(VGL_IS_MAIN_WINDOW(w), NULL);
        if (!(w->priv->is_hidden) || get_if_hidden) {
                return GTK_WINDOW(w);
        } else {
                return NULL;
        }
}

void
vgl_main_window_show                    (VglMainWindow *w,
                                         gboolean       show)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        GtkWindow *win = GTK_WINDOW(w);
        VglMainWindowPrivate *priv = w->priv;
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
vgl_main_window_toggle_visibility       (VglMainWindow *w)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        vgl_main_window_show (w, !gtk_window_is_active (GTK_WINDOW (w)));
}

gboolean
vgl_main_window_is_hidden               (VglMainWindow *w)
{
        g_return_val_if_fail(VGL_IS_MAIN_WINDOW(w), TRUE);
        return w->priv->is_hidden;
}

void
vgl_main_window_set_album_cover         (VglMainWindow *w,
                                         const char    *data,
                                         int            size)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        GdkPixbuf *pixbuf = get_pixbuf_from_image(data, size, ALBUM_COVER_SIZE);
        gtk_image_set_from_pixbuf (GTK_IMAGE (w->priv->album_cover), pixbuf);
        gtk_widget_set_sensitive (w->priv->album_cover, TRUE);
        if (pixbuf != NULL) g_object_unref(pixbuf);
}

static void
vgl_main_window_update_track_info       (VglMainWindow     *w,
                                         const LastfmTrack *t)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w) && t != NULL);
        VglMainWindowPrivate *priv = w->priv;
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
set_progress_bar_text                   (VglMainWindow *w,
                                         const char    *text)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        if (w->priv->showing_msg) {
                /* If showing a status message, save text for later */
                g_string_assign (w->priv->progressbar_text, text);
        } else {
                gtk_progress_bar_set_text (w->priv->progressbar, text);
        }
}

static void
vgl_main_window_controller_progress_cb  (VglController *controller,
                                         guint          played,
                                         guint          length,
                                         VglMainWindow *w)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
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
        gtk_progress_bar_set_fraction (w->priv->progressbar, fraction);
        set_progress_bar_text(w, count);
}

typedef struct {
        VglMainWindow *win;
        guint msgid;
} remove_status_msg_data;

static gboolean
remove_status_msg                       (gpointer data)
{
        g_return_val_if_fail(data != NULL, FALSE);
        remove_status_msg_data *d = (remove_status_msg_data *) data;
        VglMainWindowPrivate *priv = d->win->priv;
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
vgl_main_window_show_status_msg         (VglMainWindow *w,
                                         const char    *text)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w) && text != NULL);
        remove_status_msg_data *d;
        VglMainWindowPrivate *priv = w->priv;
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
        gdk_threads_add_timeout (1500, remove_status_msg, d);
}

void
vgl_main_window_set_track_as_loved      (VglMainWindow *w)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        gtk_widget_set_sensitive (w->priv->lovebutton, FALSE);
        gtk_widget_set_sensitive (w->priv->love, FALSE);
}

void
vgl_main_window_set_track_as_added_to_playlist
                                        (VglMainWindow *w,
                                         gboolean       added)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        gtk_widget_set_sensitive (w->priv->addplbutton, !added);
        gtk_widget_set_sensitive (w->priv->addtopls, !added);
}

static void
menu_enable_all_items                   (GtkWidget *menu,
                                         gboolean   enable)
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
vgl_main_window_set_state               (VglMainWindow      *w,
                                         VglMainWindowState  state,
                                         const LastfmTrack  *t,
                                         const char         *radio_url)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        VglMainWindowPrivate *priv = w->priv;
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
                menu_enable_all_items (priv->bmkmenu, TRUE);
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
                gtk_widget_set_sensitive (priv->bmkartist, FALSE);
                gtk_widget_set_sensitive (priv->bmktrack, FALSE);
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
                menu_enable_all_items (priv->bmkmenu, TRUE);
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
                gtk_widget_set_sensitive (priv->bmkartist, t->artistid > 0);
                gtk_widget_set_sensitive (priv->bmktrack, t->id > 0);
                gtk_widget_set_sensitive (priv->dload,
                                          t->free_track_url != NULL);
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
                menu_enable_all_items (priv->bmkmenu, FALSE);
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
                break;
        default:
                g_critical("Unknown ui state received: %d", state);
                break;
        }
        if (state == VGL_MAIN_WINDOW_STATE_DISCONNECTED) {
                gtk_label_set_text(GTK_LABEL(priv->playlist), _("Disconnected"));
        }
        gtk_widget_set_sensitive (priv->bmkradio, radio_url != NULL);
        gtk_widget_set_sensitive (priv->artist, !dim_labels);
        gtk_widget_set_sensitive (priv->track, !dim_labels);
        gtk_widget_set_sensitive (priv->album, !dim_labels);
        gtk_progress_bar_set_fraction(priv->progressbar, 0);
}

static gboolean
window_state_cb                         (GtkWidget           *widget,
                                         GdkEventWindowState *event,
                                         VglMainWindow       *win)
{
        GdkWindowState st = event->new_window_state;
        VglMainWindowPrivate *priv = win->priv;
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
is_topmost_cb                           (GObject       *obj,
                                         GParamSpec    *arg,
                                         VglMainWindow *win)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(win));
        HildonWindow *hildonwin = HILDON_WINDOW(win);
        win->priv->is_hidden = !hildon_window_get_is_topmost(hildonwin);
        if (!(win->priv->is_hidden)) {
                controller_show_cover();
        }
}
#endif /* MAEMO */

#if defined(MAEMO) || defined(HAVE_GST_MIXER)
static gboolean
key_press_cb                            (GtkWidget     *widget,
                                         GdkEventKey   *event,
                                         VglMainWindow *win)
{
        int volchange = 0;
        switch (event->keyval) {
#ifdef MAEMO
        case GDK_F6:
                if (win->priv->is_fullscreen) {
                        gtk_window_unfullscreen(GTK_WINDOW(win));
                } else {
                        gtk_window_fullscreen(GTK_WINDOW(win));
                }
                break;
        case GDK_F7:
#else
        case GDK_KP_Add:
        case GDK_plus:
#endif
                volchange = 5;
                break;
#ifdef MAEMO
        case GDK_F8:
#else
        case GDK_KP_Subtract:
        case GDK_minus:
#endif
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
#endif /* defined(MAEMO) || defined(HAVE_GST_MIXER) */

static void
play_selected                           (GtkWidget *widget,
                                         gpointer   data)
{
        controller_start_playing();
}

static void
stop_selected                           (GtkWidget *widget,
                                         gpointer   data)
{
        controller_stop_playing();
}

static void
skip_selected                           (GtkWidget *widget,
                                         gpointer   data)
{
        controller_skip_track();
}

static void
close_app                               (GtkWidget *widget,
                                         gpointer   data)
{
        controller_quit_app();
}

static gboolean
delete_event                            (GtkWidget *widget,
                                         GdkEvent  *event,
                                         gpointer   data)
{
        controller_close_mainwin();
        return TRUE;
}

static void
radio_selected                          (GtkWidget *widget,
                                         gpointer   data)
{
        LastfmRadio type = GPOINTER_TO_INT (data);
        controller_play_radio(type);
}

static void
others_radio_selected                   (GtkWidget *widget,
                                         gpointer   data)
{
        LastfmRadio type = GPOINTER_TO_INT (data);
        controller_play_others_radio(type);
}

static void
group_radio_selected                    (GtkWidget *widget,
                                         gpointer   data)
{
        controller_play_group_radio();
}

static void
globaltag_radio_selected                (GtkWidget *widget,
                                         gpointer   data)
{
        controller_play_globaltag_radio();
}

static void
similarartist_radio_selected            (GtkWidget *widget,
                                         gpointer   data)
{
        controller_play_similarartist_radio();
}

static void
url_radio_selected                      (GtkWidget *widget,
                                         gpointer   data)
{
        controller_play_radio_ask_url();
}

static void
stop_after_selected                     (GtkWidget *widget,
                                         gpointer   data)
{
        controller_set_stop_after ();
}

static void
love_track_selected                     (GtkWidget *widget,
                                         gpointer   data)
{
        controller_love_track(TRUE);
}

static void
ban_track_selected                      (GtkWidget *widget,
                                         gpointer   data)
{
        controller_ban_track(TRUE);
}

static void
tag_track_selected                      (GtkWidget *widget,
                                         gpointer   data)
{
        controller_tag_track();
}

static void
recomm_track_selected                   (GtkWidget *widget,
                                         gpointer   data)
{
        controller_recomm_track();
}

static void
add_to_playlist_selected                (GtkWidget *widget,
                                         gpointer   data)
{
        controller_add_to_playlist();
}

static void
download_track_selected                 (GtkWidget *widget,
                                         gpointer   data)
{
        controller_download_track (FALSE);
}

static void
manage_bookmarks_selected               (GtkWidget *widget,
                                         gpointer   data)
{
        controller_manage_bookmarks();
}

static void
add_bookmark_selected                   (GtkWidget *widget,
                                         gpointer   data)
{
        BookmarkType type = GPOINTER_TO_INT(data);
        controller_add_bookmark(type);
}

static void
show_about_dialog                       (GtkWidget *widget,
                                         gpointer   data)
{
        controller_show_about ();
}

static void
open_user_settings                      (GtkWidget *widget,
                                         gpointer   data)
{
        controller_open_usercfg();
}

static void
apply_icon_theme                        (GtkIconTheme *icon_theme,
                                         GtkButton    *button)
{
        g_return_if_fail(button != NULL);
        const button_data *data;
        GdkPixbuf *pixbuf = NULL;
        GtkWidget *image;

        data = (const button_data *) g_object_get_data (G_OBJECT (button),
                                                        "button_data");

        g_return_if_fail(data && data->icon_name && data->icon_size > 0);

        pixbuf = gtk_icon_theme_load_icon(icon_theme, data->icon_name,
                                          data->icon_size, 0, NULL);

        if (pixbuf != NULL) {
                image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_button_set_image (button, image);
                g_object_unref (pixbuf);
        }
}

static GtkWidget *
image_button_new                        (const button_data *data)
{
        g_return_val_if_fail(data->icon_name && data->icon_size > 0, NULL);
        GtkWidget *button = compat_gtk_button_new();
        GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
        static GtkTooltips *tooltips = NULL;

        g_object_set_data(G_OBJECT(button), "button_data", (gpointer)data);

        apply_icon_theme (icon_theme, GTK_BUTTON (button));

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
create_main_menu                        (VglMainWindow *w,
                                         GtkAccelGroup *accel)
{
        VglMainWindowPrivate *priv = w->priv;
        GtkMenuItem *lastfm, *radio, *actions, *bookmarks, *help;
        GtkMenuItem *user, *others;
        GtkWidget *group, *globaltag, *similarartist, *urlradio;
        GtkMenuShell *lastfmsub, *radiosub, *actionssub, *bmksub, *helpsub;
        GtkMenuShell *usersub, *othersub;
        GtkWidget *settings, *quit;
        GtkWidget *play, *stop, *skip, *separ1, *separ2, *separ3;
        GtkWidget *stopafter, *love, *ban, *tag, *dorecomm, *addtopls, *dload;
        GtkWidget *library, *neigh, *loved, *playlist, *recomm, *usertag;
        GtkWidget *library2, *neigh2, *loved2, *playlist2, *recomm2, *usertag2;
        GtkWidget *managebmk, *addbmk, *bmkartist, *bmktrack, *bmkradio;
        GtkWidget *about;
#ifdef USE_HILDON_WINDOW
        GtkMenuShell *bar = GTK_MENU_SHELL(gtk_menu_new());
#else
        GtkMenuShell *bar = GTK_MENU_SHELL(gtk_menu_bar_new());
#endif

        /* Last.fm */
        lastfm = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(_("_Last.fm")));
        lastfmsub = GTK_MENU_SHELL(gtk_menu_new());
        settings = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES,
                                                       NULL);
        quit = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, accel);
        gtk_menu_shell_append(bar, GTK_WIDGET(lastfm));
        gtk_menu_item_set_submenu(lastfm, GTK_WIDGET(lastfmsub));
        gtk_menu_shell_append(lastfmsub, settings);
#ifndef USE_HILDON_WINDOW
        separ1 = gtk_separator_menu_item_new();
        gtk_menu_shell_append(lastfmsub, separ1);
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
        library = gtk_menu_item_new_with_label(_("My library"));
        neigh = gtk_menu_item_new_with_label(_("My neighbours"));
        loved = gtk_menu_item_new_with_label(_("My loved tracks"));
        playlist = gtk_menu_item_new_with_label(_("My playlist"));
        recomm = gtk_menu_item_new_with_label(_("My recommendations"));
        usertag = gtk_menu_item_new_with_label(_("My music tagged..."));
        gtk_menu_shell_append(usersub, library);
        gtk_menu_shell_append(usersub, neigh);
        gtk_menu_shell_append(usersub, loved);
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
        playlist2 = gtk_menu_item_new_with_label(_("Playlist..."));
        recomm2 = gtk_menu_item_new_with_label(_("Recommendations..."));
        usertag2 = gtk_menu_item_new_with_label(_("Music tagged..."));
        gtk_menu_shell_append(othersub, library2);
        gtk_menu_shell_append(othersub, neigh2);
        gtk_menu_shell_append(othersub, loved2);
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
        stopafter = gtk_menu_item_new_with_label (_("Stop after ..."));
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
        gtk_menu_shell_append(bar, GTK_WIDGET(actions));
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
        addbmk = ui_menu_item_create_from_icon ("gtk-new",
                                                _("Add bookmark..."));
        managebmk = ui_menu_item_create_from_icon ("gtk-edit",
                                                   _("Manage bookmarks..."));
        bmkartist = gtk_menu_item_new_with_label(_("Bookmark this artist..."));
        bmktrack = gtk_menu_item_new_with_label(_("Bookmark this track..."));
        bmkradio = gtk_menu_item_new_with_label(_("Bookmark this radio..."));
        separ1 = gtk_separator_menu_item_new();
        gtk_menu_shell_append(bar, GTK_WIDGET(bookmarks));
        gtk_menu_item_set_submenu(bookmarks, GTK_WIDGET(bmksub));
        gtk_menu_shell_append(bmksub, addbmk);
        gtk_menu_shell_append(bmksub, managebmk);
        gtk_menu_shell_append(bmksub, separ1);
        gtk_menu_shell_append(bmksub, bmkartist);
        gtk_menu_shell_append(bmksub, bmktrack);
        gtk_menu_shell_append(bmksub, bmkradio);
        g_signal_connect(G_OBJECT(managebmk), "activate",
                         G_CALLBACK(manage_bookmarks_selected), NULL);
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
        gtk_menu_shell_append(bar, GTK_WIDGET(help));
        gtk_menu_item_set_submenu(help, GTK_WIDGET(helpsub));
        gtk_menu_shell_append(helpsub, about);
        g_signal_connect(G_OBJECT(about), "activate",
                         G_CALLBACK(show_about_dialog), NULL);
#ifdef USE_HILDON_WINDOW
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
        gtk_widget_add_accelerator(addtopls, "activate", accel, GDK_a,
                                   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator(settings, "activate", accel, GDK_p,
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
        priv->bmkmenu = GTK_WIDGET (bmksub);
        priv->bmkartist = bmkartist;
        priv->bmktrack = bmktrack;
        priv->bmkradio = bmkradio;
        return GTK_WIDGET(bar);
}

static void
vgl_main_window_init                    (VglMainWindow *self)
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
        self->priv = priv;
        priv->progressbar_text = g_string_sized_new(30);
        g_string_assign(priv->progressbar_text, " ");
        /* Window */
#if defined(MAEMO5)
        gtk_container_set_border_width(GTK_CONTAINER(win), 10);
#elif defined(USE_HILDON_WINDOW)
        gtk_container_set_border_width(GTK_CONTAINER(win), 2);
#else
        gtk_window_set_default_size(win, 500, -1);
#endif
        gtk_window_add_accel_group(win, accel);
        gtk_window_set_icon_from_file(win, APP_ICON, NULL);
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
#ifdef MAEMO5
        gtk_widget_set_name ((GtkWidget *) priv->progressbar,
                             "small-progress-bar");
#endif
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

#ifdef USE_HILDON_WINDOW
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
                         G_CALLBACK(gtk_main_quit), NULL);
        g_signal_connect(G_OBJECT(win), "delete-event",
                         G_CALLBACK(delete_event), NULL);
#ifdef MAEMO
        g_signal_connect(G_OBJECT(win), "notify::is-topmost",
                         G_CALLBACK(is_topmost_cb), self);
#endif
#if defined(MAEMO) || defined(HAVE_GST_MIXER)
        g_signal_connect(G_OBJECT(win), "key_press_event",
                         G_CALLBACK(key_press_cb), self);
#endif
        g_signal_connect(G_OBJECT(win), "window_state_event",
                         G_CALLBACK(window_state_cb), self);
        /* Initial state */
        gtk_widget_show_all(GTK_WIDGET(vbox));
        vgl_main_window_set_state(self, VGL_MAIN_WINDOW_STATE_DISCONNECTED,
                                  NULL, NULL);
}

static void
vgl_main_window_finalize                (GObject *object)
{
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW (object)->priv;
        g_debug("Destroying main window ...");
        g_string_free(priv->progressbar_text, TRUE);
        g_signal_handlers_destroy(object);
        G_OBJECT_CLASS(vgl_main_window_parent_class)->finalize(object);
}

static void
vgl_main_window_class_init              (VglMainWindowClass *klass)
{
        GObjectClass *obj_class = (GObjectClass *)klass;
        obj_class->finalize = vgl_main_window_finalize;
        g_type_class_add_private(obj_class, sizeof(VglMainWindowPrivate));
}

GtkWidget *
vgl_main_window_new                     (VglController *controller)
{
        GtkWidget *win;
        g_return_val_if_fail (VGL_IS_CONTROLLER (controller), NULL);
        win = g_object_new (VGL_TYPE_MAIN_WINDOW,
                            "type", GTK_WINDOW_TOPLEVEL, NULL);
        g_signal_connect (controller, "playback-progress",
                          G_CALLBACK (vgl_main_window_controller_progress_cb),
                          win);
        return win;
}
