#include "../config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <sys/socket.h>

#include "apl.h"
#include "aplio.h"
#include "menu.h"
#include "aplwrap.h"
#include "txtbuf.h"
#include "options.h"
#include "edit.h"

static GHashTable *buffers = NULL;
static gint seq_nr = 1;

static void
edit_close (GtkWidget *widget,
	    gpointer  data)
{
  window_s *tw = data;
  if (!gtk_widget_in_destruction (GTK_WIDGET (window (tw)))) {
    buffer_s *tb = buffer (tw);
    tb->ref_count--;
    if (0 == tb->ref_count) {
      g_hash_table_remove (buffers, tb->name);
      g_free (tb->name);
      g_free (tb);
    }
    gtk_widget_destroy(window (tw));
    g_free (tw);
  }
}

static void
edit_save_cb (gchar *text, void *data)
{
  set_socket_cb (NULL, NULL);
  gchar **lines = g_strsplit (text, "\n", 0);
  for (int i = 0; lines[i]; i++) {
    if (!*lines[i]) continue;
    if (!g_strcmp0 (lines[i], END_TAG)) break;
    tagged_insert (lines[i], -1, TAG_OUT);
    tagged_insert (" ", -1, TAG_OUT);
  }
  g_strfreev (lines);
  tagged_insert ("\n      ", -1, TAG_OUT);
  if (data) {
    window_s *tw = data;
    buffer_s *tb = buffer (tw);
    tb->modified = FALSE;
  }
}

static void
clone_object (GtkWidget *widget,
	      gpointer  data)
{
  window_s *tw = data;
  buffer_s *tb = buffer (tw);

  edit_object (tb->name, tb->nc);
}

static void
edit_save (GtkWidget *widget,
	   gpointer  data)
{
  window_s *tw = data;
  buffer_s *tb = buffer (tw);
  GtkTextIter start_iter, end_iter;
  gtk_text_buffer_get_bounds (tb->buffer, &start_iter, &end_iter);
  gchar *text =
    gtk_text_buffer_get_text (tb->buffer, &start_iter, &end_iter, FALSE);
  if (text[strlen (text) - 1] != '\n') {
    gtk_text_buffer_insert (tb->buffer, &end_iter, "\n", 1);
    gtk_text_buffer_get_bounds (tb->buffer, &start_iter, &end_iter);
    g_free (text);
    text =
      gtk_text_buffer_get_text (tb->buffer, &start_iter, &end_iter, FALSE);
  }

  // widget is just serving as a flag as to whether the window is bein closed
  set_socket_cb (edit_save_cb, (widget == NULL) ? NULL : tw);
  send (sockfd, "def\n", strlen("def\n"), 0);
  gchar *ptr     = text;
  while (ptr && *ptr) {
    gchar *end_ptr = strchr (ptr, '\n');
    if (end_ptr++) {
      send (sockfd, ptr, end_ptr - ptr, 0);
      ptr = end_ptr;
    }
  }
  g_free (text);
  send (sockfd, END_TAGNL, strlen(END_TAGNL), 0);
}

static gboolean
edit_delete_real (GtkWidget *widget,
		  gpointer   data)
{
  /* If you return FALSE in the "delete-event" signal handler,
   * GTK will emit the "destroy" signal. Returning TRUE means
   * you don't want the window to be destroyed.
   * This is useful for popping up 'are you sure you want to quit?'
   * type dialogs. */

  /* Change TRUE to FALSE and the main window will be destroyed with
   * a "delete-event". */
  
  window_s *tw = data;

  buffer_s *tb = buffer (tw);
  if (tb->modified) {
    GtkWidget *e_dialog;
    gint response;
    e_dialog = gtk_message_dialog_new (NULL,
				       GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_QUESTION,
				       GTK_BUTTONS_NONE,
				       _ ("Buffer %s modified.  Save it?"),
				       tb->name);
    gtk_dialog_add_buttons (GTK_DIALOG (e_dialog),
			    _("Yes"),    GTK_RESPONSE_YES,
			    _("No"),     GTK_RESPONSE_NO,
			    _("Cancel"), GTK_RESPONSE_CANCEL,
			    NULL);
    gtk_window_set_keep_above (GTK_WINDOW (e_dialog), TRUE);
    gtk_window_set_position (GTK_WINDOW (e_dialog), GTK_WIN_POS_MOUSE);
    response = gtk_dialog_run (GTK_DIALOG (e_dialog));
    gtk_widget_destroy (e_dialog);
    switch(response) {
    case GTK_RESPONSE_CANCEL:
      return TRUE;			// abort close
      break;
    case GTK_RESPONSE_NO:
      return FALSE;			// close without saving
      break;
    case GTK_RESPONSE_YES:
      edit_save (NULL, tw);
      return FALSE;			// close
      break;
    }
  }
  return FALSE;			// close
}

/***
    this is called as a result of a "destroy" signal to the window.
    a return FALSE will automatically result in a real closure
 ***/
static gboolean
edit_delete_event (GtkWidget *widget,
		   GdkEvent  *event,
		   gpointer   data)
{
  return edit_delete_real (widget, data);
}

/***
    this is called as a result of pressing the close button in the
    file pulldown.
 ***/
static void
edit_delete (GtkWidget *widget,
	     gpointer   data)
{
  gboolean rc = edit_delete_real (widget, data);
  if (!rc) edit_close (widget, data);
}

static void
build_edit_menubar (GtkWidget *vbox, window_s *tw)
{
  GtkWidget *menubar;
  GtkWidget *menu;
  GtkWidget *item;

  buffer_s *tb = buffer (tw);
  menubar = gtk_menu_bar_new();

  menu = gtk_menu_new();

  item = gtk_menu_item_new_with_label (_ ("File"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);

  item = gtk_menu_item_new_with_label(_ ("New"));
  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (new_object), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Open"));
  g_signal_connect(G_OBJECT (item), "activate",
		   G_CALLBACK (open_object), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Clone"));
  g_signal_connect(G_OBJECT (item), "activate",
  		   G_CALLBACK (clone_object), tw);
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_mnemonic ("_Save");
  g_signal_connect(G_OBJECT (item), "activate",
		   G_CALLBACK (edit_save), tw);
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Save File"));
  g_signal_connect(G_OBJECT (item), "activate",
		   G_CALLBACK (save_log), tb->buffer);
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Save File As"));
  g_signal_connect(G_OBJECT (item), "activate",
		   G_CALLBACK (save_log_as), tb->buffer);
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
  
  item = gtk_separator_menu_item_new();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  
  item = gtk_menu_item_new_with_label (_ ("Close"));
#if 1
  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (edit_delete), tw);
#else
  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (edit_close), tw);
#endif
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (menubar), FALSE, FALSE, 2);
}

static gboolean
edit_key_press_event (GtkWidget *widget,
		      GdkEvent  *event,
		      gpointer   user_data)
{
  if (event->type != GDK_KEY_PRESS) return FALSE;

  window_s *tw = user_data;
  buffer_s *tb = buffer (tw);
  tb->modified = TRUE;
  GdkEventKey *key_event = (GdkEventKey *)event;

  if (!(key_event->state & GDK_MOD1_MASK)) return FALSE;

  gsize bw;
  gchar *res = handle_apl_characters (&bw, key_event);
  if (res) {
    gtk_text_buffer_insert_at_cursor (tb->buffer, res, bw);
    g_free (res);
    return TRUE;
  }

  return FALSE;				// pass the event on
}

static void
edit_function_cb (gchar *text, void *data)
{
  set_socket_cb (NULL, NULL);

  window_s *tw = data;
  buffer_s *tb = buffer (tw);
  gchar **lines = g_strsplit (text, "\n", 0);
  for (int i = 1; lines[i]; i++) {
    if (!g_strcmp0 (lines[i], END_TAG)) break;
    gtk_text_buffer_insert_at_cursor (tb->buffer, lines[i], -1);
    gtk_text_buffer_insert_at_cursor (tb->buffer, "\n", -1);
  }
  g_strfreev (lines);
}

static void
edit_variable_cb (gchar *text, void *data)
{
  set_socket_cb (NULL, NULL);
  g_print ("vbl: %s\n", text);
#if 0
  window_s *tw = data;
  gchar **lines = g_strsplit (text, "\n", 0);
  for (int i = 1; lines[i]; i++) {
    if (!g_strcmp0 (lines[i], END_TAG)) break;
    gtk_text_buffer_insert_at_cursor (edit_buffer, lines[i], -1);
    gtk_text_buffer_insert_at_cursor (edit_buffer, "\n", -1);
  }
  g_strfreev (lines);
#endif
}

void
edit_object (gchar* name, gint nc)
{
  GtkWidget *vbox;
  GtkWidget *scroll;
  GtkWidget *view;
  PangoFontDescription *desc = NULL;
  window_s *this_window = NULL;
  buffer_s *this_buffer = NULL;
  GtkWidget *window;

  if (!buffers) buffers = g_hash_table_new (g_str_hash, g_str_equal);

  gchar *lname =
    name ? g_strdup (name) : g_strdup_printf ("Unnamed_%d", seq_nr++);

  this_window = g_malloc (sizeof(window_s));
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  window (this_window) = window;
  gtk_window_set_title (GTK_WINDOW (window), name ? : "Unnamed");
  gtk_window_set_default_size (GTK_WINDOW (window), width, height);

  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  
  desc =
    pango_font_description_from_string (vwidth ? "UnifontMedium" : "FreeMono");
  pango_font_description_set_size (desc, ft_size * PANGO_SCALE);

  scroll = gtk_scrolled_window_new (NULL, NULL);

  if (g_hash_table_contains (buffers, lname)) {
    this_buffer = g_hash_table_lookup (buffers, lname);
    this_buffer->ref_count++;
  }
  
  if (!this_buffer) {
    this_buffer = g_malloc (sizeof(buffer_s));
    this_buffer->buffer = gtk_text_buffer_new (NULL);
    this_buffer->modified = FALSE;
    this_buffer->name = lname;
    this_buffer->ref_count = 1;
    this_buffer->nc = name ? nc : NC_FUNCTION; // fixme
    g_hash_table_insert (buffers, this_buffer->name, this_buffer);

    if (name) {
      if (nc == NC_FUNCTION) {
	set_socket_cb (edit_function_cb, this_window);
	gchar *cmd = g_strdup_printf ("fn:%s\n", name);
	if (send(sockfd, cmd, strlen(cmd), 0) < 0) {
	  perror("Error in send()");	// fixme
	}
	g_free (cmd);
      }
      else if (nc == NC_VARIABLE) {
	set_socket_cb (edit_variable_cb, this_window);
	gchar *cmd = g_strdup_printf ("getvar:%s\n", name);
	if (send(sockfd, cmd, strlen(cmd), 0) < 0) {
	  perror("Error in send()");	// fixme
	}
	g_free (cmd);
      }
    }
  }

  buffer (this_window) = this_buffer;

  build_edit_menubar (vbox, this_window);

#if 1
  g_signal_connect (window, "delete-event",
		      G_CALLBACK (edit_delete_event), this_window);
#endif

  g_signal_connect (window, "destroy",
		    G_CALLBACK (edit_close), this_window);
  
  view = gtk_text_view_new_with_buffer (this_buffer->buffer);
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 8);

  g_signal_connect (view, "key-press-event",
		    G_CALLBACK (edit_key_press_event), this_window);
  if (desc) gtk_widget_override_font (view, desc);
  gtk_container_add (GTK_CONTAINER (scroll), view);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (scroll), TRUE, TRUE, 2);
  gtk_widget_show_all (window);
}
