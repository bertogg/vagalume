/*
 * uimisc-gtk.c -- GTK+ UI functions
 *
 * Copyright (C) 2007-2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include "uimisc.h"
#include "util.h"
#include "compat.h"

#include <glib/gi18n.h>
#include <string.h>

typedef struct {
        GtkDialog *dialog;
        GtkNotebook *nb;
        GtkEntry *user, *pw, *proxy, *dlentry;
        GtkComboBox *srvcombo;
        GtkWidget *dlbutton, *scrobble, *discovery, *useproxy, *lowbitrate;
        GtkWidget *dlfreetracks;
        GtkWidget *disableconfdiags;
        GtkWidget *helpbtn;
#ifdef SET_IM_STATUS
        GtkEntry *imtemplateentry;
        GtkWidget *impidgin, *imgajim, *imgossip, *imtelepathy;
#endif
#ifdef HAVE_TRAY_ICON
        GtkWidget *shownotifications;
        GtkWidget *closetosystray;
#endif
} usercfgwin;

typedef struct {
        VglObject parent;
        GtkWindow *window;
        GtkEntry *entry;
        GtkComboBox *selcombo;
        GtkComboBox *globalcombo;
        LastfmTrack *track;
        LastfmWsSession *ws_session;
        char *user;
        char *tags_artist;
        char *tags_track;
        char *tags_album;
        GtkTreeModel *poptags_artist;
        GtkTreeModel *poptags_track;
        GtkTreeModel *poptags_album;
        GtkTreeModel *nonemodel;
        GtkTreeModel *retrmodel;
        TagComboState artist_state, track_state, album_state;
} tagwin;

typedef struct {
        tagwin *w;
        LastfmTrackComponent type;
        GList *userlist;
        GList *globallist;
} GetTrackTagsData;

enum {
        ARTIST_TRACK_ALBUM_TYPE = 0,
        ARTIST_TRACK_ALBUM_TEXT,
        ARTIST_TRACK_ALBUM_NCOLS
};

static void
tagwin_selcombo_changed                 (GtkComboBox *combo,
                                         gpointer     data);

static GtkWidget *
ui_create_combo_box_entry               (const GList *elems)
{
        GtkWidget *combo;
        GtkEntryCompletion *completion;
        GtkTreeModel *model;

        model = ui_create_options_list (elems);
        combo = gtk_combo_box_entry_new_with_model (model, 0);

        completion = gtk_entry_completion_new ();
        gtk_entry_completion_set_model (completion, model);
        gtk_entry_completion_set_text_column (completion, 0);
#ifdef HAVE_ENTRY_COMPLETION_INLINE_SELECTION
        gtk_entry_completion_set_inline_selection (completion, TRUE);
#endif

        gtk_entry_set_completion (
                GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo))), completion);

        g_object_unref (model);
        g_object_unref (completion);

        return combo;
}

void
ui_usertag_dialog                       (GtkWindow    *parent,
                                         char        **user,
                                         char        **tag,
                                         const GList  *userlist)
{
        GtkDialog *dialog;
        GtkWidget *userlabel, *taglabel;
        GtkWidget *usercombo, *tagentry;
        GtkTable *table;

        g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
        g_return_if_fail (user != NULL && tag != NULL);

        dialog = ui_base_dialog (parent, _("User tag radio"));
        userlabel = gtk_label_new (_("Username:"));
        taglabel = gtk_label_new (_("Tag:"));
        usercombo = ui_create_combo_box_entry (userlist);
        tagentry = gtk_entry_new ();
        table = GTK_TABLE (gtk_table_new (2, 2, FALSE));

        gtk_table_attach_defaults (table, userlabel, 0, 1, 0, 1);
        gtk_table_attach_defaults (table, taglabel, 0, 1, 1, 2);
        gtk_table_attach_defaults (table, usercombo, 1, 2, 0, 1);
        gtk_table_attach_defaults (table, tagentry, 1, 2, 1, 2);
        gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area(dialog)),
                           GTK_WIDGET (table));

        gtk_entry_set_activates_default (
                GTK_ENTRY (GTK_BIN (usercombo)->child), TRUE);
        gtk_entry_set_activates_default (GTK_ENTRY (tagentry), TRUE);
        gtk_misc_set_alignment (GTK_MISC (userlabel), 0, 0.5);
        gtk_misc_set_alignment (GTK_MISC (taglabel), 0, 0.5);
        gtk_table_set_col_spacing (table, 0, 10);
        gtk_table_set_row_spacing (table, 0, 5);
        gtk_container_set_border_width (GTK_CONTAINER (table), 10);

        if (*user != NULL) {
                gtk_entry_set_text (
                        GTK_ENTRY (GTK_BIN (usercombo)->child), *user);
                g_free (*user);
        }
        if (*tag != NULL) {
                gtk_entry_set_text (GTK_ENTRY (tagentry), *tag);
                g_free (*tag);
        }
        *user = *tag = NULL;

        gtk_widget_show_all (GTK_WIDGET (dialog));

        if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
                GtkEntry *entry = GTK_ENTRY (GTK_BIN (usercombo)->child);
                *user = g_strstrip (g_strdup (gtk_entry_get_text (entry)));
                *tag = g_strstrip (g_strdup (gtk_entry_get_text (
                                                     GTK_ENTRY (tagentry))));
        }
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

typedef struct {
        GtkWindow *win;
        GtkEntry *entry;
} change_dir_selected_data;

static void
change_dir_selected                     (GtkWidget *widget,
                                         gpointer   data)
{
        change_dir_selected_data *d = (change_dir_selected_data *) data;
        g_return_if_fail(d != NULL && GTK_IS_ENTRY(d->entry));
        char *dir;
        dir = ui_select_download_dir(d->win, gtk_entry_get_text(d->entry));
        if (dir != NULL) {
                gtk_entry_set_text(d->entry, dir);
                g_free(dir);
        }
}

static GtkComboBox *
usercfg_create_server_combobox          (VglUserCfg *cfg)
{
        GtkComboBox *combo;
        const GList *srvlist, *iter;
        int i;

        g_return_val_if_fail (cfg != NULL, NULL);

        combo = GTK_COMBO_BOX (gtk_combo_box_new_text ());

        srvlist = vgl_server_list_get ();
        for (iter = srvlist, i = 0; iter != NULL; iter = iter->next, i++) {
                const VglServer *srv = iter->data;
                gtk_combo_box_append_text (combo, srv->name);
                if (srv == cfg->server) {
                        gtk_combo_box_set_active (combo, i);
                }

        }

        return combo;
}

static void
usercfg_help_button_clicked             (GtkButton  *button,
                                         usercfgwin *win)
{
        g_return_if_fail(win != NULL);
        int page = gtk_notebook_get_current_page(win->nb);
        GtkWidget *w = gtk_notebook_get_nth_page(win->nb, page);
        const char *help = g_object_get_data(G_OBJECT(w), "help-message");
        ui_info_dialog(GTK_WINDOW(win->dialog),
                       help ? help : _("No help available"));
}

static void
usercfg_user_pw_modified                (GObject    *obj,
                                         GParamSpec *arg,
                                         usercfgwin *win)
{
        const char *user = gtk_entry_get_text(win->user);
        const char *pw = gtk_entry_get_text(win->pw);
        gboolean userpw = (user && pw && strlen(user) > 0 && strlen(pw) > 0);
        gtk_dialog_set_response_sensitive(win->dialog,
                                          GTK_RESPONSE_ACCEPT, userpw);
}

static void
usercfg_add_account_settings            (usercfgwin *win,
                                         VglUserCfg *cfg)
{
        g_return_if_fail(win != NULL && GTK_IS_NOTEBOOK(win->nb));
        GtkTable *table;
        GtkWidget *userlabel, *pwlabel, *srvlabel, *scroblabel, *discovlabel;
        const char *help;

        /* Create widgets */
        table = GTK_TABLE (gtk_table_new (4, 2, FALSE));
        userlabel = gtk_label_new(_("Username:"));
        pwlabel = gtk_label_new(_("Password:"));
        srvlabel = gtk_label_new(_("Service:"));
        scroblabel = gtk_label_new(_("Enable scrobbling:"));
        discovlabel = gtk_label_new(_("Discovery mode:"));
        win->user = GTK_ENTRY(gtk_entry_new());
        win->pw = GTK_ENTRY(gtk_entry_new());
        win->srvcombo = usercfg_create_server_combobox (cfg);
        win->scrobble = gtk_check_button_new();
        win->discovery = gtk_check_button_new();

        /* Set widget properties */
#ifdef MAEMO
        /* This disables the automatic capitalization */
        hildon_gtk_entry_set_input_mode(win->pw, HILDON_GTK_INPUT_MODE_FULL);
#endif
        gtk_entry_set_visibility(win->pw, FALSE);
        gtk_entry_set_activates_default(win->user, TRUE);
        gtk_entry_set_activates_default(win->pw, TRUE);

        g_signal_connect(G_OBJECT(win->user), "notify::text",
                         G_CALLBACK(usercfg_user_pw_modified), win);
        g_signal_connect(G_OBJECT(win->pw), "notify::text",
                         G_CALLBACK(usercfg_user_pw_modified), win);

        /* Set initial values */
        gtk_entry_set_text(win->user, cfg->username);
        gtk_entry_set_text(win->pw, cfg->password);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->scrobble),
                                     cfg->enable_scrobbling);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->discovery),
                                     cfg->discovery_mode);

        /* Set the initial state of the OK button */
        usercfg_user_pw_modified(NULL, NULL, win);

        /* Pack widgets */
        gtk_table_attach(table, userlabel, 0, 1, 0, 1, 0, 0, 5, 5);
        gtk_table_attach(table, pwlabel, 0, 1, 1, 2, 0, 0, 5, 5);
        gtk_table_attach(table, srvlabel, 0, 1, 2, 3, 0, 0, 5, 5);
        gtk_table_attach(table, scroblabel, 0, 1, 3, 4, 0, 0, 5, 5);
        gtk_table_attach(table, discovlabel, 0, 1, 4, 5, 0, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET(win->user), 1, 2, 0, 1,
                         GTK_EXPAND | GTK_FILL, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET(win->pw), 1, 2, 1, 2,
                         GTK_EXPAND | GTK_FILL, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET (win->srvcombo),
                         1, 2, 2, 3, 0, 0, 5, 5);
        gtk_table_attach(table, win->scrobble, 1, 2, 3, 4, 0, 0, 5, 5);
        gtk_table_attach(table, win->discovery, 1, 2, 4, 5, 0, 0, 5, 5);
        gtk_notebook_append_page(win->nb, GTK_WIDGET(table),
                                 gtk_label_new(_("Account")));

        /* Set help */
        help = _("* Username and password:\nFrom your account.\n\n"
                 "* Service:\nLast.fm-compatible service to connect to.\n\n"
                 "* Scrobbling:\nDisplay the music that you listen to on "
                 "your profile.\n\n"
                 "* Discovery mode:\nDon't play music you've already listened "
                 "to. Requires a Last.fm subscription.");
        g_object_set_data(G_OBJECT(table), "help-message", (gpointer) help);
}

static void
usercfg_add_connection_settings         (usercfgwin *win,
                                         VglUserCfg *cfg)
{
        g_return_if_fail(win != NULL && GTK_IS_NOTEBOOK(win->nb));
        GtkTable *table;
        GtkWidget *useproxylabel, *proxylabel, *lowbitratelabel;
        const char *help;

        /* Create widgets */
        table = GTK_TABLE (gtk_table_new (3, 2, FALSE));
        useproxylabel = gtk_label_new(_("Use HTTP proxy"));
        proxylabel = gtk_label_new(_("Proxy address:"));
        lowbitratelabel = gtk_label_new(_("Low bitrate stream"));
        win->proxy = GTK_ENTRY(gtk_entry_new());
        win->useproxy = gtk_check_button_new();
        win->lowbitrate = gtk_check_button_new();

        /* Set widget properties */
        gtk_entry_set_activates_default(win->proxy, TRUE);

        /* Set initial values */
        gtk_entry_set_text(win->proxy, cfg->http_proxy);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->useproxy),
                                     cfg->use_proxy);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->lowbitrate),
                                     cfg->low_bitrate);

        /* Pack widgets */
        gtk_table_attach(table, useproxylabel, 0, 1, 0, 1, 0, 0, 5, 5);
        gtk_table_attach(table, proxylabel, 0, 1, 1, 2, 0, 0, 5, 5);
        gtk_table_attach(table, lowbitratelabel, 0, 1, 2, 3, 0, 0, 5, 5);
        gtk_table_attach(table, win->useproxy, 1, 2, 0, 1, 0, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET(win->proxy), 1, 2, 1, 2,
                         GTK_EXPAND | GTK_FILL, 0, 5, 5);
        gtk_table_attach(table, win->lowbitrate, 1, 2, 2, 3, 0, 0, 5, 5);
        gtk_notebook_append_page(win->nb, GTK_WIDGET(table),
                                 gtk_label_new(_("Connection")));

        /* Set help */
        help = _("* Use HTTP proxy:\nEnable this to use an HTTP proxy.\n\n"
                 "* Proxy address:\nuser:password@hostname:port\n"
                 "Only the hostname (can be an IP address) is required.\n\n"
                 "* Low bitrate stream: Try to get a low bitrate stream\n"
                 "from the server to save bandwidth.");
        g_object_set_data(G_OBJECT(table), "help-message", (gpointer) help);
}

static void
usercfg_add_download_settings           (usercfgwin *win,
                                         VglUserCfg *cfg)
{
        g_return_if_fail(win != NULL && GTK_IS_NOTEBOOK(win->nb));
        GtkTable *table;
        GtkWidget *dllabel, *autodllabel;
        const char *help;

        /* Create widgets */
        table = GTK_TABLE (gtk_table_new (3, 3, FALSE));
        dllabel = gtk_label_new(_("Select download directory"));
        autodllabel = gtk_label_new (_("Automatically download free tracks"));
        win->dlbutton = compat_gtk_button_new();
        win->dlentry = GTK_ENTRY(gtk_entry_new());
        win->dlfreetracks = gtk_check_button_new ();

        /* Set widget properties */
        gtk_button_set_image(GTK_BUTTON(win->dlbutton),
                             gtk_image_new_from_stock(GTK_STOCK_DIRECTORY,
                                                      GTK_ICON_SIZE_BUTTON));

        /* Set initial values */
        gtk_entry_set_text(win->dlentry, cfg->download_dir);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (win->dlfreetracks),
                                      cfg->autodl_free_tracks);

        /* Pack widgets */
        gtk_table_attach (table, dllabel, 0, 3, 0, 1, 0, 0, 5, 5);
        gtk_table_attach (table, GTK_WIDGET (win->dlentry), 0, 2, 1, 2,
                          GTK_EXPAND | GTK_FILL, 0, 5, 5);
        gtk_table_attach (table, win->dlbutton, 2, 3, 1, 2, 0, 0, 5, 5);
        gtk_table_attach (table, autodllabel, 0, 1, 2, 3, 0, 0, 5, 5);
        gtk_table_attach (table, win->dlfreetracks, 1, 3, 2, 3, 0, 0, 5, 5);
        gtk_notebook_append_page(win->nb, GTK_WIDGET(table),
                                 gtk_label_new(_("Download")));

        /* Set help */
        help = _("* Download directory:\nWhere to download mp3 files. "
                 "Note that you can only download those songs "
                 "marked as freely downloadable by the server.");
        g_object_set_data(G_OBJECT(table), "help-message", (gpointer) help);
}

static void
usercfg_add_imstatus_settings           (usercfgwin *win,
                                         VglUserCfg *cfg)
{
#ifdef SET_IM_STATUS
        g_return_if_fail(win != NULL && GTK_IS_NOTEBOOK(win->nb));
        GtkTable *table;
        GtkWidget *impidginlabel, *imgajimlabel, *imgossiplabel;
        GtkWidget *imtemplatelabel, *imtelepathylabel;
        const char *help;

        /* Create widgets */
        table = GTK_TABLE (gtk_table_new (5, 2, FALSE));
        imtemplatelabel = gtk_label_new(_("IM message template:"));
        impidginlabel = gtk_label_new(_("Update Pidgin status:"));
        imgajimlabel = gtk_label_new(_("Update Gajim status:"));
        imgossiplabel = gtk_label_new(_("Update Gossip status:"));
        imtelepathylabel = gtk_label_new(_("Update Telepathy status:"));
        win->imtemplateentry = GTK_ENTRY(gtk_entry_new());
        win->impidgin = gtk_check_button_new();
        win->imgajim = gtk_check_button_new();
        win->imgossip = gtk_check_button_new();
        win->imtelepathy = gtk_check_button_new();

        /* Set initial values */
        gtk_entry_set_text(win->imtemplateentry, cfg->imstatus_template);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->impidgin),
                                     cfg->im_pidgin);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->imgajim),
                                     cfg->im_gajim);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->imgossip),
                                     cfg->im_gossip);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->imtelepathy),
                                     cfg->im_telepathy);

        /* Pack widgets */
        gtk_table_attach(table, imtemplatelabel, 0, 1, 0, 1, 0, 0, 5, 5);
        gtk_table_attach(table, impidginlabel, 0, 1, 1, 2, 0, 0, 5, 5);
        gtk_table_attach(table, imgajimlabel, 0, 1, 2, 3, 0, 0, 5, 5);
        gtk_table_attach(table, imgossiplabel, 0, 1, 3, 4, 0, 0, 5, 5);
        gtk_table_attach(table, imtelepathylabel, 0, 1, 4, 5, 0, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET(win->imtemplateentry), 1, 2, 0, 1,
                         GTK_EXPAND | GTK_FILL, 0, 5, 5);
        gtk_table_attach(table, win->impidgin, 1, 2, 1, 2, 0, 0, 5, 5);
        gtk_table_attach(table, win->imgajim, 1, 2, 2, 3, 0, 0, 5, 5);
        gtk_table_attach(table, win->imgossip, 1, 2, 3, 4, 0, 0, 5, 5);
        gtk_table_attach(table, win->imtelepathy, 1, 2, 4, 5, 0, 0, 5, 5);
        gtk_notebook_append_page(win->nb, GTK_WIDGET(table),
                                 gtk_label_new(_("Update IM status")));

        /* Set help */
        help = _("Vagalume can update the status message of your IM client "
                 "each time a new song starts. The template can contain the "
                 "following keywords:\n\n"
                 "{artist}: Name of the artist\n"
                 "{title}: Song title\n"
                 "{station}: Radio station\n"
                 "{version}: Vagalume version");
        g_object_set_data(G_OBJECT(table), "help-message", (gpointer) help);
#endif
}

static void
usercfg_add_misc_settings               (usercfgwin *win,
                                         VglUserCfg *cfg)
{
        g_return_if_fail(win != NULL && GTK_IS_NOTEBOOK(win->nb));
        GtkTable *table;
        GtkWidget *disableconfdiagslabel;
        GString *help = g_string_sized_new(100);
#ifdef HAVE_TRAY_ICON
        GtkWidget *shownotificationslabel;
        GtkWidget *closetosystraylabel;

        table = GTK_TABLE (gtk_table_new (3, 2, FALSE));
#else
        table = GTK_TABLE (gtk_table_new (1, 2, FALSE));
#endif

        /* Disable confirm dialogs */
        disableconfdiagslabel =
                gtk_label_new(_("Disable confirmation dialogs:"));

        win->disableconfdiags = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->disableconfdiags),
                                     cfg->disable_confirm_dialogs);

        gtk_table_attach(table, disableconfdiagslabel, 0, 1, 0, 1, 0, 0, 5, 5);
        gtk_table_attach(table, win->disableconfdiags, 1, 2, 0, 1, 0, 0, 5, 5);

        g_string_append(help,
                        _("* Disable confirmation dialogs:\n"
                          "Don't ask for confirmation when "
                          "loving/banning tracks."));

#ifdef HAVE_TRAY_ICON
        /* Show playback notifications */
        shownotificationslabel =
                gtk_label_new(_("Show playback notifications:"));

        win->shownotifications = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->shownotifications),
                                     cfg->show_notifications);

        gtk_table_attach(table, shownotificationslabel, 0, 1, 1, 2, 0, 0, 5, 5);
        gtk_table_attach(table, win->shownotifications, 1, 2, 1, 2, 0, 0, 5, 5);

        g_string_append(help,
                        _("\n\n* Show playback notifications:\n"
                          "Pop up a notification box each time "
                          "a new song starts.\n\n"));

        /* Close window to systray */
        closetosystraylabel =
                gtk_label_new(_("Close to systray:"));

        win->closetosystray = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->closetosystray),
                                     cfg->close_to_systray);

        gtk_table_attach(table, closetosystraylabel, 0, 1, 2, 3, 0, 0, 5, 5);
        gtk_table_attach(table, win->closetosystray, 1, 2, 2, 3, 0, 0, 5, 5);

        g_string_append(help,
                        _("* Close to systray:\n"
                          "Hide the Vagalume window when "
                          "the close button is pressed."));
#endif

        /* Add page to notebook */
        gtk_notebook_append_page(win->nb, GTK_WIDGET(table),
                                 gtk_label_new(_("Misc")));

        /* Set help */
        g_object_set_data_full(G_OBJECT(table), "help-message",
                               g_string_free(help, FALSE), g_free);
}

gboolean
ui_usercfg_window                       (GtkWindow   *parent,
                                         VglUserCfg **cfg)
{
        g_return_val_if_fail(cfg != NULL, FALSE);
        change_dir_selected_data *windata;
        gboolean changed = FALSE;
        usercfgwin win;
        const VglUserCfg *origcfg = *cfg;

        if (*cfg == NULL) *cfg = vgl_user_cfg_new();
        memset (&win, 0, sizeof(win));
        win.dialog = ui_base_dialog(parent, _("User settings"));
        win.nb = GTK_NOTEBOOK(gtk_notebook_new());
        win.helpbtn = gtk_button_new_from_stock(GTK_STOCK_HELP);
        gtk_box_pack_start (GTK_BOX (gtk_dialog_get_action_area (win.dialog)),
                            win.helpbtn, FALSE, FALSE, 0);
        gtk_button_box_set_child_secondary (
                GTK_BUTTON_BOX (gtk_dialog_get_action_area (win.dialog)),
                win.helpbtn, TRUE);

        usercfg_add_account_settings(&win, *cfg);
        usercfg_add_connection_settings(&win, *cfg);
        usercfg_add_download_settings(&win, *cfg);
        usercfg_add_imstatus_settings(&win, *cfg);
        usercfg_add_misc_settings(&win, *cfg);

        gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (win.dialog)),
                            GTK_WIDGET (win.nb), FALSE, FALSE, 10);

        windata = g_slice_new(change_dir_selected_data);
        windata->win = GTK_WINDOW(win.dialog);
        windata->entry = GTK_ENTRY(win.dlentry);

        g_signal_connect(G_OBJECT(win.dlbutton), "clicked",
                         G_CALLBACK(change_dir_selected), windata);
        g_signal_connect(G_OBJECT(win.helpbtn), "clicked",
                         G_CALLBACK(usercfg_help_button_clicked), &win);

        gtk_widget_show_all(GTK_WIDGET(win.dialog));
        if (gtk_dialog_run(win.dialog) == GTK_RESPONSE_ACCEPT) {
                vgl_user_cfg_set_username(*cfg, gtk_entry_get_text(win.user));
                vgl_user_cfg_set_password(*cfg, gtk_entry_get_text(win.pw));
                vgl_user_cfg_set_http_proxy(*cfg, gtk_entry_get_text(win.proxy));
                vgl_user_cfg_set_download_dir(*cfg,
                                                gtk_entry_get_text(win.dlentry));
                vgl_user_cfg_set_server_name(
                        *cfg, gtk_combo_box_get_active_text (win.srvcombo));
                (*cfg)->enable_scrobbling = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.scrobble));
                (*cfg)->discovery_mode = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.discovery));
                (*cfg)->use_proxy = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.useproxy));
                (*cfg)->low_bitrate = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.lowbitrate));
                (*cfg)->autodl_free_tracks = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.dlfreetracks));
#ifdef SET_IM_STATUS
                vgl_user_cfg_set_imstatus_template(*cfg,
                                                     gtk_entry_get_text(win.imtemplateentry));
                (*cfg)->im_pidgin = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.impidgin));
                (*cfg)->im_gajim = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.imgajim));
                (*cfg)->im_gossip = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.imgossip));
                (*cfg)->im_telepathy = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.imtelepathy));
#endif
                (*cfg)->disable_confirm_dialogs = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.disableconfdiags));
#ifdef HAVE_TRAY_ICON
                (*cfg)->show_notifications = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.shownotifications));
                (*cfg)->close_to_systray = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(win.closetosystray));
#endif
                changed = TRUE;
        } else if (origcfg == NULL) {
                /* If settings haven't been modified, restore the
                   original pointer if it was NULL */
                vgl_user_cfg_destroy(*cfg);
                *cfg = NULL;
        }
        gtk_widget_destroy(GTK_WIDGET(win.dialog));
        g_slice_free(change_dir_selected_data, windata);
        return changed;
}

char *
ui_input_dialog_with_list               (GtkWindow   *parent,
                                         const char  *title,
                                         const char  *text,
                                         const GList *elems,
                                         const char  *value)
{
        GtkDialog *dialog;
        GtkWidget *label;
        GtkWidget *combo;
        GtkBox *vbox;
        char *retvalue = NULL;
        dialog = ui_base_dialog(parent, title);
        vbox = GTK_BOX (gtk_dialog_get_content_area (dialog));
        label = gtk_label_new(text);
        combo = ui_create_combo_box_entry(elems);
        gtk_box_pack_start (vbox, label, FALSE, FALSE, 10);
        gtk_box_pack_start (vbox, combo, FALSE, FALSE, 10);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (value != NULL) {
                gtk_entry_set_text(GTK_ENTRY(GTK_BIN(combo)->child), value);
        }
        gtk_entry_set_activates_default(GTK_ENTRY(GTK_BIN(combo)->child),
                                        TRUE);
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                GtkEntry *entry = GTK_ENTRY(GTK_BIN(combo)->child);
                retvalue = g_strstrip(g_strdup(gtk_entry_get_text(entry)));
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return retvalue;
}

static GtkComboBox *
artist_track_album_selection_combo      (const LastfmTrack *t)
{
        char *text;
        GtkWidget *combo;
        GtkCellRenderer *renderer;
        GtkTreeIter iter;
        GtkListStore *store;

        store = gtk_list_store_new(ARTIST_TRACK_ALBUM_NCOLS,
                                   G_TYPE_INT, G_TYPE_STRING);

        text = g_strconcat(_("Artist: "), t->artist, NULL);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           ARTIST_TRACK_ALBUM_TYPE,
                           LASTFM_TRACK_COMPONENT_ARTIST,
                           ARTIST_TRACK_ALBUM_TEXT, text, -1);
        g_free(text);

        text = g_strconcat(_("Track: "), t->title, NULL);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           ARTIST_TRACK_ALBUM_TYPE,
                           LASTFM_TRACK_COMPONENT_TRACK,
                           ARTIST_TRACK_ALBUM_TEXT, text, -1);
        g_free(text);

        if (t->album[0] != '\0') {
                text = g_strconcat(_("Album: "), t->album, NULL);
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter,
                                   ARTIST_TRACK_ALBUM_TYPE,
                                   LASTFM_TRACK_COMPONENT_ALBUM,
                                   ARTIST_TRACK_ALBUM_TEXT, text, -1);
                g_free(text);
        }
        combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
        g_object_unref (store);
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END,
                     "ellipsize-set", TRUE, "width", 300, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, FALSE);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo),
                                       renderer, "text", 1, NULL);

        return GTK_COMBO_BOX(combo);
}

static LastfmTrackComponent
artist_track_album_combo_get_selected   (GtkComboBox *combo)
{
        g_return_val_if_fail(combo != NULL, 0);
        GtkTreeModel *model;
        GtkTreeIter iter;
        LastfmTrackComponent type;
        model = gtk_combo_box_get_model(combo);
        gtk_combo_box_get_active_iter(combo, &iter);
        gtk_tree_model_get(model, &iter, ARTIST_TRACK_ALBUM_TYPE, &type, -1);
        return type;
}

gboolean
recommwin_run                           (GtkWindow             *parent,
                                         char                 **user,
                                         char                 **message,
                                         const GList           *friends,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent  *type)
{
        g_return_val_if_fail(user && message && track && type, FALSE);
        gboolean retval = FALSE;
        GtkDialog *dialog;
        GtkBox *selbox, *userbox, *vbox;
        GtkWidget *sellabel;
        GtkComboBox *selcombo;
        GtkWidget *userlabel, *usercombo;
        GtkWidget *textframe, *textlabel;
        GtkTextView *textview;
        GtkWidget *sw;

        /* Dialog and basic settings */
        dialog = ui_base_dialog(parent, _("Send a recommendation"));
        vbox = GTK_BOX (gtk_dialog_get_content_area (dialog));
        gtk_box_set_homogeneous (vbox, FALSE);
        gtk_box_set_spacing (vbox, 5);

        /* Combo to select what to recommend */
        selbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        sellabel = gtk_label_new(_("Recommend this"));
        selcombo = artist_track_album_selection_combo(track);
        gtk_box_pack_start(selbox, sellabel, FALSE, FALSE, 0);
        gtk_box_pack_start(selbox, GTK_WIDGET(selcombo), TRUE, TRUE, 0);

        switch (*type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                gtk_combo_box_set_active(selcombo, 0);
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                gtk_combo_box_set_active(selcombo, 1);
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_val_if_fail(track->album[0] != '\0', FALSE);
                gtk_combo_box_set_active(selcombo, 2);
                break;
        default:
                g_return_val_if_reached(FALSE);
                break;
        }

        /* Combo to select the recipient of the recommendation */
        userbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        userlabel = gtk_label_new(_("Send recommendation to"));
        usercombo = ui_create_combo_box_entry(friends);
        gtk_entry_set_activates_default(GTK_ENTRY(GTK_BIN(usercombo)->child),
                                        TRUE);
        gtk_box_pack_start(userbox, userlabel, FALSE, FALSE, 0);
        gtk_box_pack_start(userbox, usercombo, TRUE, TRUE, 0);

        /* Message of the recommendation */
        textlabel = gtk_label_new(_("Recommendation message"));
        textframe = gtk_frame_new(NULL);
        sw = gtk_scrolled_window_new(NULL, NULL);
        textview = GTK_TEXT_VIEW(gtk_text_view_new());
        g_object_set(textframe, "height-request", 50, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_text_view_set_accepts_tab(textview, FALSE);
        gtk_text_view_set_wrap_mode(textview, GTK_WRAP_WORD_CHAR);
        gtk_container_add(GTK_CONTAINER(textframe), GTK_WIDGET(sw));
        gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(textview));

        /* Widget packing */
        gtk_box_pack_start (vbox, GTK_WIDGET (selbox), FALSE, FALSE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (userbox), FALSE, FALSE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (textlabel), TRUE, TRUE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (textframe), TRUE, TRUE, 0);

        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                GtkEntry *entry = GTK_ENTRY(GTK_BIN(usercombo)->child);
                GtkTextIter start, end;
                GtkTextBuffer *buf = gtk_text_view_get_buffer(textview);
                gtk_text_buffer_get_bounds(buf, &start, &end);
                *message = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
                *type = artist_track_album_combo_get_selected(selcombo);
                *user = g_strstrip(g_strdup(gtk_entry_get_text(entry)));
                retval = TRUE;
        } else {
                *message = NULL;
                *user = NULL;
                retval = FALSE;
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return retval;
}

static void
tagwin_destroy                          (tagwin *w)
{
        gtk_widget_destroy(GTK_WIDGET(w->window));
        vgl_object_unref(w->track);
        vgl_object_unref(w->ws_session);
        g_free(w->user);
        g_free(w->tags_artist);
        g_free(w->tags_track);
        g_free(w->tags_album);
        if (w->poptags_artist) g_object_unref(w->poptags_artist);
        if (w->poptags_track) g_object_unref(w->poptags_track);
        if (w->poptags_album) g_object_unref(w->poptags_album);
        if (w->nonemodel) g_object_unref(w->nonemodel);
        if (w->retrmodel) g_object_unref(w->retrmodel);
}

static tagwin *
tagwin_create                           (void)
{
        return vgl_object_new (tagwin, (GDestroyNotify) tagwin_destroy);
}

static gboolean
get_track_tags_idle                     (gpointer userdata)
{
        GetTrackTagsData *data = userdata;
        GtkTreeModel *model = NULL;
        char *usertags = str_glist_join (data->userlist, ", ");

        if (data->globallist != NULL) {
                model = ui_create_options_list (data->globallist);
        }
        switch (data->type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                data->w->poptags_artist = model;
                data->w->tags_artist = usertags;
                data->w->artist_state = TAGCOMBO_STATE_READY;
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                data->w->poptags_track = model;
                data->w->tags_track = usertags;
                data->w->track_state = TAGCOMBO_STATE_READY;
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                data->w->poptags_album = model;
                data->w->tags_album = usertags;
                data->w->album_state = TAGCOMBO_STATE_READY;
                break;
        default:
                g_return_val_if_reached (FALSE);
        }

        tagwin_selcombo_changed (data->w->selcombo, data->w);

        g_list_foreach (data->userlist, (GFunc) g_free, NULL);
        g_list_free (data->userlist);
        g_list_foreach (data->globallist, (GFunc) g_free, NULL);
        g_list_free (data->globallist);
        vgl_object_unref (data->w);
        g_slice_free (GetTrackTagsData, data);

        return FALSE;
}

static gpointer
get_track_tags_thread                   (gpointer userdata)
{
        GetTrackTagsData *data = userdata;

        g_return_val_if_fail (data && data->w && data->w->track, NULL);

        lastfm_ws_get_user_track_tags(data->w->ws_session, data->w->track,
                                      data->type, &(data->userlist));
        lastfm_ws_get_track_tags (data->w->ws_session, data->w->track,
                                  data->type, &(data->globallist));

        gdk_threads_add_idle (get_track_tags_idle, data);

        return NULL;
}

static void
tagwin_selcombo_changed                 (GtkComboBox *combo,
                                         gpointer     data)
{
        tagwin *w = (tagwin *) data;
        GtkTreeModel *model = NULL;
        const char *usertags = NULL;
        TagComboState oldstate;
        LastfmTrackComponent type;
        type = artist_track_album_combo_get_selected (combo);
        switch (type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                oldstate = w->artist_state;
                if (oldstate == TAGCOMBO_STATE_READY) {
                        model = w->poptags_artist;
                        usertags = w->tags_artist;
                } else {
                        w->artist_state = TAGCOMBO_STATE_LOADING;
                }
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                oldstate = w->track_state;
                if (oldstate == TAGCOMBO_STATE_READY) {
                        model = w->poptags_track;
                        usertags = w->tags_track;
                } else {
                        w->track_state = TAGCOMBO_STATE_LOADING;
                }
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_if_fail(w->track->album[0] != '\0');
                oldstate = w->album_state;
                if (oldstate == TAGCOMBO_STATE_READY) {
                        model = w->poptags_album;
                        usertags = w->tags_album;
                } else {
                        w->album_state = TAGCOMBO_STATE_LOADING;
                }
                break;
        default:
                g_return_if_reached();
                break;
        }
        gtk_widget_set_sensitive(GTK_WIDGET(w->globalcombo), model != NULL);
        if (oldstate == TAGCOMBO_STATE_READY) {
                if (model != NULL) {
                        gtk_combo_box_set_model(w->globalcombo, model);
                } else {
                        gtk_combo_box_set_model(w->globalcombo, w->nonemodel);
                        gtk_combo_box_set_active(w->globalcombo, 0);
                }
                gtk_widget_set_sensitive(GTK_WIDGET(w->entry), TRUE);
                gtk_entry_set_text(w->entry, usertags ? usertags : "");
        } else {
                gtk_combo_box_set_model(w->globalcombo, w->retrmodel);
                gtk_combo_box_set_active(w->globalcombo, 0);
                gtk_widget_set_sensitive(GTK_WIDGET(w->entry), FALSE);
                gtk_entry_set_text(w->entry, _("retrieving..."));
        }
        if (oldstate == TAGCOMBO_STATE_NULL) {
                GetTrackTagsData *data = g_slice_new (GetTrackTagsData);
                data->w = vgl_object_ref (w);
                data->type = type;
                data->userlist = data->globallist = NULL;
                g_thread_create(get_track_tags_thread, data, FALSE, NULL);
        }
}

static void
tagwin_tagcombo_changed                 (GtkComboBox *combo,
                                         gpointer     data)
{
        if (gtk_combo_box_get_active(combo) == -1 ||
            !GTK_WIDGET_IS_SENSITIVE(combo)) return;
        tagwin *w = (tagwin *) data;
        g_return_if_fail(w != NULL && w->entry != NULL);
        char *current = g_strchomp(g_strdup(gtk_entry_get_text(w->entry)));
        const char *selected = gtk_combo_box_get_active_text(combo);
        char *new;
        if (current[0] == '\0') {
                new = g_strdup(selected);
        } else {
                int len = strlen(current);
                if (current[len-1] == ',') {
                        new = g_strconcat(current, " ", selected, NULL);
                } else {
                        new = g_strconcat(current, ", ", selected, NULL);
                }
        }
        gtk_entry_set_text(w->entry, g_strstrip(new));
        g_free(current);
        g_free(new);
}

gboolean
tagwin_run                              (GtkWindow             *parent,
                                         const char            *user,
                                         char                 **newtags,
                                         const GList           *usertags,
                                         LastfmWsSession       *ws_session,
                                         LastfmTrack           *track,
                                         LastfmTrackComponent  *type)
{
        g_return_val_if_fail (ws_session && track && type &&
                              user && newtags, FALSE);
        tagwin *t;
        gboolean retvalue = FALSE;
        GtkWidget *sellabel;
        GtkComboBox *selcombo;
        GtkWidget *entrylabel, *entry;
        GtkWidget *userlabel, *usercombo, *globallabel, *globalcombo;
        GtkTreeModel *usermodel, *nonemodel, *retrmodel;
        GtkCellRenderer *userrender, *globalrender;
        GtkDialog *dialog;
        GtkBox *vbox;
        GList *nonelist;
        GtkTable *table;
        GtkWidget *alig;

        /* A treemodel for combos with no elements */
        nonelist = g_list_append(NULL, _("(none)"));
        nonemodel = ui_create_options_list(nonelist);
        g_list_free(nonelist);
        nonelist = g_list_append(NULL, _("retrieving..."));
        retrmodel = ui_create_options_list(nonelist);
        g_list_free(nonelist);

        /* Dialog and basic settings */
        dialog = ui_base_dialog(parent, _("Tagging"));
        vbox = GTK_BOX (gtk_dialog_get_content_area (dialog));
        gtk_box_set_homogeneous (vbox, FALSE);
        gtk_box_set_spacing (vbox, 5);

        /* Combo to select what to tag */
        sellabel = gtk_label_new(_("Tag this"));
        selcombo = artist_track_album_selection_combo(track);

        switch (*type) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                gtk_combo_box_set_active(selcombo, 0);
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                gtk_combo_box_set_active(selcombo, 1);
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_val_if_fail(track->album[0] != '\0', FALSE);
                gtk_combo_box_set_active(selcombo, 2);
                break;
        default:
                g_return_val_if_reached(FALSE);
                break;
        }

        /* Text entry */
        entrylabel = gtk_label_new(_("Enter a comma-separated\nlist of tags"));
        gtk_label_set_justify(GTK_LABEL(entrylabel), GTK_JUSTIFY_RIGHT);
        entry = gtk_entry_new();
        gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

        /* Combo boxes */
        userlabel = gtk_label_new(_("Your favourite tags"));
        globallabel = gtk_label_new(_("Popular tags for this"));
        if (usertags != NULL) {
                usermodel = ui_create_options_list(usertags);
        } else {
                usermodel = g_object_ref(nonemodel);
        }
        usercombo = gtk_combo_box_new_with_model(usermodel);
        globalcombo = gtk_combo_box_new_with_model(retrmodel);
        g_object_unref(G_OBJECT(usermodel));
        userrender = gtk_cell_renderer_text_new();
        globalrender = gtk_cell_renderer_text_new();
        g_object_set(userrender, "ellipsize", PANGO_ELLIPSIZE_END,
                     "ellipsize-set", TRUE, "width", 300, NULL);
        g_object_set(globalrender, "ellipsize", PANGO_ELLIPSIZE_END,
                     "ellipsize-set", TRUE, "width", 300, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(usercombo),
                                   userrender, FALSE);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(globalcombo),
                                   globalrender, FALSE);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(usercombo),
                                       userrender, "text", 0, NULL);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(globalcombo),
                                       globalrender, "text", 0, NULL);
        if (usertags == NULL) {
                gtk_widget_set_sensitive(GTK_WIDGET(usercombo), FALSE);
                gtk_combo_box_set_active(GTK_COMBO_BOX(usercombo), 0);
        }

        /* Widget packing */
        table = GTK_TABLE(gtk_table_new(4, 2, FALSE));
        gtk_table_set_row_spacings(table, 5);
        gtk_table_set_col_spacings(table, 20);
        gtk_box_pack_start (vbox, GTK_WIDGET(table), TRUE, TRUE, 5);

        alig = gtk_alignment_new(1, 0.5, 0, 0);
        gtk_container_add(GTK_CONTAINER(alig), sellabel);
        gtk_table_attach_defaults(table, alig, 0, 1, 0, 1);

        alig = gtk_alignment_new(0, 0.5, 0, 0);
        gtk_container_add(GTK_CONTAINER(alig), GTK_WIDGET(selcombo));
        gtk_table_attach_defaults(table, alig, 1, 2, 0, 1);

        alig = gtk_alignment_new(1, 0.5, 0, 0);
        gtk_container_add(GTK_CONTAINER(alig), entrylabel);
        gtk_table_attach_defaults(table, alig, 0, 1, 1, 2);

        alig = gtk_alignment_new(0, 0.5, 1, 0);
        gtk_container_add(GTK_CONTAINER(alig), GTK_WIDGET(entry));
        gtk_table_attach_defaults(table, alig, 1, 2, 1, 2);

        alig = gtk_alignment_new(1, 0.5, 0, 0);
        gtk_container_add(GTK_CONTAINER(alig), userlabel);
        gtk_table_attach_defaults(table, alig, 0, 1, 2, 3);

        alig = gtk_alignment_new(0, 0.5, 0, 0);
        gtk_container_add(GTK_CONTAINER(alig), usercombo);
        gtk_table_attach_defaults(table, alig, 1, 2, 2, 3);

        alig = gtk_alignment_new(1, 0.5, 0, 0);
        gtk_container_add(GTK_CONTAINER(alig), globallabel);
        gtk_table_attach_defaults(table, alig, 0, 1, 3, 4);

        alig = gtk_alignment_new(0, 0.5, 0, 0);
        gtk_container_add(GTK_CONTAINER(alig), globalcombo);
        gtk_table_attach_defaults(table, alig, 1, 2, 3, 4);

        t = tagwin_create();
        t->track = vgl_object_ref(track);
        t->ws_session = vgl_object_ref(ws_session);
        t->window = GTK_WINDOW(dialog);
        t->entry = GTK_ENTRY(entry);
        t->selcombo = selcombo;
        t->globalcombo = GTK_COMBO_BOX(globalcombo);
        t->nonemodel = nonemodel;
        t->retrmodel = retrmodel;
        t->user = g_strdup(user);

        /* Signals */
        g_signal_connect(G_OBJECT(selcombo), "changed",
                         G_CALLBACK(tagwin_selcombo_changed), t);
        g_signal_connect(G_OBJECT(usercombo), "changed",
                         G_CALLBACK(tagwin_tagcombo_changed), t);
        g_signal_connect(G_OBJECT(globalcombo), "changed",
                         G_CALLBACK(tagwin_tagcombo_changed), t);

        tagwin_selcombo_changed(selcombo, t);
        gtk_widget_grab_focus(entry);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                *newtags = g_strdup(gtk_entry_get_text(t->entry));
                *type = artist_track_album_combo_get_selected(selcombo);
                retvalue = TRUE;
        } else {
                *newtags = NULL;
                retvalue = FALSE;
        }
        gtk_widget_hide(GTK_WIDGET(t->window));
        vgl_object_unref(t);
        return retvalue;
}
