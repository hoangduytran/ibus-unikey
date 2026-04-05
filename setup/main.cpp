#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libintl.h>
#include <locale.h>
#include <gtk/gtk.h>

#include "unikey_config.h"

#include "ui/setup_view.h"
#include "config/settings_store.h"
#include "controller/setup_controller.h"

// Application entry point for the setup UI.
// Initializes localization, the GTK runtime, and the setup view/controller.
// @param argc count of command-line arguments
// @param argv array of command-line argument strings
// @return 0 on successful application exit
int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    // Initialize the Unikey configuration backend.
    ibus_unikey_config_init();

    // Initialize GTK and parse command-line arguments.
    gtk_init(&argc, &argv);
    gtk_window_set_default_icon_from_file(PKGDATADIR "/icons/ibus-unikey.svg", NULL);

    // Create and initialize the setup UI view.
    SetupView view;
    view.init();

    // Create the settings store and controller, then register the global controller.
    SettingsStore store;
    SetupController controller(view, store);
    global_setup_controller_set(&controller);
    controller.init();

    // Show the main window and enter the GTK main loop.
    view.showMainWindow();
    gtk_main();

    return 0;
}
