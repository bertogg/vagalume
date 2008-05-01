/*
 * main.c -- Main file, basic initialization
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume.
 *
 * Vagalume is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Vagalume is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Vagalume. If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <signal.h>

#include "controller.h"
#include "mainwin.h"
#include "globaldefs.h"

int
main (int argc, char **argv)
{
        lastfm_mainwin *mainwin;
        char *radio = NULL;

        /* check input arguments */
        if (argc != 1) {
                if (argc != 2 || strncmp("lastfm://", argv[1], 9)) {
                        g_print (_("Usage: %s [lastfm radio url]\n"), argv[0]);
                        return -1;
                }
        }
        if (argc == 2) {
                radio = g_strdup(argv[1]);
        }

        signal(SIGPIPE, SIG_IGN);
        g_thread_init (NULL);
        gdk_threads_init ();
        gdk_threads_enter ();
        gtk_init (&argc, &argv);
        g_set_application_name(APP_NAME);

        bindtextdomain (GETTEXT_PACKAGE, VAGALUME_LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        gtk_icon_theme_append_search_path(
                gtk_icon_theme_get_default(), THEME_ICONS_DIR);

        mainwin = lastfm_mainwin_create();
        controller_run_app(mainwin, radio);
        g_free(radio);

        gdk_threads_leave ();

        return 0;
}
