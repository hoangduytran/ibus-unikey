#ifndef SETUP_CONTROLLER_H
#define SETUP_CONTROLLER_H

#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "ui/setup_view.h"
#include "config/settings_store.h"

// Coordinates setup UI events and persistent configuration state
// for the Unikey setup application.
class SetupController
{
public:
    /**
     * @brief Construct controller bound to view and settings store.
     * @param view UI view wrapper for widget access and presentation.
     * @param store Persistent settings backend for config read/write.
     */
    SetupController(SetupView &view, SettingsStore &store);

    /** @brief Initialize controller-level runtime state. */
    void init();

    /** @brief Handle main window destroy and terminate GTK main loop. */
    void handleMainWindowDestroy();

    /**
     * @brief Handle key press events for main setup window.
     * @param widget Source widget receiving the event.
     * @param event Key event payload.
     * @return TRUE when handled; FALSE to continue event propagation.
     */
    gboolean handleMainWindowKeyPress(GtkWidget *widget, GdkEventKey *event);

    /** @brief Handle close button click and terminate GTK main loop. */
    void handleBtnClose();

    /**
     * @brief Persist one boolean setting after toggle change.
     * @param btn Toggle widget whose name maps to config key.
     */
    void handleSettingToggled(GtkToggleButton *btn);

    /**
     * @brief Initialize one toggle widget from stored settings.
     * @param btn Toggle widget whose state should be restored.
     */
    void handleSettingRealize(GtkToggleButton *btn);

    /**
     * @brief Handle input-method combo-box selection change.
     * @param cbb Combo box with input-method options.
     */
    void handleInputMethodChanged(GtkComboBox *cbb);

    /**
     * @brief Handle output-charset combo-box selection change.
     * @param cbb Combo box with output-charset options.
     */
    void handleOutputCharsetChanged(GtkComboBox *cbb);

    /**
     * @brief Restore input-method combo-box selection from settings.
     * @param cbb Combo box with input-method options.
     */
    void handleInputMethodRealize(GtkComboBox *cbb);

    /**
     * @brief Restore output-charset combo-box selection from settings.
     * @param cbb Combo box with output-charset options.
     */
    void handleOutputCharsetRealize(GtkComboBox *cbb);

    /** @brief Open macro dialog, run edit workflow, and persist on Save. */
    void handleMacroEdit();

    /**
     * @brief Handle macro dialog delete-event by hiding dialog.
     * @return TRUE to indicate delete-event was fully handled.
     */
    gboolean handleMacroDialogDelete();

    /** @brief Hide macro dialog without committing additional actions. */
    void handleMacroDialogHide();

    /**
     * @brief Legacy inline key-cell edit handler (currently disabled/no-op).
     * @param celltext Edited cell renderer.
     * @param string_path GTK tree path string for edited row.
     * @param newkey New key text entered by user.
     */
    void handleCellKeyEdited(GtkCellRendererText *celltext, const gchar *string_path, const gchar *newkey);

    /**
     * @brief Legacy inline value-cell edit handler (currently disabled/no-op).
     * @param celltext Edited cell renderer.
     * @param string_path GTK tree path string for edited row.
     * @param newvalue New replacement text entered by user.
     */
    void handleCellValueEdited(GtkCellRendererText *celltext, const gchar *string_path, const gchar *newvalue);

    /** @brief Add one macro entry using dedicated non-modal editor dialog. */
    void handleMacroAdd();

    /** @brief Edit currently selected macro entry in dedicated editor dialog. */
    void handleMacroEditSelected();

    /** @brief Delete currently selected macro entry from canonical list. */
    void handleMacroDel();

    /** @brief Clear search mode or clear all entries (mode-specific behavior). */
    void handleMacroClear();

    /** @brief Import macros from selected file and merge into canonical list. */
    void handleMacroImport();

    /** @brief Export current macros to selected file. */
    void handleMacroExport();

    /** @brief Run search from entry text and render search projection list. */
    void handleMacroSearchActivate();

    /** @brief Return from search-list mode to default-list mode. */
    void handleMacroReturnToDefaultList();

    /**
     * @brief Handle search entry key events.
     * @param widget Search entry widget source.
     * @param event Key event payload.
     * @return TRUE when handled (e.g. Esc), FALSE otherwise.
     */
    gboolean handleMacroSearchKeyPress(GtkWidget *widget, GdkEventKey *event);

    /**
     * @brief Handle row activation (double-click/Enter) and open editor.
     * @param path Activated row path.
     */
    void handleMacroTreeRowActivated(GtkTreePath *path);

private:
    // One canonical UI row in UTF-8 text as displayed/edited in setup.
    struct MacroUiRow
    {
        std::string key;
        std::string value;
    };

    /**
     * @brief Persist current combo-box value to one config key.
     * @param cbb Combo box widget source.
     * @param key Config key to update.
     */
    void cbbConfigUpdate(GtkComboBox *cbb, const char *key);

    /**
     * @brief Set combo-box active row from stored config key value.
     * @param cbb Combo box widget target.
     * @param key Config key to read.
     */
    void cbbConfigSetActive(GtkComboBox *cbb, const char *key);

    /** @brief Return tree model used by macro table view. */
    GtkListStore *macroStore() const;
    /** @brief Return selection helper used by macro table view. */
    GtkTreeSelection *macroSelection() const;
    /** @brief Check whether search projection list is active. */
    bool isSearchMode() const;
    /** @brief Read current search query text from search entry. */
    std::string currentSearchQuery() const;
    /** @brief Resolve selected rendered row to canonical row index. */
    int selectedCanonicalIndex() const;
    /**
     * @brief Case-insensitive match against visible searchable columns.
     * @param row Candidate canonical row.
     * @param queryFolded Already-casefolded query string.
     * @return TRUE when row matches query.
     */
    bool rowMatchesQuery(const MacroUiRow &row, const std::string &queryFolded) const;
    /** @brief Configure tree-view sortable columns once after model wiring. */
    void ensureSortColumns();
    /**
     * @brief Load canonical rows from list-store model.
     * @param store Source GTK list store.
     */
    void loadMacroRowsFromStore(GtkListStore *store);
    /** @brief Render active list (default or searched) into GTK list store. */
    void renderActiveMacroRows();
    /** @brief Update search-only return button visibility. */
    void updateReturnButtonVisibility();
    /** @brief Recompute searched-index projection from current query text. */
    void refreshSearchProjection();
    /** @brief Exit search mode and clear search query/projection. */
    void clearSearchMode();
    /**
     * @brief Programmatically set search text with re-entrancy guard.
     * @param text Null-terminated UTF-8 text.
     */
    void setSearchText(const char *text);
    /**
     * @brief Open non-modal add/edit macro dialog and apply save.
     * @param addMode TRUE for add flow; FALSE for edit flow.
     * @param canonicalIndex Canonical row index for edit mode.
     * @return TRUE when Save applied successfully.
     */
    bool openMacroEditor(bool addMode, int canonicalIndex);
    /** @brief Show confirm dialog then clear all canonical rows if accepted. */
    void showConfirmAndClearAll();

    // Canonical data source for macro table UI.
    std::vector<MacroUiRow> m_defaultEntries;
    // Search projection: each item references m_defaultEntries by index.
    std::vector<int> m_searchedIndices;
    // Active mode flag: false=default list, true=search list.
    bool m_searchMode = false;
    // Guards one-time sorting setup for tree columns.
    bool m_sortConfigured = false;
    // Guards entry text updates to avoid re-entrant search activation.
    bool m_syncingSearchText = false;

    SetupView &m_view;      // reference to the setup UI view
    SettingsStore &m_store; // reference to persistent settings storage
};

// Retrieve the global setup controller instance.
// @return pointer to the currently registered setup controller
SetupController *global_setup_controller();

// Register the global setup controller instance.
// @param c controller instance to set as global
void global_setup_controller_set(SetupController *c);

#endif // SETUP_CONTROLLER_H
