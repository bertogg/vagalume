/*
 * main.c -- Main file, basic initialization
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
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
#include <unistd.h>

#include "controller.h"
#include "globaldefs.h"
#include "audio.h"

#ifdef HAVE_DBUS_SUPPORT
#   include <dbus/dbus-glib.h>
#endif

int
main                                    (int    argc,
                                         char **argv)
{
        char *radio = NULL;

        signal(SIGPIPE, SIG_IGN);
        g_thread_init (NULL);
        gdk_threads_init ();
#ifdef HAVE_DBUS_SUPPORT
        dbus_g_thread_init ();
#endif
        gdk_threads_enter ();
        gtk_init (&argc, &argv);
        g_set_application_name(APP_NAME);

        bindtextdomain (GETTEXT_PACKAGE, VAGALUME_LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

#if defined(HAVE_GETOPT) && !defined(MAEMO)
        int opt;
        while ((opt = getopt(argc, argv, "c:d:m:s:h")) != -1) {
                switch (opt) {
                case 'c':
                        g_setenv(GST_CONVERT_ENVVAR, optarg, TRUE);
                        break;
                case 'd':
                        g_setenv(GST_DECODER_ENVVAR, optarg, TRUE);
                        break;
                case 'm':
                        g_setenv(GST_MIXER_ENVVAR, optarg, TRUE);
                        break;
                case 's':
                        g_setenv(GST_SINK_ENVVAR, optarg, TRUE);
                        break;
                default:
                        g_print(_("Usage:\n  %s [-c converter] [-d decoder] "
                                  "[-m mixer] [-s sink] [lastfm radio url]\n\n"
                                  "  converter: GStreamer converter "
                                  "(default: '%s', 'none' to disable)\n"
                                  "  decoder:   GStreamer decoder "
                                  "(default: '%s')\n"
                                  "  mixer:     GStreamer mixer "
                                  "(default: '%s')\n"
                                  "  sink:      GStreamer sink "
                                  "(default: '%s')\n"),
                                argv[0],
                                lastfm_audio_default_convert_name(),
                                lastfm_audio_default_decoder_name(),
                                lastfm_audio_default_mixer_name(),
                                lastfm_audio_default_sink_name());
                        return -1;
                }
        }
        if (optind + 1 < argc) {
                g_print(_("Too many unrecognized options\n"));
                return -1;
        } else if (optind + 1 == argc) {
                radio = g_strdup(argv[optind]);
        }
#else
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
#endif

        gtk_icon_theme_append_search_path(
                gtk_icon_theme_get_default(), THEME_ICONS_DIR);

        controller_run_app (radio);
        g_free(radio);

        gdk_threads_leave ();

        return 0;
}
