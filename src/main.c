#include <gtk/gtk.h>
#include <string.h>

#include "controller.h"
#include "mainwin.h"

int
main (int argc, char **argv)
{
  lastfm_mainwin *mainwin;
  char *radio = NULL;

  /* check input arguments */
  if (argc != 1) {
          if (argc != 2 || strncmp("lastfm://", argv[1], 9)) {
                  g_print ("Usage: %s [lastfm radio url]\n", argv[0]);
                  return -1;
          }
  }
  if (argc == 2) {
          radio = g_strdup(argv[1]);
  }

  g_thread_init (NULL);
  gdk_threads_init ();
  gdk_threads_enter ();
  gtk_init (&argc, &argv);

  mainwin = lastfm_mainwin_create();
  controller_run_app(mainwin, radio);
  g_free(radio);

  gdk_threads_leave ();

  return 0;
}
