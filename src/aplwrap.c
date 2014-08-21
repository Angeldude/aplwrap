#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include "Avec.h"
#include "keymap.h"

#include "txtbuf.h"
#include "history.h"
#include "aplio.h"
#include "options.h"
#include "apl.h"
#include "menu.h"

#include <signal.h>
int kill(pid_t pid, int sig);	// compiler issue

GtkWidget *window;
GtkWidget *scroll;
GtkWidget *view;
PangoFontDescription *desc = NULL;

static gboolean
key_press_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  if (event->type != GDK_KEY_PRESS) return FALSE;

  GdkEventKey *key_event = (GdkEventKey *)event;

  if (key_event->state == GDK_CONTROL_MASK &&
      (key_event->keyval == GDK_KEY_Super_L ||
       key_event->keyval == GDK_KEY_Break)) {
    if (apl_pid != -1) kill ((pid_t)apl_pid, SIGINT);
    return FALSE;
  }

  /* Filter out NumLock status from event state */
  /* and Honor numeric keypad enter key */
  if (( key_event->state & ~ GDK_MOD2_MASK ) == 0 &&
      (key_event->keyval == GDK_KEY_Return ||
       key_event->keyval == GDK_KEY_KP_Enter)) {
    GtkTextIter end_iter;
    gchar *text;
    gint sz;
    int from_selection;
 
    gtk_text_buffer_get_end_iter (buffer, &end_iter);
    gtk_text_buffer_place_cursor (buffer, &end_iter);

    // FINISH -- rework with copy-down
    text = get_input_text(&sz, &from_selection);

    if (at_prompt)
      history_insert(text+6, sz-6);
    apl_send_inp (text+prompt_len, sz-prompt_len);

    gtk_text_buffer_insert_at_cursor (buffer, "\n", 1);
    g_free (text);
    prompt_len = 0;
    return TRUE;
  }

  // if alt is inactive return false to treat the char as a normal char
  if (!(key_event->state & GDK_MOD1_MASK)) return FALSE; 

  if (key_event->keyval == GDK_KEY_Up) {
    handle_history_replacement(history_prev());
    return TRUE;
  }

  if (key_event->keyval == GDK_KEY_Down) {
    handle_history_replacement(history_next());
    return TRUE;
  }

  guint16 kc = key_event->hardware_keycode;
  if (kc < sizeof(keymap) / sizeof(keymap_s)) {
    CHT_Index ix = (key_event->state & GDK_SHIFT_MASK)
      ? key_shift_alt (kc) :  key_alt (kc);
    if (ix) {
      gshort uic = char_unicode (ix);
      gsize br, bw;
      gchar *res = g_convert ((const gchar *)(&uic),
			      sizeof(gshort),
			      "utf-8",
			      "unicode",
			      &br,
			      &bw,
			      NULL);
      gtk_text_buffer_insert_at_cursor (buffer, res, bw);
      g_free (res);
      return TRUE;
    }
  }

  return FALSE;				// pass the event on
}

int
main (int   argc,
      char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;
  GtkWidget *vbox;

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
    
  gtk_init (&argc, &argv);
  
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_warning ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
  }

  if (apl_spawn(argc, argv)) return 1;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), width, height);
    
  g_signal_connect (window, "destroy",
		    G_CALLBACK (gapl2_quit), NULL);
    
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  build_menubar (vbox);

  desc =
    pango_font_description_from_string (vwidth ? "UnifontMedium" : "FreeMono");
  pango_font_description_set_size (desc, ft_size * PANGO_SCALE);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  view = gtk_text_view_new ();
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 8);
  g_signal_connect (view, "key-press-event",
		    G_CALLBACK (key_press_event), NULL);
  if (desc) gtk_widget_override_font (view, desc);
  gtk_container_add (GTK_CONTAINER (scroll), view);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (scroll), TRUE, TRUE, 2);
  
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  define_tags ();

  gtk_widget_show_all (window);
  gtk_main ();
    
  return 0;
}
