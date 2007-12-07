/*
 * uimisc.c -- Misc UI-related functions
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is published under the GNU GPLv3.
 */

#include "config.h"
#include "uimisc.h"
#include "metadata.h"

#include <gtk/gtk.h>
#if defined(MAEMO2) || defined(MAEMO3)
#include <hildon-widgets/hildon-program.h>
#include <hildon-widgets/hildon-banner.h>
#elif defined(MAEMO4)
#include <hildon/hildon-program.h>
#include <hildon/hildon-banner.h>
#endif
#include <string.h>

static void tagwin_selcombo_changed(GtkComboBox *combo, gpointer data);

typedef struct {
        lastfm_tagwin *w;
        request_type type;
} get_track_tags_data;

enum {
        ARTIST_TRACK_ALBUM_TYPE = 0,
        ARTIST_TRACK_ALBUM_TEXT,
        ARTIST_TRACK_ALBUM_NCOLS
};

void
flush_ui_events(void)
{
        while (gtk_events_pending()) gtk_main_iteration();
}

static void
ui_show_dialog(GtkWindow *parent, const char *text, GtkMessageType type)
{
        g_return_if_fail(text != NULL);
        GtkDialogFlags flags = GTK_DIALOG_MODAL |
                GTK_DIALOG_DESTROY_WITH_PARENT;
        GtkWidget *dialog = gtk_message_dialog_new(parent, flags, type,
                                                   GTK_BUTTONS_OK,
                                                   "%s", text);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

void
ui_info_dialog(GtkWindow *parent, const char *text)
{
        ui_show_dialog(parent, text, GTK_MESSAGE_INFO);
}

void
ui_warning_dialog(GtkWindow *parent, const char *text)
{
        ui_show_dialog(parent, text, GTK_MESSAGE_WARNING);
}

void
ui_error_dialog(GtkWindow *parent, const char *text)
{
        ui_show_dialog(parent, text, GTK_MESSAGE_ERROR);
}

void
ui_info_banner(GtkWindow *parent, const char *text)
{
        g_return_if_fail(parent != NULL && text != NULL);
#ifdef MAEMO
        hildon_banner_show_information(GTK_WIDGET(parent), NULL, text);
#else
        /* TODO: Implement a notification banner for Gnome */
#endif
}

static GtkDialog *
ui_base_dialog(GtkWindow *parent, const char *title)
{
        GtkWidget *dialog;
        dialog = gtk_dialog_new_with_buttons(title, parent,
                                             GTK_DIALOG_MODAL |
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_STOCK_OK,
                                             GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL,
                                             GTK_RESPONSE_REJECT,
                                             NULL);
        return GTK_DIALOG(dialog);
}

gboolean
ui_confirm_dialog(GtkWindow *parent, const char *text)
{
        g_return_val_if_fail(text != NULL, FALSE);
        gint response;
        GtkDialog *dialog = ui_base_dialog(parent, "Confirmation");
        GtkWidget *label = gtk_label_new(text);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), label, FALSE, FALSE, 10);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        response = gtk_dialog_run(dialog);
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return (response == GTK_RESPONSE_ACCEPT);
}

char *
ui_input_dialog(GtkWindow *parent, const char *title,
                const char *text, const char *value)
{
        GtkDialog *dialog;
        GtkWidget *label;
        GtkEntry *entry;
        char *retvalue = NULL;
        dialog = ui_base_dialog(parent, title);
        label = gtk_label_new(text);
        entry = GTK_ENTRY(gtk_entry_new());
        gtk_box_pack_start(GTK_BOX(dialog->vbox), label, FALSE, FALSE, 10);
        gtk_box_pack_start(GTK_BOX(dialog->vbox),
                           GTK_WIDGET(entry), FALSE, FALSE, 10);
        if (value != NULL) gtk_entry_set_text(entry, value);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                retvalue = g_strstrip(g_strdup(gtk_entry_get_text(entry)));
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return retvalue;
}

gboolean
ui_usercfg_dialog(GtkWindow *parent, lastfm_usercfg **cfg)
{
        g_return_val_if_fail(cfg != NULL, FALSE);
        GtkDialog *dialog;
        GtkWidget *label1, *label2, *label3, *label4, *label5;
        GtkEntry *user, *pw, *proxy;
        GtkWidget *scrobble, *discovery;
        GtkTable *table;
        gboolean changed = FALSE;

        dialog = ui_base_dialog(parent, "User settings");
        label1 = gtk_label_new("Username:");
        label2 = gtk_label_new("Password:");
        label3 = gtk_label_new("HTTP proxy (host:port):");
        label4 = gtk_label_new("Enable scrobbling:");
        label5 = gtk_label_new("Discovery mode:");
        user = GTK_ENTRY(gtk_entry_new());
        pw = GTK_ENTRY(gtk_entry_new());
        proxy = GTK_ENTRY(gtk_entry_new());
        scrobble = gtk_check_button_new();
        discovery = gtk_check_button_new();
        gtk_entry_set_visibility(pw, FALSE);
        if (*cfg != NULL) {
                gtk_entry_set_text(user, (*cfg)->username);
                gtk_entry_set_text(pw, (*cfg)->password);
                gtk_entry_set_text(proxy, (*cfg)->http_proxy);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scrobble),
                                             (*cfg)->enable_scrobbling);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(discovery),
                                             (*cfg)->discovery_mode);
        } else {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scrobble),
                                             TRUE);
        }
        table = GTK_TABLE(gtk_table_new(5, 2, FALSE));
        gtk_table_attach(table, label1, 0, 1, 0, 1, 0, 0, 5, 5);
        gtk_table_attach(table, label2, 0, 1, 1, 2, 0, 0, 5, 5);
        gtk_table_attach(table, label3, 0, 1, 2, 3, 0, 0, 5, 5);
        gtk_table_attach(table, label4, 0, 1, 3, 4, 0, 0, 5, 5);
        gtk_table_attach(table, label5, 0, 1, 4, 5, 0, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET(user), 1, 2, 0, 1, 0, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET(pw), 1, 2, 1, 2, 0, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET(proxy), 1, 2, 2, 3, 0, 0, 5, 5);
        gtk_table_attach(table, scrobble, 1, 2, 3, 4, 0, 0, 5, 5);
        gtk_table_attach(table, discovery, 1, 2, 4, 5, 0, 0, 5, 5);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table),
                           FALSE, FALSE, 10);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                if (*cfg == NULL) *cfg = lastfm_usercfg_new();
                lastfm_usercfg_set_username(*cfg, gtk_entry_get_text(user));
                lastfm_usercfg_set_password(*cfg, gtk_entry_get_text(pw));
                lastfm_usercfg_set_http_proxy(*cfg, gtk_entry_get_text(proxy));
                (*cfg)->enable_scrobbling = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(scrobble));
                (*cfg)->discovery_mode = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(discovery));
                changed = TRUE;
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return changed;
}

static GtkTreeModel *
ui_create_options_list(GList *elems)
{
        GtkTreeIter iter;
        GList *current = elems;
        GtkListStore *store = gtk_list_store_new (1, G_TYPE_STRING);;

        for (; current != NULL; current = g_list_next(current)) {
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, current->data, -1);
        }
        return GTK_TREE_MODEL(store);
}

char *
ui_input_dialog_with_list(GtkWindow *parent, const char *title,
                          const char *text, GList *elems, const char *value)
{
        GtkDialog *dialog;
        GtkWidget *label;
        GtkWidget *combo;
        GtkTreeModel *model;
        char *retvalue = NULL;
        model = ui_create_options_list(elems);
        dialog = ui_base_dialog(parent, title);
        label = gtk_label_new(text);
        combo = gtk_combo_box_entry_new_with_model(model, 0);
        g_object_unref(G_OBJECT(model));
        gtk_box_pack_start(GTK_BOX(dialog->vbox), label, FALSE, FALSE, 10);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), combo, FALSE, FALSE, 10);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (value != NULL) {
                gtk_entry_set_text(GTK_ENTRY(GTK_BIN(combo)->child), value);
        }
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                GtkEntry *entry = GTK_ENTRY(GTK_BIN(combo)->child);
                retvalue = g_strstrip(g_strdup(gtk_entry_get_text(entry)));
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return retvalue;
}

static GtkWidget *
artist_track_album_selection_combo(const lastfm_track *t)
{
        g_return_val_if_fail(t != NULL, NULL);
        char *text;
        GtkWidget *combo;
        GtkCellRenderer *renderer;
        GtkTreeIter iter;
        GtkListStore *store;
        store = gtk_list_store_new(ARTIST_TRACK_ALBUM_NCOLS,
                                   G_TYPE_INT, G_TYPE_STRING);

        text = g_strconcat("Artist: ", t->artist, NULL);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           ARTIST_TRACK_ALBUM_TYPE, REQUEST_ARTIST,
                           ARTIST_TRACK_ALBUM_TEXT, text, -1);
        g_free(text);

        text = g_strconcat("Track: ", t->title, NULL);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           ARTIST_TRACK_ALBUM_TYPE, REQUEST_TRACK,
                           ARTIST_TRACK_ALBUM_TEXT, text, -1);
        g_free(text);

        if (t->album[0] != '\0') {
                text = g_strconcat("Album: ", t->album, NULL);
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter,
                                   ARTIST_TRACK_ALBUM_TYPE, REQUEST_ALBUM,
                                   ARTIST_TRACK_ALBUM_TEXT, text, -1);
                g_free(text);
        }
        combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
        g_object_unref(G_OBJECT(store));
        renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, FALSE);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo),
                                       renderer, "text", 1, NULL);

        return combo;
}

static request_type
artist_track_album_combo_get_selected(GtkComboBox *combo)
{
        g_return_val_if_fail(combo != NULL, 0);
        GtkTreeModel *model;
        GtkTreeIter iter;
        request_type type;
        model = gtk_combo_box_get_model(combo);
        gtk_combo_box_get_active_iter(combo, &iter);
        gtk_tree_model_get(model, &iter, ARTIST_TRACK_ALBUM_TYPE, &type, -1);
        return type;
}


void
lastfm_tagwin_destroy(lastfm_tagwin *w)
{
        g_return_if_fail(w != NULL);
        gtk_widget_destroy(GTK_WIDGET(w->window));
        lastfm_track_destroy(w->track);
        g_free(w->tags_artist);
        g_free(w->tags_track);
        g_free(w->tags_album);
        if (w->poptags_artist) g_object_unref(w->poptags_artist);
        if (w->poptags_track) g_object_unref(w->poptags_track);
        if (w->poptags_album) g_object_unref(w->poptags_album);
        g_slice_free(lastfm_tagwin, w);
}

lastfm_tagwin *
lastfm_tagwin_create(void)
{
        lastfm_tagwin *w = g_slice_new0(lastfm_tagwin);
        w->refcount = 1;
        return w;
}

lastfm_tagwin *
lastfm_tagwin_ref(lastfm_tagwin *w)
{
        g_return_val_if_fail (w != NULL && w->refcount > 0, NULL);
        w->refcount++;
        return w;
}

void
lastfm_tagwin_unref(lastfm_tagwin *w)
{
        g_return_if_fail (w != NULL && w->refcount > 0);
        w->refcount--;
        if (w->refcount == 0) lastfm_tagwin_destroy(w);
}

static gpointer
get_track_tags_thread(gpointer userdata)
{
        get_track_tags_data *data = (get_track_tags_data *) userdata;
        g_return_val_if_fail(data && data->w && data->w->track, NULL);
        GList *list = NULL;
        gboolean found;
        found = lastfm_get_track_tags(data->w->track, data->type, &list);
        gdk_threads_enter();
        if (found) {
                GtkTreeModel *model = ui_create_options_list(list);
                switch (data->type) {
                case REQUEST_ARTIST:
                        data->w->poptags_artist = model;
                        break;
                case REQUEST_TRACK:
                        data->w->poptags_track = model;
                        break;
                case REQUEST_ALBUM:
                        data->w->poptags_album = model;
                        break;
                default:
                        gdk_threads_leave();
                        g_return_val_if_reached(NULL);
                }
        }
        tagwin_selcombo_changed(data->w->selcombo, data->w);
        lastfm_tagwin_unref(data->w);
        gdk_threads_leave();
        g_list_foreach(list, (GFunc) g_free, NULL);
        g_list_free(list);
        g_slice_free(get_track_tags_data, data);
        return NULL;
}

static void
tagwin_selcombo_changed(GtkComboBox *combo, gpointer data)
{
        lastfm_tagwin *w = (lastfm_tagwin *) data;
        GtkTreeModel *model = NULL;
        gboolean updated = TRUE;
        request_type type = artist_track_album_combo_get_selected(combo);
        switch (type) {
        case REQUEST_ARTIST:
                model = w->poptags_artist;
                updated = w->updating_artist;
                w->updating_artist = TRUE;
                break;
        case REQUEST_TRACK:
                model = w->poptags_track;
                updated = w->updating_track;
                w->updating_track = TRUE;
                break;
        case REQUEST_ALBUM:
                g_return_if_fail(w->track->album[0] != '\0');
                model = w->poptags_album;
                updated = w->updating_album;
                w->updating_album = TRUE;
                break;
        default:
                g_return_if_reached();
                break;
        }
        if (model != NULL) {
                gtk_combo_box_set_model(w->globalcombo, model);
        }
        if (!updated) {
                get_track_tags_data *data = g_slice_new(get_track_tags_data);
                data->w = lastfm_tagwin_ref(w);
                data->type = type;
                g_thread_create(get_track_tags_thread, data, FALSE, NULL);
        }
        gtk_widget_set_sensitive(GTK_WIDGET(w->globalcombo), updated);
        gtk_widget_set_sensitive(GTK_WIDGET(w->globallabel), updated);
}

static void
tagwin_tagcombo_changed(GtkComboBox *combo, gpointer data)
{
        if (gtk_combo_box_get_active(combo) == -1) return;
        lastfm_tagwin *w = (lastfm_tagwin *) data;
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
        gtk_combo_box_set_active(combo, -1);
}

char *
lastfm_tagwin_get_tags(GtkWindow *parent, GList *usertags,
                       const lastfm_track *track, request_type *type)
{
        g_return_val_if_fail(track != NULL && type != NULL, NULL);
        lastfm_tagwin *t;
        char *retvalue = NULL;
        GtkBox *combosbox, *userbox, *globalbox, *selbox;
        GtkWidget *sellabel, *selcombo;
        GtkWidget *entrylabel, *entry;
        GtkWidget *userlabel, *usercombo, *globallabel, *globalcombo;
        GtkTreeModel *usermodel, *globalmodel;
        GtkCellRenderer *userrender, *globalrender;
        GtkDialog *dialog;

        /* Dialog and basic settings */
        dialog = ui_base_dialog(parent, "Tagging");
        gtk_box_set_homogeneous(GTK_BOX(dialog->vbox), FALSE);
        gtk_box_set_spacing(GTK_BOX(dialog->vbox), 5);

        /* Combo to select what to tag */
        selbox = GTK_BOX(gtk_hbox_new(FALSE, 10));
        sellabel = gtk_label_new("Tag this");
        selcombo = artist_track_album_selection_combo(track);
        gtk_box_pack_start(selbox, sellabel, FALSE, FALSE, 0);
        gtk_box_pack_start(selbox, selcombo, TRUE, TRUE, 0);

        switch (*type) {
        case REQUEST_ARTIST:
                gtk_combo_box_set_active(GTK_COMBO_BOX(selcombo), 0);
                break;
        case REQUEST_TRACK:
                gtk_combo_box_set_active(GTK_COMBO_BOX(selcombo), 1);
                break;
        case REQUEST_ALBUM:
                g_return_val_if_fail(track->album[0] != '\0', NULL);
                gtk_combo_box_set_active(GTK_COMBO_BOX(selcombo), 2);
                break;
        default:
                g_return_val_if_reached(NULL);
                break;
        }

        /* Text entry */
        entrylabel = gtk_label_new("Enter a comma-separated list of tags");
        entry = gtk_entry_new();

        /* Combo boxes */
        userlabel = gtk_label_new("Your favourite tags");
        globallabel = gtk_label_new("Popular tags for this");
        usermodel = ui_create_options_list(usertags);
        globalmodel = ui_create_options_list(NULL);
        usercombo = gtk_combo_box_new_with_model(usermodel);
        globalcombo = gtk_combo_box_new_with_model(globalmodel);
        g_object_unref(G_OBJECT(usermodel));
        g_object_unref(G_OBJECT(globalmodel));
        userrender = gtk_cell_renderer_text_new();
        globalrender = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(usercombo),
                                   userrender, FALSE);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(globalcombo),
                                   globalrender, FALSE);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(usercombo),
                                       userrender, "text", 0, NULL);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(globalcombo),
                                       globalrender, "text", 0, NULL);

        /* Boxes for tag combos */
        combosbox = GTK_BOX(gtk_hbox_new(TRUE, 10));
        userbox = GTK_BOX(gtk_vbox_new(TRUE, 0));
        globalbox = GTK_BOX(gtk_vbox_new(TRUE, 0));

        /* Widget packing */
        gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(selbox),
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), entrylabel, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), entry, TRUE, TRUE, 0);
        gtk_box_pack_start(userbox, userlabel, FALSE, FALSE, 0);
        gtk_box_pack_start(userbox, usercombo, FALSE, FALSE, 0);
        gtk_box_pack_start(globalbox, globallabel, FALSE, FALSE, 0);
        gtk_box_pack_start(globalbox, globalcombo, FALSE, FALSE, 0);
        gtk_box_pack_start(combosbox, GTK_WIDGET(userbox), FALSE, FALSE, 0);
        gtk_box_pack_start(combosbox, GTK_WIDGET(globalbox), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(combosbox),
                           TRUE, TRUE, 0);

        t = lastfm_tagwin_create();
        t->track = lastfm_track_copy(track);
        t->window = GTK_WINDOW(dialog);
        t->entry = GTK_ENTRY(entry);
        t->selcombo = GTK_COMBO_BOX(selcombo);
        t->globalcombo = GTK_COMBO_BOX(globalcombo);
        t->globallabel = globallabel;

        /* Signals */
        g_signal_connect(G_OBJECT(selcombo), "changed",
                         G_CALLBACK(tagwin_selcombo_changed), t);
        g_signal_connect(G_OBJECT(usercombo), "changed",
                         G_CALLBACK(tagwin_tagcombo_changed), t);
        g_signal_connect(G_OBJECT(globalcombo), "changed",
                         G_CALLBACK(tagwin_tagcombo_changed), t);

        tagwin_selcombo_changed(t->selcombo, t);
        gtk_widget_grab_focus(entry);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                retvalue = g_strdup(gtk_entry_get_text(t->entry));
                *type = artist_track_album_combo_get_selected(
                        GTK_COMBO_BOX(selcombo));
        }
        gtk_widget_hide(GTK_WIDGET(t->window));
        lastfm_tagwin_unref(t);
        return retvalue;
}
