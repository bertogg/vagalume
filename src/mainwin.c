
#include <gtk/gtk.h>

#include "controller.h"
#include "mainwin.h"

void
mainwin_update_track_info(lastfm_mainwin *w, const char *playlist,
                          const char *artist, const char *track,
                          const char *album)
{
        g_return_if_fail(w != NULL && playlist != NULL);
        char *text;
        text = g_strconcat("Artist: ", artist, NULL);
        gtk_label_set_text(GTK_LABEL(w->artist), text);
        g_free(text);
        text = g_strconcat("Track: ", track, NULL);
        gtk_label_set_text(GTK_LABEL(w->track), text);
        g_free(text);
        text = g_strconcat("Album: ", album, NULL);
        gtk_label_set_text(GTK_LABEL(w->album), text);
        g_free(text);
        text = g_strconcat("Listening to ", playlist, NULL);
        gtk_label_set_text(GTK_LABEL(w->playlist), text);
        g_free(text);
}

void
mainwin_set_ui_state(lastfm_mainwin *w, lastfm_ui_state state)
{
        g_return_if_fail(w != NULL);
        switch (state) {
        case LASTFM_UI_STATE_STOPPED:
                gtk_label_set_text(GTK_LABEL(w->artist), NULL);
                gtk_label_set_text(GTK_LABEL(w->track), NULL);
                gtk_label_set_text(GTK_LABEL(w->album), NULL);
                gtk_label_set_text(GTK_LABEL(w->playlist), "Stopped");
                gtk_widget_set_sensitive (w->play, TRUE);
                gtk_widget_set_sensitive (w->stop, FALSE);
                gtk_widget_set_sensitive (w->next, FALSE);
                break;
        case LASTFM_UI_STATE_PLAYING:
                gtk_widget_set_sensitive (w->play, FALSE);
                gtk_widget_set_sensitive (w->stop, TRUE);
                gtk_widget_set_sensitive (w->next, TRUE);
                break;
        default:
                g_critical("Unknown ui state received: %d", state);
                break;
        }
}

static void
play_clicked(GtkWidget *widget, gpointer data)
{
        controller_start_playing();
}

static void
stop_clicked(GtkWidget *widget, gpointer data)
{
        controller_stop_playing();
}

static void
next_clicked(GtkWidget *widget, gpointer data)
{
        controller_skip_track();
}

static void
window_destroyed(GtkWidget *widget, gpointer data)
{
        controller_quit_app();
}

lastfm_mainwin *
lastfm_mainwin_create(void)
{
        lastfm_mainwin *w = g_new0(lastfm_mainwin, 1);
        w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(w->window), "Last.fm");
        gtk_container_set_border_width(GTK_CONTAINER(w->window), 10);
        w->hbox = gtk_hbox_new(TRUE, 5);
        w->vbox = gtk_vbox_new(TRUE, 0);
        w->play = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
        w->stop = gtk_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
        w->next = gtk_button_new_from_stock(GTK_STOCK_MEDIA_NEXT);
        w->playlist = gtk_label_new(NULL);
        w->artist = gtk_label_new(NULL);
        w->track = gtk_label_new(NULL);
        w->album = gtk_label_new(NULL);
        mainwin_set_ui_state(w, LASTFM_UI_STATE_STOPPED);
        gtk_misc_set_alignment(GTK_MISC(w->playlist), 0, 0);
        gtk_misc_set_alignment(GTK_MISC(w->artist), 0, 0);
        gtk_misc_set_alignment(GTK_MISC(w->track), 0, 0);
        gtk_misc_set_alignment(GTK_MISC(w->album), 0, 0);
        g_signal_connect(G_OBJECT(w->play), "clicked",
                         G_CALLBACK(play_clicked), NULL);
        g_signal_connect(G_OBJECT(w->next), "clicked",
                         G_CALLBACK(next_clicked), NULL);
        g_signal_connect(G_OBJECT(w->stop), "clicked",
                         G_CALLBACK(stop_clicked), NULL);
        g_signal_connect(G_OBJECT(w->window), "destroy",
                         G_CALLBACK(window_destroyed), NULL);
        gtk_container_add(GTK_CONTAINER(w->window), w->vbox);
        gtk_box_pack_start(GTK_BOX(w->hbox), w->play, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(w->hbox), w->stop, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(w->hbox), w->next, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->playlist, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->hbox, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->artist, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->track, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->album, FALSE, FALSE, 0);
        gtk_widget_set_sensitive(w->stop, FALSE);
        gtk_widget_set_sensitive(w->next, FALSE);
        return w;
}
