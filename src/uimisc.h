
#ifndef UIMISC_H
#define UIMISC_H

#include <gtk/gtk.h>
#include "userconfig.h"

void ui_info_dialog(GtkWindow *parent, const char *text, GtkMessageType type);
char *ui_input_dialog(GtkWindow *parent, const char *title,
                      const char *text, const char *value);
gboolean ui_usercfg_dialog(GtkWindow *parent, lastfm_usercfg **cfg);

#endif
