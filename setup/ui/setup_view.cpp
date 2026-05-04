/**
 * @file setup_view.cpp
 * @brief Loads `main_window.ui` / `macro_dialog.ui` and owns top-level GTK widget pointers for `SetupView`.
 */

#include "setup_view.h"

void SetupView::init() {
  GtkBuilder *builder = gtk_builder_new();
  GError *error = NULL;
  gtk_builder_add_from_file(builder, PKGDATADIR "/ui/main_window.ui", &error);
  if (error != NULL) {
    g_error("Failed to load setup UI: %s", error->message);
  }

  gtk_builder_add_from_file(builder, PKGDATADIR "/ui/macro_dialog.ui", &error);
  if (error != NULL) {
    g_error("Failed to load setup UI: %s", error->message);
  }

  gtk_builder_connect_signals(builder, NULL);

  m_mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
  m_macroDialog = GTK_WIDGET(gtk_builder_get_object(builder, "macro_dialog"));
  m_treeMacro = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tree_macro"));
  m_macroSearchEntry = GTK_ENTRY(gtk_builder_get_object(builder, "entry_macro_search"));
  m_macroReturnButton = GTK_BUTTON(gtk_builder_get_object(builder, "btn_macro_return"));

  if (m_mainWindow == NULL || m_macroDialog == NULL || m_treeMacro == NULL ||
      m_macroSearchEntry == NULL || m_macroReturnButton == NULL) {
    g_error("Failed to resolve required setup widgets from GTK builder");
  }

  // GtkBuilder holds a ref until destroyed; sink an extra ref so windows survive `g_object_unref(builder)`.
  g_object_ref_sink(m_mainWindow);
  g_object_ref_sink(m_macroDialog);

  gtk_window_set_transient_for(GTK_WINDOW(m_macroDialog), GTK_WINDOW(m_mainWindow));

  g_object_unref(builder);
}

void SetupView::showMainWindow() { gtk_widget_show_all(m_mainWindow); }

void SetupView::showMacroDialog() {
  gtk_widget_show_all(m_macroDialog);
  gtk_window_present(GTK_WINDOW(m_macroDialog));
}

void SetupView::hideMacroDialog() { gtk_widget_hide(m_macroDialog); }
