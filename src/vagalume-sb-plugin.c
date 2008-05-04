/*
 * vagalume-sb-plugin.c -- Status bar plugin for Vagalume
 * Copyright (C) 2008 Mario Sanchez Prada <msanchez@igalia.com>
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

#include "vagalume-sb-plugin.h"
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

#define VAGALUME_SB_PLUGIN_GET_PRIVATE(object) \
                 (G_TYPE_INSTANCE_GET_PRIVATE ((object), \
                         VAGALUME_SB_PLUGIN_TYPE, \
                         VagalumeSbPluginPrivate))

typedef struct _VagalumeSbPluginPrivate VagalumeSbPluginPrivate;
struct _VagalumeSbPluginPrivate
{
        osso_context_t *osso_context;

        gboolean is_visible;
        gboolean now_playing;
        gboolean running_app;

        gchar *artist_string;
        gchar *track_string;
        gchar *album_string;

        GtkWidget *button;
        GtkWidget *icon;
        GtkWidget *main_panel;
        GtkWidget *artist_item;
        GtkWidget *track_item;
        GtkWidget *album_item;
        GtkWidget *play_item;
        GtkWidget *stop_item;
        GtkWidget *next_item;
        GtkWidget *love_item;
        GtkWidget *ban_item;
        GtkWidget *open_vagalume_item;
        GtkWidget *close_vagalume_item;

        gint plugin_btn_handler_id;
        gint main_panel_handler_id;
        gint play_item_handler_id;
        gint stop_item_handler_id;
        gint next_item_handler_id;
        gint love_item_handler_id;
        gint ban_item_handler_id;
        gint open_vagalume_item_handler_id;
        gint close_vagalume_item_handler_id;
};

HD_DEFINE_PLUGIN (VagalumeSbPlugin, vagalume_sb_plugin, STATUSBAR_TYPE_ITEM);


/* Initialization/destruction functions */
static void vagalume_sb_plugin_class_init (VagalumeSbPluginClass *klass);
static void vagalume_sb_plugin_init (VagalumeSbPlugin *vsbp);
static void vagalume_sb_plugin_finalize (GObject *object);

/* Dbus stuff */
static gboolean dbus_init (VagalumeSbPlugin *vsbp);
static void dbus_close (VagalumeSbPlugin *vsbp);
static gint dbus_req_handler (const gchar* interface, const gchar* method,
                              GArray* arguments, gpointer data,
                              osso_rpc_t* retval);
static void dbus_send_request (VagalumeSbPlugin *vsbp, const gchar *request);
static void dbus_send_request_with_param (VagalumeSbPlugin *vsbp,
                                          const gchar *request,
                                          int param_type,
                                          gpointer param_value);

static void notify_handler (VagalumeSbPlugin *vsbp, GArray* arguments);

/* Setters */
static void set_visibility (VagalumeSbPlugin *vsbp, gboolean visible);
static void set_track_info (VagalumeSbPlugin *vsbp, const gchar *artist,
                            const gchar *track, const gchar *album);

/* Panel update functions */
static void main_panel_create (VagalumeSbPlugin *vsbp);
static void main_panel_update (VagalumeSbPlugin *vsbp);

/* Signals handlers */
static void plugin_btn_toggled (GtkWidget *button, gpointer data);
static void main_panel_item_activated (GtkWidget *item, gpointer data);
static void main_panel_hidden (GtkWidget *main_panel, gpointer user_data);

/* Utility functions */
static gboolean vagalume_running (VagalumeSbPlugin *vsbp);

/* Initialization/destruction functions */

static void
vagalume_sb_plugin_class_init (VagalumeSbPluginClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = vagalume_sb_plugin_finalize;

        g_type_class_add_private (object_class,
                                  sizeof (VagalumeSbPluginPrivate));

        bindtextdomain (GETTEXT_PACKAGE, VAGALUME_LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

static void
vagalume_sb_plugin_init (VagalumeSbPlugin *vsbp)
{
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);

        priv->osso_context = NULL;

        /* Setup dbus */
        if (dbus_init (vsbp)) {
                GdkPixbuf *icon_pixbuf;
                icon_pixbuf = gdk_pixbuf_new_from_file(APP_ICON, NULL);

                /* Init private attributes */
                priv->is_visible = FALSE;
                priv->now_playing = FALSE;
                priv->running_app = FALSE;
                priv->artist_string = NULL;
                priv->track_string = NULL;
                priv->album_string = NULL;
                priv->button = NULL;
                priv->icon = NULL;
                priv->main_panel = NULL;
                priv->artist_item = NULL;
                priv->track_item = NULL;
                priv->album_item = NULL;
                priv->play_item = NULL;
                priv->stop_item = NULL;
                priv->next_item = NULL;
                priv->love_item = NULL;
                priv->ban_item = NULL;
                priv->open_vagalume_item = NULL;
                priv->close_vagalume_item = NULL;

                /* Create status bar plugin button */
                priv->button = gtk_toggle_button_new ();
                priv->icon = gtk_image_new_from_pixbuf (icon_pixbuf);
                gtk_container_add (GTK_CONTAINER (priv->button), priv->icon);
                gtk_container_add (GTK_CONTAINER (vsbp), priv->button);

                /* Connect signals */
                priv->plugin_btn_handler_id =
                        g_signal_connect(priv->button, "toggled",
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
}

static void
vagalume_sb_plugin_finalize (GObject *object)
{
        VagalumeSbPlugin *vsbp = VAGALUME_SB_PLUGIN (object);
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);

        /* Dbus */
        dbus_close (vsbp);

        /* Destroy local widgets */
        if (priv->main_panel) {
                gtk_widget_destroy (priv->main_panel);
        }

        /* Disconnect handlers */
        g_signal_handler_disconnect (priv->button,
                                     priv->plugin_btn_handler_id);
        g_signal_handler_disconnect (priv->main_panel,
                                     priv->main_panel_handler_id);
        g_signal_handler_disconnect (priv->play_item,
                                     priv->play_item_handler_id);
        g_signal_handler_disconnect (priv->stop_item,
                                     priv->stop_item_handler_id);
        g_signal_handler_disconnect (priv->next_item,
                                     priv->next_item_handler_id);
        g_signal_handler_disconnect (priv->love_item,
                                     priv->love_item_handler_id);
        g_signal_handler_disconnect (priv->ban_item,
                                     priv->ban_item_handler_id);
        g_signal_handler_disconnect (priv->open_vagalume_item,
                                     priv->open_vagalume_item_handler_id);
        g_signal_handler_disconnect (priv->close_vagalume_item,
                                     priv->close_vagalume_item_handler_id);

        G_OBJECT_CLASS (g_type_class_peek_parent
                        (G_OBJECT_GET_CLASS(object)))->finalize(object);
}


/* Dbus stuff */

static gboolean
dbus_init(VagalumeSbPlugin *vsbp)
{
  VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);
        osso_return_t result;

        priv->osso_context = osso_initialize ("vagalume_sb_plugin",
                                              APP_VERSION, TRUE, NULL);

        if (!priv->osso_context) {
                g_debug ("Unable to initialize OSSO context");
                return FALSE;
        }

        result = osso_rpc_set_cb_f_with_free (priv->osso_context,
                                              SB_PLUGIN_DBUS_SERVICE,
                                              SB_PLUGIN_DBUS_OBJECT,
                                              SB_PLUGIN_DBUS_IFACE,
                                              dbus_req_handler,
                                              vsbp,
                                              osso_rpc_free_val);

        if (result != OSSO_OK) {
                g_debug ("Unable to set D-BUS callback");
                return FALSE;
        }

        return TRUE;
}

static void dbus_close (VagalumeSbPlugin *vsbp)
{
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);

        osso_deinitialize(priv->osso_context);
}

static gint
dbus_req_handler(const gchar* interface, const gchar* method,
                 GArray* arguments, gpointer data, osso_rpc_t* retval)
{
        VagalumeSbPlugin *vsbp = VAGALUME_SB_PLUGIN (data);

        g_debug("Received D-BUS message: %s", method);

        if (!strcmp (interface, SB_PLUGIN_DBUS_IFACE) &&
            !strcasecmp(method, SB_PLUGIN_DBUS_METHOD_NOTIFY)) {
                notify_handler (vsbp, arguments);
        }

        return OSSO_OK;
}

static void
dbus_send_request (VagalumeSbPlugin *vsbp, const gchar *request)
{
        /* Delegate on dbus_send_request_with_param() */
        dbus_send_request_with_param (vsbp, request, DBUS_TYPE_INVALID, NULL);
}

static void
dbus_send_request_with_param (VagalumeSbPlugin *vsbp,
                              const gchar *request,
                              int param_type,
                              gpointer param_value)
{
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);
        osso_return_t result;

        result = osso_rpc_async_run (priv->osso_context,
                                     APP_DBUS_SERVICE,
                                     APP_DBUS_OBJECT,
                                     APP_DBUS_IFACE,
                                     request,
                                     NULL,
                                     NULL,
                                     param_type,
                                     param_value,
                                     DBUS_TYPE_INVALID);
        if (result != OSSO_OK) {
                g_warning ("Error sending DBus message");
        }
}

static void
notify_handler (VagalumeSbPlugin *vsbp, GArray* arguments)
{
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);
        osso_rpc_t val;
        gchar *type = NULL;
        gboolean msg_handled = FALSE;

        /* Ensure you have appropiate arguments */
        g_return_if_fail (arguments->len > 0);

        /* Check notification type */
        val = g_array_index(arguments, osso_rpc_t, 0);
        type = g_strdup(val.value.s);

        /* Let's assume that an application is running
           if a message was received from Vagalume */
        priv->running_app = TRUE;

        if (!strcmp (type, SB_PLUGIN_DBUS_METHOD_NOTIFY_PLAYING)) {
                /* Retrieve arguments */

                gchar *artist = NULL;
                gchar *track = NULL;
                gchar *album = NULL;

                g_debug ("NOTIFY PLAYING RECEIVED");

                /* Artist */
                if (arguments->len > 1) {
                        val = g_array_index(arguments, osso_rpc_t, 1);
                        artist = g_strdup(val.value.s);
                }

                /* Track title */
                if (arguments->len > 2) {
                        val = g_array_index(arguments, osso_rpc_t, 2);
                        track = g_strdup(val.value.s);
                }

                /* Album */
                if (arguments->len > 3) {
                        val = g_array_index(arguments, osso_rpc_t, 3);
                        album = g_strdup(val.value.s);
                }

                /* Update private attributes */
                set_track_info (vsbp, artist, track, album);
                priv->now_playing = TRUE;

                msg_handled = TRUE;

                g_free (artist);
                g_free (track);
                g_free (album);
        } else if (!strcmp (type, SB_PLUGIN_DBUS_METHOD_NOTIFY_STOPPED)) {
                g_debug ("NOTIFY STOPPED RECEIVED");

                /* Update private attributes */
                set_track_info (vsbp, NULL, NULL, NULL);
                priv->now_playing = FALSE;

                msg_handled = TRUE;
        } else if (!strcmp (type, SB_PLUGIN_DBUS_METHOD_NOTIFY_STARTED)) {
                g_debug ("NOTIFY STARTED RECEIVED");

                /* Set the plugin invisible */
                set_visibility (vsbp, TRUE);
                dbus_send_request (vsbp, APP_DBUS_METHOD_REQUEST_STATUS);

                msg_handled = TRUE;
        } else if (!strcmp (type, SB_PLUGIN_DBUS_METHOD_NOTIFY_CLOSING)) {
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
                return;
        }

        g_free (type);

        /* If the message was handled update the panel */
        if (msg_handled) {
                main_panel_update (vsbp);
        }
}

/* Setters */

static void
set_visibility (VagalumeSbPlugin *vsbp, gboolean visible)
{
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);
        gboolean old_condition;

        g_object_get (vsbp, "condition", &old_condition, NULL);
        if (old_condition != visible)
                g_object_set (vsbp, "condition", visible, NULL);

        priv->is_visible = visible;

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
set_track_info (VagalumeSbPlugin *vsbp,
               const gchar *artist,
               const gchar *track,
               const gchar *album)
{
        g_return_if_fail(VAGALUME_IS_SB_PLUGIN(vsbp));

        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);

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
main_panel_create (VagalumeSbPlugin *vsbp)
{
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);
        GtkWidget *label;

        /* Create main_panel and main_panel items */
        priv->main_panel = gtk_menu_new ();
        gtk_widget_set_size_request (priv->main_panel, MAIN_PANEL_WIDTH, -1);

        /* Insensitive items */
        priv->artist_item = gtk_menu_item_new ();
        label = gtk_label_new (NULL);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_widget_modify_font (label, get_small_font (label));
        gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
        gtk_container_add (GTK_CONTAINER (priv->artist_item), label);
        gtk_widget_set_sensitive (priv->artist_item, FALSE);
        label->state = GTK_STATE_NORMAL;

        priv->track_item = gtk_menu_item_new ();
        label = gtk_label_new (NULL);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_widget_modify_font (label, get_small_font (label));
        gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
        gtk_container_add (GTK_CONTAINER (priv->track_item), label);
        gtk_widget_set_sensitive (priv->track_item, FALSE);
        label->state = GTK_STATE_NORMAL;

        priv->album_item = gtk_menu_item_new ();
        label = gtk_label_new (NULL);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_widget_modify_font (label, get_small_font (label));
        gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
        gtk_container_add (GTK_CONTAINER (priv->album_item), label);
        gtk_widget_set_sensitive (priv->album_item, FALSE);
        label->state = GTK_STATE_NORMAL;

        /* Active items */
        priv->play_item = gtk_menu_item_new_with_label (PLAY_ITEM_STRING);
        priv->stop_item = gtk_menu_item_new_with_label (STOP_ITEM_STRING);
        priv->next_item = gtk_menu_item_new_with_label (SKIP_ITEM_STRING);
        priv->love_item = gtk_menu_item_new_with_label (LOVE_ITEM_STRING);
        priv->ban_item = gtk_menu_item_new_with_label (BAN_ITEM_STRING);
        priv->open_vagalume_item =
                gtk_menu_item_new_with_label (SHOW_APP_ITEM_STRING);
        priv->close_vagalume_item =
                gtk_menu_item_new_with_label (CLOSE_APP_ITEM_STRING);

        /* Add items to main_panel */
        gtk_menu_append (priv->main_panel, priv->artist_item);
        gtk_menu_append (priv->main_panel, priv->track_item);
        gtk_menu_append (priv->main_panel, priv->album_item);
        gtk_menu_append (priv->main_panel, gtk_separator_menu_item_new ());
        gtk_menu_append (priv->main_panel, priv->play_item);
        gtk_menu_append (priv->main_panel, priv->stop_item);
        gtk_menu_append (priv->main_panel, priv->next_item);
        gtk_menu_append (priv->main_panel, priv->love_item);
        gtk_menu_append (priv->main_panel, priv->ban_item);
        gtk_menu_append (priv->main_panel, gtk_separator_menu_item_new ());
        gtk_menu_append (priv->main_panel, priv->open_vagalume_item);
        gtk_menu_append (priv->main_panel, priv->close_vagalume_item);

        /* Connect signals */
        priv->main_panel_handler_id =
                g_signal_connect(priv->main_panel,
                                 "selection-done",
                                 G_CALLBACK(main_panel_hidden), vsbp);
        priv->play_item_handler_id =
                g_signal_connect(priv->play_item, "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->stop_item_handler_id =
                g_signal_connect(priv->stop_item, "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->next_item_handler_id =
                g_signal_connect(priv->next_item, "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->love_item_handler_id =
                g_signal_connect(priv->love_item, "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->ban_item_handler_id =
                g_signal_connect(priv->ban_item, "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->open_vagalume_item_handler_id =
                g_signal_connect(priv->open_vagalume_item, "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);
        priv->close_vagalume_item_handler_id =
                g_signal_connect(priv->close_vagalume_item, "activate",
                                 G_CALLBACK (main_panel_item_activated), vsbp);

        /* Show widgets */
        gtk_widget_show_all (priv->main_panel);
}

static void
main_panel_update (VagalumeSbPlugin *vsbp)
{
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);
        GtkWidget *label;

        if (!priv->running_app) {
                /* Application closed */
                priv->is_visible = FALSE;
                priv->now_playing = FALSE;
        }

        /* Adjust sentitiveness for main_panel items and show/hide buttons */
        if (priv->now_playing) {
                gtk_widget_set_sensitive (priv->play_item, FALSE);
                gtk_widget_set_sensitive (priv->stop_item, TRUE);
                gtk_widget_set_sensitive (priv->next_item, TRUE);
                gtk_widget_set_sensitive (priv->love_item, TRUE);
                gtk_widget_set_sensitive (priv->ban_item, TRUE);

                gtk_widget_hide (priv->play_item);
                gtk_widget_show (priv->stop_item);
        } else {
                /* Not playing: set default strings */
                set_track_info (vsbp, NULL, NULL, NULL);

                gtk_widget_set_sensitive (priv->play_item, TRUE);
                gtk_widget_set_sensitive (priv->stop_item, FALSE);
                gtk_widget_set_sensitive (priv->next_item, FALSE);
                gtk_widget_set_sensitive (priv->love_item, FALSE);
                gtk_widget_set_sensitive (priv->ban_item, FALSE);

                gtk_widget_show (priv->play_item);
                gtk_widget_hide (priv->stop_item);
        }
        gtk_widget_show (priv->open_vagalume_item);
        gtk_widget_show (priv->close_vagalume_item);

        /* Detail labels */
        label = gtk_bin_get_child (GTK_BIN (priv->artist_item));
        gtk_label_set_markup (GTK_LABEL (label), priv->artist_string);

        label = gtk_bin_get_child (GTK_BIN (priv->track_item));
        gtk_label_set_markup (GTK_LABEL (label), priv->track_string);

        /* Show / hide the album label depending on its availability */
        if (priv->album_string != NULL) {
                label = gtk_bin_get_child (GTK_BIN (priv->album_item));
                gtk_label_set_markup (GTK_LABEL (label), priv->album_string);
                gtk_widget_show (priv->album_item);
        } else {
                gtk_widget_hide (priv->album_item);
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
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (data);

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
        VagalumeSbPlugin *vsbp = VAGALUME_SB_PLUGIN (data);
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);

        if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (button)))
                return;

        g_debug ("Plugin button toggled");

        gtk_menu_popup (GTK_MENU (priv->main_panel),
                        NULL, NULL,
                        main_panel_position_func, vsbp,
                        1,
                        gtk_get_current_event_time ());
}

static void
main_panel_item_activated (GtkWidget *item, gpointer data)
{
        VagalumeSbPlugin *vsbp = VAGALUME_SB_PLUGIN (data);
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);

        if (item == priv->play_item) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_PLAY);
                g_debug ("DBUS request sent: Play");
        } else if (item == priv->stop_item) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_STOP);
                g_debug ("DBUS request sent: Stop");
        } else if (item == priv->next_item) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_SKIP);
                g_debug ("DBUS request sent: Skip");
        } else if (item == priv->love_item) {
                dbus_send_request_with_param (vsbp,
                                              APP_DBUS_METHOD_LOVETRACK,
                                              DBUS_TYPE_BOOLEAN,
                                              GINT_TO_POINTER(TRUE));
                g_debug ("DBUS request sent: LoveTrack");
        } else if (item == priv->ban_item) {
                dbus_send_request_with_param (vsbp,
                                              APP_DBUS_METHOD_BANTRACK,
                                              DBUS_TYPE_BOOLEAN,
                                              GINT_TO_POINTER(TRUE));
                g_debug ("DBUS request sent: BanTrack");
        } else if (item == priv->open_vagalume_item) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_SHOWWINDOW);
                g_debug ("DBUS request sent: Show application");
        } else if (item == priv->close_vagalume_item) {
                dbus_send_request (vsbp, APP_DBUS_METHOD_CLOSEAPP);
                g_debug ("DBUS request sent: Close application");
        } else {
                g_debug ("Unknown action");
        }
}

static void
main_panel_hidden (GtkWidget *main_panel, gpointer data)
{
        VagalumeSbPlugin *vsbp = VAGALUME_SB_PLUGIN (data);
        VagalumeSbPluginPrivate *priv = VAGALUME_SB_PLUGIN_GET_PRIVATE (vsbp);

        g_debug ("Main panel hidden");

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->button), FALSE);
}


/* Utility functions */

static gboolean
vagalume_running (VagalumeSbPlugin *vsbp)
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
