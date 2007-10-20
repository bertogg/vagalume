#include <gtk/gtk.h>
#include <gst/gst.h>

#include "protocol.h"

static GstElement *pipeline, *gstplay;

GtkWidget *window, *play, *stop, *next, *hbox;

static gboolean
bus_call (GstBus     *bus,
	  GstMessage *msg,
	  gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_print ("End-of-stream\n");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ERROR: {
      gchar *debug;
      GError *err;

      gst_message_parse_error (msg, &err, &debug);
      g_free (debug);

      g_print ("Error: %s\n", err->message);
      g_error_free (err);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static void
play_clicked(GtkWidget *widget, gpointer data)
{
  gst_element_set_state (gstplay, GST_STATE_PLAYING);
  gtk_widget_set_sensitive (play, FALSE);
  gtk_widget_set_sensitive (stop, TRUE);
  gtk_widget_set_sensitive (next, TRUE);
}

static void
stop_clicked(GtkWidget *widget, gpointer data)
{
  gst_element_set_state (gstplay, GST_STATE_NULL);
  gtk_widget_set_sensitive (play, TRUE);
  gtk_widget_set_sensitive (stop, FALSE);
  gtk_widget_set_sensitive (next, FALSE);
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

  gtk_main_quit ();
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;
  GstBus *bus;

  /* check input arguments */
  if (argc != 1) {
    g_print ("Usage: %s\n", argv[0]);
    return -1;
  }

  lastfm_session *session = lastfm_handshake();
  if (!session || session->stream_url == NULL) {
    puts("Handshake failed!");
    return -1;
  }

  /* Gtk */
  gtk_init (&argc, &argv);

  /* initialize GStreamer */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* set up */
  gstplay = gst_element_factory_make ("playbin", "play");
  g_object_set (G_OBJECT (gstplay), "uri", session->stream_url, NULL);

  lastfm_session_destroy(session);

  bus = gst_pipeline_get_bus (GST_PIPELINE (gstplay));
  gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* Gtk */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW(window), "Last.fm");
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  hbox = gtk_hbox_new (TRUE, 5);
  play = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PLAY);
  stop = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
  next = gtk_button_new_from_stock (GTK_STOCK_MEDIA_NEXT);
  g_signal_connect (G_OBJECT (play), "clicked",
                    G_CALLBACK (play_clicked), NULL);
  g_signal_connect (G_OBJECT (next), "clicked",
                    G_CALLBACK (next_clicked), NULL);
  g_signal_connect (G_OBJECT (stop), "clicked",
                    G_CALLBACK (stop_clicked), NULL);
  g_signal_connect (G_OBJECT (window), "destroy",
                  G_CALLBACK (destroy), NULL);
  gtk_container_add (GTK_CONTAINER (window), hbox);
  gtk_box_pack_start (GTK_BOX (hbox), play, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), stop, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), next, TRUE, TRUE, 5);
  gtk_widget_set_sensitive (stop, FALSE);
  gtk_widget_set_sensitive (next, FALSE);
  gtk_widget_show_all (window);

  gtk_main();

  return 0;
}
