/*
 * vgl-tray-icon.c -- Freedesktop tray icon
 * Copyright (C) 2008 Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include <libnotify/notify.h>

#include "vgl-tray-icon.h"
#include "globaldefs.h"

#include "controller.h"
#include "playlist.h"
#include "metadata.h"
#include "util.h"
#include "uimisc.h"

#define TOOLTIP_DEFAULT_STRING _(" Stopped ")
#define TOOLTIP_FORMAT_STRING  _(" Now playing: \n %s \n   by  %s ")

#define NOTIFICATION_BODY_NO_ALBUM _(" by <i>%s</i>")
#define NOTIFICATION_BODY_WITH_ALBUM _(" by <i>%s</i>\n from <i>%s</i>")

#define NOTIFICATION_ICON_SIZE 48

#define VGL_TRAY_ICON_GET_PRIVATE(object)      \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), \
                                      VGL_TRAY_ICON_TYPE, VglTrayIconPrivate))

G_DEFINE_TYPE (VglTrayIcon, vgl_tray_icon, G_TYPE_OBJECT);

/* Private struct */
struct _VglTrayIconPrivate
{
        GtkStatusIcon *tray_icon;

        gboolean now_playing;

        GtkWidget *ctxt_menu;

        GtkWidget *settings_item;
        GtkWidget *about_item;
        GtkWidget *recommend_item;
        GtkWidget *tag_item;
        GtkWidget *add_to_pls_item;
        GtkWidget *love_item;
        GtkWidget *ban_item;
        GtkWidget *play_item;
        GtkWidget *stop_item;
        GtkWidget *next_item;
        GtkWidget *close_vagalume_item;

        gboolean show_notifications;
        NotifyNotification *notification;

        gint tray_icon_clicked_handler_id;
        gint tray_icon_popup_menu_handler_id;
        gint settings_item_handler_id;
        gint about_item_handler_id;
        gint recommend_item_handler_id;
        gint tag_item_handler_id;
        gint add_to_pls_item_handler_id;
        gint love_item_handler_id;
        gint ban_item_handler_id;
        gint play_item_handler_id;
        gint stop_item_handler_id;
        gint next_item_handler_id;
        gint close_vagalume_item_handler_id;
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

static void ctxt_menu_item_activated (GtkWidget *item, gpointer data);

/* Private */

/* Initialization/destruction functions */

static void
vgl_tray_icon_class_init(VglTrayIconClass *klass)
{
        GObjectClass *obj_class = G_OBJECT_CLASS(klass);

        obj_class -> finalize = vgl_tray_icon_finalize;
        g_type_class_add_private (obj_class, sizeof (VglTrayIconPrivate));
}

static void
vgl_tray_icon_init (VglTrayIcon *vti)
{
        VglTrayIconPrivate *priv = VGL_TRAY_ICON_GET_PRIVATE (vti);

        /* Init private attributes */
        vti->priv = priv;
        priv->tray_icon = gtk_status_icon_new ();

        priv->now_playing = FALSE;

        priv->settings_item = NULL;
        priv->about_item = NULL;
        priv->recommend_item = NULL;
        priv->tag_item = NULL;
        priv->add_to_pls_item = NULL;
        priv->love_item = NULL;
        priv->ban_item = NULL;
        priv->play_item = NULL;
        priv->stop_item = NULL;
        priv->next_item = NULL;
        priv->close_vagalume_item = NULL;

        priv->notification = NULL;
        priv->show_notifications = FALSE;

        /* Create main panel */
        ctxt_menu_create (vti);

        /* Connect signals */
        priv->tray_icon_clicked_handler_id =
                g_signal_connect(G_OBJECT(priv->tray_icon), "activate",
                                 G_CALLBACK(tray_icon_clicked), vti);
        priv->tray_icon_popup_menu_handler_id =
                g_signal_connect(G_OBJECT(priv->tray_icon),
                                 "popup-menu",
                                 G_CALLBACK(tray_icon_popup_menu), vti);

        /* Set icon and tooltip */
        gtk_status_icon_set_from_file (priv->tray_icon, APP_ICON);
        gtk_status_icon_set_tooltip(priv->tray_icon, TOOLTIP_DEFAULT_STRING);
        gtk_status_icon_set_visible(priv->tray_icon, TRUE);

        /* Init libnotify */
        setup_libnotify (vti);

        /* Update contextual menu */
        ctxt_menu_update (vti);
}

static void
vgl_tray_icon_finalize (GObject* object)
{
        VglTrayIcon *vti = VGL_TRAY_ICON (object);
        VglTrayIconPrivate *priv = vti->priv;

        /* Cleanup libnotify */
        cleanup_libnotify (vti);

        /* Disconnect handlers */
        g_signal_handler_disconnect (priv->settings_item,
                                     priv->settings_item_handler_id);
        g_signal_handler_disconnect (priv->about_item,
                                     priv->about_item_handler_id);
        g_signal_handler_disconnect (priv->recommend_item,
                                     priv->recommend_item_handler_id);
        g_signal_handler_disconnect (priv->tag_item,
                                     priv->tag_item_handler_id);
        g_signal_handler_disconnect (priv->add_to_pls_item,
                                     priv->add_to_pls_item_handler_id);
        g_signal_handler_disconnect (priv->love_item,
                                     priv->love_item_handler_id);
        g_signal_handler_disconnect (priv->ban_item,
                                     priv->ban_item_handler_id);
        g_signal_handler_disconnect (priv->play_item,
                                     priv->play_item_handler_id);
        g_signal_handler_disconnect (priv->stop_item,
                                     priv->stop_item_handler_id);
        g_signal_handler_disconnect (priv->next_item,
                                     priv->next_item_handler_id);
        g_signal_handler_disconnect (priv->close_vagalume_item,
                                     priv->close_vagalume_item_handler_id);
        g_signal_handler_disconnect (priv->tray_icon,
                                     priv->tray_icon_clicked_handler_id);
        g_signal_handler_disconnect (priv->tray_icon,
                                     priv->tray_icon_popup_menu_handler_id);

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
setup_libnotify  (VglTrayIcon *vti)
{
        if (!notify_init (APP_NAME)) {
                g_debug ("[TRAY ICON] :: Error initializing libnotify");
        } else {
                g_debug ("[TRAY ICON] :: Success initializing libnotify");
        }
}

static void
cleanup_libnotify  (VglTrayIcon *vti)
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
ctxt_menu_create (VglTrayIcon *vti)
{
        VglTrayIconPrivate *priv = vti->priv;

        /* Create ctxt_menu and ctxt_menu items */
        priv->ctxt_menu = gtk_menu_new ();
        priv->settings_item =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES,
                                                    NULL);
        priv->about_item =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
        priv->recommend_item =
                ui_menu_item_create_from_icon (RECOMMEND_ITEM_ICON_NAME,
                                               RECOMMEND_ITEM_STRING);
        priv->tag_item =
                ui_menu_item_create_from_icon (TAG_ITEM_ICON_NAME,
                                               TAG_ITEM_STRING);
        priv->add_to_pls_item =
                ui_menu_item_create_from_icon (ADD_TO_PLS_ITEM_ICON_NAME,
                                               ADD_TO_PLS_ITEM_STRING);
        priv->love_item =
                ui_menu_item_create_from_icon (LOVE_ITEM_ICON_NAME,
                                               LOVE_ITEM_STRING);
        priv->ban_item =
                ui_menu_item_create_from_icon (BAN_ITEM_ICON_NAME,
                                               BAN_ITEM_STRING);
        priv->play_item =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_PLAY,NULL);
        priv->stop_item =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_STOP,NULL);
        priv->next_item =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_NEXT,NULL);
        priv->close_vagalume_item =
                gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);

        /* Add items to ctxt_menu */
        gtk_menu_append (priv->ctxt_menu, priv->recommend_item);
        gtk_menu_append (priv->ctxt_menu, priv->tag_item);
        gtk_menu_append (priv->ctxt_menu, priv->add_to_pls_item);
        gtk_menu_append (priv->ctxt_menu, gtk_separator_menu_item_new ());
        gtk_menu_append (priv->ctxt_menu, priv->love_item);
        gtk_menu_append (priv->ctxt_menu, priv->ban_item);
        gtk_menu_append (priv->ctxt_menu, gtk_separator_menu_item_new ());
        gtk_menu_append (priv->ctxt_menu, priv->play_item);
        gtk_menu_append (priv->ctxt_menu, priv->stop_item);
        gtk_menu_append (priv->ctxt_menu, priv->next_item);
        gtk_menu_append (priv->ctxt_menu, gtk_separator_menu_item_new ());
        gtk_menu_append (priv->ctxt_menu, priv->settings_item);
        gtk_menu_append (priv->ctxt_menu, priv->about_item);
        gtk_menu_append (priv->ctxt_menu, gtk_separator_menu_item_new ());
        gtk_menu_append (priv->ctxt_menu, priv->close_vagalume_item);

        /* Connect signals */
        priv->settings_item_handler_id =
                g_signal_connect(priv->settings_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->about_item_handler_id =
                g_signal_connect(priv->about_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->recommend_item_handler_id =
                g_signal_connect(priv->recommend_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->tag_item_handler_id =
                g_signal_connect(priv->tag_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->add_to_pls_item_handler_id =
                g_signal_connect(priv->add_to_pls_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->love_item_handler_id =
                g_signal_connect(priv->love_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->ban_item_handler_id =
                g_signal_connect(priv->ban_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->play_item_handler_id =
                g_signal_connect(priv->play_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->stop_item_handler_id =
                g_signal_connect(priv->stop_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->next_item_handler_id =
                g_signal_connect(priv->next_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);
        priv->close_vagalume_item_handler_id =
                g_signal_connect(priv->close_vagalume_item, "activate",
                                 G_CALLBACK (ctxt_menu_item_activated), vti);

        /* Show widgets */
        gtk_widget_show_all (priv->ctxt_menu);
}

static void
ctxt_menu_update (VglTrayIcon *vti)
{
        VglTrayIconPrivate *priv = vti->priv;

        /* Adjust sentitiveness for ctxt_menu items and show/hide buttons */
        if (priv->now_playing) {
                gtk_widget_set_sensitive (priv->recommend_item, TRUE);
                gtk_widget_set_sensitive (priv->tag_item, TRUE);
                gtk_widget_set_sensitive (priv->add_to_pls_item, TRUE);
                gtk_widget_set_sensitive (priv->love_item, TRUE);
                gtk_widget_set_sensitive (priv->ban_item, TRUE);
                gtk_widget_set_sensitive (priv->play_item, FALSE);
                gtk_widget_set_sensitive (priv->stop_item, TRUE);
                gtk_widget_set_sensitive (priv->next_item, TRUE);

                gtk_widget_hide (priv->play_item);
                gtk_widget_show (priv->stop_item);
        } else {
                gtk_widget_set_sensitive (priv->recommend_item, FALSE);
                gtk_widget_set_sensitive (priv->tag_item, FALSE);
                gtk_widget_set_sensitive (priv->add_to_pls_item, FALSE);
                gtk_widget_set_sensitive (priv->love_item, FALSE);
                gtk_widget_set_sensitive (priv->ban_item, FALSE);
                gtk_widget_set_sensitive (priv->play_item, TRUE);
                gtk_widget_set_sensitive (priv->stop_item, FALSE);
                gtk_widget_set_sensitive (priv->next_item, FALSE);

                gtk_widget_show (priv->play_item);
                gtk_widget_hide (priv->stop_item);
        }
        gtk_widget_show (priv->close_vagalume_item);
}


/* Signals handlers */

static void
tray_icon_clicked (GtkStatusIcon *status_icon, gpointer data)
{
        controller_toggle_mainwin_visibility ();
}

static void
tray_icon_popup_menu (GtkStatusIcon *status_icon, guint button,
                      guint activate_time, gpointer data)
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

static void
ctxt_menu_item_activated (GtkWidget *item, gpointer data)
{
        VglTrayIcon *vti = VGL_TRAY_ICON (data);
        VglTrayIconPrivate *priv = vti->priv;

        if (item == priv->settings_item) {
                controller_open_usercfg();
        } else if (item == priv->about_item) {
                controller_show_about();
        } else if (item == priv->recommend_item) {
                controller_recomm_track();
        } else if (item == priv->tag_item) {
                controller_tag_track();
        } else if (item == priv->add_to_pls_item) {
                controller_add_to_playlist();
        } else if (item == priv->love_item) {
                controller_love_track (TRUE);
        } else if (item == priv->ban_item) {
                controller_ban_track (TRUE);
        } else if (item == priv->play_item) {
                controller_start_playing ();
        } else if (item == priv->stop_item) {
                controller_stop_playing ();
        } else if (item == priv->next_item) {
                controller_skip_track ();
        } else if (item == priv->close_vagalume_item) {
                controller_quit_app ();
        } else {
                g_warning ("[TRAY ICON] :: Unknown action");
        }
}


/* Public */

VglTrayIcon *
vgl_tray_icon_create (void)
{
        return g_object_new(VGL_TRAY_ICON_TYPE, NULL);
}

static GdkPixbuf *
get_default_album_cover_icon (void)
{
        static GdkPixbuf *pixbuf = NULL;

        /* Create "empty" GdkPixbuf if not created yet */
        if (pixbuf == NULL) {
                pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
        }

        return g_object_ref (pixbuf);
}

static GdkPixbuf *
get_album_cover_icon (LastfmTrack *track)
{
        g_return_val_if_fail(track != NULL && track->image_url != NULL, NULL);
        GdkPixbuf *pixbuf = NULL;

        /* This can take some time, so release the GDK lock */
        gdk_threads_leave();
        lastfm_get_track_cover_image(track);
        gdk_threads_enter();

        if (track->image_data != NULL) {
                pixbuf = get_pixbuf_from_image(track->image_data,
                                               track->image_data_size,
                                               NOTIFICATION_ICON_SIZE);
        } else {
                g_debug("Error getting cover image");
        }

        return pixbuf;
}

typedef struct {
        LastfmTrack *track;
        VglTrayIcon *vti;
} VglTrayIconPlaybackData;

static void
show_notification (VglTrayIcon *vti, LastfmTrack *track)
{
        g_return_if_fail(VGL_IS_TRAY_ICON(vti) && track != NULL);
        VglTrayIconPrivate *priv = vti->priv;

        GdkPixbuf *icon = NULL;
        gchar *notification_summary = NULL;
        gchar *notification_body = NULL;
        int i;

        if (track->image_url != NULL) {
                /* Set album image as icon if specified */
                icon = get_album_cover_icon (track);
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
                        notify_notification_new_with_status_icon (
                                notification_summary,
                                notification_body,
                                NULL,
                                priv->tray_icon);
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

static gpointer
notify_playback_thread (VglTrayIconPlaybackData *d)
{
        g_return_val_if_fail(d != NULL, NULL);
        VglTrayIconPrivate *priv;

        gdk_threads_enter();
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
                gtk_status_icon_set_tooltip(priv->tray_icon, tooltip_string);

                g_free (tooltip_string);

                /* Show the notification, if required */
                if (priv->show_notifications) {
                        show_notification (d->vti, d->track);
                }
        } else {
                gtk_status_icon_set_tooltip(priv->tray_icon, TOOLTIP_DEFAULT_STRING);
        }
        gdk_threads_leave();

        /* Cleanup */
        g_object_unref(d->vti);
        if (d->track != NULL) lastfm_track_unref(d->track);
        g_slice_free(VglTrayIconPlaybackData, d);

        return NULL;
}

void
vgl_tray_icon_notify_playback (VglTrayIcon *vti, LastfmTrack *track)
{
        g_return_if_fail(VGL_IS_TRAY_ICON(vti));
        VglTrayIconPlaybackData *data;
        data = g_slice_new(VglTrayIconPlaybackData);
        data->vti = g_object_ref(vti);
        data->track = track ? lastfm_track_ref(track) : NULL;
        g_thread_create((GThreadFunc)notify_playback_thread,data,FALSE,NULL);
}

void
vgl_tray_icon_show_notifications (VglTrayIcon *vti, gboolean show_notifications)
{
        vti->priv->show_notifications = show_notifications;
}
