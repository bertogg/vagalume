/*
 * vgl-sb-plugin.c -- Status bar plugin for Vagalume
 *
 * Copyright (C) 2007-2008 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *          Mario Sanchez Prada <msanchez@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include <string.h>
#include <gtk/gtk.h>
#include <dbus/dbus.h>
#include <libosso.h>
#include <hildon/hildon-banner.h>
#include <libhildonwm/hd-wm.h>
#include <libhildonwm/hd-wm-application.h>
#include <libhildondesktop/statusbar-item.h>
#include <libhildondesktop/libhildondesktop.h>

#include "vgl-sb-plugin.h"
#include "globaldefs.h"
#include "dbus.h"

/* This is run by hildon-desktop, so we need to redefine
   the _(String) macro to use the Vagalume domain */
#undef _
#define _(String) dgettext (GETTEXT_PACKAGE, String)

#define MAIN_PANEL_WIDTH 300

#define NO_ARTIST_STRING _("No artist")
#define NO_TRACK_STRING _("No track")
#define NO_ALBUM_STRING _("No album")

#define PLAY_ITEM_STRING _("Play")
#define STOP_ITEM_STRING _("Stop")
#define SKIP_ITEM_STRING _("Skip")
#define LOVE_ITEM_STRING _("Love")
#define BAN_ITEM_STRING _("Ban")
#define SHOW_APP_ITEM_STRING _("Show main window")
#define CLOSE_APP_ITEM_STRING _("Close Vagalume")

#define VGL_SB_PLUGIN_GET_PRIVATE(object)      \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), \
                                      VGL_SB_PLUGIN_TYPE, VglSbPluginPrivate))

enum {
        ARTIST_ITEM,
        TRACK_ITEM,
        ALBUM_ITEM,
        PLAY_ITEM,
        STOP_ITEM,
        NEXT_ITEM,
        LOVE_ITEM,
        BAN_ITEM,
        SHOW_WIN_ITEM,
        QUIT_ITEM,
        N_MENU_ITEMS
};

struct _VglSbPluginPrivate
{
        osso_context_t *osso_context;
        DBusConnection *dbus_connection;
        gboolean dbus_filter_added;

        gboolean is_visible;
        gboolean now_playing;
        gboolean running_app;

        gchar *artist_string;
        gchar *track_string;
        gchar *album_string;

        GtkWidget *button;
        GtkWidget *icon;
        GtkWidget *main_panel;
        GtkWidget *menu_item[N_MENU_ITEMS];

        gulong plugin_btn_handler_id;
        gulong main_panel_handler_id;
        gulong menu_handler_id[N_MENU_ITEMS];

        gboolean dispose_has_run;
};

HD_DEFINE_PLUGIN (VglSbPlugin, vgl_sb_plugin, STATUSBAR_TYPE_ITEM);


/* Initialization/destruction functions */
static void vgl_sb_plugin_class_init (VglSbPluginClass *klass);
static void vgl_sb_plugin_init (VglSbPlugin *vsbp);
static void vgl_sb_plugin_dispose (GObject *object);

/* Dbus stuff */
static gboolean dbus_init (VglSbPlugin *vsbp);
static void dbus_cleanup (VglSbPlugin *vsbp);
static DBusHandlerResult dbus_req_handler (DBusConnection *connection,
                                           DBusMessage *message,
                                           gpointer data);
static void dbus_send_request (VglSbPlugin *vsbp, const gchar *request);
static void dbus_send_request_with_params (VglSbPlugin *vsbp, const char *request,
                                           int first_type, ...);

static void notify_handler (VglSbPlugin *vsbp, DBusMessage *message);

/* Setters */
static void set_visibility (VglSbPlugin *vsbp, gboolean visible);
static void set_track_info (VglSbPlugin *vsbp, const gchar *artist,
                            const gchar *track, const gchar *album);

/* Panel update functions */
static void main_panel_create (VglSbPlugin *vsbp);
static void main_panel_update (VglSbPlugin *vsbp);

/* Signals handlers */
static void plugin_btn_toggled (GtkWidget *button, gpointer data);
static void main_panel_item_activated (GtkWidget *item, gpointer data);
static void main_panel_hidden (GtkWidget *main_panel, gpointer user_data);

/* Utility functions */
static gboolean vagalume_running (VglSbPlugin *vsbp);

/* Initialization/destruction functions */

static void
vgl_sb_plugin_class_init (VglSbPluginClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->dispose = vgl_sb_plugin_dispose;

        g_type_class_add_private (object_class,
                                  sizeof (VglSbPluginPrivate));

        bindtextdomain (GETTEXT_PACKAGE, VAGALUME_LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

static void
vgl_sb_plugin_init (VglSbPlugin *vsbp)
{
        int i;
        VglSbPluginPrivate *priv = VGL_SB_PLUGIN_GET_PRIVATE (vsbp);
        GdkPixbuf *icon_pixbuf;

        /* Init private attributes to their default values */
        vsbp->priv = priv;
        priv->osso_context = NULL;
        priv->dbus_connection = NULL;
        priv->dbus_filter_added = FALSE;
        priv->is_visible = FALSE;
        priv->now_playing = FALSE;
        priv->running_app = FALSE;
        priv->artist_string = NULL;
        priv->track_string = NULL;
        priv->album_string = NULL;
        priv->button = NULL;
        priv->icon = NULL;
        priv->main_panel = NULL;
        for (i = 0; i < N_MENU_ITEMS; i++) {
                priv->menu_item[i] = NULL;
                priv->menu_handler_id[i] = 0;
        }
        priv->dispose_has_run = FALSE;

        /* Setup libosso */
        priv->osso_context = osso_initialize ("vgl_sb_plugin",
                                              APP_VERSION, TRUE, NULL);
        if (!priv->osso_context) {
                g_debug ("Unable to initialize OSSO context");
                return;
        }

        /* Setup dbus */
        if (!dbus_init (vsbp)) {
                g_debug ("Unable to initialize D-Bus system");
                return;
        }

        /* Create status bar plugin button */
        icon_pixbuf = gdk_pixbuf_new_from_file (APP_ICON, NULL);
        priv->button = gtk_toggle_button_new ();
        priv->icon = gtk_image_new_from_pixbuf (icon_pixbuf);
        gtk_container_add (GTK_CONTAINER (priv->button), priv->icon);
        gtk_container_add (GTK_CONTAINER (vsbp), priv->button);

        /* Connect signals */
        priv->plugin_btn_handler_id =
                g_signal_connect (priv->button, "toggled",
                                  G_CALLBACK(plugin_btn_toggled), vsbp);

        /* Show widgets */
        gtk_widget_show_all (priv->button);

        /* Create main panel */
        main_panel_create (vsbp);

        /* Check if vagalume is running */
        if (vagalume_running (vsbp)) {
                priv->is_visible = TRUE;
                dbus_send_request (vsbp,
                                   APP_DBUS_METHOD_REQUEST_STATUS);
        }

        /* Update visibililty */
        set_visibility (vsbp, priv->is_visible);
}

static void
vgl_sb_plugin_dispose (GObject *object)
{
        int i;
        VglSbPlugin *vsbp = VGL_SB_PLUGIN (object);
        VglSbPluginPrivate *priv = vsbp->priv;

        /* Ensure dispose is called only once */
        if (priv->dispose_has_run) {
                return;
        }
        priv->dispose_has_run = TRUE;

        /* Dbus cleanup */
        dbus_cleanup (vsbp);

        /* Libosso cleanup */
        osso_deinitialize (priv->osso_context);

        /* Disconnect handlers */
        g_signal_handler_disconnect (priv->button,
                                     priv->plugin_btn_handler_id);
        g_signal_handler_disconnect (priv->main_panel,
                                     priv->main_panel_handler_id);
        for (i = 0; i < N_MENU_ITEMS; i++) {
                if (priv->menu_handler_id[ARTIST_ITEM] != 0) {
                        g_signal_handler_disconnect (priv->menu_item[i],
                                                     priv->menu_handler_id[i]);
                }
        }

        /* Destroy local widgets */
        if (priv->main_panel) {
                gtk_widget_destroy (priv->main_panel);
        }

        /* Free private strings */
        if (priv->artist_string != NULL) {
                g_free (priv->artist_string);
        }
        if (priv->track_string != NULL) {
                g_free (priv->track_string);
        }
        if (priv->album_string != NULL) {
                g_free (priv->album_string);
        }

        G_OBJECT_CLASS (g_type_class_peek_parent
                        (G_OBJECT_GET_CLASS(object)))->dispose(object);
}

/* Dbus stuff */

static gboolean
dbus_init(VglSbPlugin *vsbp)
{
        VglSbPluginPrivate *priv = vsbp->priv;


        /* Get the D-Bus connection to the session bus */
        priv->dbus_connection = dbus_bus_get (DBUS_BUS_SESSION, NULL);
        if (!priv->dbus_connection) {
                g_debug ("Unable to get DBUS connection");
                return FALSE;
        }

        /* Integrate D-Bus with GMainLoop */
        dbus_connection_setup_with_g_main (priv->dbus_connection, NULL);

        /* Match only signals emitted from Vagalume */
        dbus_bus_add_match (priv->dbus_connection,
                            "type='signal',interface='" APP_DBUS_IFACE "'", NULL);
        dbus_connection_flush (priv->dbus_connection);

        /* Add a handler for incoming messages */
        if (!dbus_connection_add_filter (priv->dbus_connection, dbus_req_handler,
                                         vsbp, NULL)) {
                g_debug ("Unable to add D-Bus filter");
                return FALSE;
        }
        priv->dbus_filter_added = TRUE;

        return TRUE;
}

static void dbus_cleanup (VglSbPlugin *vsbp)
{
        VglSbPluginPrivate *priv = vsbp->priv;

        /* Remove filter and matching rules */
        if (priv->dbus_filter_added) {
                dbus_connection_remove_filter (priv->dbus_connection,
                                               dbus_req_handler,
                                               vsbp);
                dbus_bus_remove_match (priv->dbus_connection,
                                       "type='signal',interface='" APP_DBUS_IFACE "'", NULL);
        }

        /* Unref the D-bus connection */
        if (priv->dbus_connection) {
                dbus_connection_unref (priv->dbus_connection);
        }
}

static DBusHandlerResult
dbus_req_handler (DBusConnection *connection, DBusMessage *message, gpointer data)
{
        VglSbPlugin *vsbp = VGL_SB_PLUGIN (data);

        if (dbus_message_is_signal (message,
                                    APP_DBUS_IFACE,
                                    APP_DBUS_SIGNAL_NOTIFY)) {
                /* Received a playback notification signal */
                notify_handler (vsbp, message);
                return DBUS_HANDLER_RESULT_HANDLED;
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
dbus_send_request (VglSbPlugin *vsbp, const gchar *request)
{
        /* Delegate on dbus_send_request_with_params() */
        dbus_send_request_with_params (vsbp, request, DBUS_TYPE_INVALID);
}

static void
dbus_send_request_with_params (VglSbPlugin *vsbp, const char *request, int first_type, ...)
{
        VglSbPluginPrivate *priv = vsbp->priv;
        DBusMessage *dbus_msg = NULL;
        va_list ap;

        dbus_msg = dbus_message_new_method_call (APP_DBUS_SERVICE,
                                                 APP_DBUS_OBJECT,
                                                 APP_DBUS_IFACE,
                                                 request);

        if (first_type != DBUS_TYPE_INVALID)
        {
                va_start (ap, first_type);
                dbus_message_append_args_valist (dbus_msg, first_type, ap);
                va_end (ap);
        }
        dbus_connection_send (priv->dbus_connection, dbus_msg, 0);
        dbus_connection_flush (priv->dbus_connection);
        dbus_message_unref (dbus_msg);
}

static void
notify_handler (VglSbPlugin *vsbp, DBusMessage *message)
{
        VglSbPluginPrivate *priv = vsbp->priv;
        DBusMessageIter iter;
        gchar *type = NULL;
        gboolean msg_handled = FALSE;

        /* Ensure you have appropiate arguments */
        if (!dbus_message_iter_init (message, &iter))
        {
                g_debug ("Message has no parameters");
                g_return_if_reached ();
        }

        /* Check notification type */
        if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_STRING)
        {
                g_debug ("First parameter is not a string");
                g_return_if_reached ();
        }
        dbus_message_iter_get_basic (&iter, &type);

        /* Let's assume that an application is running
           if a message was received from Vagalume */
        priv->running_app = TRUE;

        if (!strcmp (type, APP_DBUS_SIGNAL_NOTIFY_PLAYING)) {
                /* Retrieve arguments */
                gchar *artist = NULL;
                gchar *track = NULL;
                gchar *album = NULL;

                g_debug ("NOTIFY PLAYING RECEIVED");

                /* Artist */
                if (dbus_message_iter_next (&iter) &&
                    dbus_message_iter_get_arg_type (&iter) == DBUS_TYPE_STRING) {
                        dbus_message_iter_get_basic (&iter, &artist);
                }

                /* Track title */
                if (dbus_message_iter_next (&iter) &&
                    dbus_message_iter_get_arg_type (&iter) == DBUS_TYPE_STRING) {
                        dbus_message_iter_get_basic (&iter, &track);
                }

                /* Album */
                if (dbus_message_iter_next (&iter) &&
                    dbus_message_iter_get_arg_type (&iter) == DBUS_TYPE_STRING) {
                        dbus_message_iter_get_basic (&iter, &album);
                }

                /* Update private attributes */
                set_track_info (vsbp, artist, track, album);
                priv->now_playing = TRUE;

                msg_handled = TRUE;
        } else if (!strcmp (type, APP_DBUS_SIGNAL_NOTIFY_STOPPED)) {
                g_debug ("NOTIFY STOPPED RECEIVED");

                /* Update private attributes */
                set_track_info (vsbp, NULL, NULL, NULL);
                priv->now_playing = FALSE;

                msg_handled = TRUE;
        } else if (!strcmp (type, APP_DBUS_SIGNAL_NOTIFY_STARTED)) {
                g_debug ("NOTIFY STARTED RECEIVED");

                /* Set the plugin invisible */
                set_visibility (vsbp, TRUE);
                dbus_send_request (vsbp, APP_DBUS_METHOD_REQUEST_STATUS);

                msg_handled = TRUE;
        } else if (!strcmp (type, APP_DBUS_SIGNAL_NOTIFY_CLOSING)) {
                g_debug ("NOTIFY CLOSING RECEIVED");

                /* Update private attributes */
                priv->running_app = FALSE;
                priv->now_playing = FALSE;

                /* Set the plugin invisible */
                set_visibility (vsbp, FALSE);

                msg_handled = TRUE;
        } else {
                /* Wrong notification */
                g_debug ("Wrong notification received: %s", type);
                msg_handled = FALSE;
        }

        /* If the message was handled update the panel */
        if (msg_handled) {
                main_panel_update (vsbp);
        }
}

/* Setters */

static void
set_visibility (VglSbPlugin *vsbp, gboolean visible)
{
        gboolean old_condition;

        g_object_get (vsbp, "condition", &old_condition, NULL);
        if (old_condition != visible)
                g_object_set (vsbp, "condition", visible, NULL);

        vsbp->priv->is_visible = visible;

        g_debug ("Setting visibility to '%s'",
                 (visible ? "visible" : "hidden"));
}


static void
set_field (gchar **field, const gchar *text, const gchar *markup_fmt)
{
        g_return_if_fail (text != NULL);
        gchar *final_text = NULL;
        gchar *final_markup_fmt = NULL;

        if (*field != NULL) {
                g_free (*field);
                *field = NULL;
        }

        final_text = g_strstrip (g_strdup (text));

        if (markup_fmt != NULL) {
                final_markup_fmt = g_strdup (markup_fmt);
        } else {
                final_markup_fmt = g_strdup ("<span>%s</span>");
        }

        *field = g_markup_printf_escaped (final_markup_fmt,
                                          final_text);

        g_free (final_markup_fmt);
        g_free (final_text);

        g_debug ("Setting field to '%s'", *field);
}

static void
set_track_info (VglSbPlugin *vsbp,
               const gchar *artist,
               const gchar *track,
               const gchar *album)
{
        g_return_if_fail(VGL_IS_SB_PLUGIN(vsbp));

        VglSbPluginPrivate *priv = vsbp->priv;

        /* Call to set_field (), always with valid data */
        set_field (&priv->artist_string,
                   (artist?artist:NO_ARTIST_STRING), "<b>%s</b>");
        set_field (&priv->track_string,
                   (track?track:NO_TRACK_STRING), "<i>%s</i>");

        if ((album != NULL) && g_str_equal (album, "")) {
                /* Set album string to NULL if not available yet */
                if (priv->album_string != NULL) {
                        g_free (priv->album_string);
                }
                priv->album_string = NULL;
        } else {
                set_field (&priv->album_string,
                           (album?album:NO_ALBUM_STRING), NULL);
        }
}


/* Panel update functions */

static PangoFontDescription *
get_small_font (GtkWidget *widget)
{
        static PangoFontDescription *new_font = NULL;

        if (new_font == NULL) {
                new_font = pango_font_description_from_string
                        ("Nokia Sans 13.625");
        }

        return new_font;
}

static void
main_panel_create (VglSbPlugin *vsbp)
{
        VglSbPluginPrivate *priv = vsbp->priv;
        GtkWidget *label;

        /* Create main_panel and main_panel items */
        priv->main_panel = gtk_menu_new ();
        gtk_widget_set_size_request (priv->main_panel, MAIN_PANEL_WIDTH, -1);

        /* Insensitive items */
        priv->menu_item[ARTIST_ITEM] = gtk_menu_item_new ();
        label = gtk_label_new (NULL);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_widget_modify_font (label, get_small_font (label));
        gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
        gtk_container_add (GTK_CONTAINER (priv->menu_item[ARTIST_ITEM]), label);
        gtk_widget_set_sensitive (priv->menu_item[ARTIST_ITEM], FALSE);
        label->state = GTK_STATE_NORMAL;

        priv->menu_item[TRACK_ITEM] = gtk_menu_item_new ();
        label = gtk_label_new (NULL);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_widget_modify_font (label, get_small_font (label));
        gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
        gtk_container_add (GTK_CONTAINER (priv->menu_item[TRACK_ITEM]), label);
        gtk_widget_set_sensitive (priv->menu_item[TRACK_ITEM], FALSE);
        label->state = GTK_STATE_NORMAL;

        priv->menu_item[ALBUM_ITEM] = gtk_menu_item_new ();
        label = gtk_label_new (NULL);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_widget_modify_font (label, get_small_font (label));
        gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
        gtk_container_add (GTK_CONTAINER (priv->menu_item[ALBUM_ITEM]), label);
        gtk_widget_set_sensitive (priv->menu_item[ALBUM_ITEM], FALSE);
        label->state = GTK_STATE_NORMAL;

        /* Active items */
        priv->menu_item[PLAY_ITEM] =
                gtk_menu_item_new_with_label (PLAY_ITEM_STRING);
        priv->menu_item[STOP_ITEM] =
                gtk_menu_item_new_with_label (STOP_ITEM_STRING);
        priv->menu_item[NEXT_ITEM] =
                gtk_menu_item_new_with_label (SKIP_ITEM_STRING);
        priv->menu_item[LOVE_ITEM] =
                gtk_menu_item_new_with_label (LOVE_ITEM_STRING);
        priv->menu_item[BAN_ITEM] =
                gtk_menu_item_new_with_label (BAN_ITEM_STRING);
        priv->menu_item[SHOW_WIN_ITEM] =
                gtk_menu_item_new_with_label (SHOW_APP_ITEM_STRING);
        priv->menu_item[QUIT_ITEM] =
                gtk_menu_item_new_with_label (CLOSE_APP_ITEM_STRING);

        /* Add items to main_panel */
        gtk_menu_append (priv->main_panel, priv->menu_item[ARTIST_ITEM]);
        gtk_menu_append (priv->main_panel, priv->menu_item[TRACK_ITEM]);
        gtk_menu_append (priv->main_panel, priv->menu_item[ALBUM_ITEM]);
        gtk_menu_append (priv->main_panel, gtk_separator_menu_item_new ());
        gtk_menu_append (priv->main_panel, priv->menu_item[PLAY_ITEM]);
        gtk_menu_append (priv->main_panel, priv->menu_item[STOP_ITEM]);
        gtk_menu_append (priv->main_panel, priv->menu_item[NEXT_ITEM]);
        gtk_menu_append (priv->main_panel, priv->menu_item[LOVE_ITEM]);
        gtk_menu_append (priv->main_panel, priv->menu_item[BAN_ITEM]);
        gtk_menu_append (priv->main_panel, gtk_separator_menu_item_new ());
        gtk_menu_append (priv->main_panel, priv->menu_item[SHOW_WIN_ITEM]);
        gtk_menu_append (priv->main_panel, priv->menu_item[QUIT_ITEM]);

        /* Connect signals */
        priv->main_panel_handler_id =
                g_signal_connect(priv->main_panel,
                                 "selection-done",
                                 G_CALLBACK(main_panel_hidden), vsbp);
        priv->menu_handler_id[PLAY_ITEM] =
                g_signal_connect(priv->menu_item[PLAY_ITEM], "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->menu_handler_id[STOP_ITEM] =
                g_signal_connect(priv->menu_item[STOP_ITEM], "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->menu_handler_id[NEXT_ITEM] =
                g_signal_connect(priv->menu_item[NEXT_ITEM], "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->menu_handler_id[LOVE_ITEM] =
                g_signal_connect(priv->menu_item[LOVE_ITEM], "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->menu_handler_id[BAN_ITEM] =
                g_signal_connect(priv->menu_item[BAN_ITEM], "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->menu_handler_id[SHOW_WIN_ITEM] =
                g_signal_connect(priv->menu_item[SHOW_WIN_ITEM], "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->menu_handler_id[QUIT_ITEM] =
                g_signal_connect(priv->menu_item[QUIT_ITEM], "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);

        /* Show widgets */
        gtk_widget_show_all (priv->main_panel);
}

static void
main_panel_update (VglSbPlugin *vsbp)
{
        VglSbPluginPrivate *priv = vsbp->priv;
        gboolean np = priv->now_playing;
        GtkWidget *label;

        if (!priv->running_app) {
                /* Application closed */
                priv->is_visible = FALSE;
                priv->now_playing = FALSE;
        }

        /* Adjust sentitiveness for main_panel items and show/hide buttons */
        gtk_widget_set_sensitive (priv->menu_item[PLAY_ITEM], !np);
        gtk_widget_set_sensitive (priv->menu_item[STOP_ITEM], np);
        gtk_widget_set_sensitive (priv->menu_item[NEXT_ITEM], np);
        gtk_widget_set_sensitive (priv->menu_item[LOVE_ITEM], np);
        gtk_widget_set_sensitive (priv->menu_item[BAN_ITEM], np);

        if (np) {
                gtk_widget_hide (priv->menu_item[PLAY_ITEM]);
                gtk_widget_show (priv->menu_item[STOP_ITEM]);
        } else {
                /* Not playing: set default strings */
                set_track_info (vsbp, NULL, NULL, NULL);

                gtk_widget_show (priv->menu_item[PLAY_ITEM]);
                gtk_widget_hide (priv->menu_item[STOP_ITEM]);
        }

        /* Detail labels */
        label = gtk_bin_get_child (GTK_BIN (priv->menu_item[ARTIST_ITEM]));
        gtk_label_set_markup (GTK_LABEL (label), priv->artist_string);

        label = gtk_bin_get_child (GTK_BIN (priv->menu_item[TRACK_ITEM]));
        gtk_label_set_markup (GTK_LABEL (label), priv->track_string);

        /* Show / hide the album label depending on its availability */
        if (priv->album_string != NULL) {
                label = gtk_bin_get_child (
                        GTK_BIN (priv->menu_item[ALBUM_ITEM]));
                gtk_label_set_markup (GTK_LABEL (label), priv->album_string);
                gtk_widget_show (priv->menu_item[ALBUM_ITEM]);
        } else {
                gtk_widget_hide (priv->menu_item[ALBUM_ITEM]);
        }
}


/* Signals handlers */

static void
main_panel_position_func (GtkMenu   *main_panel,
                          gint      *x,
                          gint      *y,
                          gboolean  *push_in,
                          gpointer   data)
{
        VglSbPluginPrivate *priv = VGL_SB_PLUGIN (data)->priv;

        GtkRequisition req;

        gtk_widget_size_request (GTK_WIDGET (main_panel->toplevel), &req);

        gdk_window_get_origin (priv->button->window, x, y);
        *x += (priv->button->allocation.x + priv->button->allocation.width
               - req.width);
        *y += (priv->button->allocation.y + priv->button->allocation.height +
               10);
}

static void
plugin_btn_toggled (GtkWidget *button, gpointer data)
{
        VglSbPlugin *vsbp = VGL_SB_PLUGIN (data);

        if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (button)))
                return;

        g_debug ("Plugin button toggled");

        gtk_menu_popup (GTK_MENU (vsbp->priv->main_panel),
                        NULL, NULL,
                        main_panel_position_func, vsbp,
                        1,
                        gtk_get_current_event_time ());
}

static void
main_panel_item_activated (GtkWidget *item, gpointer data)
{
        VglSbPlugin *vsbp = VGL_SB_PLUGIN (data);
        VglSbPluginPrivate *priv = vsbp->priv;
        const gboolean interactive = TRUE;

        if (item == priv->menu_item[PLAY_ITEM]) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_PLAY);
                g_debug ("DBUS request sent: Play");
        } else if (item == priv->menu_item[STOP_ITEM]) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_STOP);
                g_debug ("DBUS request sent: Stop");
        } else if (item == priv->menu_item[NEXT_ITEM]) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_SKIP);
                g_debug ("DBUS request sent: Skip");
        } else if (item == priv->menu_item[LOVE_ITEM]) {
                dbus_send_request_with_params (vsbp,
                                               APP_DBUS_METHOD_LOVETRACK,
                                               DBUS_TYPE_BOOLEAN,
                                               &interactive,
                                               DBUS_TYPE_INVALID);
                g_debug ("DBUS request sent: LoveTrack");
        } else if (item == priv->menu_item[BAN_ITEM]) {
                dbus_send_request_with_params (vsbp,
                                               APP_DBUS_METHOD_BANTRACK,
                                               DBUS_TYPE_BOOLEAN,
                                               &interactive,
                                               DBUS_TYPE_INVALID);
                g_debug ("DBUS request sent: BanTrack");
        } else if (item == priv->menu_item[SHOW_WIN_ITEM]) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_SHOWWINDOW);
                g_debug ("DBUS request sent: Show application");
        } else if (item == priv->menu_item[QUIT_ITEM]) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_CLOSEAPP);
                g_debug ("DBUS request sent: Close application");
        } else {
                g_debug ("Unknown action");
        }
}

static void
main_panel_hidden (GtkWidget *main_panel, gpointer data)
{
        VglSbPlugin *vsbp = VGL_SB_PLUGIN (data);
        VglSbPluginPrivate *priv = vsbp->priv;

        g_debug ("Main panel hidden");

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->button), FALSE);
}


/* Utility functions */

static gboolean
vagalume_running (VglSbPlugin *vsbp)
{
        HDWM *hdwm;
        HDWMEntryInfo *info = NULL;
        GList *apps_list = NULL;
        GList *l = NULL;
        gchar *appname = NULL;
        gboolean app_found = FALSE;

        hdwm = hd_wm_get_singleton ();
        hd_wm_update_client_list (hdwm);

        apps_list = hd_wm_get_applications (hdwm);

        g_debug ("Searching for application: %s", APP_NAME);

        for (l = apps_list; l; l = l->next) {
                info = (HDWMEntryInfo *) l->data;

                if (appname != NULL)
                        g_free (appname);

                appname = hd_wm_entry_info_get_app_name (info);

                g_debug ("Application\n\tApp:'%s', Title:'%s'",
                         hd_wm_entry_info_get_app_name (info),
                         hd_wm_entry_info_get_title (info));

                if (appname && !strcmp (APP_NAME, appname)) {
                        app_found = TRUE;
                        break;
                }
        }

        /* Free resources */
        g_free (appname);

        return app_found;
}
