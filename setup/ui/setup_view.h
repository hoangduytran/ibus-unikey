#ifndef SETUP_VIEW_H
#define SETUP_VIEW_H

#include <gtk/gtk.h>

class SetupView
{
public:
    SetupView() = default;

    // Load and initialize the GTK UI from builder files.
    // This resolves the main window, macro dialog, and macro tree widgets.
    void init();

    // Show the main setup window.
    void showMainWindow();

    // Show the macro editor dialog.
    void showMacroDialog();

    // Hide the macro editor dialog.
    void hideMacroDialog();

    // Access the main setup window widget.
    // @return pointer to the main GtkWidget window
    GtkWidget *getMainWindow() const { return m_mainWindow; }

    // Access the macro dialog widget.
    // @return pointer to the macro dialog GtkWidget
    GtkWidget *getMacroDialog() const { return m_macroDialog; }

    // Access the macro tree view widget.
    // @return pointer to the macro tree GtkTreeView
    GtkTreeView *getMacroTree() const { return m_treeMacro; }

private:
    GtkWidget *m_mainWindow = NULL;  // main setup window widget
    GtkWidget *m_macroDialog = NULL; // macro editor dialog widget
    GtkTreeView *m_treeMacro = NULL; // tree view widget for macro entries
};

#endif // SETUP_VIEW_H
