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

int main(int argc, char** argv)
{
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    ibus_unikey_config_init();

    gtk_init(&argc, &argv);
    gtk_window_set_default_icon_from_file(PKGDATADIR "/icons/ibus-unikey.svg", NULL);

    SetupView view;
    view.init();

    SettingsStore store;
    SetupController controller(view, store);
    global_setup_controller_set(&controller);
    controller.init();

    view.showMainWindow();
    gtk_main();

    return 0;
}
