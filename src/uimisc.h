
#ifndef UIMISC_H
#define UIMISC_H

void ui_info_dialog(GtkWindow *parent, const char *text);
char *ui_input_dialog(GtkWindow *parent, const char *title,
                      const char *text, const char *value);

#endif
