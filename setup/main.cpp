#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libintl.h>
#include <locale.h>
#include <gtk/gtk.h>

#include "unikey_config.h"

#define _(string) gettext(string)

GtkWidget* mwin;
GtkWidget* dlgMacro;
GtkTreeView* tree_macro;

void init_gtk_builder()
{
    GtkBuilder* builder = gtk_builder_new();
    GError* error = NULL;
    gtk_builder_add_from_file(builder, PKGDATADIR "/ui/ibus-unikey.ui", &error);
    gtk_builder_connect_signals(builder, NULL);

    mwin = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    dlgMacro = GTK_WIDGET(gtk_builder_get_object(builder, "macro_dialog"));
    tree_macro = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tree_macro"));

    if (error != NULL)
    {
        g_error("Failed to load setup UI: %s", error->message);
    }

    if (mwin == NULL || dlgMacro == NULL || tree_macro == NULL)
    {
        g_error("Failed to resolve required setup widgets from GTK builder");
    }

    // GtkBuilder owns a reference to top-level widgets. Keep explicit
    // references so the macro dialog remains valid after the builder is freed.
    g_object_ref_sink(mwin);
    g_object_ref_sink(dlgMacro);

    gtk_window_set_transient_for(GTK_WINDOW(dlgMacro), GTK_WINDOW(mwin));

    g_object_unref(builder);
}

int main(int argc, char** argv)
{
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    ibus_unikey_config_init();

    gtk_init(&argc, &argv);
    gtk_window_set_default_icon_from_file(PKGDATADIR "/icons/ibus-unikey.svg", NULL);

    init_gtk_builder();

    gtk_widget_show_all(mwin);
    gtk_main();

    return 0;
}

