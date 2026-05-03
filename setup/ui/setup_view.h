#ifndef SETUP_VIEW_H
#define SETUP_VIEW_H

#include <gtk/gtk.h>

class SetupView
{
public:
    SetupView() = default;

    /**
     * @brief Load and initialize GTK UI from builder files.
     *
     * Resolves all required setup widgets, including macro search entry.
     */
    void init();

    /** @brief Show main setup window and child widgets. */
    void showMainWindow();

    /** @brief Show macro dialog and bring it to foreground. */
    void showMacroDialog();

    /** @brief Hide macro dialog without destroying widget tree. */
    void hideMacroDialog();

    /**
     * @brief Get main window widget.
     * @return Pointer to main `GtkWidget`.
     */
    GtkWidget *getMainWindow() const { return m_mainWindow; }

    /**
     * @brief Get macro dialog widget.
     * @return Pointer to macro dialog `GtkWidget`.
     */
    GtkWidget *getMacroDialog() const { return m_macroDialog; }

    /**
     * @brief Get macro table tree view widget.
     * @return Pointer to macro `GtkTreeView`.
     */
    GtkTreeView *getMacroTree() const { return m_treeMacro; }

    /**
     * @brief Get macro search entry widget.
     * @return Pointer to search `GtkEntry`.
     */
    GtkEntry *getMacroSearchEntry() const { return m_macroSearchEntry; }

    /**
     * @brief Get macro return button widget.
     * @return Pointer to return `GtkButton`.
     */
    GtkButton *getMacroReturnButton() const { return m_macroReturnButton; }

private:
    GtkWidget *m_mainWindow = NULL;  // main setup window widget
    GtkWidget *m_macroDialog = NULL; // macro editor dialog widget
    GtkTreeView *m_treeMacro = NULL; // tree view widget for macro entries
    GtkEntry *m_macroSearchEntry = NULL; // search box above macro table
    GtkButton *m_macroReturnButton = NULL; // return button for search-list mode
};

#endif // SETUP_VIEW_H
