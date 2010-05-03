/*
 * uimisc.c -- Misc UI-related functions
 *
 * Copyright (C) 2007-2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <glib/gi18n.h>
#include "config.h"
#include "uimisc.h"
#include "util.h"
#include "compat.h"

#include <string.h>
#include <gtk/gtk.h>

#ifdef MAEMO5
#        include <hildon/hildon.h>
#endif

#if defined(HILDON_LIBS)
#        include <hildon-widgets/hildon-program.h>
#        include <hildon-widgets/hildon-banner.h>
#elif defined(HILDON_1)
#        include <hildon/hildon-program.h>
#        include <hildon/hildon-banner.h>
#endif

#if defined(HILDON_FM)
#        include <hildon-widgets/hildon-file-chooser-dialog.h>
#elif defined(HILDON_FM_2)
#        include <hildon/hildon-file-chooser-dialog.h>
#endif

#define MENU_ITEM_ICON_SIZE 16

typedef struct {
        GtkDialog *dialog;
        GtkWidget *combo;
        GtkEntry *entry;
        GtkToggleButton *radio3;
} StopAfterDialog;

static const char *authors[] = {
        "Alberto Garcia Gonzalez\n<agarcia@igalia.com>\n",
        "Mario Sanchez Prada\n<msanchez@igalia.com>",
        NULL
};
static const char *artists[] = {
        "Otto Krüja\n<ottokrueja@gmx.net>",
        NULL
};

static const char appdescr[] = N_("Client for Last.fm and compatible services");
static const char copyright[] = "(c) 2007-2010 Igalia, S.L.";
static const char website[] = "http://vagalume.igalia.com/";
static const char license[] =
"Vagalume is free software: you can redistribute\n"
"it and/or modify it under the terms of the GNU\n"
"General Public License version 3 as published by\n"
"the Free Software Foundation.\n"
"\n"
"Vagalume is distributed in the hope that it will\n"
"be useful, but WITHOUT ANY WARRANTY; without even\n"
"the implied warranty of MERCHANTABILITY or FITNESS\n"
"FOR A PARTICULAR PURPOSE. See the GNU General\n"
"Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General\n"
"Public License along with Vagalume. If not, see\n"
"http://www.gnu.org/licenses/\n";
static const char translators_tpl[] =
"%s (de)\n* Stephan Reichholf <stephan@reichholf.net>\n"
         "* Oskar Welzl <mail@welzl.info>\n\n"
"%s (es)\n* Oscar A. Mata T. <omata_mac@yahoo.com>\n"
         "* Ricardo Mones <mones@debian.org>\n\n"
"%s (fi)\n* Janne Mäkinen <janne.makinen@surffi.fi>\n"
         "* Mika Tapojärvi <mika.tapojarvi@sse.fi>\n"
         "* Tommi Franttila <tommif@gmail.com>\n\n"
"%s (fr)\n* Julien Duponchelle <julien@duponchelle.info>\n"
         "* Jean-Alexandre Angles d'Auriac <jeanalexandre.angles_dauriac@ens-lyon.fr>\n\n"
"%s (gl)\n* Ignacio Casal Quinteiro <nacho.resa@gmail.com>\n"
         "* Amador Loureiro Blanco <dorfun@adorfunteca.org>\n\n"
"%s (it)\n* Andrea Grandi <a.grandi@gmail.com>\n\n"
"%s (lv)\n* Pēteris Caune <cuu508@gmail.com>\n\n"
"%s (pl)\n* Dawid Pakuła (ZuLuS) <zulus@w3des.net>\n\n"
"%s (pt)\n* Marcos Garcia <marcosgg@gmail.com>\n\n"
"%s (pt_BR)\n* Rodrigo Flores <rodrigomarquesflores@gmail.com>\n\n"
"%s (ru)\n* Sergei Ivanov <isn@vu.spb.ru>\n"
         "* Vitaly Petrov <vit.petrov@vu.spb.ru>";

/* Don't use gtk_button_new() in Nokia 770 or icons won't appear */
GtkWidget *
compat_gtk_button_new                   (void)
{
#ifdef MAEMO2
        return gtk_button_new_with_label("");
#else
        return gtk_button_new();
#endif
}

static void
ui_show_dialog                          (GtkWindow      *parent,
                                         const char     *text,
                                         GtkMessageType  type)
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
ui_info_dialog                          (GtkWindow  *parent,
                                         const char *text)
{
        ui_show_dialog(parent, text, GTK_MESSAGE_INFO);
}

void
ui_warning_dialog                       (GtkWindow  *parent,
                                         const char *text)
{
        ui_show_dialog(parent, text, GTK_MESSAGE_WARNING);
}

void
ui_error_dialog                         (GtkWindow  *parent,
                                         const char *text)
{
        ui_show_dialog(parent, text, GTK_MESSAGE_ERROR);
}

void
ui_info_banner                          (GtkWindow  *parent,
                                         const char *text)
{
        g_return_if_fail(parent != NULL && text != NULL);
#ifdef MAEMO
        hildon_banner_show_information(NULL, NULL, text);
#else
        /* TODO: Implement a notification banner for Gnome */
        g_warning("Info banner not implemented!!");
#endif
}

#ifdef HAVE_GIO
static void
about_dialog_uri_hook                   (GtkAboutDialog *about,
                                         const gchar    *link,
                                         gpointer        data)
{
        gchar *uri = g_strconcat (data, link, NULL);
        launch_url (uri, NULL);
        g_free (uri);
}
#endif

void
ui_about_dialog                         (GtkWindow *parent)
{
        GdkPixbuf *logo = gdk_pixbuf_new_from_file (APP_ICON_BIG, NULL);
        char *translators = g_strdup_printf (translators_tpl, _("German"),
                                             _("Spanish"), _("Finnish"),
                                             _("French"),
                                             _("Galician"), _("Italian"),
                                             _("Latvian"), _("Polish"),
                                             _("Portuguese"), _("Portuguese"),
                                             _("Russian"));
#ifdef HAVE_GIO
        gtk_about_dialog_set_url_hook (about_dialog_uri_hook, "", NULL);
        gtk_about_dialog_set_email_hook (about_dialog_uri_hook, "mailto:",
                                         NULL);
#endif
        gtk_show_about_dialog (parent, "authors", authors,
                               "comments", _(appdescr), "copyright", copyright,
                               "license", license, "version", APP_VERSION,
                               "website", website, "artists", artists,
                               "translator-credits", translators,
#ifdef HAVE_GTK_ABOUT_DIALOG_PROGRAM_NAME
                               "program-name", APP_NAME,
#else
                               "name", APP_NAME,
#endif /* HAVE_GTK_ABOUT_DIALOG_PROGRAM_NAME */
                               "logo", logo, NULL);
        g_object_unref (logo);
        g_free (translators);
}

GtkDialog *
ui_base_dialog                          (GtkWindow  *parent,
                                         const char *title)
{
        GtkWidget *dialog;
        dialog = gtk_dialog_new_with_buttons(title, parent,
                                             GTK_DIALOG_MODAL |
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_STOCK_CANCEL,
                                             GTK_RESPONSE_REJECT,
                                             GTK_STOCK_OK,
                                             GTK_RESPONSE_ACCEPT,
                                             NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog),
                                        GTK_RESPONSE_ACCEPT);
        return GTK_DIALOG(dialog);
}

gboolean
ui_confirm_dialog                       (GtkWindow  *parent,
                                         const char *text)
{
        g_return_val_if_fail(text != NULL, FALSE);
        gint response;
        GtkDialog *dialog = ui_base_dialog(parent, _("Confirmation"));
        GtkWidget *label = gtk_label_new(text);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), label, FALSE, FALSE, 10);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        response = gtk_dialog_run(dialog);
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return (response == GTK_RESPONSE_ACCEPT);
}

char *
ui_input_dialog                         (GtkWindow  *parent,
                                         const char *title,
                                         const char *text,
                                         const char *value)
{
        GtkDialog *dialog;
        GtkWidget *label;
        GtkEntry *entry;
        char *retvalue = NULL;
        dialog = ui_base_dialog(parent, title);
        label = gtk_label_new(text);
#ifdef MAEMO5
        entry = GTK_ENTRY(hildon_entry_new(FINGER_SIZE));
#else
        entry = GTK_ENTRY(gtk_entry_new());
#endif
        gtk_box_pack_start(GTK_BOX(dialog->vbox), label, FALSE, FALSE, 10);
        gtk_box_pack_start(GTK_BOX(dialog->vbox),
                           GTK_WIDGET(entry), FALSE, FALSE, 10);
        if (value != NULL) gtk_entry_set_text(entry, value);
        gtk_entry_set_activates_default(entry, TRUE);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                retvalue = g_strstrip(g_strdup(gtk_entry_get_text(entry)));
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return retvalue;
}

gboolean
ui_edit_bookmark_dialog                 (GtkWindow  *parent,
                                         char      **name,
                                         char      **url,
                                         gboolean    add)
{
        GtkDialog *dialog;
        GtkWidget *namelabel, *urllabel;
        GtkEntry *nameentry, *urlentry;
        GtkBox *vbox;
        gboolean done, retvalue;
        dialog = ui_base_dialog(parent,
                                add ? _("Add bookmark") : _("Edit bookmark"));
        namelabel = gtk_label_new(_("Bookmark name"));
        urllabel = gtk_label_new(_("Last.fm radio address"));
#ifdef MAEMO5
        nameentry = GTK_ENTRY (hildon_entry_new (FINGER_SIZE));
        urlentry = GTK_ENTRY (hildon_entry_new (FINGER_SIZE));
#else
        nameentry = GTK_ENTRY(gtk_entry_new());
        urlentry = GTK_ENTRY(gtk_entry_new());
#endif /* MAEMO5 */

        if (*name != NULL) gtk_entry_set_text(nameentry, *name);
        if (*url != NULL) gtk_entry_set_text(urlentry, *url);

        vbox = GTK_BOX (dialog->vbox);
        gtk_box_pack_start(vbox, namelabel, FALSE, FALSE, 4);
        gtk_box_pack_start(vbox, GTK_WIDGET(nameentry), FALSE, FALSE, 4);
        gtk_box_pack_start(vbox, urllabel, FALSE, FALSE, 4);
        gtk_box_pack_start(vbox, GTK_WIDGET(urlentry), FALSE, FALSE, 4);
        gtk_entry_set_activates_default(nameentry, TRUE);
        gtk_entry_set_activates_default(urlentry, TRUE);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        done = FALSE;
        while (!done) {
                if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
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

GtkWidget *
ui_menu_item_create_from_icon           (const gchar *icon_name,
                                         const gchar *label)
{
        g_return_val_if_fail (icon_name != NULL && label != NULL, NULL);

        GtkWidget *item = gtk_image_menu_item_new_with_mnemonic (label);
        GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
        GdkPixbuf *pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name,
                                                      MENU_ITEM_ICON_SIZE, 0,
                                                      NULL);

        if (pixbuf != NULL) {
                GtkWidget *image = gtk_image_new_from_pixbuf (pixbuf);
                gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                               image);
                g_object_unref (pixbuf);
        }

        return item;
}

static GtkWidget *
create_file_chooser_dialog              (GtkWindow            *parent,
                                         const char           *title,
                                         GtkFileChooserAction  action)
{
        GtkWidget *dialog;
#ifdef MAEMO
        dialog = hildon_file_chooser_dialog_new (parent, action);
        gtk_window_set_title (GTK_WINDOW (dialog), title);
#else
        dialog = gtk_file_chooser_dialog_new (
                title, parent, action,
                GTK_STOCK_OK, GTK_RESPONSE_OK,
                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                NULL);
#endif
        gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
        return dialog;
}

char *
ui_select_servers_file                  (GtkWindow *parent)
{
        GtkWidget *dialog;
        char *file = NULL;
        dialog = create_file_chooser_dialog (
                parent, _("Select server file to import"),
                GTK_FILE_CHOOSER_ACTION_OPEN);
        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
                file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        }
        gtk_widget_destroy (dialog);
        return file;
}

char *
ui_select_download_dir                  (GtkWindow  *parent,
                                         const char *curdir)
{
        GtkWidget *dialog;
        char *dir = NULL;
        dialog = create_file_chooser_dialog (
                parent, _("Select download directory"),
                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        if (curdir != NULL) {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),
                                              curdir);
        }
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
                dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        }
        gtk_widget_destroy(dialog);
        return dir;
}

GtkTreeModel *
ui_create_options_list                  (const GList *elems)
{
        GtkTreeIter iter;
        const GList *current = elems;
        GtkListStore *store = gtk_list_store_new (1, G_TYPE_STRING);

        for (; current != NULL; current = g_list_next(current)) {
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, current->data, -1);
        }
        return GTK_TREE_MODEL(store);
}

static void
stop_after_dialog_update_sensitivity    (StopAfterDialog *win)
{
        gboolean radio3_active = gtk_toggle_button_get_active (win->radio3);
        gboolean ok_button_sensitive = TRUE;
        if (radio3_active) {
                const char *text = gtk_entry_get_text (win->entry);
                guint64 value = g_ascii_strtoull (text, NULL, 10);
                ok_button_sensitive = (value > 0 && value < 10080);
        }
        gtk_widget_set_sensitive (win->combo, radio3_active);
        gtk_dialog_set_response_sensitive (win->dialog,
                                           GTK_RESPONSE_ACCEPT,
                                           ok_button_sensitive);
}

static void
stop_after_entry_changed                (GtkWidget       *w,
                                         GParamSpec      *arg,
                                         StopAfterDialog *win)
{
        stop_after_dialog_update_sensitivity (win);
}

static void
stop_after_radio_button_toggled         (GtkWidget       *button,
                                         StopAfterDialog *win)
{
        stop_after_dialog_update_sensitivity (win);
}

gboolean
ui_stop_after_dialog                    (GtkWindow     *parent,
                                         StopAfterType *stopafter,
                                         int           *minutes)
{
        GtkDialog *dialog;
        GtkWidget *alignment;
        GtkWidget *hbox, *vbox;
        GtkWidget *descrlabel, *minuteslabel;
        GtkWidget *combo;
        GtkEntry *entry;
        GtkListStore *model;
        GtkWidget *radio1, *radio2, *radio3;
        StopAfterDialog win;
        int default_times[] = { 5, 15, 30, 45, 60, 90, 120, 240, -1 };
        int i;
        gboolean retvalue = FALSE;

        g_return_val_if_fail (minutes != NULL && stopafter != NULL, FALSE);

        /* Create all widgets */
        dialog = ui_base_dialog (parent, _("Stop automatically"));
        descrlabel = gtk_label_new (_("When do you want to stop playback?"));
        minuteslabel = gtk_label_new (_("minutes"));
        alignment = gtk_alignment_new (0.5, 0.5, 0, 0);
        hbox = gtk_hbox_new (FALSE, 5);
        vbox = gtk_vbox_new (TRUE, 5);
        radio1 = gtk_radio_button_new (NULL);
        radio2 = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (radio1));
        radio3 = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (radio1));

        gtk_button_set_label (GTK_BUTTON (radio1), _("Don't stop"));
        gtk_button_set_label (GTK_BUTTON (radio2), _("Stop after this track"));
        /* Translators: the full text is "Stop after ____ minutes" */
        gtk_button_set_label (GTK_BUTTON (radio3), _("Stop after"));

        /* Create and fill model */
        model = gtk_list_store_new (1, G_TYPE_STRING);
        for (i = 0; default_times[i] != -1; i++) {
                GtkTreeIter iter;
                char str[4];
                g_snprintf (str, 4, "%d", default_times[i]);
                gtk_list_store_append (model, &iter);
                gtk_list_store_set (model, &iter, 0, str, -1);
        }

        /* Create combo box */
        combo = gtk_combo_box_entry_new_with_model (GTK_TREE_MODEL (model), 0);
        g_object_unref (model);
        entry = GTK_ENTRY (GTK_BIN (combo)->child);
        gtk_entry_set_width_chars (entry, 4);
#ifdef MAEMO
        hildon_gtk_entry_set_input_mode (entry, HILDON_GTK_INPUT_MODE_NUMERIC);
#endif

        /* Set default values */
        if (*stopafter == STOP_AFTER_DONT_STOP) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio1),
                                              TRUE);
        } else if (*stopafter == STOP_AFTER_THIS_TRACK) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio2),
                                              TRUE);
        } else if (*stopafter == STOP_AFTER_N_MINUTES) {
                char *str;
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio3),
                                              TRUE);
                if (*minutes > 0) {
                        str = g_strdup_printf ("%d", *minutes);
                        gtk_entry_set_text (entry, str);
                        g_free (str);
                }
        } else {
                g_return_val_if_reached (FALSE);
        }

        /* Pack widgets */
        gtk_box_pack_start (GTK_BOX (hbox), radio3, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), minuteslabel, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), descrlabel, FALSE, FALSE, 5);
        gtk_box_pack_start (GTK_BOX (vbox), radio1, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), radio2, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (alignment), vbox);
        gtk_box_pack_start (GTK_BOX (dialog->vbox), alignment, TRUE, TRUE, 10);

        /* Fill StopAfterDialog struct */
        win.dialog = dialog;
        win.combo = combo;
        win.entry = entry;
        win.radio3 = GTK_TOGGLE_BUTTON (radio3);

        /* Set sensitivity of combo box and OK button */
        stop_after_dialog_update_sensitivity (&win);

        /* Connect signals */
        g_signal_connect (radio3, "toggled",
                          G_CALLBACK (stop_after_radio_button_toggled), &win);
        g_signal_connect (entry, "notify::text",
                          G_CALLBACK (stop_after_entry_changed), &win);

        /* Run dialog */
        gtk_widget_show_all (GTK_WIDGET (dialog));
        if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
                if (gtk_toggle_button_get_active (
                            GTK_TOGGLE_BUTTON (radio1))) {
                        *stopafter = STOP_AFTER_DONT_STOP;
                } else if (gtk_toggle_button_get_active (
                                   GTK_TOGGLE_BUTTON (radio2))) {
                        *stopafter = STOP_AFTER_THIS_TRACK;
                } else {
                        const char *str = gtk_entry_get_text (entry);
                        *minutes = g_ascii_strtoull (str, NULL, 10);
                        *stopafter = STOP_AFTER_N_MINUTES;
                }
                retvalue = TRUE;
        }
        gtk_widget_destroy (GTK_WIDGET (dialog));

        return retvalue;
}
