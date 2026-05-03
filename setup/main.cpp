#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/**
 * @file main.cpp
 * @brief `ibus-setup-unikey` entry point: locale, config, GTK, view/controller wiring, main loop.
 */

#include <libintl.h>
#include <locale.h>
#include <gtk/gtk.h>

#include "unikey_config.h"

#include "ui/setup_view.h"
#include "config/settings_store.h"
#include "controller/setup_controller.h"

/**
 * @brief Initializes gettext, UniKey config, GTK, builds the setup UI, and runs `gtk_main()`.
 * @param argc Argument count from the runtime.
 * @param argv Argument vector; consumed by `gtk_init()`.
 * @return Exit status (always 0 on normal shutdown).
 */
int main(int argc, char **argv) {
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
