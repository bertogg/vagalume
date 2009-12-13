/*
 * dlwin.c -- Download window
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include "globaldefs.h"
#include "dlwin.h"
#include "http.h"
#include "vgl-object.h"
#include "compat.h"

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <string.h>

typedef struct {
        VglObject parent;
        GtkWidget *window;
        GtkWidget *button;
        GtkProgressBar *progressbar;
        char *url;
        char *dstpath;
        double dlnow;
        double dltotal;
        gboolean win_needs_update;
        gboolean success;
        guint timeout_id;
        dlwin_cb callback;
        gpointer cbdata;
} dlwin;

static gboolean
update_window                           (gpointer data)
{
        dlwin *w = data;
        if (w->window && w->win_needs_update) {
                const int bufsize = 30;
                char text[bufsize];
                guint now = w->dlnow / 1024;
                guint total = w->dltotal / 1024;
                double fraction = w->dlnow / w->dltotal;
                fraction = CLAMP (fraction, 0, 1);
                snprintf(text, bufsize, "%u / %u KB", now, total);
                gtk_progress_bar_set_text(w->progressbar, text);
                gtk_progress_bar_set_fraction(w->progressbar, fraction);
                w->win_needs_update = FALSE;
        }
        return TRUE;
}

static gboolean
dlwin_progress_cb                       (gpointer data,
                                         double   dltotal,
                                         double   dlnow)
{
        dlwin *w = data;
        gdk_threads_enter ();
        w->dlnow = dlnow;
        w->dltotal = dltotal;
        w->win_needs_update = TRUE;
        gdk_threads_leave ();
        return (w->window != NULL);
}

static void
dlwin_destroy                           (gpointer data)
{
        dlwin *w = data;
        g_free (w->url);
        g_free (w->dstpath);
}

static gboolean
dlwin_download_file_idle                (gpointer data)
{
        dlwin *w = data;

        if (w->callback) {
                w->callback (w->success, w->cbdata);
        }

        /* Remove the timeout handler */
        g_source_remove (w->timeout_id);

        if (w->window) {
                update_window (w);
                gtk_progress_bar_set_text (w->progressbar,
                                           w->success ?
                                           _("Download complete!") :
                                           _("Download error!"));
                gtk_button_set_label (GTK_BUTTON (w->button),
                                      _("C_lose"));
        }

        vgl_object_unref (w);

        return FALSE;
}

static gpointer
dlwin_download_file_thread              (gpointer data)
{
        dlwin *w = (dlwin *) data;

        /* Remove the file if it exists and then download it */
        g_unlink (w->dstpath);
        w->success = http_download_file (w->url, w->dstpath,
                                         dlwin_progress_cb, w);

        /* Remove if the download was unsuccessful */
        if (!w->success)
                g_unlink (w->dstpath);

        gdk_threads_add_idle (dlwin_download_file_idle, w);

        return NULL;
}

void
dlwin_download_file                     (const char *url,
                                         const char *filename,
                                         const char *dstpath,
                                         dlwin_cb    cb,
                                         gpointer    cbdata)
{
        GtkWidget *label;
        const int textsize = 100;
        char text[textsize];
        GtkBox *box;
        dlwin *w;

        g_return_if_fail (url && filename && dstpath);

        w = vgl_object_new (dlwin, dlwin_destroy);
        w->url = g_strdup(url);
        w->dstpath = g_strdup(dstpath);
        w->dlnow = 0.0;
        w->dltotal = 0.0;
        w->success = FALSE;
        w->win_needs_update = FALSE;
        w->callback = cb;
        w->cbdata = cbdata;

        /* Widget creation */
        w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_container_set_border_width(GTK_CONTAINER(w->window), 10);
        gtk_window_set_icon_from_file(GTK_WINDOW(w->window), APP_ICON, NULL);
        snprintf(text, textsize, _("Downloading %s"), filename);
        text[textsize - 1] = '\0';
        gtk_window_set_title(GTK_WINDOW(w->window), text);
        snprintf(text, textsize, _("Downloading file\n%s"), filename);
        text[textsize - 1] = '\0';
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

        g_object_add_weak_pointer (G_OBJECT (w->window),
                                   (gpointer) &(w->window));
        g_signal_connect_swapped (G_OBJECT (w->button), "clicked",
                                  G_CALLBACK (gtk_widget_destroy), w->window);
        g_signal_connect_swapped (G_OBJECT (w->window), "destroy",
                                  G_CALLBACK (vgl_object_unref), w);

        w->timeout_id = gdk_threads_add_timeout_full
                (G_PRIORITY_DEFAULT_IDLE, 200, update_window,
                 vgl_object_ref (w), vgl_object_unref);
        gtk_widget_show_all(w->window);
        g_thread_create (dlwin_download_file_thread,
                         vgl_object_ref (w), FALSE, NULL);
}
