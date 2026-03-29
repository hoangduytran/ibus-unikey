#ifndef SETUP_VIEW_H
#define SETUP_VIEW_H

#include <gtk/gtk.h>

class SetupView {
public:
    SetupView() = default;

    void init();
    void showMainWindow();
    void showMacroDialog();
    void hideMacroDialog();

    GtkWidget* getMainWindow() const { return m_mainWindow; }
    GtkWidget* getMacroDialog() const { return m_macroDialog; }
    GtkTreeView* getMacroTree() const { return m_treeMacro; }

private:
    GtkWidget* m_mainWindow = NULL;
    GtkWidget* m_macroDialog = NULL;
    GtkTreeView* m_treeMacro = NULL;
};

#endif // SETUP_VIEW_H
