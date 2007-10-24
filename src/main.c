#include <gtk/gtk.h>
#include <string.h>

#include "audio.h"
#include "protocol.h"
#include "userconfig.h"
#include "radio.h"
#include "controller.h"
#include "mainwin.h"

int
main (int argc, char **argv)
{
  char *radio;
  lastfm_session *session;
  lastfm_mainwin *mainwin;

  /* check input arguments */
  if (argc != 1) {
          if (argc != 2 || strncmp("lastfm://", argv[1], 9)) {
                  g_print ("Usage: %s [lastfm radio url]\n", argv[0]);
                  return -1;
          }
  }

  /* Check user configuration */
  if (!read_user_cfg()) {
          return -1;
  }

  session = lastfm_session_new(user_cfg_get_usename(),
                               user_cfg_get_password());
  if (!session || session->stream_url == NULL) {
    puts("Handshake failed!");
    return -1;
  }
  if (argc == 1) {
          radio = lastfm_neighbours_radio_url(user_cfg_get_usename());
  } else {
          radio = g_strdup(argv[1]);
  }
  if (!lastfm_set_radio(session, radio)) {
          printf("Unable to set radio %s!\n", radio);
          return -1;
  }
  g_free(radio);
  if (!lastfm_request_playlist(session)) {
          puts("No playlist found!");
          return -1;
  }

  /* Gtk */
  g_thread_init (NULL);
  gdk_threads_init ();
  gdk_threads_enter ();
  gtk_init (&argc, &argv);

  mainwin = lastfm_mainwin_create();
  lastfm_audio_init();
  controller_set_session(session);
  controller_set_mainwin(mainwin);
  gtk_widget_show_all(mainwin->window);

  gtk_main();
  gdk_threads_leave ();

  return 0;
}
