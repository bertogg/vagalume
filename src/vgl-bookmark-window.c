/*
 * vgl-bookmark-window.c -- Bookmark manager window
 * Copyright (C) 2008 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "vgl-bookmark-window.h"
#include "controller.h"
#include "uimisc.h"

#include <glib/gi18n.h>
#include <string.h>

/* Singleton, only one bookmark window can exist */
static VglBookmarkWindow *bookmark_window = NULL;

enum {
        NAME_COLUMN,
        URL_COLUMN,
        N_COLUMNS
};

#define VGL_BOOKMARK_WINDOW_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), VGL_TYPE_BOOKMARK_WINDOW, \
                                      VglBookmarkWindowPrivate))
G_DEFINE_TYPE (VglBookmarkWindow, vgl_bookmark_window, GTK_TYPE_DIALOG);

typedef struct _VglBookmarkWindowPrivate VglBookmarkWindowPrivate;
struct _VglBookmarkWindowPrivate {
        GtkTreeView *treeview;
        GtkWidget *playbtn, *addbtn, *editbtn, *delbtn, *closebtn;
};

static void vgl_bookmark_window_close(void);

static gboolean
edit_bookmark_dialog(GtkWindow *parent, char **name, char **url)
{
        GtkWidget *dialog;
        GtkWidget *namelabel, *urllabel;
        GtkEntry *nameentry, *urlentry;
        GtkBox *vbox;
        gboolean done, retvalue;
        dialog = gtk_dialog_new_with_buttons(
                *name ? _("Edit bookmark") : _("Add bookmark"), parent,
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog),
                                        GTK_RESPONSE_ACCEPT);

        namelabel = gtk_label_new(_("Bookmark name"));
        urllabel = gtk_label_new(_("Last.fm radio address"));
        nameentry = GTK_ENTRY(gtk_entry_new());
        urlentry = GTK_ENTRY(gtk_entry_new());

        if (*name != NULL) gtk_entry_set_text(nameentry, *name);
        if (*url != NULL) gtk_entry_set_text(urlentry, *url);

        vbox = GTK_BOX (GTK_DIALOG (dialog)->vbox);
        gtk_box_pack_start(vbox, namelabel, FALSE, FALSE, 4);
        gtk_box_pack_start(vbox, GTK_WIDGET(nameentry), FALSE, FALSE, 4);
        gtk_box_pack_start(vbox, urllabel, FALSE, FALSE, 4);
        gtk_box_pack_start(vbox, GTK_WIDGET(urlentry), FALSE, FALSE, 4);
        gtk_entry_set_activates_default(nameentry, TRUE);
        gtk_entry_set_activates_default(urlentry, TRUE);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        done = FALSE;
        while (!done) {
                if (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT) {
                        char *tmpname, *tmpurl;
                        tmpname = g_strdup(gtk_entry_get_text(nameentry));
                        tmpurl = g_strdup(gtk_entry_get_text(urlentry));
                        g_strstrip(tmpname);
                        g_strstrip(tmpurl);
                        if (tmpname[0] == '\0') {
                                ui_info_dialog(GTK_WINDOW(dialog),
                                               _("Invalid bookmark name"));
                                gtk_widget_grab_focus(GTK_WIDGET(nameentry));
                        } else if (strncmp(tmpurl, "lastfm://", 9)) {
                                ui_info_dialog(GTK_WINDOW(dialog),
                                               _("Last.fm radio URLs must "
                                                 "start with lastfm://"));
                                gtk_widget_grab_focus(GTK_WIDGET(urlentry));
                        } else {
                                retvalue = done = TRUE;
                                g_free(*name);
                                g_free(*url);
                                *name = tmpname;
                                *url = tmpurl;
                        }
                        if (!done) {
                                g_free(tmpname);
                                g_free(tmpurl);
                        }
                } else {
                        retvalue = FALSE;
                        done = TRUE;
                }
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return retvalue;
}

static void
play_button_clicked(GtkWidget *widget, VglBookmarkWindow *win)
{
        GtkTreeSelection *sel;
        GtkTreeIter iter;
        GtkTreeModel *model;
        char *url = NULL;
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        sel = gtk_tree_view_get_selection(priv->treeview);
        if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
                GValue val = { 0, };
                gtk_tree_model_get_value(model, &iter, URL_COLUMN, &val);
                url = g_strdup(g_value_get_string(&val));
                g_value_unset(&val);
        } else {
                g_critical("Play button clicked with no item selected!");
        }
        vgl_bookmark_window_close();
        if (url != NULL) {
                controller_play_radio_by_url(url);
                g_free(url);
        }
}

static void
add_button_clicked(GtkWidget *widget, VglBookmarkWindow *win)
{
        GtkTreeIter iter;
        char *name = NULL, *url = NULL;
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        if (edit_bookmark_dialog(GTK_WINDOW(win), &name, &url)) {
                GtkListStore *st;
                st = GTK_LIST_STORE(gtk_tree_view_get_model(priv->treeview));
                gtk_list_store_append(st, &iter);
                gtk_list_store_set(st, &iter, NAME_COLUMN, name, -1);
                gtk_list_store_set(st, &iter, URL_COLUMN, url, -1);
                g_free(name);
                g_free(url);
        }
}

static void
edit_button_clicked(GtkWidget *widget, VglBookmarkWindow *win)
{
        GtkTreeSelection *sel;
        GtkTreeIter iter;
        GtkTreeModel *model;
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        sel = gtk_tree_view_get_selection(priv->treeview);
        if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
                GtkListStore *store = GTK_LIST_STORE(model);
                char *name, *url;
                GValue nameval = { 0, };
                GValue urlval = { 0, };
                gtk_tree_model_get_value(model, &iter, NAME_COLUMN, &nameval);
                gtk_tree_model_get_value(model, &iter, URL_COLUMN, &urlval);
                name = g_strdup(g_value_get_string(&nameval));
                url = g_strdup(g_value_get_string(&urlval));
                if (edit_bookmark_dialog(GTK_WINDOW(win), &name, &url)) {
                        gtk_list_store_set(store, &iter, NAME_COLUMN, name, -1);
                        gtk_list_store_set(store, &iter, URL_COLUMN, url, -1);
                }
                g_free(name);
                g_free(url);
                g_value_unset(&nameval);
                g_value_unset(&urlval);
        } else {
                g_critical("Edit button clicked with no item selected!");
        }
}

static void
delete_button_clicked(GtkWidget *widget, VglBookmarkWindow *win)
{
        GtkTreeSelection *sel;
        GtkTreeIter iter;
        GtkTreeModel *model;
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        sel = gtk_tree_view_get_selection(priv->treeview);
        if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        } else {
                g_critical("Delete button clicked with no item selected!");
        }
}

static void
close_button_clicked(GtkWidget *widget, VglBookmarkWindow *win)
{
        vgl_bookmark_window_close();
}

static void
selection_changed(GtkTreeSelection *sel, VglBookmarkWindow *win)
{
        gboolean selected;
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        selected = gtk_tree_selection_get_selected(sel, NULL, NULL);
        gtk_widget_set_sensitive(priv->playbtn, selected);
        gtk_widget_set_sensitive(priv->editbtn, selected);
        gtk_widget_set_sensitive(priv->delbtn, selected);
}

static void
vgl_bookmark_window_finalize (GObject *object)
{
        g_signal_handlers_destroy(object);
        G_OBJECT_CLASS(vgl_bookmark_window_parent_class)->finalize(object);
}

static void
vgl_bookmark_window_class_init (VglBookmarkWindowClass *klass)
{
        GObjectClass *obj_class = (GObjectClass *)klass;
        obj_class->finalize = vgl_bookmark_window_finalize;
        g_type_class_add_private(obj_class, sizeof(VglBookmarkWindowPrivate));
}

static void
vgl_bookmark_window_init (VglBookmarkWindow *self)
{
        GtkWidget *scrwindow;
        GtkBox *vbox, *action_area;
        GtkCellRenderer *render;
        GtkTreeViewColumn *col;
        GtkTreeSelection *sel;
        GtkListStore *store;
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(self);

        /* Create widgets and private data */
        store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
        scrwindow = gtk_scrolled_window_new(NULL, NULL);
        priv->treeview = GTK_TREE_VIEW(gtk_tree_view_new());
        priv->playbtn = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
        priv->addbtn = gtk_button_new_from_stock(GTK_STOCK_ADD);
        priv->editbtn = gtk_button_new_from_stock(GTK_STOCK_EDIT);
        priv->delbtn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
        priv->closebtn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
        render = gtk_cell_renderer_text_new();
        col = gtk_tree_view_column_new_with_attributes("Bookmark", render,
                                                       "text", NAME_COLUMN,
                                                       NULL);
        sel = gtk_tree_view_get_selection(priv->treeview);

        /* Configure widgets */
        gtk_window_set_title(GTK_WINDOW(self), _("Bookmarks"));
        gtk_window_set_modal(GTK_WINDOW(self), TRUE);
        gtk_window_set_destroy_with_parent(GTK_WINDOW(self), TRUE);
        gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);

        gtk_tree_view_set_model(priv->treeview, GTK_TREE_MODEL(store));
        g_object_unref(store);
        gtk_tree_view_set_headers_visible(priv->treeview, FALSE);
        gtk_tree_view_append_column(priv->treeview, col);

        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwindow),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);

        g_signal_connect(priv->playbtn, "clicked",
                         G_CALLBACK(play_button_clicked), self);
        g_signal_connect(priv->addbtn, "clicked",
                         G_CALLBACK(add_button_clicked), self);
        g_signal_connect(priv->editbtn, "clicked",
                         G_CALLBACK(edit_button_clicked), self);
        g_signal_connect(priv->delbtn, "clicked",
                         G_CALLBACK(delete_button_clicked), self);
        g_signal_connect(priv->closebtn, "clicked",
                         G_CALLBACK(close_button_clicked), self);
        g_signal_connect(sel, "changed", G_CALLBACK(selection_changed), self);
        g_signal_connect(self, "delete-event",
                         G_CALLBACK(vgl_bookmark_window_close), NULL);

        /* Set initial state of buttons */
        selection_changed(sel, self);

        /* Add widgets to window */
        vbox = GTK_BOX(GTK_DIALOG(self)->vbox);
        action_area = GTK_BOX(GTK_DIALOG(self)->action_area);
        gtk_container_add(GTK_CONTAINER(scrwindow),
                          GTK_WIDGET(priv->treeview));
        gtk_box_pack_start(vbox, scrwindow, TRUE, TRUE, 0);
        gtk_box_pack_start(action_area, priv->playbtn,  FALSE, FALSE, 0);
        gtk_box_pack_start(action_area, priv->addbtn,   FALSE, FALSE, 0);
        gtk_box_pack_start(action_area, priv->editbtn,  FALSE, FALSE, 0);
        gtk_box_pack_start(action_area, priv->delbtn,   FALSE, FALSE, 0);
        gtk_box_pack_start(action_area, priv->closebtn, FALSE, FALSE, 0);

        gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(self)));
}

static GtkWidget *
vgl_bookmark_window_new(GtkWindow *parent)
{
        GtkWidget *win = g_object_new(VGL_TYPE_BOOKMARK_WINDOW, NULL);
        if (parent) {
                gtk_window_set_transient_for(GTK_WINDOW(win), parent);
        }
        return win;
}

static void
vgl_bookmark_window_close(void)
{
        gtk_widget_hide(GTK_WIDGET(bookmark_window));
}

void
vgl_bookmark_window_show(GtkWindow *parent)
{
        if (bookmark_window == NULL) {
                bookmark_window = VGL_BOOKMARK_WINDOW(
                        vgl_bookmark_window_new(parent));
        }
        gtk_window_present(GTK_WINDOW(bookmark_window));
}
