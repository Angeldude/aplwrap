#ifndef MENU_H
#define MENU_H

#include <gtk/gtk.h>

void build_menubar (GtkWidget *vbox);
void open_object (GtkWidget *widget, gpointer   data);
void new_object (GtkWidget *widget, gpointer   data);

#endif
