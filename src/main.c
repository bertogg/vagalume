#include <gtk/gtk.h>
#include <gst/gst.h>
#include <string.h>

#include "protocol.h"
#include "userconfig.h"
#include "radio.h"

static void stop_clicked(GtkWidget *widget, gpointer data);
static void next_clicked(GtkWidget *widget, gpointer data);

static GstElement *gstplay, *source, *decoder, *sink;
static lastfm_session *session;

static struct {
        GtkWidget *window, *play, *stop, *next, *hbox, *vbox;
        GtkWidget *playlist, *artist, *track, *album;
} win;

static gboolean
bus_call (GstBus     *bus,
	  GstMessage *msg,
	  gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      gdk_threads_enter ();
      next_clicked(NULL, NULL);
      gdk_threads_leave ();
      break;
    case GST_MESSAGE_ERROR: {
      gchar *debug;
      GError *err;

      gst_message_parse_error (msg, &err, &debug);
      g_free (debug);

      g_print ("Error: %s\n", err->message);
      g_error_free (err);

      g_main_loop_quit (loop);
      gdk_threads_enter ();
      stop_clicked(NULL, NULL);
      gdk_threads_leave ();
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static void
update_track_labels(lastfm_track *track)
{
        char *text;
        const char *playlist = session->playlist->title;
        const char *artist = track ? track->artist : NULL;
        const char *title = track ? track->title : NULL;
        const char *album = track ? track->album : NULL;
        text = g_strconcat("Artist: ", artist, NULL);
        gtk_label_set_text(GTK_LABEL(win.artist), text);
        g_free(text);
        text = g_strconcat("Track: ", title, NULL);
        gtk_label_set_text(GTK_LABEL(win.track), text);
        g_free(text);
        text = g_strconcat("Album: ", album, NULL);
        gtk_label_set_text(GTK_LABEL(win.album), text);
        g_free(text);
        if (track != NULL) {
                text = g_strconcat("Listening to ", playlist, NULL);
                gtk_label_set_text(GTK_LABEL(win.playlist), text);
                g_free(text);
        } else {
                gtk_label_set_text(GTK_LABEL(win.playlist), "Stopped");
        }
}

static void
play_clicked(GtkWidget *widget, gpointer data)
{
  if (lastfm_pls_size(session->playlist) == 0) {
          if (!lastfm_request_playlist(session)) {
                  g_debug("No more content to play");
                  return;
          }
  }
  lastfm_track *track = lastfm_pls_get_track(session->playlist);
  g_object_set (G_OBJECT (source), "location", track->stream_url, NULL);
  update_track_labels(track);
  lastfm_track_destroy(track);
  gst_element_set_state (gstplay, GST_STATE_PLAYING);
  gtk_widget_set_sensitive (win.play, FALSE);
  gtk_widget_set_sensitive (win.stop, TRUE);
  gtk_widget_set_sensitive (win.next, TRUE);
}

static void
stop_clicked(GtkWidget *widget, gpointer data)
{
  update_track_labels(NULL);
  gst_element_set_state (gstplay, GST_STATE_NULL);
  gtk_widget_set_sensitive (win.play, TRUE);
  gtk_widget_set_sensitive (win.stop, FALSE);
  gtk_widget_set_sensitive (win.next, FALSE);
}

static void
next_clicked(GtkWidget *widget, gpointer data)
{
  stop_clicked(widget, data);
  play_clicked(widget, data);
}

static void destroy( GtkWidget *widget,
                     gpointer   data )
{
  gst_element_set_state (gstplay, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (gstplay));
  lastfm_session_destroy(session);

  gtk_main_quit ();
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;
  GstBus *bus;
  char *radio;

  /* check input arguments */
  if (argc != 1) {
          if (argc != 2 || strncmp("lastfm://", argv[1], 9)) {
                  g_print ("Usage: %s [lastfm radio url]\n", argv[0]);
                  return -1;
          }
  }

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

  /* initialize GStreamer */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* set up */
  gstplay = gst_pipeline_new (NULL);
  source = gst_element_factory_make ("gnomevfssrc", NULL);
#ifdef MAEMO
  sink = gst_element_factory_make ("dspmp3sink", NULL);
  decoder = sink; /* Not used, this is only for the next if */
#else
  decoder = gst_element_factory_make ("mad", NULL);
  sink = gst_element_factory_make ("autoaudiosink", NULL);
#endif
  if (!gstplay || !source || !decoder || !sink) {
          puts("Error creating GStreamer elements");
          return -1;
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (gstplay));
  gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

#ifdef MAEMO
  gst_bin_add_many (GST_BIN (gstplay), source, sink, NULL);
  gst_element_link_many (source, sink, NULL);
#else
  gst_bin_add_many (GST_BIN (gstplay), source, decoder, sink, NULL);
  gst_element_link_many (source, decoder, sink, NULL);
#endif

  /* Gtk */
  win.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW(win.window), "Last.fm");
  gtk_container_set_border_width (GTK_CONTAINER (win.window), 10);
  win.hbox = gtk_hbox_new (TRUE, 5);
  win.vbox = gtk_vbox_new (TRUE, 0);
  win.play = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PLAY);
  win.stop = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
  win.next = gtk_button_new_from_stock (GTK_STOCK_MEDIA_NEXT);
  win.playlist = gtk_label_new(NULL);
  win.artist = gtk_label_new(NULL);
  win.track = gtk_label_new(NULL);
  win.album = gtk_label_new(NULL);
  update_track_labels(NULL);
  gtk_misc_set_alignment(GTK_MISC(win.playlist), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(win.artist), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(win.track), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(win.album), 0, 0);
  g_signal_connect (G_OBJECT (win.play), "clicked",
                    G_CALLBACK (play_clicked), NULL);
  g_signal_connect (G_OBJECT (win.next), "clicked",
                    G_CALLBACK (next_clicked), NULL);
  g_signal_connect (G_OBJECT (win.stop), "clicked",
                    G_CALLBACK (stop_clicked), NULL);
  g_signal_connect (G_OBJECT (win.window), "destroy",
                  G_CALLBACK (destroy), NULL);
  gtk_container_add (GTK_CONTAINER (win.window), win.vbox);
  gtk_box_pack_start (GTK_BOX (win.hbox), win.play, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (win.hbox), win.stop, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (win.hbox), win.next, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (win.vbox), win.playlist, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (win.vbox), win.hbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (win.vbox), win.artist, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (win.vbox), win.track, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (win.vbox), win.album, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (win.stop, FALSE);
  gtk_widget_set_sensitive (win.next, FALSE);
  gtk_widget_show_all (win.window);

  gtk_main();
  gdk_threads_leave ();

  return 0;
}
