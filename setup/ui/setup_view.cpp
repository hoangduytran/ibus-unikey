#include "setup_view.h"

// Load the setup UI builder files and resolve required widgets.
// This keeps references to the top-level main window and macro dialog
// so they remain valid after the builder is destroyed.
void SetupView::init()
{
    GtkBuilder *builder = gtk_builder_new();
    GError *error = NULL;
    gtk_builder_add_from_file(builder, PKGDATADIR "/ui/main_window.ui", &error);
    if (error != NULL)
    {
        g_error("Failed to load setup UI: %s", error->message);
    }

    gtk_builder_add_from_file(builder, PKGDATADIR "/ui/macro_dialog.ui", &error);
    if (error != NULL)
    {
        g_error("Failed to load setup UI: %s", error->message);
    }

    gtk_builder_connect_signals(builder, NULL);

    m_mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    m_macroDialog = GTK_WIDGET(gtk_builder_get_object(builder, "macro_dialog"));
    m_treeMacro = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tree_macro"));
    m_macroSearchEntry = GTK_ENTRY(gtk_builder_get_object(builder, "entry_macro_search"));
    m_macroReturnButton = GTK_BUTTON(gtk_builder_get_object(builder, "btn_macro_return"));

    if (m_mainWindow == NULL || m_macroDialog == NULL || m_treeMacro == NULL ||
        m_macroSearchEntry == NULL || m_macroReturnButton == NULL)
    {
        g_error("Failed to resolve required setup widgets from GTK builder");
    }

    // GtkBuilder owns a reference to top-level widgets. Keep explicit
    // references so the macro dialog remains valid after the builder is freed.
    g_object_ref_sink(m_mainWindow);
    g_object_ref_sink(m_macroDialog);

    gtk_window_set_transient_for(GTK_WINDOW(m_macroDialog), GTK_WINDOW(m_mainWindow));

    g_object_unref(builder);
}

// Show the main setup window and all child widgets.
void SetupView::showMainWindow()
{
    gtk_widget_show_all(m_mainWindow);
}

// Show the macro editor dialog and bring it to the front.
void SetupView::showMacroDialog()
{
    gtk_widget_show_all(m_macroDialog);
    gtk_window_present(GTK_WINDOW(m_macroDialog));
}

// Hide the macro editor dialog without destroying it.
void SetupView::hideMacroDialog()
{
    gtk_widget_hide(m_macroDialog);
}
