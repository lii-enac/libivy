#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <Ivy/ivy.h>
#include <Ivy/ivyglibloop.h>




void sendText( GtkWidget *widget, gpointer user_data ) {
  IvySendMsg(gtk_label_get_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(widget)))));
}

void setText(IvyClientPtr app, void *user_data, int argc, char *argv[]){
    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(user_data))), argv[0]); 
}

int main( int   argc, char *argv[] ) {
    GtkWidget *window;
    GtkWidget *button;
    char *bus=getenv("IVYBUS");
    gtk_init(&argc, &argv);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER (window), 10);
    button = gtk_button_new_with_label("ivy is cool");
    gtk_container_add (GTK_CONTAINER(window), button);
    g_signal_connect(G_OBJECT(button),"clicked",G_CALLBACK(sendText), NULL); 
    gtk_widget_show_all (window);
    IvyInit ("IvyGtkButton", "IvyGtkButton READY", NULL, NULL, NULL, NULL);
    IvyBindMsg(setText, button, "^Ivy Button text=(.*)");
    IvyStart(bus);
    gtk_main();
    return 0;
}
