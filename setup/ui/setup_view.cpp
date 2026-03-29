#include "setup_view.h"
#include <stdexcept>

void SetupView::init()
{
    GtkBuilder* builder = gtk_builder_new();
    GError* error = NULL;
    gtk_builder_add_from_file(builder, PKGDATADIR "/ui/ibus-unikey.ui", &error);
    if (error != NULL)
    {
        std::string msg = "Failed to load setup UI: ";
        msg += error->message;
        g_error_free(error);
        g_error("%s", msg.c_str());
        throw std::runtime_error(msg);
    }

    gtk_builder_connect_signals(builder, NULL);

    m_mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    m_macroDialog = GTK_WIDGET(gtk_builder_get_object(builder, "macro_dialog"));
    m_treeMacro = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tree_macro"));

    if (m_mainWindow == NULL || m_macroDialog == NULL || m_treeMacro == NULL)
    {
        g_error("Failed to resolve required setup widgets from GTK builder");
        throw std::runtime_error("Missing required widgets in setup UI");
    }

    g_object_ref_sink(m_mainWindow);
    g_object_ref_sink(m_macroDialog);

    gtk_window_set_transient_for(GTK_WINDOW(m_macroDialog), GTK_WINDOW(m_mainWindow));
    g_object_unref(builder);
}

SetupView::~SetupView()
{
    if (m_macroDialog) g_object_unref(m_macroDialog);
    if (m_mainWindow) g_object_unref(m_mainWindow);
}

void SetupView::showMainWindow()
{
    gtk_widget_show_all(m_mainWindow);
}

void SetupView::showMacroDialog()
{
    gtk_widget_show_all(m_macroDialog);
    gtk_window_present(GTK_WINDOW(m_macroDialog));
}

void SetupView::hideMacroDialog()
{
    gtk_widget_hide(m_macroDialog);
}
