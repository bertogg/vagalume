
#include <gtk/gtk.h>

#include "uimisc.h"
#include "userconfig.h"

void
ui_info_dialog(GtkWindow *parent, const char *text, GtkMessageType type)
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
        gtk_entry_set_text(entry, value);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                retvalue = g_strdup(gtk_entry_get_text(entry));
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return retvalue;
}

gboolean
ui_usercfg_dialog(GtkWindow *parent, lastfm_usercfg **cfg)
{
        g_return_val_if_fail(cfg != NULL, FALSE);
        GtkDialog *dialog;
        GtkWidget *label1, *label2;
        GtkEntry *user, *pw;
        GtkTable *table;
        gboolean changed = FALSE;

        dialog = ui_base_dialog(parent, "User settings");
        label1 = gtk_label_new("Username:");
        label2 = gtk_label_new("Password:");
        user = GTK_ENTRY(gtk_entry_new());
        pw = GTK_ENTRY(gtk_entry_new());
        gtk_entry_set_visibility(pw, FALSE);
        if (*cfg != NULL) {
                gtk_entry_set_text(user, (*cfg)->username);
                gtk_entry_set_text(pw, (*cfg)->password);
        }
        table = GTK_TABLE(gtk_table_new(2, 2, FALSE));
        gtk_table_attach(table, label1, 0, 1, 0, 1, 0, 0, 5, 5);
        gtk_table_attach(table, label2, 0, 1, 1, 2, 0, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET(user), 1, 2, 0, 1, 0, 0, 5, 5);
        gtk_table_attach(table, GTK_WIDGET(pw), 1, 2, 1, 2, 0, 0, 5, 5);
        gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table),
                           FALSE, FALSE, 10);
        gtk_widget_show_all(GTK_WIDGET(dialog));
        if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
                if (*cfg == NULL) *cfg = lastfm_usercfg_new();
                lastfm_usercfg_set_username(*cfg, gtk_entry_get_text(user));
                lastfm_usercfg_set_password(*cfg, gtk_entry_get_text(pw));
                changed = TRUE;
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return changed;
}
