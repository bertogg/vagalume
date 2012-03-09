/*
 * vgl-tray-icon.c -- Freedesktop tray icon
 *
 * Copyright (C) 2007-2009, 2011 Igalia, S.L.
 * Authors: Mario Sanchez Prada <msanchez@igalia.com>
 *          Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include <libnotify/notify.h>

#include "vgl-tray-icon.h"
#include "globaldefs.h"

#include "util.h"
#include "uimisc.h"
#include "scrobbler.h"
#include "compat.h"

#define TOOLTIP_DEFAULT_STRING _(" Stopped ")
#define TOOLTIP_FORMAT_STRING  _(" Now playing: \n %s \n   by  %s ")

#define NOTIFICATION_BODY_NO_ALBUM _(" by <i>%s</i>")
#define NOTIFICATION_BODY_WITH_ALBUM _(" by <i>%s</i>\n from <i>%s</i>")

#define NOTIFICATION_ICON_SIZE 48

#define VGL_TRAY_ICON_GET_PRIVATE(object)      \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), \
                                      VGL_TRAY_ICON_TYPE, VglTrayIconPrivate))

G_DEFINE_TYPE (VglTrayIcon, vgl_tray_icon, G_TYPE_OBJECT);

enum {
        SETTINGS_ITEM,
        ABOUT_ITEM,
        RECOMMEND_ITEM,
        TAG_ITEM,
        ADD_TO_PLS_ITEM,
        LOVE_ITEM,
        BAN_ITEM,
        PLAY_ITEM,
        STOP_ITEM,
        NEXT_ITEM,
        QUIT_ITEM,
        N_MENU_ITEMS
};

/* Private struct */
struct _VglTrayIconPrivate
{
        GtkStatusIcon *tray_icon;
        gboolean now_playing;
        GtkWidget *ctxt_menu;
        GtkWidget *menu_item[N_MENU_ITEMS];
        gboolean show_notifications;
        NotifyNotification *notification;
};

/* Private */

/* Initialization/destruction functions */
static void vgl_tray_icon_finalize (GObject* object);
static void vgl_tray_icon_class_init(VglTrayIconClass *klass);
static void vgl_tray_icon_init (VglTrayIcon *vgl_tray_icon);

/* Libnotify functions */
static void setup_libnotify  (VglTrayIcon *vti);
static void cleanup_libnotify  (VglTrayIcon *vti);

/* Panel update functions */
static void ctxt_menu_create (VglTrayIcon *vti);
static void ctxt_menu_update (VglTrayIcon *vti);

/* Signals handlers */
static void tray_icon_clicked (GtkStatusIcon *status_icon,
                               gpointer user_data);

static void tray_icon_popup_menu (GtkStatusIcon *status_icon, guint button,
                                  guint activate_time, gpointer data);

static gboolean tray_icon_scroll_event (GtkWidget      *widget,
                                        GdkEventScroll *event,
                                        gpointer        data);

static void ctxt_menu_item_activated (GtkWidget *item, gpointer data);

static void vgl_tray_icon_notify_playback (VglTrayIcon *vti,
                                           LastfmTrack *track);

/* Private */

/* Initialization/destruction functions */

static void
vgl_tray_icon_class_init                (VglTrayIconClass *klass)
{
        GObjectClass *obj_class = G_OBJECT_CLASS(klass);

        obj_class -> finalize = vgl_tray_icon_finalize;
        g_type_class_add_private (obj_class, sizeof (VglTrayIconPrivate));
}

static void
vgl_tray_icon_init                      (VglTrayIcon *vti)
{
        int i;
        VglTrayIconPrivate *priv = VGL_TRAY_ICON_GET_PRIVATE (vti);

        /* Init private attributes */
        vti->priv = priv;
        priv->tray_icon = gtk_status_icon_new ();

        priv->now_playing = FALSE;

        for (i = 0; i < N_MENU_ITEMS; i++) {
                priv->menu_item[i] = NULL;
        }

        priv->notification = NULL;
        priv->show_notifications = FALSE;

        /* Create main panel */
        ctxt_menu_create (vti);

        /* Connect signals */
        g_signal_connect (priv->tray_icon, "activate",
                          G_CALLBACK (tray_icon_clicked), vti);
        g_signal_connect (priv->tray_icon, "popup-menu",
                          G_CALLBACK (tray_icon_popup_menu), vti);
#if GTK_CHECK_VERSION(2,16,0)
        g_signal_connect (priv->tray_icon, "scroll-event",
                          G_CALLBACK (tray_icon_scroll_event), vti);
#endif /* GTK_CHECK_VERSION(2,16,0) */

        /* Set icon and tooltip */
        gtk_status_icon_set_from_file (priv->tray_icon, APP_ICON);
        gtk_status_icon_set_tooltip_text (priv->tray_icon,
                                          TOOLTIP_DEFAULT_STRING);
        gtk_status_icon_set_visible(priv->tray_icon, TRUE);

        /* Init libnotify */
        setup_libnotify (vti);

        /* Update contextual menu */
        ctxt_menu_update (vti);
}

static void
vgl_tray_icon_finalize                  (GObject* object)
{
        VglTrayIcon *vti = VGL_TRAY_ICON (object);
        VglTrayIconPrivate *priv = vti->priv;

        /* Cleanup libnotify */
        cleanup_libnotify (vti);

        /* Destroy local widgets */
        if (priv->ctxt_menu) {
                gtk_widget_destroy (priv->ctxt_menu);
        }

        if (priv->tray_icon) {
                g_object_unref (priv->tray_icon);
        }

        /* call super class */
        G_OBJECT_CLASS (vgl_tray_icon_parent_class) -> finalize(object);
}

/* Libnotify functions */
static void
setup_libnotify                         (VglTrayIcon *vti)
{
        if (!notify_init (APP_NAME)) {
                g_debug ("[TRAY ICON] :: Error initializing libnotify");
        } else {
                g_debug ("[TRAY ICON] :: Success initializing libnotify");
        }
}

static void
cleanup_libnotify                       (VglTrayIcon *vti)
{
        VglTrayIconPrivate *priv = vti->priv;

        if (notify_is_initted ()) {
                if (priv->notification) {
                        notify_notification_close (priv->notification, NULL);
                        g_object_unref (priv->notification);
                }
                notify_uninit ();
        }
}

/* Panel update functions */

static void
ctxt_menu_create                        (VglTrayIcon *vti)
{
        VglTrayIconPrivate *priv = vti->priv;
        GtkMenuShell *ctxt_menu;

        /* Create ctxt_menu and ctxt_menu items */
        priv->ctxt_menu = gtk_menu_new ();
        ctxt_menu = GTK_MENU_SHELL (priv->ctxt_menu);

        priv->menu_item[SETTINGS_ITEM] =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES,
                                                    NULL);
        priv->menu_item[ABOUT_ITEM] =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
        priv->menu_item[RECOMMEND_ITEM] =
                ui_menu_item_create_from_icon (RECOMMEND_ITEM_ICON_NAME,
                                               RECOMMEND_ITEM_STRING);
        priv->menu_item[TAG_ITEM] =
                ui_menu_item_create_from_icon (TAG_ITEM_ICON_NAME,
                                               TAG_ITEM_STRING);
        priv->menu_item[ADD_TO_PLS_ITEM] =
                ui_menu_item_create_from_icon (ADD_TO_PLS_ITEM_ICON_NAME,
                                               ADD_TO_PLS_ITEM_STRING);
        priv->menu_item[LOVE_ITEM] =
                ui_menu_item_create_from_icon (LOVE_ITEM_ICON_NAME,
                                               LOVE_ITEM_STRING);
        priv->menu_item[BAN_ITEM] =
                ui_menu_item_create_from_icon (BAN_ITEM_ICON_NAME,
                                               BAN_ITEM_STRING);
        priv->menu_item[PLAY_ITEM] =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_PLAY,NULL);
        priv->menu_item[STOP_ITEM] =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_STOP,NULL);
        priv->menu_item[NEXT_ITEM] =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_NEXT,NULL);
        priv->menu_item[QUIT_ITEM] =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);

        /* Add items to ctxt_menu */
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[RECOMMEND_ITEM]);
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[TAG_ITEM]);
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[ADD_TO_PLS_ITEM]);
        gtk_menu_shell_append (ctxt_menu, gtk_separator_menu_item_new ());
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[LOVE_ITEM]);
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[BAN_ITEM]);
        gtk_menu_shell_append (ctxt_menu, gtk_separator_menu_item_new ());
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[PLAY_ITEM]);
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[STOP_ITEM]);
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[NEXT_ITEM]);
        gtk_menu_shell_append (ctxt_menu, gtk_separator_menu_item_new ());
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[SETTINGS_ITEM]);
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[ABOUT_ITEM]);
        gtk_menu_shell_append (ctxt_menu, gtk_separator_menu_item_new ());
        gtk_menu_shell_append (ctxt_menu, priv->menu_item[QUIT_ITEM]);

        /* Connect signals */
        g_signal_connect (priv->menu_item[SETTINGS_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[ABOUT_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[RECOMMEND_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[TAG_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[ADD_TO_PLS_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[LOVE_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[BAN_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[PLAY_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[STOP_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[NEXT_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);
        g_signal_connect (priv->menu_item[QUIT_ITEM], "activate",
                          G_CALLBACK (ctxt_menu_item_activated), vti);

        /* Show widgets */
        gtk_widget_show_all (priv->ctxt_menu);
}

static void
ctxt_menu_update                        (VglTrayIcon *vti)
{
        VglTrayIconPrivate *priv = vti->priv;
        gboolean np = priv->now_playing;

        /* Adjust sentitiveness for ctxt_menu items and show/hide buttons */
        gtk_widget_set_sensitive (priv->menu_item[RECOMMEND_ITEM], np);
        gtk_widget_set_sensitive (priv->menu_item[TAG_ITEM], np);
        gtk_widget_set_sensitive (priv->menu_item[ADD_TO_PLS_ITEM], np);
        gtk_widget_set_sensitive (priv->menu_item[LOVE_ITEM], np);
        gtk_widget_set_sensitive (priv->menu_item[BAN_ITEM], np);
        gtk_widget_set_sensitive (priv->menu_item[PLAY_ITEM], !np);
        gtk_widget_set_sensitive (priv->menu_item[STOP_ITEM], np);
        gtk_widget_set_sensitive (priv->menu_item[NEXT_ITEM], np);

        if (np) {
                gtk_widget_hide (priv->menu_item[PLAY_ITEM]);
                gtk_widget_show (priv->menu_item[STOP_ITEM]);
        } else {
                gtk_widget_show (priv->menu_item[PLAY_ITEM]);
                gtk_widget_hide (priv->menu_item[STOP_ITEM]);
        }
}


/* Signals handlers */

static void
tray_icon_clicked                       (GtkStatusIcon *status_icon,
                                         gpointer       data)
{
        controller_toggle_mainwin_visibility ();
}

static void
tray_icon_popup_menu                    (GtkStatusIcon *status_icon,
                                         guint          button,
                                         guint          activate_time,
                                         gpointer       data)
{
        VglTrayIcon *vti = VGL_TRAY_ICON (data);
        VglTrayIconPrivate *priv = vti->priv;

        gtk_menu_popup (GTK_MENU (priv->ctxt_menu),
                        NULL, NULL,
                        gtk_status_icon_position_menu,
                        priv->tray_icon,
                        button,
                        activate_time);
}

#if GTK_CHECK_VERSION(2,16,0)
static gboolean
tray_icon_scroll_event                  (GtkWidget      *widget,
                                         GdkEventScroll *event,
                                         gpointer        data)
{
        if (event->direction == GDK_SCROLL_UP) {
                controller_increase_volume (1);
        } else if (event->direction == GDK_SCROLL_DOWN) {
                controller_increase_volume (-1);
        }
        return FALSE;
}
#endif /* GTK_CHECK_VERSION(2,16,0) */

static void
ctxt_menu_item_activated                (GtkWidget *item,
                                         gpointer   data)
{
        VglTrayIcon *vti = VGL_TRAY_ICON (data);
        VglTrayIconPrivate *priv = vti->priv;

        if (item == priv->menu_item[SETTINGS_ITEM]) {
                controller_open_usercfg();
        } else if (item == priv->menu_item[ABOUT_ITEM]) {
                controller_show_about();
        } else if (item == priv->menu_item[RECOMMEND_ITEM]) {
                controller_recomm_track();
        } else if (item == priv->menu_item[TAG_ITEM]) {
                controller_tag_track();
        } else if (item == priv->menu_item[ADD_TO_PLS_ITEM]) {
                controller_add_to_playlist();
        } else if (item == priv->menu_item[LOVE_ITEM]) {
                controller_love_track (TRUE);
        } else if (item == priv->menu_item[BAN_ITEM]) {
                controller_ban_track (TRUE);
        } else if (item == priv->menu_item[PLAY_ITEM]) {
                controller_start_playing ();
        } else if (item == priv->menu_item[STOP_ITEM]) {
                controller_stop_playing ();
        } else if (item == priv->menu_item[NEXT_ITEM]) {
                controller_skip_track ();
        } else if (item == priv->menu_item[QUIT_ITEM]) {
                controller_quit_app ();
        } else {
                g_warning ("[TRAY ICON] :: Unknown action");
        }
}

static void
usercfg_changed_cb                      (VglController *ctrl,
                                         VglUserCfg    *cfg,
                                         VglTrayIcon   *icon)
{
        icon->priv->show_notifications = cfg->show_notifications;
}

static void
track_stopped_cb                        (VglController *ctrl,
                                         RspRating      rating,
                                         VglTrayIcon   *icon)
{
        vgl_tray_icon_notify_playback (icon, NULL);
}

static void
track_started_cb                        (VglController *ctrl,
                                         LastfmTrack   *track,
                                         VglTrayIcon   *icon)
{
        vgl_tray_icon_notify_playback (icon, track);
}

static void
controller_destroyed_cb                 (gpointer  data,
                                         GObject  *controller)
{
        VglTrayIcon *icon = VGL_TRAY_ICON (data);
        g_object_unref (icon);
}

/* Public */

VglTrayIcon *
vgl_tray_icon_create                    (VglController *controller)
{
        VglTrayIcon *icon;
        g_return_val_if_fail (VGL_IS_CONTROLLER (controller), NULL);
        icon = g_object_new (VGL_TRAY_ICON_TYPE, NULL);
        g_signal_connect (controller, "usercfg-changed",
                          G_CALLBACK (usercfg_changed_cb), icon);
        g_signal_connect (controller, "track-stopped",
                          G_CALLBACK (track_stopped_cb), icon);
        g_signal_connect (controller, "track-started",
                          G_CALLBACK (track_started_cb), icon);
        g_object_weak_ref (G_OBJECT (controller),
                           controller_destroyed_cb, icon);
        return icon;
}

static GdkPixbuf *
get_default_album_cover_icon            (void)
{
        static GdkPixbuf *pixbuf = NULL;

        /* Create "empty" GdkPixbuf if not created yet */
        if (pixbuf == NULL) {
                pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
        }

        return g_object_ref (pixbuf);
}

typedef struct {
        LastfmTrack *track;
        VglTrayIcon *vti;
} VglTrayIconPlaybackData;

static void
show_notification                       (VglTrayIcon *vti,
                                         LastfmTrack *track)
{
        g_return_if_fail(VGL_IS_TRAY_ICON(vti) && track != NULL);
        VglTrayIconPrivate *priv = vti->priv;

        GdkPixbuf *icon = NULL;
        gchar *notification_summary = NULL;
        gchar *notification_body = NULL;
        int i;

        if (track->image_data != NULL) {
                /* Set album image as icon if specified */
                icon = get_pixbuf_from_image (track->image_data,
                                              track->image_data_size,
                                              NOTIFICATION_ICON_SIZE);
        }

        /* Set summary text (title) */
        /* We need to replace "&" with "and" since it fails on some
           environments due to a strange bug (probably in libnotify) */
        notification_summary =
                string_replace(track->title, "&", "and");

        /* Remove also '<' and '>', which are conflicting chars */
        for (i = 0; notification_summary[i] != '\0'; i++) {
                if (notification_summary[i] == '<' ||
                    notification_summary[i] == '>') {
                        notification_summary[i] = ' ';
                }
        }

        /* Set body text (artist and, perhaps, the album) */
        if (track->album[0] == '\0') {
                /* No album */
                notification_body =
                        g_markup_printf_escaped (NOTIFICATION_BODY_NO_ALBUM,
                                                 track->artist);
        } else {
                /* Both artist and album */
                notification_body =
                        g_markup_printf_escaped (NOTIFICATION_BODY_WITH_ALBUM,
                                                 track->artist,
                                                 track->album);
        }

        /* Try loading the default cover icon if an error occurred or
         * not image_url was set for the current track */
        if (icon == NULL) {
                icon = get_default_album_cover_icon ();
        }

        /* Create the notification if not already created */
        if (priv->notification == NULL) {
                priv->notification =
#if !defined(NOTIFY_VERSION_MINOR) || (NOTIFY_VERSION_MAJOR == 0 && NOTIFY_VERSION_MINOR < 7)
                        notify_notification_new_with_status_icon (
                                notification_summary,
                                notification_body,
                                NULL,
                                priv->tray_icon);
#else
                        notify_notification_new (
                                notification_summary,
                                notification_body,
                                NULL);
#endif
        } else {
                notify_notification_update (priv->notification,
                                            notification_summary,
                                            notification_body,
                                            NULL);
        }

        /* Set cover image */
        notify_notification_set_icon_from_pixbuf (priv->notification, icon);

        /* Show notification */
        notify_notification_show (priv->notification, NULL);

        /* Cleanup */
        g_object_unref(icon);
        g_free (notification_summary);
        g_free (notification_body);
}

static gboolean
notify_playback_idle                    (gpointer userdata)
{
        VglTrayIconPlaybackData *d = userdata;
        VglTrayIconPrivate *priv;

        g_return_val_if_fail (d != NULL, FALSE);

        /* Set the now_playing private attribute and update panel */
        priv = d->vti->priv;
        priv->now_playing = (d->track != NULL);
        ctxt_menu_update (d->vti);

        /* Update tooltip and show notification, if needed */
        if (priv->now_playing) {
                gchar *tooltip_string = NULL;

                /* Set the tooltip */
                tooltip_string = g_strdup_printf (TOOLTIP_FORMAT_STRING,
                                                  d->track->title,
                                                  d->track->artist);
                gtk_status_icon_set_tooltip_text (priv->tray_icon,
                                                  tooltip_string);

                g_free (tooltip_string);

                /* Show the notification, if required */
                if (priv->show_notifications) {
                        show_notification (d->vti, d->track);
                }
        } else {
                gtk_status_icon_set_tooltip_text (priv->tray_icon,
                                                  TOOLTIP_DEFAULT_STRING);
        }

        /* Cleanup */
        g_object_unref(d->vti);
        if (d->track != NULL) vgl_object_unref(d->track);
        g_slice_free(VglTrayIconPlaybackData, d);

        return FALSE;
}

static gpointer
download_cover_thread                   (gpointer userdata)
{
        VglTrayIconPlaybackData *data = userdata;

        lastfm_get_track_cover_image (data->track);
        if (data->track->image_data == NULL) {
                g_debug ("Error getting cover image");
        }

        gdk_threads_add_idle (notify_playback_idle, data);
        return NULL;
}

static void
vgl_tray_icon_notify_playback           (VglTrayIcon *vti,
                                         LastfmTrack *track)
{
        g_return_if_fail(VGL_IS_TRAY_ICON(vti));
        VglTrayIconPlaybackData *data;
        gboolean dl_cover;

        data = g_slice_new(VglTrayIconPlaybackData);
        data->vti = g_object_ref(vti);
        data->track = track ? vgl_object_ref(track) : NULL;

        dl_cover = track && track->image_url && track->image_data == NULL;

        if (vti->priv->show_notifications && dl_cover) {
                g_thread_create (download_cover_thread, data, FALSE, NULL);
        } else {
                gdk_threads_add_idle (notify_playback_idle, data);
        }
}
