/**
 * @file setup_controller_macro_io.cpp
 * @brief Part of SetupController implementation (split from setup_controller.cpp).
 */

#include "setup_controller.h"

#include <libintl.h>

#define _(str) gettext(str)

#include <setup/macro_export_filename.h>
#include <setup/macro_file_io.h>
#include <ukengine/mapping/mactab.h>

#include "setup_controller_components/setup_controller_internal.h"
#include "macro_utils.h"
#include "unikey_config.h"

/**
 * @brief Open macro dialog, load macros from disk, run modal workflow, save on OK.
 *
 * Progression:
 * 1. Resolve path via `get_macro_file()` and load into `CMacroTable`.
 * 2. `applyLoadedMacroTableToDialogUi` syncs list + `m_defaultEntries`.
 * 3. Show/present dialog; on `GTK_RESPONSE_OK`, `commitMacroEditDialogSave`.
 * 4. Always `g_free` the macro file path heap string from (1).
 */
void SetupController::handleMacroEdit() {
  gchar *macrofile = get_macro_file();

  CMacroTable macro;
  macro.init();
  macro.loadFromFile(macrofile);

  GtkListStore *store =
      GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));
  applyLoadedMacroTableToDialogUi(&macro, store);

  gtk_widget_show_all(m_view.getMacroDialog());
  gtk_window_present(GTK_WINDOW(m_view.getMacroDialog()));

  const int ret = gtk_dialog_run(GTK_DIALOG(m_view.getMacroDialog()));
  const bool userCommittedMacroDialog = (ret == GTK_RESPONSE_OK);
  if (userCommittedMacroDialog)
    commitMacroEditDialogSave(macrofile, store, &macro);

  g_free(macrofile);
}

/**
 * @brief Push a loaded engine table into the GTK list store and refresh controller state.
 * @param macro Table already filled e.g. from `loadFromFile`.
 * @param store Macro dialog `GtkListStore` bound to the tree view.
 *
 * Progression: `unikey_macro_to_store` → `loadMacroRowsFromStore` → sort columns →
 * `clearSearchMode` → `renderActiveMacroRows`.
 */
void SetupController::applyLoadedMacroTableToDialogUi(CMacroTable *macro,
                                                       GtkListStore *store) {
  unikey_macro_to_store(macro, store);
  loadMacroRowsFromStore(store);
  ensureSortColumns();
  clearSearchMode();
  renderActiveMacroRows();
}

/**
 * @brief Rebuild the GTK list from `m_defaultEntries` alone (before `unikey_store_to_macro`).
 * @param store Target list store (cleared first).
 *
 * Progression:
 * 1. `gtk_list_store_clear`.
 * 2. One `append_macro_list_row` per canonical row (no trailing placeholder row here).
 */
void SetupController::repopulateMacroStoreFromDefaultEntriesForSave(
    GtkListStore *store) {
  gtk_list_store_clear(store);
  for (size_t i = 0; i < m_defaultEntries.size(); i++) {
    const MacroUiRow &row = m_defaultEntries[i];
    append_macro_list_row(store, row.key.c_str(), row.value.c_str(),
                          static_cast<int>(i));
  }
}

/**
 * @brief Persist macro dialog edits: list → engine table, mkdir parent if needed, write file.
 * @param macro_path Target macro file path (UTF-8, not freed here).
 * @param store List store rebuilt from `m_defaultEntries` for conversion.
 * @param macro Engine table receiving `unikey_store_to_macro` output.
 *
 * Progression:
 * 1. `repopulateMacroStoreFromDefaultEntriesForSave` then `unikey_store_to_macro`.
 * 2. On conversion failure: error dialog and return.
 * 3. Ensure parent directory exists when resolvable via `GFile`.
 * 4. `writeToFile`; on failure show error dialog.
 */
void SetupController::commitMacroEditDialogSave(const gchar *macro_path,
                                                GtkListStore *store,
                                                CMacroTable *macro) {
  repopulateMacroStoreFromDefaultEntriesForSave(store);

  UnikeyMacroTableFillResult sync = unikey_store_to_macro(store, macro);
  const bool macroConversionHadFailures = (sync.failed > 0);
  GtkWindow *parentWin = GTK_WINDOW(m_view.getMacroDialog());

  if (macroConversionHadFailures) {
    run_macro_error_dialog(parentWin, macro,
                           _("Not all macros could be saved. The macro file was not written."));
    return;
  }

  GFile *pathObj = g_file_new_for_path(macro_path);
  GFile *parentDir = g_file_get_parent(pathObj);
  g_object_unref(pathObj);

  const bool parentDirResolved = (parentDir != nullptr);
  if (parentDirResolved) {
    const bool parentDirExists =
        (g_file_query_exists(parentDir, nullptr) != FALSE);
    if (!parentDirExists)
      g_file_make_directory_with_parents(parentDir, nullptr, nullptr);
    g_object_unref(parentDir);
  }

  const bool wroteMacroFile = macro->writeToFile(macro_path);
  if (!wroteMacroFile) {
    run_macro_error_dialog(parentWin, macro,
                           _("Could not write the macro file."));
  }
}

/**
 * @brief Hide macro dialog on window manager delete; treat as handled.
 */
gboolean SetupController::handleMacroDialogDelete() {
  // hide the macro dialog
  gtk_widget_hide(m_view.getMacroDialog());
  // return True
  return true;
}

/**
 * @brief Hide macro dialog from explicit cancel/close without extra side
 * effects.
 */
void SetupController::handleMacroDialogHide() {
  // hide the macro dialog
  gtk_widget_hide(m_view.getMacroDialog());
}

/**
 * @brief Reserved for inline cell editing; currently unused (no-op parameters).
 */
void SetupController::handleCellKeyEdited(GtkCellRendererText *celltext,
                                          const gchar *string_path,
                                          const gchar *newkey) {
  (void)celltext;
  (void)string_path;
  (void)newkey;
}

/**
 * @brief Reserved for inline cell editing; currently unused (no-op parameters).
 */
void SetupController::handleCellValueEdited(GtkCellRendererText *celltext,
                                            const gchar *string_path,
                                            const gchar *newvalue) {
  (void)celltext;
  (void)string_path;
  (void)newvalue;
}

/**
 * @brief Start add flow in the dedicated non-modal macro editor.
 *
 * Progression: `openMacroEditor(true, -1)` -> row append on successful save
 * path inside editor.
 */
void SetupController::handleMacroAdd() { openMacroEditor(true, -1); }

/**
 * @brief Start edit flow for the currently selected canonical index.
 *
 * Progression: resolve `selectedCanonicalIndex` bounds-check ->
 * `openMacroEditor(false, idx)`.
 */
void SetupController::handleMacroEditSelected() {

  // get the selected canonical index
  const int idx = selectedCanonicalIndex();
  const bool selectedRowInCanonicalRange =
      (idx >= 0 && idx < static_cast<int>(m_defaultEntries.size()));
  if (!selectedRowInCanonicalRange)
    return;
  openMacroEditor(false, idx);
}

/**
 * @brief Remove selected canonical row and refresh search projection if active.
 *
 * Progression: resolve index -> erase from `m_defaultEntries` -> optional
 * `refreshSearchProjection` -> `renderActiveMacroRows`.
 */
void SetupController::handleMacroDel() {

  // get the selected canonical index
  const int idx = selectedCanonicalIndex();
  const bool selectedRowInCanonicalRange =
      (idx >= 0 && idx < static_cast<int>(m_defaultEntries.size()));
  if (!selectedRowInCanonicalRange)
    return;
  m_defaultEntries.erase(m_defaultEntries.begin() + idx);
  const bool searchProjectionStaleAfterDelete = isSearchMode();
  if (searchProjectionStaleAfterDelete)
    refreshSearchProjection();
  // render the active macro rows
  renderActiveMacroRows();
}

/**
 * @brief Clear-all or exit search depending on `m_searchMode`.
 *
 * Progression:
 * - Search mode: `clearSearchMode` rerender (canonical data untouched).
 * - Default mode: confirm dialog then erase all canonical rows on OK.
 */
void SetupController::handleMacroClear() {
  const bool actingInsideSearchHitsView = isSearchMode();
  if (actingInsideSearchHitsView) {
    // clear the search mode
    clearSearchMode();
    // render the active macro rows
    renderActiveMacroRows();
    // return
    return;
  }
  // show the confirm and clear all dialog
  showConfirmAndClearAll();
}

/**
 * @brief Search entry "activate" / committed text: rebuild projection from
 * query.
 *
 * Progression: ignore if `m_syncingSearchText` -> `refreshSearchProjection` ->
 * `renderActiveMacroRows`.
 */
void SetupController::handleMacroSearchActivate() {
  const bool ignoreSearchWhileSyncingUiTextField = m_syncingSearchText;
  if (ignoreSearchWhileSyncingUiTextField)
    return;
  // refresh the search projection
  refreshSearchProjection();
  // render the active macro rows
  renderActiveMacroRows();
}

/**
 * @brief User pressed Return-to-default-list control; leave search without
 * deleting data.
 */
void SetupController::handleMacroReturnToDefaultList() {
  // clear the search mode
  clearSearchMode();
  // render the active macro rows
  renderActiveMacroRows();
}

/**
 * @brief Keyboard routing on the search entry (Escape exits search in default
 * mode).
 * @return TRUE when the key is consumed.
 */
gboolean SetupController::handleMacroSearchKeyPress(GtkWidget *widget,
                                                    GdkEventKey *event) {
  (void)widget;
  const bool pressedEscape = (event->keyval == GDK_KEY_Escape);
  const bool ignoreEscapeInActiveSearchHitList =
      (isSearchMode() && pressedEscape);
  if (ignoreEscapeInActiveSearchHitList) {
    // Esc is no longer used to leave search-list mode.
    return TRUE;
  }
  if (pressedEscape) {
    clearSearchMode();
    renderActiveMacroRows();
    return TRUE;
  }
  return FALSE;
}

/**
 * @brief Tree row activation (double-click / Enter) opens edit dialog for
 * selection.
 */
void SetupController::handleMacroTreeRowActivated(GtkTreePath *path) {
  (void)path;
  handleMacroEditSelected();
}

/**
 * @brief Merge macros from user-selected UTF-8 / legacy text via `CMacroTable`
 * parser.
 *
 * Open chooser from `build_macro_import_open_chooser`; OK path merges via
 * `commitMacroImportMergeFromPath`.
 */
void SetupController::handleMacroImport() {
  GtkWidget *chooser =
      build_macro_import_open_chooser(GTK_WINDOW(m_view.getMacroDialog()));
  const bool userChoseImportPath =
      (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_OK);
  if (userChoseImportPath) {
    gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    commitMacroImportMergeFromPath(path);
  }
  gtk_widget_destroy(chooser);
}

/**
 * @brief Load a macro file into a scratch engine table and merge rows into `m_defaultEntries`.
 * @param path_owned Heap path from the file chooser; always `g_free`d here.
 *
 * Progression:
 * 1. `macro_interchange_import_path` into temporary `CMacroTable` (extension-based interchange; no `.ukmcache`).
 * 2. Drain scratch `GtkListStore` rows into `m_defaultEntries`.
 * 3. If search mode active, `refreshSearchProjection`; always `renderActiveMacroRows`.
 */
void SetupController::commitMacroImportMergeFromPath(gchar *path_owned) {
  CMacroTable macro;
  macro.init();
  GError *importErr = nullptr;
  const gboolean importedOk =
      macro_interchange_import_path(path_owned, &macro, MACRO_INTERCHANGE_FORMAT_AUTO,
                                    nullptr, &importErr);
  g_free(path_owned);

  if (!importedOk) {
    gchar *summary = g_strdup_printf(
        "%s%s%s",
        _("The macro file could not be loaded."),
        importErr ? "\n" : "",
        importErr ? importErr->message : "");
    run_macro_error_dialog(GTK_WINDOW(m_view.getMacroDialog()), &macro, summary);
    g_free(summary);
    g_clear_error(&importErr);
    return;
  }

  GtkListStore *tmp =
      gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
  list_store_append(tmp, &macro);
  GtkTreeIter iter;
  gboolean iterWalkImports =
      gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tmp), &iter);
  while (iterWalkImports == TRUE) {
    gchar *key = NULL;
    gchar *value = NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(tmp), &iter, COL_KEY, &key, COL_VALUE,
                       &value, -1);
    MacroUiRow row;
    row.key = key ? key : "";
    row.value = value ? value : "";
    refreshMacroUiRowSearchFoldCaches(row);
    m_defaultEntries.push_back(std::move(row));
    g_free(key);
    g_free(value);
    iterWalkImports =
        gtk_tree_model_iter_next(GTK_TREE_MODEL(tmp), &iter);
  }
  g_object_unref(tmp);

  const bool needToRefreshSearchHitsAfterImport = isSearchMode();
  if (needToRefreshSearchHitsAfterImport)
    refreshSearchProjection();
  renderActiveMacroRows();
}

/**
 * @brief Serialize current tree model to `CMacroTable` and write user path.
 *
 * Modal save chooser from `build_macro_export_save_chooser`; on OK delegates
 * serialization and I/O to `commitMacroExportToPath` (frees chosen path).
 */
void SetupController::handleMacroExport() {
  GtkWidget *chooser =
      build_macro_export_save_chooser(GTK_WINDOW(m_view.getMacroDialog()));
  const bool userChoseExportPath =
      (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_OK);
  if (userChoseExportPath) {
    gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    GtkFileFilter *export_filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(chooser));
    const gchar *preferred_extension =
        macro_file_chooser_filter_preferred_export_extension(export_filter);
    gchar *final_export_path =
        macro_export_filename_finalize_for_save(path, preferred_extension, "macro");
    g_free(path);
    commitMacroExportToPath(final_export_path);
  }
  gtk_widget_destroy(chooser);
}

/**
 * @brief Fill `CMacroTable` from the macro tree model and write the export path.
 * @param path_owned Heap path from the save chooser; always `g_free`d here.
 *
 * Progression:
 * 1. `unikey_gtk_model_fill_macro_table` from current tree model.
 * 2. Error dialog when fill reports failures.
 * 3. Else `macro_interchange_export_path` (extension selects interchange codec; no `.ukmcache` for non-native formats).
 */
void SetupController::commitMacroExportToPath(gchar *path_owned) {
  CMacroTable macro;
  macro.init();
  GtkTreeModel *model =
      GTK_TREE_MODEL(gtk_tree_view_get_model(m_view.getMacroTree()));
  UnikeyMacroTableFillResult fill =
      unikey_gtk_model_fill_macro_table(model, &macro, FALSE);
  GtkWindow *parentWin = GTK_WINDOW(m_view.getMacroDialog());

  const bool modelToTableHadFailures = (fill.failed > 0);
  if (modelToTableHadFailures) {
    run_macro_error_dialog(
        parentWin, &macro,
        _("Not all macros could be written to the export file."));
  } else {
    GError *exportErr = nullptr;
    const gboolean wroteExportFile =
        macro_interchange_export_path(path_owned, &macro, MACRO_INTERCHANGE_FORMAT_AUTO,
                                      nullptr, &exportErr);
    if (!wroteExportFile) {
      gchar *summary = g_strdup_printf(
          "%s%s%s",
          _("Could not write the export file."),
          exportErr ? "\n" : "",
          exportErr ? exportErr->message : "");
      run_macro_error_dialog(parentWin, &macro, summary);
      g_free(summary);
      g_clear_error(&exportErr);
    }
  }
  g_free(path_owned);
}
