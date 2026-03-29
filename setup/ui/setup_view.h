#ifndef SETUP_VIEW_H
#define SETUP_VIEW_H

#include <gtk/gtk.h>
#include <string>

class SetupView {
public:
    SetupView() = default;
    ~SetupView();

    void init();
    void showMainWindow();
    void showMacroDialog();
    void hideMacroDialog();

    GtkWidget* getMainWindow() const { return m_mainWindow; }
    GtkWidget* getMacroDialog() const { return m_macroDialog; }
    GtkTreeView* getMacroTree() const { return m_treeMacro; }

private:
    GtkWidget* m_mainWindow = nullptr;
    GtkWidget* m_macroDialog = nullptr;
    GtkTreeView* m_treeMacro = nullptr;
};

#endif // SETUP_VIEW_H
