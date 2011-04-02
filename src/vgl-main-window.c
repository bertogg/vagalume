/*
 * vgl-main-window.c -- Main program window
 *
 * Copyright (C) 2007-2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>

#include "controller.h"
#include "vgl-main-menu.h"
#include "vgl-main-window.h"
#include "radio.h"
#include "uimisc.h"
#include "util.h"
#include "compat.h"

#if defined(SCR_RESOLUTION_800X480)
#        define COVER_FRAME_HEIGHT 215
#        define COVER_FRAME_WIDTH 230
#        define BIGBUTTON_IMG_SIZE 64
#        define SMALLBUTTON_IMG_SIZE 50
#elif defined(SCR_RESOLUTION_1024X600)
#        define COVER_FRAME_HEIGHT 233
#        define COVER_FRAME_WIDTH 250
#        define BIGBUTTON_IMG_SIZE 98
#        define SMALLBUTTON_IMG_SIZE 82
#else
#        define COVER_FRAME_HEIGHT 121
#        define COVER_FRAME_WIDTH 130
#        define BIGBUTTON_IMG_SIZE 48
#        define SMALLBUTTON_IMG_SIZE 24
#endif

#define ALBUM_COVER_PADDING 2
#define ALBUM_COVER_SIZE (COVER_FRAME_HEIGHT - 2 * ALBUM_COVER_PADDING)
#define BIGBUTTON_SIZE ((COVER_FRAME_WIDTH - 5) / 2)

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
        GtkWidget *playbutton, *stopbutton, *skipbutton;
        GtkWidget *lovebutton, *banbutton, *recommendbutton;
        GtkWidget *dloadbutton, *tagbutton, *addplbutton;
        GtkWidget *playlist, *artist, *track, *album;
        GtkWidget *album_cover;
        VglMainMenu *menu;
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

static const char cover_bg[] = APP_DATA_DIR "/cover.png";

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

        gtk_widget_set_tooltip_text (priv->artist, t->artist);
        gtk_widget_set_tooltip_text (priv->track, t->title);
        gtk_widget_set_tooltip_text (priv->album, t->album);
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
        vgl_main_menu_set_track_as_loved (w->priv->menu);
}

void
vgl_main_window_set_track_as_added_to_playlist
                                        (VglMainWindow *w,
                                         gboolean       added)
{
        g_return_if_fail(VGL_IS_MAIN_WINDOW(w));
        gtk_widget_set_sensitive (w->priv->addplbutton, !added);
        vgl_main_menu_set_track_as_added_to_playlist (w->priv->menu, added);
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
        vgl_main_menu_set_state (priv->menu, state, t, radio_url);
        switch (state) {
        case VGL_MAIN_WINDOW_STATE_DISCONNECTED:
        case VGL_MAIN_WINDOW_STATE_STOPPED:
                dim_labels = FALSE;
                set_progress_bar_text(w, _("Stopped"));
                gtk_label_set_text(GTK_LABEL(priv->playlist), _("Stopped"));
                gtk_label_set_text(GTK_LABEL(priv->artist), NULL);
                gtk_label_set_text(GTK_LABEL(priv->track), NULL);
                gtk_label_set_text(GTK_LABEL(priv->album), NULL);
                gtk_widget_show (priv->playbutton);
                gtk_widget_hide (priv->stopbutton);
                gtk_widget_set_sensitive (priv->playbutton, TRUE);
                gtk_widget_set_sensitive (priv->skipbutton, FALSE);
                gtk_widget_set_sensitive (priv->lovebutton, FALSE);
                gtk_widget_set_sensitive (priv->banbutton, FALSE);
                gtk_widget_set_sensitive (priv->recommendbutton, FALSE);
                gtk_widget_set_sensitive (priv->dloadbutton, FALSE);
                gtk_widget_set_sensitive (priv->tagbutton, FALSE);
                gtk_widget_set_sensitive (priv->addplbutton, FALSE);
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
                gtk_widget_hide (priv->playbutton);
                gtk_widget_show (priv->stopbutton);
                gtk_widget_set_sensitive (priv->stopbutton, TRUE);
                gtk_widget_set_sensitive (priv->skipbutton, TRUE);
                gtk_widget_set_sensitive (priv->lovebutton, TRUE);
                gtk_widget_set_sensitive (priv->banbutton, TRUE);
                gtk_widget_set_sensitive (priv->recommendbutton, TRUE);
                gtk_widget_set_sensitive (priv->dloadbutton,
                                          t->free_track_url != NULL);
                gtk_widget_set_sensitive (priv->tagbutton, TRUE);
                gtk_widget_set_sensitive (priv->addplbutton, TRUE);
                break;
        case VGL_MAIN_WINDOW_STATE_CONNECTING:
                dim_labels = TRUE;
                set_progress_bar_text(w, _("Connecting..."));
                gtk_label_set_text(GTK_LABEL(priv->playlist), _("Connecting..."));
                gtk_widget_hide (priv->playbutton);
                gtk_widget_show (priv->stopbutton);
                gtk_widget_set_sensitive (priv->stopbutton, FALSE);
                gtk_widget_set_sensitive (priv->skipbutton, FALSE);
                gtk_widget_set_sensitive (priv->lovebutton, FALSE);
                gtk_widget_set_sensitive (priv->recommendbutton, FALSE);
                gtk_widget_set_sensitive (priv->banbutton, FALSE);
                gtk_widget_set_sensitive (priv->dloadbutton, FALSE);
                gtk_widget_set_sensitive (priv->tagbutton, FALSE);
                gtk_widget_set_sensitive (priv->addplbutton, FALSE);
                gtk_window_set_title(GTK_WINDOW(w), APP_NAME);
                vgl_main_window_set_album_cover (w, NULL, 0);
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

#ifdef ENABLE_VOLUME_KEY_HANDLER
static gboolean
key_press_cb                            (GtkWidget     *widget,
                                         GdkEventKey   *event,
                                         VglMainWindow *win)
{
        int volchange = 0;
        switch (event->keyval) {
#ifdef MAEMO
        case GDK_KEY_F6:
                if (win->priv->is_fullscreen) {
                        gtk_window_unfullscreen(GTK_WINDOW(win));
                } else {
                        gtk_window_fullscreen(GTK_WINDOW(win));
                }
                break;
        case GDK_KEY_F7:
#else
        case GDK_KEY_KP_Add:
        case GDK_KEY_plus:
#endif
                volchange = 5;
                break;
#ifdef MAEMO
        case GDK_KEY_F8:
#else
        case GDK_KEY_KP_Subtract:
        case GDK_KEY_minus:
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
#endif /* ENABLE_VOLUME_KEY_HANDLER */

static gboolean
delete_event                            (GtkWidget *widget,
                                         GdkEvent  *event,
                                         gpointer   data)
{
        controller_close_mainwin();
        return TRUE;
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
download_track_selected                 (GtkWidget *widget,
                                         gpointer   data)
{
        controller_download_track (FALSE);
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

        g_object_set_data(G_OBJECT(button), "button_data", (gpointer)data);

        apply_icon_theme (icon_theme, GTK_BUTTON (button));

        g_signal_connect(G_OBJECT(icon_theme), "changed",
                         G_CALLBACK(apply_icon_theme), button);

        gtk_widget_set_size_request(button, data->button_width,
                                    data->button_height);

        if (data->tooltip != NULL) {
                gtk_widget_set_tooltip_text (button, _(data->tooltip));
        }

        return button;
}

#ifdef MAEMO5
static void
vgl_main_window_set_hildon22_accels     (VglMainWindow *win,
                                         GtkAccelGroup *accel)
{
        VglMainWindowPrivate *p = win->priv;
        gtk_widget_add_accelerator (p->playbutton, "activate", accel,
                                    GDK_KEY_space, 0, 0);
        gtk_widget_add_accelerator (p->stopbutton, "activate", accel,
                                    GDK_KEY_space, 0, 0);
        gtk_widget_add_accelerator (p->skipbutton, "activate", accel,
                                    GDK_KEY_Right, GDK_CONTROL_MASK, 0);
        gtk_widget_add_accelerator (p->banbutton, "activate", accel,
                                    GDK_KEY_b, GDK_CONTROL_MASK, 0);
        gtk_widget_add_accelerator (p->lovebutton, "activate", accel,
                                    GDK_KEY_l, GDK_CONTROL_MASK, 0);
        gtk_widget_add_accelerator (p->recommendbutton, "activate", accel,
                                    GDK_KEY_r, GDK_CONTROL_MASK, 0);
        gtk_widget_add_accelerator (p->tagbutton, "activate", accel,
                                    GDK_KEY_t, GDK_CONTROL_MASK, 0);
        gtk_widget_add_accelerator (p->addplbutton, "activate", accel,
                                    GDK_KEY_a, GDK_CONTROL_MASK, 0);
}
#endif /* MAEMO5 */

static void
vgl_main_window_init                    (VglMainWindow *self)
{
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW_GET_PRIVATE(self);
        GtkWindow *win = GTK_WINDOW(self);
        GtkBox *mainvbox, *vbox, *centralbox;
        GtkBox *image_box_holder, *image_box_holder2, *labelbox;
        GtkBox *buttonshbox, *secondary_bbox, *secondary_bar_vbox;
        GtkWidget* image_box;
        GtkAccelGroup *accel = gtk_accel_group_new();
        self->priv = priv;
        priv->progressbar_text = g_string_sized_new(30);
        g_string_assign(priv->progressbar_text, " ");
        /* Window */
#ifndef USE_HILDON_WINDOW
        gtk_window_set_default_size(win, 500, -1);
#endif
        gtk_window_add_accel_group(win, accel);
        gtk_window_set_icon_from_file(win, APP_ICON, NULL);
        /* Boxes */
        mainvbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
        vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
        centralbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        image_box_holder = GTK_BOX(gtk_hbox_new(FALSE, 0));
        image_box_holder2 = GTK_BOX(gtk_vbox_new(FALSE, 5));
        labelbox = GTK_BOX(gtk_vbox_new(TRUE, 5));
        buttonshbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        secondary_bbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        secondary_bar_vbox = GTK_BOX(gtk_vbox_new(FALSE, 2));
#if defined(MAEMO5)
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
#else
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
#endif
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
        {
#ifdef GTK_TYPE_CSS_PROVIDER
        guint priority = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;
        GtkStyleContext *ctx = gtk_widget_get_style_context (image_box);
        GtkCssProvider *provider = gtk_css_provider_new ();
        char *css = g_strdup_printf ("* { background-image: url(\"%s\") }",
                                     cover_bg);
        gtk_css_provider_load_from_data (provider, css, -1, NULL);
        gtk_style_context_add_provider (ctx, GTK_STYLE_PROVIDER (provider),
                                        priority);
        g_object_unref (provider);
        g_free (css);
#else
        GtkRcStyle *style = gtk_rc_style_new ();
        style->bg_pixmap_name[GTK_STATE_NORMAL]      = g_strdup (cover_bg);
        style->bg_pixmap_name[GTK_STATE_ACTIVE]      = g_strdup (cover_bg);
        style->bg_pixmap_name[GTK_STATE_PRELIGHT]    = g_strdup (cover_bg);
        style->bg_pixmap_name[GTK_STATE_SELECTED]    = g_strdup (cover_bg);
        style->bg_pixmap_name[GTK_STATE_INSENSITIVE] = g_strdup (cover_bg);
        gtk_widget_modify_style (image_box, style);
        g_object_unref (style);
#endif /* GTK_TYPE_CSS_PROVIDER */
        }
        gtk_widget_set_size_request(GTK_WIDGET(image_box),
                                    COVER_FRAME_WIDTH, COVER_FRAME_HEIGHT);
        priv->album_cover = gtk_image_new();
        gtk_container_add(GTK_CONTAINER(image_box), priv->album_cover);
        gtk_misc_set_alignment (GTK_MISC (priv->album_cover), 1.0, 0.5);
        gtk_misc_set_padding (GTK_MISC (priv->album_cover),
                              ALBUM_COVER_PADDING, ALBUM_COVER_PADDING);
        /* Menu */
        priv->menu = vgl_main_menu_new (accel);

        /* Progress bar */
        priv->progressbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
        gtk_progress_set_text_alignment(GTK_PROGRESS(priv->progressbar),
                                        0.5, 0.5);
        gtk_progress_bar_set_show_text (priv->progressbar, TRUE);
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

        gtk_box_pack_end (mainvbox, GTK_WIDGET (vbox), TRUE, TRUE, 0);
        gtk_container_add (GTK_CONTAINER (win), GTK_WIDGET (mainvbox));

#ifdef MAEMO5
        hildon_window_set_app_menu (HILDON_WINDOW (win),
                                    HILDON_APP_MENU (priv->menu));
        vgl_main_window_set_hildon22_accels (self, accel);
#elif defined(USE_HILDON_WINDOW)
        hildon_window_set_menu (HILDON_WINDOW (win), GTK_MENU (priv->menu));
#else
        gtk_box_pack_end (mainvbox, GTK_WIDGET (priv->menu), FALSE, FALSE, 0);
#endif
        gtk_box_pack_start(vbox, priv->playlist, FALSE, FALSE, 0);
        gtk_box_pack_start(vbox, gtk_hseparator_new(), FALSE, FALSE, 0);
        gtk_box_pack_start(vbox, GTK_WIDGET(centralbox), TRUE, TRUE, 0);
        gtk_box_pack_start(vbox, GTK_WIDGET(buttonshbox), TRUE, TRUE, 0);

        /* Signals */
        g_signal_connect(G_OBJECT(priv->playbutton), "clicked",
                         G_CALLBACK(controller_start_playing), NULL);
        g_signal_connect(G_OBJECT(priv->skipbutton), "clicked",
                         G_CALLBACK(controller_skip_track), NULL);
        g_signal_connect(G_OBJECT(priv->stopbutton), "clicked",
                         G_CALLBACK(controller_stop_playing), NULL);
        g_signal_connect(G_OBJECT(priv->lovebutton), "clicked",
                         G_CALLBACK(love_track_selected), NULL);
        g_signal_connect(G_OBJECT(priv->banbutton), "clicked",
                         G_CALLBACK(ban_track_selected), NULL);
        g_signal_connect(G_OBJECT(priv->recommendbutton), "clicked",
                         G_CALLBACK(controller_recomm_track), NULL);
        g_signal_connect(G_OBJECT(priv->dloadbutton), "clicked",
                         G_CALLBACK(download_track_selected), NULL);
        g_signal_connect(G_OBJECT(priv->tagbutton), "clicked",
                         G_CALLBACK(controller_tag_track), NULL);
        g_signal_connect(G_OBJECT(priv->addplbutton), "clicked",
                         G_CALLBACK(controller_add_to_playlist), NULL);
        g_signal_connect(G_OBJECT(win), "destroy",
                         G_CALLBACK(gtk_main_quit), NULL);
        g_signal_connect(G_OBJECT(win), "delete-event",
                         G_CALLBACK(delete_event), NULL);
#ifdef MAEMO
        g_signal_connect(G_OBJECT(win), "notify::is-topmost",
                         G_CALLBACK(is_topmost_cb), self);
#endif
#ifdef ENABLE_VOLUME_KEY_HANDLER
        g_signal_connect(G_OBJECT(win), "key_press_event",
                         G_CALLBACK(key_press_cb), self);
#endif
        g_signal_connect(G_OBJECT(win), "window_state_event",
                         G_CALLBACK(window_state_cb), self);
        /* Initial state */
        gtk_widget_show_all (GTK_WIDGET (mainvbox));
        vgl_main_window_set_state(self, VGL_MAIN_WINDOW_STATE_DISCONNECTED,
                                  NULL, NULL);
}

static void
vgl_main_window_finalize                (GObject *object)
{
        VglMainWindowPrivate *priv = VGL_MAIN_WINDOW (object)->priv;
        g_debug("Destroying main window ...");
        g_string_free(priv->progressbar_text, TRUE);
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
