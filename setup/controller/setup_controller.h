#ifndef SETUP_CONTROLLER_H
#define SETUP_CONTROLLER_H

#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "ui/setup_view.h"
#include "config/settings_store.h"

class CMacroTable;

/**
 * @class SetupController
 * @brief Coordinates GTK setup UI callbacks with GSettings and macro table state.
 *
 * Progression typical session: realise widgets -> load config into toggles/combos macro dialog manipulates `m_defaultEntries` then persists via engine `CMacroTable` writers.
 */
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
    /** @brief One canonical macro row (UTF-8 trigger and replacement as edited in setup). */
    struct MacroUiRow
    {
        std::string key;   /**< Trigger text (macro key). */
        std::string value; /**< Replacement text (macro value). */
        /** `g_utf8_casefold` of `key` for substring search (kept in sync when key changes). */
        std::string key_folded;
        /** `g_utf8_casefold` of `value` for substring search (kept in sync when value changes). */
        std::string value_folded;
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
    /**
     * @brief Walk combo column 0; activate first row whose id equals `expected_row_id`.
     * @param cbb Combo using column 0 for persisted option ids.
     * @param expected_row_id NUL-terminated string from config for row match.
     */
    void activateComboBoxRowIfStoredIdMatches(GtkComboBox *cbb,
                                              const gchar *expected_row_id);

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
    /** @brief Recompute `key_folded` / `value_folded` after mutating `key` / `value`. */
    void refreshMacroUiRowSearchFoldCaches(MacroUiRow &row);
    /** @brief Configure tree-view sortable columns once after model wiring. */
    void ensureSortColumns();
    /**
     * @brief Load canonical rows from list-store model.
     * @param store Source GTK list store.
     */
    void loadMacroRowsFromStore(GtkListStore *store);
    /** @brief Render active list (default or searched) into GTK list store. */
    void renderActiveMacroRows();
    /**
     * @brief Fill `store` from full canonical rows then append sentinel placeholder row.
     * @param store Macro GtkListStore for non-search browsing mode.
     */
    void renderFullCanonicalMacroListInto(GtkListStore *store);
    /**
     * @brief Fill `store` using `m_searchedIndices` (skips invalid indices silently).
     * @param store Macro GtkListStore for active search-hit projection mode.
     */
    void renderSearchProjectionInto(GtkListStore *store);
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

    /**
     * @brief Hydrate GtkEntry / GtkTextView from canonical macro before editor run.
     * @param entryKey Replace-field entry.
     * @param txtValue With-field text view buffer target.
     * @param canonicalIndex Valid index inside `m_defaultEntries`.
     */
    void prefillMacroEditorFromCanonical(GtkWidget *entryKey, GtkWidget *txtValue,
                                         int canonicalIndex);
    /**
     * @brief Write validated editor payloads into canonical storage and repaint list.
     * @param key_utf8 Trigger borrowed from GTK entry (`const gchar*`).
     * @param value_utf8 Expansion text (borrowed or empty literal).
     * @param addMode Append vs mutate canonical row semantics.
     * @param canonicalIndex Row index honoured when mutating (`addMode == false`).
     * @param canonicalIndexInRange Must be true to assign into existing canonical row slot.
     */
    void commitMacroEditorSavePayload(const gchar *key_utf8, const gchar *value_utf8,
                                      bool addMode, int canonicalIndex,
                                      bool canonicalIndexInRange);
    /**
     * @brief Export displayed macro rows to filesystem path.
     * @param path_owned UTF-8 path from GTK file chooser; always freed inside.
     */
    void commitMacroExportToPath(gchar *path_owned);

    /**
     * @brief Import-merge path: load macro file, optionally warn, drain into canonical list.
     * @param path_owned UTF-8 path from GTK chooser; always freed inside.
     */
    void commitMacroImportMergeFromPath(gchar *path_owned);

    /**
     * @brief After `macro` load from disk push model into GTK + controller canonical state.
     * @param macro Engine table hydrated from persistence.
     * @param store Visible macro GtkListStore bound to GtkTreeView.
     */
    void applyLoadedMacroTableToDialogUi(CMacroTable *macro, GtkListStore *store);
    /**
     * @brief Rewrite list store purely from canonical vector (no sentinel row yet).
     * @param store Cleared GtkListStore used as staging before `unikey_store_to_macro`.
     */
    void repopulateMacroStoreFromDefaultEntriesForSave(GtkListStore *store);
    /**
     * @brief Finish macro dialog OK: marshal store → macro, mkdir parent dirs, atomic save.
     * @param macro_path UTF-8 path where macro file should persist.
     * @param store Rows synchronized from GTK before engine conversion.
     * @param macro Target `CMacroTable` mutated by converters + writer.
     */
    void commitMacroEditDialogSave(const gchar *macro_path, GtkListStore *store,
                                   CMacroTable *macro);

    /** @brief Full canonical macro list backing the table (source of truth for save/import). */
    std::vector<MacroUiRow> m_defaultEntries;
    /** @brief Indices into `m_defaultEntries` matching the current search query. */
    std::vector<int> m_searchedIndices;
    /** @brief When true, the tree shows `m_searchedIndices` projection instead of full list. */
    bool m_searchMode = false;
    /** @brief Prevents repeated `gtk_tree_view_column_set_sort_column_id` setup. */
    bool m_sortConfigured = false;
    /** @brief Suppresses `handleMacroSearchActivate` during programmatic entry updates. */
    bool m_syncingSearchText = false;

    SetupView &m_view;      /**< UI widget access and dialog handles. */
    SettingsStore &m_store; /**< Persisted settings dependency (constructor-captured; extend as needed). */
};

/** @brief Read the pointer published by `global_setup_controller_set`. */
SetupController *global_setup_controller();

/**
 * @brief Register the singleton pointer used by `global_setup_controller()`.
 * @param c Instance to expose; callers replace prior value blindly.
 */
void global_setup_controller_set(SetupController *c);

#endif // SETUP_CONTROLLER_H
