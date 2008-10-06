/*
 * dlwin.c -- Download window
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include "globaldefs.h"
#include "dlwin.h"
#include "http.h"

#include <gtk/gtk.h>
#include <unistd.h>
#include <string.h>

typedef struct {
        GtkWidget *window;
        GtkWidget *cancelbutton;
        GtkWidget *closebutton;
        GtkProgressBar *progressbar;
        char *url;
        char *dstpath;
        gboolean cancelled;
        dlwin_cb callback;
        gpointer cbdata;
} dlwin;

static gboolean
dlwin_progress_cb(gpointer data, double dltotal, double dlnow)
{
        dlwin *w = (dlwin *) data;
        g_return_val_if_fail(w && w->window && w->progressbar, FALSE);
        const int bufsize = 30;
        char text[bufsize];
        if (!w->cancelled) {
                guint now = dlnow / 1024;
                guint total = dltotal / 1024;
                double fraction = dlnow / dltotal;
                if (fraction > 1) fraction = 1;
                if (fraction < 0) fraction = 0;
                snprintf(text, bufsize, "%u / %u KB", now, total);
                gdk_threads_enter();
                gtk_progress_bar_set_text(w->progressbar, text);
                gtk_progress_bar_set_fraction(w->progressbar, fraction);
                gdk_threads_leave();
        }
        return !w->cancelled;
}

static void
cancel_clicked(GtkWidget *widget, gpointer data)
{
        dlwin *w = (dlwin *) data;
        g_return_if_fail(w != NULL);
        w->cancelled = TRUE;
        gtk_widget_hide(w->window);
}

static void
close_clicked(GtkWidget *widget, gpointer data)
{
        dlwin *w = (dlwin *) data;
        g_return_if_fail(w != NULL);
        gtk_widget_destroy(w->window);
        /* Cleanup */
        g_free(w->url);
        g_free(w->dstpath);
        g_slice_free(dlwin, w);
}

static gboolean
delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
        dlwin *w = (dlwin *) data;
        g_return_val_if_fail(w != NULL, FALSE);
        if (GTK_WIDGET_VISIBLE(w->closebutton)) {
                close_clicked(w->closebutton, w);
        } else {
                cancel_clicked(w->cancelbutton, w);
        }
        return TRUE;
}

static gpointer
dlwin_download_file_thread(gpointer data)
{
        dlwin *w = (dlwin *) data;
        g_return_val_if_fail(w && w->window && w->progressbar &&
                             w->url && w->dstpath, NULL);
        gboolean success;

        /* Remove the file if it exists and then download it */
        unlink(w->dstpath);
        success = http_download_file(w->url, w->dstpath, dlwin_progress_cb, w);

        gdk_threads_enter();
        if (success) {
                gtk_widget_set_sensitive(w->cancelbutton, FALSE);
                gtk_progress_bar_set_text(w->progressbar,
                                          _("Download complete!"));
                gtk_widget_hide(w->cancelbutton);
                gtk_widget_show(w->closebutton);
        } else if (w->cancelled) {
                unlink(w->dstpath);
                close_clicked(w->closebutton, w);
        } else {
                unlink(w->dstpath);
                gtk_widget_set_sensitive(w->cancelbutton, FALSE);
                gtk_progress_bar_set_text(w->progressbar,
                                          _("Download error!"));
                gtk_widget_hide(w->cancelbutton);
                gtk_widget_show(w->closebutton);
        }
        gdk_threads_leave();

        if (w->callback) {
                w->callback (success, w->cbdata);
        }

        return NULL;
}

void
dlwin_download_file(const char *url, const char *filename,
                    const char *dstpath, dlwin_cb cb, gpointer cbdata)
{
        GtkWidget *label;
        const int textsize = 100;
        char text[textsize];
        GtkBox *box, *butbox;
        dlwin *w = g_slice_new(dlwin);
        w->cancelled = FALSE;
        w->url = g_strdup(url);
        w->dstpath = g_strdup(dstpath);
        w->callback = cb;
        w->cbdata = cbdata;

        /* Widget creation */
        w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_container_set_border_width(GTK_CONTAINER(w->window), 10);
        gtk_window_set_icon_from_file(GTK_WINDOW(w->window), APP_ICON, NULL);
        snprintf(text, textsize, _("Downloading %s"), filename);
        gtk_window_set_title(GTK_WINDOW(w->window), text);
        snprintf(text, textsize, _("Downloading file\n%s"), filename);
        label = gtk_label_new(text);
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
        w->progressbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
        w->cancelbutton = gtk_button_new_with_mnemonic(_("_Cancel"));
        w->closebutton = gtk_button_new_with_mnemonic(_("C_lose"));
        box = GTK_BOX(gtk_vbox_new(TRUE, 5));
        butbox = GTK_BOX(gtk_hbox_new(FALSE, 0));

        /* Widget packing */
        gtk_container_add(GTK_CONTAINER(w->window), GTK_WIDGET(box));
        gtk_box_pack_start(box, label, FALSE, FALSE, 0);
        gtk_box_pack_start(box, GTK_WIDGET(w->progressbar), FALSE, FALSE, 0);
        gtk_box_pack_start(box, GTK_WIDGET(butbox), FALSE, FALSE, 0);
        gtk_box_pack_start(butbox, w->cancelbutton, TRUE, FALSE, 0);
        gtk_box_pack_start(butbox, w->closebutton, TRUE, FALSE, 0);

#ifdef MAEMO
        /* Small hack to make this window a bit less ugly */
        gtk_widget_set_size_request(GTK_WIDGET(w->cancelbutton), 300, 80);
        gtk_widget_set_size_request(GTK_WIDGET(w->closebutton), 300, 80);
#endif

        g_signal_connect(G_OBJECT(w->cancelbutton), "clicked",
                         G_CALLBACK(cancel_clicked), w);
        g_signal_connect(G_OBJECT(w->closebutton), "clicked",
                         G_CALLBACK(close_clicked), w);
        g_signal_connect(G_OBJECT(w->window), "delete-event",
                         G_CALLBACK(delete_event), w);

        gtk_widget_show_all(w->window);
        gtk_widget_hide(w->closebutton);
        g_thread_create(dlwin_download_file_thread, w, FALSE, NULL);
}
