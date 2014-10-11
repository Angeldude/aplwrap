#ifndef EDIT_H
#define EDIT_H

#include <gtk/gtk.h>

#include "search.h"

typedef struct {
  GtkTextBuffer *buffer;
  gchar *name;
  gint ref_count;
  gint nc;
} buffer_s;

typedef struct {
  GtkWidget *window;
  GtkWidget *status_line;
  buffer_s  *buffer;
  gchar     *path;
  gboolean   error;
  gboolean   cb_done;
  gboolean   closing;
#if ENABLE_SEARCH
  search_context_t *search;
#endif
} window_s;
#define window(w) (w)->window
#define buffer(w) (w)->buffer
#define status(w) (w)->status_line
#define path(w)   (w)->path
#define error(w)  (w)->error
#define cb_done(w) (w)->cb_done
#define closing(w) (w)->closing
#if ENABLE_SEARCH
#define search(w) (w)->search
#endif

void edit_object (gchar* name, gint nc);

void edit_file (gchar *path);

gboolean dirty_edit_buffers ();

void message_dialog (GtkMessageType type,
                       gchar         *message,
                       gchar         *secondary);

#endif  /* EDIT_H */
