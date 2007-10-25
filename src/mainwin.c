
#include <gtk/gtk.h>

#include "controller.h"
#include "mainwin.h"
#include "radio.h"

static const char *authors[] = {
        "Alberto Garcia <agarcia@igalia.com>",
        NULL
};

static const char *appname = "Last.fm player";
static const char *appdescr = "A small (and still unnamed) Last.fm player";

void
ui_show_info_dialog(GtkWindow *parent, const char *text)
{
        g_return_if_fail(text != NULL);
        GtkDialogFlags flags = GTK_DIALOG_MODAL |
                GTK_DIALOG_DESTROY_WITH_PARENT;
        GtkWidget *dialog = gtk_message_dialog_new(parent, flags,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "%s", text);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

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
        gboolean dim_labels = FALSE;
        switch (state) {
        case LASTFM_UI_STATE_STOPPED:
                dim_labels = FALSE;
                gtk_label_set_text(GTK_LABEL(w->playlist), "Stopped");
                gtk_label_set_text(GTK_LABEL(w->artist), NULL);
                gtk_label_set_text(GTK_LABEL(w->track), NULL);
                gtk_label_set_text(GTK_LABEL(w->album), NULL);
                gtk_widget_set_sensitive (w->play, TRUE);
                gtk_widget_set_sensitive (w->stop, FALSE);
                gtk_widget_set_sensitive (w->next, FALSE);
                break;
        case LASTFM_UI_STATE_PLAYING:
                dim_labels = FALSE;
                gtk_widget_set_sensitive (w->play, FALSE);
                gtk_widget_set_sensitive (w->stop, TRUE);
                gtk_widget_set_sensitive (w->next, TRUE);
                break;
        case LASTFM_UI_STATE_CONNECTING:
                dim_labels = TRUE;
                gtk_label_set_text(GTK_LABEL(w->playlist), "Connecting...");
                gtk_widget_set_sensitive (w->play, FALSE);
                gtk_widget_set_sensitive (w->stop, FALSE);
                gtk_widget_set_sensitive (w->next, FALSE);
                break;
        default:
                g_critical("Unknown ui state received: %d", state);
                break;
        }
        gtk_widget_set_sensitive (w->artist, !dim_labels);
        gtk_widget_set_sensitive (w->track, !dim_labels);
        gtk_widget_set_sensitive (w->album, !dim_labels);
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
close_app(GtkWidget *widget, gpointer data)
{
        controller_quit_app();
}

static void
radio_selected(GtkWidget *widget, gpointer data)
{
        lastfm_radio type = GPOINTER_TO_INT(data);
        controller_play_radio(type);
}

static void
show_about_dialog(GtkWidget *widget, gpointer data)
{
        GtkWindow *win = (GtkWindow *) data;
        gtk_show_about_dialog(win, "name", appname, "authors", authors,
                              "comments", appdescr, NULL);
}

static GtkWidget *
create_menu_bar(lastfm_mainwin *w)
{
        GtkMenuItem *lastfm, *radio, *help;
        GtkMenuShell *lastfmsub, *radiosub, *helpsub;
        GtkWidget *settings, *quit;
        GtkWidget *personal, *neigh, *loved, *recomm;
        GtkWidget *about;
        GtkMenuShell *bar = GTK_MENU_SHELL(gtk_menu_bar_new());

        /* Last.fm */
        lastfm = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("_Last.fm"));
        lastfmsub = GTK_MENU_SHELL(gtk_menu_new());
        settings = gtk_menu_item_new_with_mnemonic("_Settings");
        quit = gtk_menu_item_new_with_mnemonic("_Quit");
        gtk_menu_shell_append(bar, GTK_WIDGET(lastfm));
        gtk_menu_item_set_submenu(lastfm, GTK_WIDGET(lastfmsub));
        gtk_menu_shell_append(lastfmsub, settings);
        gtk_menu_shell_append(lastfmsub, quit);
        g_signal_connect(G_OBJECT(quit), "activate",
                         G_CALLBACK(close_app), NULL);

        /* Radio */
        radio = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("Play _Radio"));
        radiosub = GTK_MENU_SHELL(gtk_menu_new());
        personal = gtk_menu_item_new_with_mnemonic("_Personal");
        neigh = gtk_menu_item_new_with_mnemonic("_Neighbours");
        loved = gtk_menu_item_new_with_mnemonic("_Loved tracks");
        recomm = gtk_menu_item_new_with_mnemonic("R_ecommendations");
        gtk_menu_shell_append(bar, GTK_WIDGET(radio));
        gtk_menu_item_set_submenu(radio, GTK_WIDGET(radiosub));
        gtk_menu_shell_append(radiosub, personal);
        gtk_menu_shell_append(radiosub, neigh);
        gtk_menu_shell_append(radiosub, loved);
        gtk_menu_shell_append(radiosub, recomm);
        g_signal_connect(G_OBJECT(personal), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_PERSONAL_RADIO));
        g_signal_connect(G_OBJECT(neigh), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_NEIGHBOURS_RADIO));
        g_signal_connect(G_OBJECT(loved), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_LOVEDTRACKS_RADIO));
        g_signal_connect(G_OBJECT(recomm), "activate",
                         G_CALLBACK(radio_selected),
                         GINT_TO_POINTER(LASTFM_RECOMMENDED_RADIO));

        /* Help */
        help = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic("_Help"));
        helpsub = GTK_MENU_SHELL(gtk_menu_new());
        about = gtk_menu_item_new_with_mnemonic("_About");
        gtk_menu_shell_append(bar, GTK_WIDGET(help));
        gtk_menu_item_set_submenu(help, GTK_WIDGET(helpsub));
        gtk_menu_shell_append(helpsub, about);
        g_signal_connect(G_OBJECT(about), "activate",
                         G_CALLBACK(show_about_dialog), w->window);

        return GTK_WIDGET(bar);
}

lastfm_mainwin *
lastfm_mainwin_create(void)
{
        lastfm_mainwin *w = g_new0(lastfm_mainwin, 1);
        /* Window */
        w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(w->window), "Last.fm");
        gtk_container_set_border_width(GTK_CONTAINER(w->window), 10);
        /* Boxes */
        w->hbox = gtk_hbox_new(TRUE, 5);
        w->vbox = gtk_vbox_new(TRUE, 0);
        /* Buttons */
        w->play = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
        w->stop = gtk_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
        w->next = gtk_button_new_from_stock(GTK_STOCK_MEDIA_NEXT);
        /* Text labels */
        w->playlist = gtk_label_new(NULL);
        w->artist = gtk_label_new(NULL);
        w->track = gtk_label_new(NULL);
        w->album = gtk_label_new(NULL);
        /* Menu */
        w->menubar = create_menu_bar(w);
        /* Layout */
        gtk_misc_set_alignment(GTK_MISC(w->playlist), 0, 0);
        gtk_misc_set_alignment(GTK_MISC(w->artist), 0, 0);
        gtk_misc_set_alignment(GTK_MISC(w->track), 0, 0);
        gtk_misc_set_alignment(GTK_MISC(w->album), 0, 0);
        gtk_container_add(GTK_CONTAINER(w->window), w->vbox);
        gtk_box_pack_start(GTK_BOX(w->hbox), w->play, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(w->hbox), w->stop, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(w->hbox), w->next, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->menubar, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->playlist, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->hbox, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->artist, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->track, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(w->vbox), w->album, FALSE, FALSE, 0);
        /* Signals */
        g_signal_connect(G_OBJECT(w->play), "clicked",
                         G_CALLBACK(play_clicked), NULL);
        g_signal_connect(G_OBJECT(w->next), "clicked",
                         G_CALLBACK(next_clicked), NULL);
        g_signal_connect(G_OBJECT(w->stop), "clicked",
                         G_CALLBACK(stop_clicked), NULL);
        g_signal_connect(G_OBJECT(w->window), "destroy",
                         G_CALLBACK(close_app), NULL);
        /* Initial state */
        mainwin_set_ui_state(w, LASTFM_UI_STATE_STOPPED);
        return w;
}
