/*
 * uimisc-hildon22.c -- Hildon 2.2 UI functions
 *
 * Copyright (C) 2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include <hildon/hildon.h>

#include "uimisc.h"
#include "util.h"

static const char pidgin_name[] = "Pidgin";
static const char gajim_name[] = "Gajim";
static const char gossip_name[] = "Gossip";
static const char telepathy_name[] = "Telepathy";

typedef struct {
        VglObject parent;
        GtkWindow *window;
        GtkEntry *entry;
        GtkWidget *selbutton;
        HildonButton *globalbutton;
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
        TagComboState artist_state, track_state, album_state;
} tagwin;

typedef struct {
        GtkEntry *user;
        GtkEntry *pw;
        GtkDialog *dialog;
} UserCfgWin;

typedef struct {
        tagwin *w;
        LastfmTrackComponent type;
        GList *userlist;
        GList *globallist;
} GetTrackTagsData;

static void
tagwin_selbutton_changed                (GtkWidget *button,
                                         tagwin    *w);

static void
user_pw_modified                        (GObject    *obj,
                                         GParamSpec *arg,
                                         UserCfgWin *win)
{
        const char *user = gtk_entry_get_text (win->user);
        const char *pw = gtk_entry_get_text (win->pw);
        gboolean userpw = (user && pw && strlen (user) > 0 && strlen (pw) > 0);
        gtk_dialog_set_response_sensitive (win->dialog,
                                           GTK_RESPONSE_ACCEPT, userpw);
}

static void
dlbutton_clicked                        (HildonButton *button,
                                         GtkWindow    *win)
{
        char *dir;
        dir = ui_select_download_dir (win, hildon_button_get_value (button));
        if (dir != NULL) {
                hildon_button_set_value (button, dir);
                g_free (dir);
        }
}

static HildonTouchSelector *
usercfg_create_server_selector          (VglUserCfg *cfg)
{
        HildonTouchSelector *selector;
        const GList *srvlist, *iter;
        int i;

        g_return_val_if_fail (cfg != NULL, NULL);

        selector = HILDON_TOUCH_SELECTOR (hildon_touch_selector_new_text ());

        srvlist = vgl_server_list_get ();
        for (iter = srvlist, i = 0; iter != NULL; iter = iter->next, i++) {
                const VglServer *srv = iter->data;
                hildon_touch_selector_append_text (selector, srv->name);
                if (srv == cfg->server) {
                        hildon_touch_selector_set_active (selector, 0, i);
                }

        }

        return selector;
}

static HildonTouchSelector *
usercfg_create_im_selector              (VglUserCfg *cfg,
                                         gboolean   *none_selected)
{
        HildonTouchSelector *selector;
        GtkTreeModel *model;
        GtkTreeIter iter;

        g_return_val_if_fail (cfg != NULL && none_selected != NULL, NULL);

        selector = HILDON_TOUCH_SELECTOR (hildon_touch_selector_new_text ());
        *none_selected = FALSE;

        hildon_touch_selector_append_text (selector, "Disabled");
        hildon_touch_selector_append_text (selector, pidgin_name);
        hildon_touch_selector_append_text (selector, gajim_name);
        hildon_touch_selector_append_text (selector, gossip_name);
        hildon_touch_selector_append_text (selector, telepathy_name);

        hildon_touch_selector_set_column_selection_mode (
                selector, HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE);

        hildon_touch_selector_unselect_all (selector, 0);
        model = hildon_touch_selector_get_model (selector, 0);
        gtk_tree_model_get_iter_first (model, &iter);

        if (!(cfg->im_pidgin || cfg->im_gajim ||
              cfg->im_gossip || cfg->im_telepathy)) {
                *none_selected = TRUE;
                hildon_touch_selector_select_iter (selector, 0, &iter, FALSE);
        }

        gtk_tree_model_iter_next (model, &iter);
        if (cfg->im_pidgin)
                hildon_touch_selector_select_iter (selector, 0, &iter, FALSE);

        gtk_tree_model_iter_next (model, &iter);
        if (cfg->im_gajim)
                hildon_touch_selector_select_iter (selector, 0, &iter, FALSE);

        gtk_tree_model_iter_next (model, &iter);
        if (cfg->im_gossip)
                hildon_touch_selector_select_iter (selector, 0, &iter, FALSE);

        gtk_tree_model_iter_next (model, &iter);
        if (cfg->im_telepathy)
                hildon_touch_selector_select_iter (selector, 0, &iter, FALSE);

        return selector;
}

static void
im_selection_changed_cb                 (HildonTouchSelector *sel,
                                         gint                 col,
                                         gboolean            *none_selected)
{
        GtkTreeModel *model;
        GtkTreeIter iter;

        /* If the first element ("Disabled") was not selected but is
         * selected now, unselect all the other ones */
        if (!*none_selected) {
                GList *l, *liter;
                l = hildon_touch_selector_get_selected_rows (sel, col);

                for (liter = l; liter && !*none_selected; liter=liter->next) {
                        GtkTreePath *path = liter->data;
                        *none_selected = !gtk_tree_path_prev (path);
                }

                g_list_foreach (l, (GFunc) gtk_tree_path_free, NULL);
                g_list_free (l);
        } else {
                *none_selected = FALSE;
        }

        model = hildon_touch_selector_get_model (sel, col);
        gtk_tree_model_get_iter_first (model, &iter);

        g_signal_handlers_block_by_func (sel, im_selection_changed_cb,
                                         none_selected);
        if (*none_selected) {
                while (gtk_tree_model_iter_next (model, &iter)) {
                        hildon_touch_selector_unselect_iter (sel, col, &iter);
                }
        } else {
                hildon_touch_selector_unselect_iter (sel, col, &iter);
        }
        g_signal_handlers_unblock_by_func (sel, im_selection_changed_cb,
                                           none_selected);
}

static void
update_imstatus_config                  (HildonTouchSelector *sel,
                                         VglUserCfg          *cfg)
{
        GList *l, *iter;
        GtkTreeModel *model;

        l = hildon_touch_selector_get_selected_rows (sel, 0);
        model = hildon_touch_selector_get_model (sel, 0);

        cfg->im_pidgin = cfg->im_gajim = FALSE;
        cfg->im_gossip = cfg->im_telepathy = FALSE;

        for (iter = l; iter != NULL; iter = iter->next) {
                char *str;
                GtkTreeIter treeiter;
                GtkTreePath *path = iter->data;
                gtk_tree_model_get_iter (model, &treeiter, path);
                gtk_tree_model_get (model, &treeiter, 0, &str, -1);
                if (g_str_equal (str, pidgin_name)) {
                        cfg->im_pidgin = TRUE;
                } else if (g_str_equal (str, gajim_name)) {
                        cfg->im_gajim = TRUE;
                } else if (g_str_equal (str, gossip_name)) {
                        cfg->im_gossip = TRUE;
                } else if (g_str_equal (str, telepathy_name)) {
                        cfg->im_telepathy = TRUE;
                }
                g_free (str);
        }

        g_list_foreach (l, (GFunc) gtk_tree_path_free, NULL);
        g_list_free (l);
}

gboolean
ui_usercfg_window                       (GtkWindow   *parent,
                                         VglUserCfg **cfg)
{
        UserCfgWin win;
        GtkWidget *d, *panarea;
        GtkWidget *label;
        GtkWidget *frame;
        GtkEntry *user, *pw, *proxy, *imtemplate;
        HildonButton *service, *dlbutton, *imbutton;
        HildonCheckButton *scrob, *discov, *useproxy, *autodl, *nodiags;
        HildonTouchSelector *servicesel, *imsel;
        GtkBox *vbox, *hbox, *framebox;
        GtkSizeGroup *size_group;
        gboolean changed = FALSE;
        gboolean none_selected;

        g_return_val_if_fail (cfg != NULL, FALSE);
        if (*cfg == NULL) *cfg = vgl_user_cfg_new ();

        size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

        /* Create dialog */
        d = gtk_dialog_new_with_buttons (_("User settings"), parent,
                                         GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_SAVE,
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
        gtk_window_set_default_size (GTK_WINDOW (d), -1, 300);

        /* Pannable area and table */
        panarea = hildon_pannable_area_new ();
        framebox = GTK_BOX (gtk_vbox_new (FALSE, 0));

        /* Account frame */
        frame = gtk_frame_new (_("Account"));
        vbox = GTK_BOX (gtk_vbox_new (TRUE, 0));
        gtk_box_pack_start (framebox, frame, FALSE, FALSE, 10);
        gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (vbox));

        /* User name */
        hbox = GTK_BOX (gtk_hbox_new (FALSE, 0));
        label = gtk_label_new (_("Username"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_size_group_add_widget (size_group, label);
        gtk_box_pack_start (hbox, label, FALSE, FALSE, 10);

        user = GTK_ENTRY (hildon_entry_new (FINGER_SIZE));
        gtk_box_pack_start (hbox, GTK_WIDGET (user), TRUE, TRUE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (hbox), FALSE, FALSE, 0);

        /* Password */
        hbox = GTK_BOX (gtk_hbox_new (FALSE, 0));
        label = gtk_label_new (_("Password"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_size_group_add_widget (size_group, label);
        gtk_box_pack_start (hbox, label, FALSE, FALSE, 10);

        pw = GTK_ENTRY (hildon_entry_new (FINGER_SIZE));
        hildon_gtk_entry_set_input_mode (pw, HILDON_GTK_INPUT_MODE_FULL);
        gtk_entry_set_visibility (pw, FALSE);
        gtk_box_pack_start (hbox, GTK_WIDGET (pw), TRUE, TRUE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (hbox), FALSE, FALSE, 0);

        /* Service */
        service = HILDON_BUTTON (hildon_picker_button_new (
               FINGER_SIZE, HILDON_BUTTON_ARRANGEMENT_VERTICAL));
        servicesel = usercfg_create_server_selector (*cfg);
        hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (service),
                                           servicesel);
        hildon_button_set_title (service, _("Service"));
        hildon_button_set_alignment (service, 0.0, 0.5, 1.0, 0.0);
        hildon_button_set_title_alignment (service, 0.0, 0.5);
        hildon_button_set_value_alignment (service, 0.0, 0.5);
        gtk_box_pack_start (vbox, GTK_WIDGET (service), FALSE, FALSE, 0);

        /* Enable scrobbling */
        scrob = HILDON_CHECK_BUTTON (hildon_check_button_new (FINGER_SIZE));
        gtk_button_set_label (GTK_BUTTON (scrob), _("Enable scrobbling"));
        gtk_box_pack_start (vbox, GTK_WIDGET (scrob), FALSE, FALSE, 0);

        /* Discovery mode */
        discov = HILDON_CHECK_BUTTON (hildon_check_button_new (FINGER_SIZE));
        gtk_button_set_label (GTK_BUTTON (discov), _("Discovery mode"));
        gtk_box_pack_start (vbox, GTK_WIDGET (discov), FALSE, FALSE, 0);

        /* Connection frame */
        frame = gtk_frame_new (_("Connection"));
        vbox = GTK_BOX (gtk_vbox_new (TRUE, 0));
        gtk_box_pack_start (framebox, frame, FALSE, FALSE, 10);
        gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (vbox));

        /* Use HTTP proxy */
        useproxy = HILDON_CHECK_BUTTON (hildon_check_button_new (FINGER_SIZE));
        gtk_button_set_label (GTK_BUTTON (useproxy), _("Use HTTP proxy"));
        gtk_box_pack_start (vbox, GTK_WIDGET (useproxy), FALSE, FALSE, 0);

        /* Proxy address */
        hbox = GTK_BOX (gtk_hbox_new (FALSE, 0));
        label = gtk_label_new (_("Proxy address"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_size_group_add_widget (size_group, label);
        gtk_box_pack_start (hbox, label, FALSE, FALSE, 10);

        proxy = GTK_ENTRY (hildon_entry_new (FINGER_SIZE));
        gtk_box_pack_start (hbox, GTK_WIDGET (proxy), TRUE, TRUE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (hbox), FALSE, FALSE, 0);

        /* Download frame */
        frame = gtk_frame_new (_("Download"));
        vbox = GTK_BOX (gtk_vbox_new (TRUE, 0));
        gtk_box_pack_start (framebox, frame, FALSE, FALSE, 10);
        gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (vbox));

        /* Download directory */
        dlbutton = HILDON_BUTTON (hildon_button_new (
                          FINGER_SIZE, HILDON_BUTTON_ARRANGEMENT_VERTICAL));
        hildon_button_set_title (dlbutton, _("Download directory"));
        hildon_button_set_style (dlbutton, HILDON_BUTTON_STYLE_PICKER);
        hildon_button_set_alignment (dlbutton, 0.0, 0.5, 1.0, 0.0);
        hildon_button_set_title_alignment (dlbutton, 0.0, 0.5);
        hildon_button_set_value_alignment (dlbutton, 0.0, 0.5);
        gtk_box_pack_start (vbox, GTK_WIDGET (dlbutton), FALSE, FALSE, 0);

        /* Auto download free tracks */
        autodl = HILDON_CHECK_BUTTON (hildon_check_button_new (FINGER_SIZE));
        g_object_set (autodl, "width-request", 1, NULL);
        gtk_button_set_label (GTK_BUTTON (autodl),
                              /* Translators: keep this string short!! */
                              _("Automatically download free tracks"));
        gtk_box_pack_start (vbox, GTK_WIDGET (autodl), FALSE, FALSE, 0);

        /* IM status frame */
        frame = gtk_frame_new (_("Update IM status"));
        vbox = GTK_BOX (gtk_vbox_new (TRUE, 0));
        gtk_box_pack_start (framebox, frame, FALSE, FALSE, 10);
        gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (vbox));

        /* Update IM status */
        imbutton = HILDON_BUTTON (hildon_picker_button_new (
                FINGER_SIZE, HILDON_BUTTON_ARRANGEMENT_VERTICAL));
        imsel = usercfg_create_im_selector (*cfg, &none_selected);
        hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (imbutton),
                                           imsel);
        hildon_button_set_title (imbutton, _("Update IM status"));
        hildon_button_set_alignment (imbutton, 0.0, 0.5, 1.0, 0.0);
        hildon_button_set_title_alignment (imbutton, 0.0, 0.5);
        hildon_button_set_value_alignment (imbutton, 0.0, 0.5);
        gtk_box_pack_start (vbox, GTK_WIDGET (imbutton), FALSE, FALSE, 0);

        /* IM message template */
        hbox = GTK_BOX (gtk_hbox_new (FALSE, 0));
        label = gtk_label_new (_("IM message template"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_size_group_add_widget (size_group, label);
        gtk_box_pack_start (hbox, label, FALSE, FALSE, 10);

        imtemplate = GTK_ENTRY (hildon_entry_new (FINGER_SIZE));
        gtk_box_pack_start (hbox, GTK_WIDGET (imtemplate), TRUE, TRUE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (hbox), FALSE, FALSE, 0);

        /* Misc frame */
        frame = gtk_frame_new (_("Misc"));
        vbox = GTK_BOX (gtk_vbox_new (TRUE, 0));
        gtk_box_pack_start (framebox, frame, FALSE, FALSE, 10);
        gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (vbox));

        /* Disable confirmation dialogs */
        nodiags = HILDON_CHECK_BUTTON (hildon_check_button_new (FINGER_SIZE));
        gtk_button_set_label (GTK_BUTTON (nodiags),
                              _("Disable confirmation dialogs"));
        gtk_box_pack_start (vbox, GTK_WIDGET (nodiags), FALSE, FALSE, 0);

        /* Pack everything else */
        hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (panarea),
                                                GTK_WIDGET (framebox));
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (d)->vbox), panarea);

        /* Connect signals */
        win.user = user;
        win.pw = pw;
        win.dialog = GTK_DIALOG (d);
        g_signal_connect (user, "notify::text",
                          G_CALLBACK (user_pw_modified), &win);
        g_signal_connect (pw, "notify::text",
                          G_CALLBACK (user_pw_modified), &win);
        g_signal_connect (dlbutton, "clicked", G_CALLBACK(dlbutton_clicked),d);
        g_signal_connect (imsel, "changed",
                          G_CALLBACK (im_selection_changed_cb),&none_selected);

        /* Set current config values */
        gtk_entry_set_text (user, (*cfg)->username);
        gtk_entry_set_text (pw, (*cfg)->password);
        gtk_entry_set_text (proxy, (*cfg)->http_proxy);
        gtk_entry_set_text (imtemplate, (*cfg)->imstatus_template);
        hildon_button_set_value (dlbutton, (*cfg)->download_dir);
        hildon_check_button_set_active (scrob, (*cfg)->enable_scrobbling);
        hildon_check_button_set_active (discov, (*cfg)->discovery_mode);
        hildon_check_button_set_active (useproxy, (*cfg)->use_proxy);
        hildon_check_button_set_active (autodl, (*cfg)->autodl_free_tracks);
        hildon_check_button_set_active (nodiags,
                                        (*cfg)->disable_confirm_dialogs);

        /* Run the dialog */
        gtk_widget_show_all (d);

        if (gtk_dialog_run (GTK_DIALOG (d)) == GTK_RESPONSE_ACCEPT) {
                vgl_user_cfg_set_username (*cfg, gtk_entry_get_text (user));
                vgl_user_cfg_set_password (*cfg, gtk_entry_get_text (pw));
                vgl_user_cfg_set_http_proxy (*cfg, gtk_entry_get_text (proxy));
                vgl_user_cfg_set_download_dir (
                        *cfg, hildon_button_get_value (dlbutton));
                vgl_user_cfg_set_server_name(
                        *cfg, hildon_button_get_value (service));
                (*cfg)->enable_scrobbling =
                        hildon_check_button_get_active (scrob);
                (*cfg)->discovery_mode =
                        hildon_check_button_get_active (discov);
                (*cfg)->use_proxy =
                        hildon_check_button_get_active (useproxy);
                (*cfg)->autodl_free_tracks =
                        hildon_check_button_get_active (autodl);
                vgl_user_cfg_set_imstatus_template (
                        *cfg, gtk_entry_get_text (imtemplate));
                update_imstatus_config (imsel, *cfg);
                (*cfg)->disable_confirm_dialogs =
                        hildon_check_button_get_active (nodiags);
                changed = TRUE;
        }

        gtk_widget_destroy (d);

        return changed;
}

char *
ui_input_dialog_with_list               (GtkWindow   *parent,
                                         const char  *title,
                                         const char  *text,
                                         const GList *elems,
                                         const char  *value)
{
        GtkWidget *dialog;
        HildonTouchSelector *selector;
        char *retvalue = NULL;

        selector = HILDON_TOUCH_SELECTOR (
                hildon_touch_selector_entry_new_text ());
        if (elems == NULL) {
                /* Hack for libhildon < 2.2.5 */
                hildon_touch_selector_append_text (selector, "");
        } else for (; elems != NULL; elems = elems->next) {
                hildon_touch_selector_append_text (selector, elems->data);
        }

        dialog = hildon_picker_dialog_new (parent);
        gtk_window_set_title (GTK_WINDOW (dialog), title);
        hildon_picker_dialog_set_selector (HILDON_PICKER_DIALOG (dialog),
                                           selector);

        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
                retvalue = g_strstrip (
                        hildon_touch_selector_get_current_text (selector));
        }
        gtk_widget_destroy (dialog);

        return retvalue;
}

static GtkWidget *
ui_picker_button_new_from_list          (const char  *title,
                                         const GList *items,
                                         gboolean     entry,
                                         gboolean     multiple)
{
        HildonButton *b;
        HildonTouchSelector *sel;

        if (entry) {

                sel = HILDON_TOUCH_SELECTOR (
                        hildon_touch_selector_entry_new_text ());
        } else {
                sel = HILDON_TOUCH_SELECTOR (
                        hildon_touch_selector_new_text ());
                hildon_touch_selector_set_column_selection_mode (
                        sel, multiple ?
                        HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE :
                        HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE);
        }
        if (items == NULL && entry) {
                /* Hack for libhildon < 2.2.5 */
                hildon_touch_selector_append_text (sel, "");
        } else for (; items != NULL; items = items->next) {
                hildon_touch_selector_append_text (sel, items->data);
        }

        b = HILDON_BUTTON (
                hildon_picker_button_new (
                        FINGER_SIZE, HILDON_BUTTON_ARRANGEMENT_VERTICAL));
        hildon_button_set_title (b, title);
        hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (b), sel);

        hildon_button_set_alignment (b, 0.0, 0.5, 1.0, 0.0);
        hildon_button_set_title_alignment (b, 0.0, 0.5);
        hildon_button_set_value_alignment (b, 0.0, 0.5);

        return GTK_WIDGET (b);
}

static GtkWidget *
artist_track_album_button_new           (const char           *title,
                                         const LastfmTrack    *track,
                                         LastfmTrackComponent  active)
{
        GtkWidget *button;
        HildonTouchSelector *selector;
        GList *l = NULL;
        GString *str = g_string_sized_new (50);

        g_string_assign (str, _("Artist: "));
        g_string_append (str, track->artist);
        l = g_list_append (l, g_strdup (str->str));

        g_string_assign (str, _("Track: "));
        g_string_append (str, track->title);
        l = g_list_append (l, g_strdup (str->str));

        if (track->album[0] != '\0') {
                g_string_assign (str, _("Album: "));
                g_string_append (str, track->album);
                l = g_list_append (l, g_strdup (str->str));
        }

        button = ui_picker_button_new_from_list (title, l, FALSE, FALSE);

        selector = hildon_picker_button_get_selector (
                HILDON_PICKER_BUTTON (button));

        switch (active) {
        case LASTFM_TRACK_COMPONENT_ARTIST:
                hildon_touch_selector_set_active (selector, 0, 0);
                break;
        case LASTFM_TRACK_COMPONENT_TRACK:
                hildon_touch_selector_set_active (selector, 0, 1);
                break;
        case LASTFM_TRACK_COMPONENT_ALBUM:
                g_return_val_if_fail (track->album[0] != '\0', FALSE);
                hildon_touch_selector_set_active (selector, 0, 2);
                break;
        default:
                g_return_val_if_reached (NULL);
                break;
        }

        g_list_foreach (l, (GFunc) g_free, NULL);
        g_list_free (l);
        g_string_free (str, TRUE);

        return button;
}

static LastfmTrackComponent
artist_track_album_button_get_selected  (GtkWidget *button)
{
        LastfmTrackComponent ret;
        HildonTouchSelector *selector;

        g_return_val_if_fail (HILDON_IS_PICKER_BUTTON (button),
                              LASTFM_TRACK_COMPONENT_ARTIST);

        selector = hildon_picker_button_get_selector (
                HILDON_PICKER_BUTTON (button));

        switch (hildon_touch_selector_get_active (selector, 0)) {
        case 0:
                ret = LASTFM_TRACK_COMPONENT_ARTIST;
                break;
        case 1:
                ret = LASTFM_TRACK_COMPONENT_TRACK;
                break;
        case 2:
                ret = LASTFM_TRACK_COMPONENT_ALBUM;
                break;
        default:
                g_return_val_if_reached (LASTFM_TRACK_COMPONENT_ARTIST);
                break;
        }
        return ret;
}

gboolean
recommwin_run                           (GtkWindow             *parent,
                                         char                 **user,
                                         char                 **message,
                                         const GList           *friends,
                                         const LastfmTrack     *track,
                                         LastfmTrackComponent  *type)
{
        gboolean retval = FALSE;
        GtkDialog *dialog;
        GtkWidget *panarea;
        GtkBox *vbox;
        GtkWidget *selbutton;
        GtkWidget *userbutton;
        GtkWidget *textlabel;
        GtkTextView *textview;

        g_return_val_if_fail (user && message && track && type, FALSE);

        /* Dialog and basic settings */
        dialog = ui_base_dialog (parent, _("Send a recommendation"));
        gtk_box_set_homogeneous (GTK_BOX (dialog->vbox), FALSE);
        gtk_box_set_spacing (GTK_BOX (dialog->vbox), 5);

        /* Picker button to select what to recommend */
        selbutton = artist_track_album_button_new (
                _("Recommend this"), track, *type);

        /* Picker button to select the recipient of the recommendation */
        userbutton = ui_picker_button_new_from_list (
                _("Send recommendation to"), friends, TRUE, FALSE);
        if (friends) {
                hildon_button_set_value (HILDON_BUTTON (userbutton),
                                         friends->data);
        }

        /* Message of the recommendation */
        textlabel = gtk_label_new(_("Recommendation message"));
        textview = GTK_TEXT_VIEW (hildon_text_view_new ());
        gtk_text_view_set_accepts_tab(textview, FALSE);
        gtk_text_view_set_wrap_mode(textview, GTK_WRAP_WORD_CHAR);

        /* Widget packing */
        vbox = GTK_BOX (gtk_vbox_new (FALSE, 0));
        panarea = hildon_pannable_area_new ();
        gtk_box_pack_start (vbox, GTK_WIDGET (selbutton), FALSE, FALSE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (userbutton), FALSE, FALSE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (textlabel), TRUE, TRUE, 0);
        gtk_box_pack_start (vbox, GTK_WIDGET (textview), TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (dialog->vbox), panarea, TRUE, TRUE, 0);
        hildon_pannable_area_add_with_viewport (
                HILDON_PANNABLE_AREA (panarea), GTK_WIDGET (vbox));

        gtk_widget_set_size_request (GTK_WIDGET (dialog), -1, 300);
        gtk_widget_show_all (GTK_WIDGET (dialog));
        if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
                GtkTextIter start, end;
                GtkTextBuffer *buf = gtk_text_view_get_buffer (textview);
                HildonButton *b = HILDON_BUTTON (userbutton);
                gtk_text_buffer_get_bounds(buf, &start, &end);
                *message = gtk_text_buffer_get_text (buf, &start, &end, FALSE);
                *type = artist_track_album_button_get_selected (selbutton);
                *user = g_strstrip (g_strdup (hildon_button_get_value (b)));
                retval = TRUE;
        } else {
                *message = NULL;
                *user = NULL;
                retval = FALSE;
        }
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return retval;
}

static void
tagwin_destroy                          (tagwin *w)
{
        gtk_widget_destroy (GTK_WIDGET (w->window));
        vgl_object_unref (w->track);
        vgl_object_unref (w->ws_session);
        g_free (w->user);
        g_free (w->tags_artist);
        g_free (w->tags_track);
        g_free (w->tags_album);
        if (w->poptags_artist) g_object_unref (w->poptags_artist);
        if (w->poptags_track) g_object_unref (w->poptags_track);
        if (w->poptags_album) g_object_unref (w->poptags_album);
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

        tagwin_selbutton_changed (data->w->selbutton, data->w);

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
tagwin_selbutton_changed                (GtkWidget *button,
                                         tagwin    *w)
{
        GtkTreeModel *model = NULL;
        const char *usertags = NULL;
        HildonTouchSelector *sel;
        TagComboState oldstate;
        LastfmTrackComponent type;
        sel = hildon_picker_button_get_selector (
                HILDON_PICKER_BUTTON (w->globalbutton));
        type = artist_track_album_button_get_selected (button);
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
        gtk_widget_set_sensitive (GTK_WIDGET (w->globalbutton), model != NULL);
        if (oldstate == TAGCOMBO_STATE_READY) {
                if (model != NULL) {
                        hildon_touch_selector_set_model (sel, 0, model);
                        hildon_touch_selector_unselect_all (sel, 0);
                } else {
                        hildon_touch_selector_set_model (sel, 0, w->nonemodel);
                }
                gtk_widget_set_sensitive (GTK_WIDGET (w->entry), TRUE);
                gtk_entry_set_text (w->entry, usertags ? usertags : "");
        } else {
                hildon_touch_selector_set_model (sel, 0, w->nonemodel);
                hildon_button_set_value (w->globalbutton, _("retrieving..."));
                gtk_widget_set_sensitive (GTK_WIDGET (w->entry), FALSE);
                gtk_entry_set_text (w->entry, _("retrieving..."));
        }
        if (oldstate == TAGCOMBO_STATE_NULL) {
                GetTrackTagsData *data = g_slice_new (GetTrackTagsData);
                data->w = vgl_object_ref (w);
                data->type = type;
                data->userlist = data->globallist = NULL;
                g_thread_create (get_track_tags_thread, data, FALSE, NULL);
        }
}

static void
tagwin_tagbutton_changed                (HildonPickerButton *button,
                                         tagwin             *w)
{
        GList *rows, *l;
        GString *tags;
        HildonTouchSelector *sel;
        GtkTreeModel *model;

        sel = hildon_picker_button_get_selector (button);
        model = hildon_touch_selector_get_model (sel, 0);
        rows = hildon_touch_selector_get_selected_rows (sel, 0);
        tags = g_string_new (gtk_entry_get_text (w->entry));

        g_strstrip (tags->str);
        g_string_truncate (tags, strlen (tags->str));

        for (l = rows; l != NULL; l = l->next) {
                GtkTreeIter iter;
                char *str;
                gtk_tree_model_get_iter (model, &iter, l->data);
                gtk_tree_model_get (model, &iter, 0, &str, -1);
                if (tags->len > 0) {
                        if (tags->str[tags->len - 1] != ',') {
                                g_string_append_c (tags, ',');
                        }
                        g_string_append_c (tags, ' ');
                }
                g_string_append (tags, str);
                g_free (str);
        }

        g_list_foreach (rows, (GFunc) gtk_tree_path_free, NULL);
        g_list_free (rows);

        g_signal_handlers_block_by_func (button, tagwin_tagbutton_changed, w);
        hildon_touch_selector_unselect_all (sel, 0);
        g_signal_handlers_unblock_by_func (button, tagwin_tagbutton_changed,w);

        gtk_entry_set_text (w->entry, tags->str);
        gtk_editable_set_position (GTK_EDITABLE (w->entry), -1);
        g_string_free (tags, TRUE);
}

static char *
userbutton_print_func                   (HildonTouchSelector *selector,
                                         gpointer             user_data)
{
        GtkTreeModel *model = hildon_touch_selector_get_model (selector, 0);
        gint size = gtk_tree_model_iter_n_children (model, NULL);
        return g_strdup_printf (_("%d tags"), size);
}

static char *
globalbutton_print_func                 (HildonTouchSelector *selector,
                                         gpointer             user_data)
{
        LastfmTrackComponent type;
        TagComboState state;
        tagwin *w = user_data;

        type = artist_track_album_button_get_selected (
                GTK_WIDGET (w->selbutton));
        if (type == LASTFM_TRACK_COMPONENT_ARTIST) {
                state = w->artist_state;
        } else if (type == LASTFM_TRACK_COMPONENT_TRACK) {
                state = w->track_state;
        } else if (type == LASTFM_TRACK_COMPONENT_ALBUM) {
                state = w->album_state;
        } else {
                g_return_val_if_reached (NULL);
        }

        if (state == TAGCOMBO_STATE_READY) {
                gint size;
                GtkTreeModel *model;
                model = hildon_touch_selector_get_model (selector, 0);
                size = gtk_tree_model_iter_n_children (model, NULL);
                return g_strdup_printf (_("%d tags"), size);
        } else {
                return g_strdup ("(retrieving...)");
        }
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
        tagwin *t;
        gboolean retvalue = FALSE;
        GtkWidget *selbutton;
        GtkWidget *entrylabel, *entry, *userbutton, *globalbutton;
        HildonTouchSelector *sel;
        GtkDialog *dialog;

        g_return_val_if_fail (ws_session && track && type &&
                              user && newtags, FALSE);

        /* Dialog and basic settings */
        t = tagwin_create ();
        dialog = ui_base_dialog (parent, _("Tagging"));
        gtk_box_set_homogeneous (GTK_BOX (dialog->vbox), FALSE);
        gtk_box_set_spacing (GTK_BOX (dialog->vbox), 5);

        /* Combo to select what to tag */
        selbutton = artist_track_album_button_new (_("Tag this"),track,*type);

        /* Text entry */
        entrylabel = gtk_label_new(_("Enter a comma-separated list of tags"));
        entry = hildon_entry_new (FINGER_SIZE);

        /* User tags */
        userbutton = ui_picker_button_new_from_list (
                _("Your favourite tags"), usertags, FALSE, TRUE);
        sel = hildon_picker_button_get_selector (
                HILDON_PICKER_BUTTON (userbutton));
        hildon_touch_selector_set_print_func (sel, userbutton_print_func);
        hildon_touch_selector_unselect_all (sel, 0);

        /* Global tags */
        globalbutton = ui_picker_button_new_from_list (
                _("Popular tags for this"), NULL, FALSE, TRUE);
        sel = hildon_picker_button_get_selector (
                HILDON_PICKER_BUTTON (globalbutton));
        hildon_touch_selector_set_print_func_full (
                sel, globalbutton_print_func, t, NULL);

        if (usertags == NULL) {
                gtk_widget_set_sensitive (userbutton, FALSE);
        }

        /* Widget packing */
        gtk_box_pack_start (GTK_BOX (dialog->vbox), selbutton, TRUE, TRUE, 5);
        gtk_box_pack_start (GTK_BOX (dialog->vbox), entrylabel, TRUE, TRUE, 5);
        gtk_box_pack_start (GTK_BOX (dialog->vbox), entry, TRUE, TRUE, 5);
        gtk_box_pack_start (GTK_BOX (dialog->vbox), userbutton, TRUE, TRUE, 5);
        gtk_box_pack_start (GTK_BOX (dialog->vbox),
                            globalbutton, TRUE, TRUE, 5);

        t->track = vgl_object_ref (track);
        t->ws_session = vgl_object_ref (ws_session);
        t->window = GTK_WINDOW (dialog);
        t->entry = GTK_ENTRY (entry);
        t->selbutton = selbutton;
        t->globalbutton = HILDON_BUTTON (globalbutton);
        t->user = g_strdup (user);
        t->nonemodel = ui_create_options_list (NULL);

        /* Signals */
        g_signal_connect (selbutton, "value-changed",
                          G_CALLBACK (tagwin_selbutton_changed), t);
        g_signal_connect (userbutton, "value-changed",
                          G_CALLBACK (tagwin_tagbutton_changed), t);
        g_signal_connect (globalbutton, "value-changed",
                          G_CALLBACK (tagwin_tagbutton_changed), t);

        tagwin_selbutton_changed (selbutton, t);
        gtk_widget_grab_focus (entry);
        gtk_widget_show_all (GTK_WIDGET (dialog));
        if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
                *newtags = g_strdup (gtk_entry_get_text (t->entry));
                *type = artist_track_album_button_get_selected (selbutton);
                retvalue = TRUE;
        } else {
                *newtags = NULL;
                retvalue = FALSE;
        }
        gtk_widget_hide (GTK_WIDGET (t->window));
        vgl_object_unref (t);
        return retvalue;
}
