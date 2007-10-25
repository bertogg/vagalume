
#include <gtk/gtk.h>

#include "uimisc.h"

void
ui_info_dialog(GtkWindow *parent, const char *text)
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

char *
ui_input_dialog(GtkWindow *parent, const char *title,
                const char *text, const char *value)
{
        GtkWidget *dialog;
        GtkWidget *label;
        GtkEntry *entry;
        label = gtk_label_new(text);
        entry = GTK_ENTRY(gtk_entry_new());
        gint response;
        char *retvalue = NULL;
        dialog = gtk_dialog_new_with_buttons(title, parent,
                                             GTK_DIALOG_MODAL |
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_STOCK_OK,
                                             GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL,
                                             GTK_RESPONSE_REJECT,
                                             NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                           label, FALSE, FALSE, 10);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                           GTK_WIDGET(entry), FALSE, FALSE, 10);
        gtk_entry_set_text(entry, value);
        gtk_widget_show_all(dialog);
        do {
                response = gtk_dialog_run(GTK_DIALOG(dialog));
                if (response == GTK_RESPONSE_ACCEPT) {
                        retvalue = g_strdup(gtk_entry_get_text(entry));
                }
        } while (response != GTK_RESPONSE_REJECT && retvalue == NULL);
        gtk_widget_destroy(dialog);
        return retvalue;
}
