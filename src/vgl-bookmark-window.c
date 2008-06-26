/*
 * vgl-bookmark-window.c -- Bookmark manager window
 * Copyright (C) 2008 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "vgl-bookmark-window.h"
#include "vgl-bookmark-mgr.h"
#include "controller.h"
#include "uimisc.h"

#include <glib/gi18n.h>
#include <string.h>

enum {
        ID_COLUMN,
        NAME_COLUMN,
        URL_COLUMN,
        N_COLUMNS
};

enum {
        BMK_ADDED,
        BMK_CHANGED,
        BMK_REMOVED,
        N_SIGNALS
};

#define VGL_BOOKMARK_WINDOW_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), VGL_TYPE_BOOKMARK_WINDOW, \
                                      VglBookmarkWindowPrivate))
G_DEFINE_TYPE (VglBookmarkWindow, vgl_bookmark_window, GTK_TYPE_DIALOG);

typedef struct _VglBookmarkWindowPrivate VglBookmarkWindowPrivate;
struct _VglBookmarkWindowPrivate {
        GtkTreeView *treeview;
        VglBookmarkMgr *mgr;
        GtkWidget *playbtn, *addbtn, *editbtn, *delbtn, *closebtn;
        gulong sig[N_SIGNALS];
};

static void vgl_bookmark_window_close(VglBookmarkWindow *win);

/* After a drag and drop, make sure that the bookmark manager keeps
 * the items in the same order as the treeview */
static void
drag_end_cb (GtkWidget *widget, GdkDragContext *ctx, VglBookmarkWindow *win)
{
        g_return_if_fail (VGL_IS_BOOKMARK_WINDOW (win));
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE (win);
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model (priv->treeview);
        gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
        int length = gtk_tree_model_iter_n_children (model, NULL);
        int *ids = g_new (int, length + 1); /* List of bookmark IDs */
        int pos = 0;
        while (valid) {
                GValue val = { 0 };
                gtk_tree_model_get_value (model, &iter, ID_COLUMN, &val);
                ids [pos++] = g_value_get_int (&val);
                g_value_unset (&val);
                valid = gtk_tree_model_iter_next (model, &iter);
        }
        ids [pos] = -1; /* Terminate the list */
        vgl_bookmark_mgr_reorder (priv->mgr, ids);
        g_free (ids);
}

static gboolean
find_bookmark_by_id(GtkTreeModel *model, int id, GtkTreeIter *iter)
{
        gboolean valid = gtk_tree_model_get_iter_first(model, iter);
        while (valid) {
                GValue val = { 0 };
                int this_id;
                gtk_tree_model_get_value(model, iter, ID_COLUMN, &val);
                this_id = g_value_get_int(&val);
                g_value_unset(&val);
                if (this_id == id) {
                        return TRUE;
                }
                valid = gtk_tree_model_iter_next(model, iter);
        }
        return FALSE;
}

static void
bookmark_added_cb(VglBookmarkMgr *mgr, VglBookmark *bmk,
                  VglBookmarkWindow *win)
{
        g_return_if_fail(VGL_IS_BOOKMARK_WINDOW(win) && bmk != NULL);
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        GtkTreeIter iter;
        GtkListStore *st;
        st = GTK_LIST_STORE(gtk_tree_view_get_model(priv->treeview));
        gtk_list_store_append(st, &iter);
        gtk_list_store_set(st, &iter, ID_COLUMN, bmk->id, -1);
        gtk_list_store_set(st, &iter, NAME_COLUMN, bmk->name, -1);
        gtk_list_store_set(st, &iter, URL_COLUMN, bmk->url, -1);
}

static void
bookmark_changed_cb(VglBookmarkMgr *mgr, VglBookmark *bmk,
                    VglBookmarkWindow *win)
{
        g_return_if_fail(VGL_IS_BOOKMARK_WINDOW(win) && bmk != NULL);
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model(priv->treeview);
        GtkListStore *st = GTK_LIST_STORE(model);
        if (find_bookmark_by_id(model, bmk->id, &iter)) {
                gtk_list_store_set(st, &iter, NAME_COLUMN, bmk->name, -1);
                gtk_list_store_set(st, &iter, URL_COLUMN, bmk->url, -1);
        } else {
                g_critical("%s: Bookmark with id %d not found in TreeModel",
                           __FUNCTION__, bmk->id);
        }
}

static void
bookmark_removed_cb(VglBookmarkMgr *mgr, int id, VglBookmarkWindow *win)
{
        g_return_if_fail(VGL_IS_BOOKMARK_WINDOW(win) && id >= 0);
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model(priv->treeview);
        if (find_bookmark_by_id(model, id, &iter)) {
                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        } else {
                g_critical("%s: Bookmark with id %d not found in TreeModel",
                           __FUNCTION__, id);
        }
}

static void
play_selected_row(VglBookmarkWindow *win)
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
        vgl_bookmark_window_close(win);
        if (url != NULL) {
                controller_play_radio_by_url(url);
                g_free(url);
        }
}

static void
play_button_clicked(GtkWidget *widget, VglBookmarkWindow *win)
{
        play_selected_row (win);
}

static void
add_button_clicked(GtkWidget *widget, VglBookmarkWindow *win)
{
        char *name = NULL, *url = NULL;
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        if (ui_edit_bookmark_dialog(GTK_WINDOW(win), &name, &url, TRUE)) {
                vgl_bookmark_mgr_add_bookmark(priv->mgr, name, url);
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
                int id;
                char *name, *url;
                GValue idval = { 0, };
                GValue nameval = { 0, };
                GValue urlval = { 0, };
                gtk_tree_model_get_value(model, &iter, ID_COLUMN, &idval);
                gtk_tree_model_get_value(model, &iter, NAME_COLUMN, &nameval);
                gtk_tree_model_get_value(model, &iter, URL_COLUMN, &urlval);
                id = g_value_get_int(&idval);
                name = g_strdup(g_value_get_string(&nameval));
                url = g_strdup(g_value_get_string(&urlval));
                if (ui_edit_bookmark_dialog(GTK_WINDOW(win),
                                            &name, &url, FALSE)) {
                        vgl_bookmark_mgr_change_bookmark(priv->mgr,
                                                         id, name, url);
                }
                g_free(name);
                g_free(url);
                g_value_unset(&idval);
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
                GValue val = { 0, };
                int id;
                gtk_tree_model_get_value(model, &iter, ID_COLUMN, &val);
                id = g_value_get_int(&val);
                vgl_bookmark_mgr_remove_bookmark(priv->mgr, id);
                g_value_unset(&val);
        } else {
                g_critical("Delete button clicked with no item selected!");
        }
}

static void
close_button_clicked(GtkWidget *widget, VglBookmarkWindow *win)
{
        vgl_bookmark_window_close(win);
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
row_clicked (GtkTreeView *tree_view, GtkTreePath *path,
             GtkTreeViewColumn *column, VglBookmarkWindow *win)
{
        play_selected_row (win);
}

static void
vgl_bookmark_window_finalize (GObject *object)
{
        VglBookmarkWindow *win = VGL_BOOKMARK_WINDOW(object);
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(win);
        g_signal_handler_disconnect (priv->mgr, priv->sig[BMK_ADDED]);
        g_signal_handler_disconnect (priv->mgr, priv->sig[BMK_CHANGED]);
        g_signal_handler_disconnect (priv->mgr, priv->sig[BMK_REMOVED]);
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
        const GList *bookmarks;
        VglBookmarkWindowPrivate *priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE(self);

        /* Create widgets and private data */
        store = gtk_list_store_new(N_COLUMNS, G_TYPE_INT, G_TYPE_STRING,
                                   G_TYPE_STRING);
        scrwindow = gtk_scrolled_window_new(NULL, NULL);
        priv->treeview = GTK_TREE_VIEW(gtk_tree_view_new());
        priv->playbtn = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
        priv->addbtn = gtk_button_new_from_stock(GTK_STOCK_ADD);
        priv->editbtn = gtk_button_new_from_stock(GTK_STOCK_EDIT);
        priv->delbtn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
        priv->closebtn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
        priv->mgr = vgl_bookmark_mgr_get_instance();
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
        gtk_tree_view_set_reorderable(priv->treeview, TRUE);

        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwindow),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);

        /* Signals */
        g_signal_connect(priv->playbtn, "clicked",
                         G_CALLBACK(play_button_clicked), self);
        g_signal_connect(priv->treeview, "row-activated",
                         G_CALLBACK(row_clicked), self);
        g_signal_connect(priv->treeview, "drag-end",
                         G_CALLBACK(drag_end_cb), self);
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
                         G_CALLBACK(vgl_bookmark_window_close), self);

        priv->sig[BMK_ADDED] = g_signal_connect (
                priv->mgr, "bookmark-added",
                G_CALLBACK (bookmark_added_cb), self);
        priv->sig[BMK_CHANGED] = g_signal_connect (
                priv->mgr, "bookmark-changed",
                G_CALLBACK (bookmark_changed_cb), self);
        priv->sig[BMK_REMOVED] = g_signal_connect (
                priv->mgr, "bookmark-removed",
                G_CALLBACK (bookmark_removed_cb), self);

        /* Set initial state of buttons */
        selection_changed(sel, self);

        /* Load all existing bookmarks */
        bookmarks = vgl_bookmark_mgr_get_bookmark_list(priv->mgr);
        while (bookmarks != NULL) {
                VglBookmark *bmk = (VglBookmark *) bookmarks->data;
                bookmark_added_cb(priv->mgr, bmk, self);
                bookmarks = bookmarks->next;
        }

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
        GtkWidget *win = g_object_new (VGL_TYPE_BOOKMARK_WINDOW,
                                       "default-height", 300, NULL);
        if (parent) {
                gtk_window_set_transient_for(GTK_WINDOW(win), parent);
        }
        return win;
}

static void
vgl_bookmark_window_close(VglBookmarkWindow *win)
{
        VglBookmarkWindowPrivate *priv;
        priv = VGL_BOOKMARK_WINDOW_GET_PRIVATE (win);

        vgl_bookmark_mgr_save_to_disk (priv->mgr);

        gtk_widget_destroy(GTK_WIDGET(win));
}

void
vgl_bookmark_window_show(GtkWindow *parent)
{
        /* Singleton, only one bookmark window can exist */
        static GtkWidget *win = NULL;
        if (win == NULL) {
                win = vgl_bookmark_window_new (parent);
                g_object_add_weak_pointer (G_OBJECT (win), (gpointer) &win);
        }
        gtk_window_present (GTK_WINDOW (win));
}
