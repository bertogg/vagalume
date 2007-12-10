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
#include "util.h"

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

typedef enum {
        TAGCOMBO_STATE_NULL = 0,
        TAGCOMBO_STATE_LOADING,
        TAGCOMBO_STATE_READY
} tagcombo_state;

typedef struct {
        GtkWindow *window;
        GtkWidget *entrylabel;
        GtkEntry *entry;
        GtkComboBox *selcombo;
        GtkComboBox *globalcombo;
        GtkWidget *globallabel;
        lastfm_track *track;
        char *user;
        char *tags_artist;
        char *tags_track;
        char *tags_album;
        GtkTreeModel *poptags_artist;
        GtkTreeModel *poptags_track;
        GtkTreeModel *poptags_album;
        GtkTreeModel *nonemodel;
        tagcombo_state artist_state, track_state, album_state;
        int refcount;
} tagwin;

typedef struct {
        tagwin *w;
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
ui_create_options_list(const GList *elems)
{
        GtkTreeIter iter;
        const GList *current = elems;
        GtkListStore *store = gtk_list_store_new (1, G_TYPE_STRING);;

        for (; current != NULL; current = g_list_next(current)) {
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, current->data, -1);
        }
        return GTK_TREE_MODEL(store);
}

char *
ui_input_dialog_with_list(GtkWindow *parent, const char *title,
                          const char *text, const GList *elems,
                          const char *value)
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

static GtkComboBox *
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

        return GTK_COMBO_BOX(combo);
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


static void
tagwin_destroy(tagwin *w)
{
        g_return_if_fail(w != NULL);
        gtk_widget_destroy(GTK_WIDGET(w->window));
        lastfm_track_destroy(w->track);
        g_free(w->user);
        g_free(w->tags_artist);
        g_free(w->tags_track);
        g_free(w->tags_album);
        if (w->poptags_artist) g_object_unref(w->poptags_artist);
        if (w->poptags_track) g_object_unref(w->poptags_track);
        if (w->poptags_album) g_object_unref(w->poptags_album);
        if (w->nonemodel) g_object_unref(w->nonemodel);
        g_slice_free(tagwin, w);
}

static tagwin *
tagwin_create(void)
{
        tagwin *w = g_slice_new0(tagwin);
        w->refcount = 1;
        return w;
}

static tagwin *
tagwin_ref(tagwin *w)
{
        g_return_val_if_fail (w != NULL && w->refcount > 0, NULL);
        w->refcount++;
        return w;
}

static void
tagwin_unref(tagwin *w)
{
        g_return_if_fail (w != NULL && w->refcount > 0);
        w->refcount--;
        if (w->refcount == 0) tagwin_destroy(w);
}

static gpointer
get_track_tags_thread(gpointer userdata)
{
        get_track_tags_data *data = (get_track_tags_data *) userdata;
        g_return_val_if_fail(data && data->w && data->w->track, NULL);
        GtkTreeModel *model = NULL;
        char *usertags;
        GList *userlist = NULL;
        GList *globallist = NULL;

        lastfm_get_user_track_tags(data->w->user, data->w->track,
                                   data->type, &userlist);
        lastfm_get_track_tags(data->w->track, data->type, &globallist);

        gdk_threads_enter();
        usertags = str_glist_join(userlist, ", ");
        if (globallist != NULL) {
                model = ui_create_options_list(globallist);
        }
        switch (data->type) {
        case REQUEST_ARTIST:
                data->w->poptags_artist = model;
                data->w->tags_artist = usertags;
                data->w->artist_state = TAGCOMBO_STATE_READY;
                break;
        case REQUEST_TRACK:
                data->w->poptags_track = model;
                data->w->tags_track = usertags;
                data->w->track_state = TAGCOMBO_STATE_READY;
                break;
        case REQUEST_ALBUM:
                data->w->poptags_album = model;
                data->w->tags_album = usertags;
                data->w->album_state = TAGCOMBO_STATE_READY;
                break;
        default:
                gdk_threads_leave();
                g_return_val_if_reached(NULL);
        }
        tagwin_selcombo_changed(data->w->selcombo, data->w);
        tagwin_unref(data->w);
        gdk_threads_leave();

        g_list_foreach(userlist, (GFunc) g_free, NULL);
        g_list_free(userlist);
        g_list_foreach(globallist, (GFunc) g_free, NULL);
        g_list_free(globallist);
        g_slice_free(get_track_tags_data, data);
        return NULL;
}

static void
tagwin_selcombo_changed(GtkComboBox *combo, gpointer data)
{
        tagwin *w = (tagwin *) data;
        GtkTreeModel *model = NULL;
        const char *usertags = NULL;
        tagcombo_state oldstate;
        request_type type = artist_track_album_combo_get_selected(combo);
        switch (type) {
        case REQUEST_ARTIST:
                oldstate = w->artist_state;
                if (oldstate == TAGCOMBO_STATE_READY) {
                        model = w->poptags_artist;
                        usertags = w->tags_artist;
                } else {
                        w->artist_state = TAGCOMBO_STATE_LOADING;
                }
                break;
        case REQUEST_TRACK:
                oldstate = w->track_state;
                if (oldstate == TAGCOMBO_STATE_READY) {
                        model = w->poptags_track;
                        usertags = w->tags_track;
                } else {
                        w->track_state = TAGCOMBO_STATE_LOADING;
                }
                break;
        case REQUEST_ALBUM:
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
        gtk_widget_set_sensitive(GTK_WIDGET(w->globallabel), model != NULL);
        if (oldstate == TAGCOMBO_STATE_READY) {
                if (model != NULL) {
                        gtk_combo_box_set_model(w->globalcombo, model);
                } else {
                        gtk_combo_box_set_model(w->globalcombo, w->nonemodel);
                        gtk_combo_box_set_active(w->globalcombo, 0);
                }
                gtk_entry_set_editable(w->entry, TRUE);
                gtk_entry_set_text(w->entry, usertags ? usertags : "");
        } else {
                gtk_entry_set_editable(w->entry, FALSE);
                gtk_entry_set_text(w->entry, "");
        }
        if (oldstate == TAGCOMBO_STATE_NULL) {
                get_track_tags_data *data = g_slice_new(get_track_tags_data);
                data->w = tagwin_ref(w);
                data->type = type;
                g_thread_create(get_track_tags_thread, data, FALSE, NULL);
        }
}

static void
tagwin_tagcombo_changed(GtkComboBox *combo, gpointer data)
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
        gtk_combo_box_set_active(combo, -1);
}

gboolean
tagwin_run(GtkWindow *parent, const char *user, char **newtags,
           const GList *usertags, const lastfm_track *track,
           request_type *type)
{
        g_return_val_if_fail(track && type && user && newtags, FALSE);
        tagwin *t;
        gboolean retvalue = FALSE;
        GtkBox *combosbox, *userbox, *globalbox, *selbox;
        GtkWidget *sellabel;
        GtkComboBox *selcombo;
        GtkWidget *entrylabel, *entry;
        GtkWidget *userlabel, *usercombo, *globallabel, *globalcombo;
        GtkTreeModel *usermodel, *nonemodel;
        GtkCellRenderer *userrender, *globalrender;
        GtkDialog *dialog;
        GList *nonelist;

        /* A treemodel for combos with no elements */
        nonelist = g_list_append(NULL, "(none)");
        nonemodel = ui_create_options_list(nonelist);
        g_list_free(nonelist);

        /* Dialog and basic settings */
        dialog = ui_base_dialog(parent, "Tagging");
        gtk_box_set_homogeneous(GTK_BOX(dialog->vbox), FALSE);
        gtk_box_set_spacing(GTK_BOX(dialog->vbox), 5);

        /* Combo to select what to tag */
        selbox = GTK_BOX(gtk_hbox_new(FALSE, 10));
        sellabel = gtk_label_new("Tag this");
        selcombo = artist_track_album_selection_combo(track);
        gtk_box_pack_start(selbox, sellabel, FALSE, FALSE, 0);
        gtk_box_pack_start(selbox, GTK_WIDGET(selcombo), TRUE, TRUE, 0);

        switch (*type) {
        case REQUEST_ARTIST:
                gtk_combo_box_set_active(selcombo, 0);
                break;
        case REQUEST_TRACK:
                gtk_combo_box_set_active(selcombo, 1);
                break;
        case REQUEST_ALBUM:
                g_return_val_if_fail(track->album[0] != '\0', FALSE);
                gtk_combo_box_set_active(selcombo, 2);
                break;
        default:
                g_return_val_if_reached(FALSE);
                break;
        }

        /* Text entry */
        entrylabel = gtk_label_new("Enter a comma-separated list of tags");
        entry = gtk_entry_new();

        /* Combo boxes */
        userlabel = gtk_label_new("Your favourite tags");
        globallabel = gtk_label_new("Popular tags for this");
        if (usertags != NULL) {
                usermodel = ui_create_options_list(usertags);
        } else {
                usermodel = g_object_ref(nonemodel);
        }
        usercombo = gtk_combo_box_new_with_model(usermodel);
        globalcombo = gtk_combo_box_new_with_model(nonemodel);
        g_object_unref(G_OBJECT(usermodel));
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
        if (usertags == NULL) {
                gtk_widget_set_sensitive(GTK_WIDGET(userlabel), FALSE);
                gtk_widget_set_sensitive(GTK_WIDGET(usercombo), FALSE);
                gtk_combo_box_set_active(GTK_COMBO_BOX(usercombo), 0);
        }

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

        t = tagwin_create();
        t->track = lastfm_track_copy(track);
        t->window = GTK_WINDOW(dialog);
        t->entry = GTK_ENTRY(entry);
        t->entrylabel = entrylabel;
        t->selcombo = selcombo;
        t->globalcombo = GTK_COMBO_BOX(globalcombo);
        t->globallabel = globallabel;
        t->nonemodel = nonemodel;
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
        tagwin_unref(t);
        return retvalue;
}

gboolean
recommwin_run(GtkWindow *parent, char **user, char **message,
              const GList *friends, const lastfm_track *track,
              request_type *type)
{
        g_return_val_if_fail(user && message && track && type, FALSE);
        gboolean retval = FALSE;
        GtkDialog *dialog;
        GtkTreeModel *usermodel;
        GtkBox *selbox, *userbox;
        GtkWidget *sellabel;
        GtkComboBox *selcombo;
        GtkWidget *userlabel, *usercombo;
        GtkWidget *textframe, *textlabel;
        GtkTextView *textview;
        GtkWidget *sw;

        /* Dialog and basic settings */
        dialog = ui_base_dialog(parent, "Send a recommendation");
        gtk_box_set_homogeneous(GTK_BOX(dialog->vbox), FALSE);
        gtk_box_set_spacing(GTK_BOX(dialog->vbox), 5);

        /* Combo to select what to recommend */
        selbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        sellabel = gtk_label_new("Recommend this");
        selcombo = artist_track_album_selection_combo(track);
        gtk_box_pack_start(selbox, sellabel, FALSE, FALSE, 0);
        gtk_box_pack_start(selbox, GTK_WIDGET(selcombo), TRUE, TRUE, 0);

        switch (*type) {
        case REQUEST_ARTIST:
                gtk_combo_box_set_active(selcombo, 0);
                break;
        case REQUEST_TRACK:
                gtk_combo_box_set_active(selcombo, 1);
                break;
        case REQUEST_ALBUM:
                g_return_val_if_fail(track->album[0] != '\0', FALSE);
                gtk_combo_box_set_active(selcombo, 2);
                break;
        default:
                g_return_val_if_reached(FALSE);
                break;
        }

        /* Combo to select the recipient of the recommendation */
        userbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        userlabel = gtk_label_new("Send recommendation to");
        usermodel = ui_create_options_list(friends);
        usercombo = gtk_combo_box_entry_new_with_model(usermodel, 0);
        g_object_unref(usermodel);
        gtk_box_pack_start(userbox, userlabel, FALSE, FALSE, 0);
        gtk_box_pack_start(userbox, usercombo, TRUE, TRUE, 0);

        /* Message of the recommendation */
        textlabel = gtk_label_new("Recommendation message");
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
        gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(selbox),
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(userbox),
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(textlabel),
                           TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(textframe),
                           TRUE, TRUE, 0);

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
