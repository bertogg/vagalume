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
        GtkWidget *button;
        GtkProgressBar *progressbar;
        char *url;
        char *dstpath;
        gboolean finished;
        dlwin_cb callback;
        gpointer cbdata;
} dlwin;

static gboolean
dlwin_progress_cb(gpointer data, double dltotal, double dlnow)
{
        dlwin *w = (dlwin *) data;
        g_return_val_if_fail (w != NULL, FALSE);
        if (!w->finished && w->window) {
                const int bufsize = 30;
                char text[bufsize];
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
        return !w->finished;
}

static void
dlwin_cleanup (dlwin *w)
{
        g_free (w->url);
        g_free (w->dstpath);
        g_slice_free (dlwin, w);
}

static void
dlwin_destroyed_cb (GtkObject *object, dlwin *w)
{
        if (w->finished) {
                dlwin_cleanup (w);
        } else {
                w->window = NULL;
                w->finished = TRUE;
        }
}

static gpointer
dlwin_download_file_thread(gpointer data)
{
        gboolean success;
        dlwin *w = (dlwin *) data;

        /* Remove the file if it exists and then download it */
        unlink(w->dstpath);
        success = http_download_file(w->url, w->dstpath, dlwin_progress_cb, w);

        /* Remove if the download was unsuccessful */
        if (!success)
                unlink (w->dstpath);

        if (w->callback) {
                w->callback (success, w->cbdata);
        }

        gdk_threads_enter();
        if (w->window) {
                gtk_progress_bar_set_text (w->progressbar,
                                           success ?
                                           _("Download complete!") :
                                           _("Download error!"));
                gtk_button_set_label (GTK_BUTTON (w->button), _("C_lose"));
                w->finished = TRUE;
        } else {
                dlwin_cleanup (w);
        }
        gdk_threads_leave();

        return NULL;
}

void
dlwin_download_file(const char *url, const char *filename,
                    const char *dstpath, dlwin_cb cb, gpointer cbdata)
{
        GtkWidget *label;
        const int textsize = 100;
        char text[textsize];
        GtkBox *box;
        dlwin *w;

        g_return_if_fail (url && filename && dstpath);

        w = g_slice_new(dlwin);
        w->finished = FALSE;
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
        w->button = gtk_button_new_with_mnemonic(_("_Cancel"));
        box = GTK_BOX(gtk_vbox_new(TRUE, 5));

        /* Widget packing */
        gtk_container_add(GTK_CONTAINER(w->window), GTK_WIDGET(box));
        gtk_box_pack_start(box, label, FALSE, FALSE, 0);
        gtk_box_pack_start(box, GTK_WIDGET(w->progressbar), FALSE, FALSE, 0);
        gtk_box_pack_start(box, w->button, FALSE, FALSE, 0);

#ifdef MAEMO
        /* Small hack to make this window a bit less ugly */
        gtk_widget_set_size_request (w->button, 300, 80);
#endif

        g_signal_connect_swapped (G_OBJECT (w->button), "clicked",
                                  G_CALLBACK (gtk_widget_destroy), w->window);
        g_signal_connect (G_OBJECT (w->window), "destroy",
                          G_CALLBACK (dlwin_destroyed_cb), w);

        gtk_widget_show_all(w->window);
        g_thread_create(dlwin_download_file_thread, w, FALSE, NULL);
}
