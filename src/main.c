#include <gtk/gtk.h>
#include <string.h>

#include "audio.h"
#include "userconfig.h"
#include "controller.h"
#include "mainwin.h"
#include "http.h"

int
main (int argc, char **argv)
{
  lastfm_mainwin *mainwin;
  lastfm_usercfg *usercfg;

  /* check input arguments */
  if (argc != 1) {
          if (argc != 2 || strncmp("lastfm://", argv[1], 9)) {
                  g_print ("Usage: %s [lastfm radio url]\n", argv[0]);
                  return -1;
          }
  }

  /* Check user configuration */
  usercfg = read_usercfg();
  if (usercfg == NULL) {
          return -1;
  }

/*   if (argc == 2) { */
/*           radio = g_strdup(argv[1]); */
/*   } */

  /* Gtk */
  g_thread_init (NULL);
  gdk_threads_init ();
  gdk_threads_enter ();
  gtk_init (&argc, &argv);

  http_init();
  lastfm_audio_init();

  mainwin = lastfm_mainwin_create();
  controller_set_mainwin(mainwin);
  controller_set_usercfg(usercfg);
  gtk_widget_show_all(mainwin->window);

  gtk_main();
  gdk_threads_leave ();

  return 0;
}
