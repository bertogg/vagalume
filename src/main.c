#include <gtk/gtk.h>
#include <string.h>

#include "audio.h"
#include "protocol.h"
#include "userconfig.h"
#include "radio.h"
#include "controller.h"
#include "mainwin.h"
#include "http.h"

int
main (int argc, char **argv)
{
  char *radio;
  lastfm_session *session;
  lastfm_mainwin *mainwin;
  lastfm_usercfg *usercfg;

  http_init();
  /* check input arguments */
  if (argc != 1) {
          if (argc != 2 || strncmp("lastfm://", argv[1], 9)) {
                  g_print ("Usage: %s [lastfm radio url]\n", argv[0]);
                  return -1;
          }
  }

  /* Check user configuration */
  usercfg = read_user_cfg();
  if (usercfg == NULL) {
          return -1;
  }

  session = lastfm_session_new(usercfg->username, usercfg->password);
  if (!session || session->stream_url == NULL) {
    puts("Handshake failed!");
    return -1;
  }
  if (argc == 1) {
          radio = lastfm_radio_url(LASTFM_NEIGHBOURS_RADIO,
                                   usercfg->username);
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
  controller_set_usercfg(usercfg);
  gtk_widget_show_all(mainwin->window);

  gtk_main();
  gdk_threads_leave ();

  return 0;
}
