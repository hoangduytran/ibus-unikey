/**
 * @file setup_controller_lifecycle.cpp
 * @brief Part of SetupController implementation (split from setup_controller.cpp).
 */

#include "setup_controller.h"

#include <gdk/gdkkeysyms.h>

/**
 * @brief Capture view and settings references used by GTK callbacks.
 */
SetupController::SetupController(SetupView &view, SettingsStore &store)
    : m_view(view), m_store(store) {}

/** @brief Placeholder hook for deferred controller-wide startup wiring. */
void SetupController::init() {}

/**
 * @brief Quit application from main window Escape key routing.
 *
 * Progression: if Escape -> terminate GTK loop and consume event.
 */
gboolean SetupController::handleMainWindowKeyPress(GtkWidget *widget,
                                                   GdkEventKey *event) {
  (void)widget;
  const bool pressedEscape = (event->keyval == GDK_KEY_Escape);
  if (pressedEscape) {
    gtk_main_quit();
    return true;
  }
  return false;
}

/** @brief End GTK main loop when the main window is destroyed. */
void SetupController::handleMainWindowDestroy() { gtk_main_quit(); }

/** @brief End GTK main loop when the user closes via Close button. */
void SetupController::handleBtnClose() { gtk_main_quit(); }
