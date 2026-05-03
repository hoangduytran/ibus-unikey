/**
 * @file setup_controller.cpp
 * @brief GTK setup application: wiring UI signals to GSettings and macro
 * list/editor flows.
 *
 * Public API contracts are declared in `setup_controller.h`. This file
 * implements handlers for the main window, combo/toggle config sync, and the
 * macro dialog (canonical row list, search projection, import/export, non-modal
 * add/edit).
 */

#include "setup_controller.h"

#include <cstring>
#include <gdk/gdkkeysyms.h>
#include <libintl.h>

#include "macro_utils.h"
#include "unikey_config.h"
#include <setup/macro_export_filename.h>
#include <setup/macro_file_io.h>
#include <ukengine/mapping/mactab.h>

/** @brief Process-wide pointer for code that resolves the active setup
 * controller instance. */
static SetupController *s_global_controller = nullptr;

/**
 * @brief Query the globally registered controller (set during setup startup).
 * @return Registered instance, or null before registration.
 */
SetupController *global_setup_controller() { return s_global_controller; }

/**
 * @brief Publish the global controller reference for teardown and cross-module
 * lookups.
 * @param c Instance to expose; normally non-null once main window is built.
 */
void global_setup_controller_set(SetupController *c) {
  s_global_controller = c;
}

#define _(str) gettext(str)

/**
 * @brief Show a modal error summary with backend detail from `CMacroTable`.
 * @param parent Transient window (typically the macro dialog).
 * @param macro Engine table carrying `getLastErrorMessage()` diagnostics.
 * @param summary Short translated headline for the dialog.
 *
 * Progression:
 * 1. Choose detail string (`getLastErrorMessage` or fallback).
 * 2. Build body text run dialog modally destroy.
 */
static void run_macro_error_dialog(GtkWindow *parent, CMacroTable *macro,
                                   const char *summary) {
  const char *d = macro->getLastErrorMessage();
  const bool noBackendDetail =
      (d == NULL || d[0] == '\0');
  if (noBackendDetail)
    d = _("(no details)");
  gchar *text = g_strdup_printf("%s\n%s", summary, d);
  GtkWidget *dlg = gtk_message_dialog_new(
      parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", text);
  g_free(text);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

/**
 * @brief UTF-8 casefold used for substring search comparisons (glib semantics).
 * @param utf8 Input C string; null is treated like empty input.
 * @return Case-folded copy as `std::string`.
 *
 * Progression: normalize null-safe input -> `g_utf8_casefold` -> copy into RAII
 * -> `g_free`.
 */
static std::string utf8_casefold_string(const char *utf8) {
  // if the utf8 is null return an empty string
  gchar *f = g_utf8_casefold(utf8 ? utf8 : "", -1);
  std::string out(f ? f : ""); // create a new string from the folded utf8
  g_free(f);
  return out; // return the string
}

namespace {

/**
 * @brief Widget handles for the non-modal macro add/edit dialog (content owned by the dialog).
 */
struct MacroEditorShell {
  GtkWidget *dialog = nullptr;
  GtkWidget *entryKey = nullptr;
  GtkWidget *txtValue = nullptr;
};

/**
 * @brief Create a `GtkLabel` with start (left) horizontal alignment.
 * @param utf8_label_text Translated or plain UTF-8 label text.
 * @return New label widget (floating ref).
 */
GtkWidget *macro_editor_label_start_aligned(const char *utf8_label_text) {
  GtkWidget *label = gtk_label_new(utf8_label_text);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  return label;
}

/**
 * @brief Apply spacing and border used for the macro editor form `GtkGrid`.
 * @param grid Grid widget to style.
 */
void macro_editor_configure_form_grid(GtkWidget *grid) {
  GtkGrid *g = GTK_GRID(grid);
  gtk_grid_set_row_spacing(g, 6);
  gtk_grid_set_column_spacing(g, 6);
  gtk_container_set_border_width(GTK_CONTAINER(grid), 8);
}

/**
 * @brief Build a sized `GtkScrolledWindow` containing the multiline value `GtkTextView`.
 * @param[out] out_txtValue Receives the new text view (child of the scroll).
 * @return The scroll container (floating ref).
 *
 * Progression:
 * 1. Create scroll and text view; set minimum size and word-char wrap.
 * 2. Pack the text view into the scroll.
 */
GtkWidget *macro_editor_build_value_scrolled(GtkWidget **out_txtValue) {
  GtkWidget *scroll = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_widget_set_size_request(scroll, 360, 180);
  *out_txtValue = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(*out_txtValue),
                              GTK_WRAP_WORD_CHAR);
  gtk_container_add(GTK_CONTAINER(scroll), *out_txtValue);
  return scroll;
}

/**
 * @brief Lay out Replace / With labels, trigger entry, and value scroll (four rows).
 * @param grid Target grid (column 0 rows 0–3).
 * @param entryReplace Single-line "Replace" `GtkEntry`.
 * @param scrollValue `GtkScrolledWindow` holding the "With" `GtkTextView`.
 */
void macro_editor_attach_form_rows(GtkGrid *grid,
                                   GtkWidget *entryReplace,
                                   GtkWidget *scrollValue) {
  GtkWidget *lblReplace =
      macro_editor_label_start_aligned(_("Replace"));
  GtkWidget *lblWith = macro_editor_label_start_aligned(_("With"));
  gtk_grid_attach(grid, lblReplace, 0, 0, 1, 1);
  gtk_grid_attach(grid, entryReplace, 0, 1, 1, 1);
  gtk_grid_attach(grid, lblWith, 0, 2, 1, 1);
  gtk_grid_attach(grid, scrollValue, 0, 3, 1, 1);
}

/**
 * @brief Build the non-modal macro add/edit dialog shell (Replace/With fields, Cancel/Save).
 * @param parent Transient parent window (macro preferences dialog).
 * @param addMode When true, dialog title corresponds to adding a macro.
 * @return Populated `MacroEditorShell` (dialog modality disabled).
 *
 * Progression:
 * 1. Create `GtkDialog` with Cancel/Save responses.
 * 2. Build styled grid, entry, and scrolled text view; attach rows; pack content area.
 */
MacroEditorShell build_macro_editor_shell(GtkWindow *parent, bool addMode) {
  MacroEditorShell ui{};
  ui.dialog = gtk_dialog_new_with_buttons(
      addMode ? _("Add macro") : _("Edit macro"), parent,
      GTK_DIALOG_DESTROY_WITH_PARENT, _("_Close"), GTK_RESPONSE_CANCEL, _("_Save"),
      GTK_RESPONSE_OK, nullptr);
  gtk_window_set_modal(GTK_WINDOW(ui.dialog), FALSE);

  GtkWidget *grid = gtk_grid_new();
  macro_editor_configure_form_grid(grid);
  ui.entryKey = gtk_entry_new();
  GtkWidget *scrollValue = macro_editor_build_value_scrolled(&ui.txtValue);
  macro_editor_attach_form_rows(GTK_GRID(grid), ui.entryKey, scrollValue);

  GtkWidget *content =
      gtk_dialog_get_content_area(GTK_DIALOG(ui.dialog));
  gtk_container_add(GTK_CONTAINER(content), grid);
  return ui;
}

/**
 * @brief Snapshot the UTF-8 text from the macro editor value `GtkTextView`.
 * @param txtValue Text view for the replacement field.
 * @return Newly allocated string; caller shall `g_free` it.
 */
gchar *read_macro_editor_multiline(GtkWidget *txtValue) {
  GtkTextBuffer *buf =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(txtValue));
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buf, &start, &end);
  return gtk_text_buffer_get_text(buf, &start, &end, FALSE);
}

/**
 * @brief Append one macro row to the list store for the tree view.
 * @param store Macro `GtkListStore`.
 * @param key_utf8 NUL-terminated trigger text.
 * @param value_utf8 NUL-terminated replacement text.
 * @param canonicalIndex Index into the canonical in-memory row vector for this row.
 */
void append_macro_list_row(GtkListStore *store, const gchar *key_utf8,
                           const gchar *value_utf8, int canonicalIndex) {
  GtkTreeIter iter;
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
                     COL_KEY, key_utf8,
                     COL_VALUE, value_utf8,
                     COL_CANONICAL_INDEX, canonicalIndex,
                     -1);
}

/**
 * @brief Create a modal save file chooser for exporting macros (GTK stock button labels).
 * @param parent Parent window for the dialog.
 * @return New chooser; caller runs and destroys it.
 */
GtkWidget *build_macro_export_save_chooser(GtkWindow *parent) {
  GtkWidget *dlg = gtk_file_chooser_dialog_new(
      _("Export macro"), parent,
      GTK_FILE_CHOOSER_ACTION_SAVE, dgettext("gtk30", "_Cancel"),
      GTK_RESPONSE_CANCEL, dgettext("gtk30", "_Save"), GTK_RESPONSE_OK,
      nullptr);
  macro_file_chooser_attach_export_filters(GTK_FILE_CHOOSER(dlg));
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), "macro");
  return dlg;
}

/**
 * @brief Create a modal open file chooser for importing macros (GTK stock button labels).
 * @param parent Parent window for the dialog.
 * @return New chooser; caller runs and destroys it.
 */
GtkWidget *build_macro_import_open_chooser(GtkWindow *parent) {
  GtkWidget *dlg = gtk_file_chooser_dialog_new(
      _("Import macro"), parent,
      GTK_FILE_CHOOSER_ACTION_OPEN, dgettext("gtk30", "_Cancel"),
      GTK_RESPONSE_CANCEL, dgettext("gtk30", "_Open"), GTK_RESPONSE_OK,
      nullptr);
  macro_file_chooser_attach_import_filters(GTK_FILE_CHOOSER(dlg));
  return dlg;
}

} // namespace

/**
 * @brief Capture view and settings references used by GTK callbacks.
 */
SetupController::SetupController(SetupView &view, SettingsStore &store)
    : m_view(view), m_store(store) {}

/** @brief Placeholder hook for deferred controller-wide startup wiring. */
void SetupController::init() {}

/**
 * @brief Quit application from main window Escape key routing.
 *
 * Progression: if Escape -> terminate GTK loop and consume event.
 */
gboolean SetupController::handleMainWindowKeyPress(GtkWidget *widget,
                                                   GdkEventKey *event) {
  (void)widget;
  const bool pressedEscape = (event->keyval == GDK_KEY_Escape);
  if (pressedEscape) {
    gtk_main_quit();
    return true;
  }
  return false;
}

/** @brief End GTK main loop when the main window is destroyed. */
void SetupController::handleMainWindowDestroy() { gtk_main_quit(); }

/** @brief End GTK main loop when the user closes via Close button. */
void SetupController::handleBtnClose() { gtk_main_quit(); }

/**
 * @brief Push the active combo row's string into GSettings under `key`.
 * @param cbb Source combo box (column 0 holds id string).
 * @param key GSettings key (see `CONFIG_*` ids).
 *
 * Progression: read active iter -> `ibus_unikey_config_set_string` -> release
 * `GValue`.
 */
void SetupController::cbbConfigUpdate(GtkComboBox *cbb, const char *key) {
  GValue val = {0};    // initialize the value to 0
  GtkTreeIter iter;    // initialize the iterator
  GtkTreeModel *model; // initialize the model

  model = gtk_combo_box_get_model(cbb); // get the model from the combo box
  gtk_combo_box_get_active_iter(
      cbb, &iter); // get the active iterator from the combo box
  gtk_tree_model_get_value(model, &iter, 0,
                           &val); // get the value from the model

  ibus_unikey_config_set_string(
      key, g_value_get_string(&val)); // set the string to the key

  g_value_unset(&val); // unset the value
}

/** @brief Mirror input-method combo changes into config. */
void SetupController::handleInputMethodChanged(GtkComboBox *cbb) {
  cbbConfigUpdate(cbb, CONFIG_INPUTMETHOD);
}

/** @brief Mirror output-charset combo changes into config. */
void SetupController::handleOutputCharsetChanged(GtkComboBox *cbb) {
  cbbConfigUpdate(cbb, CONFIG_OUTPUTCHARSET);
}

/** @brief Load initial input-method selection from settings. */
void SetupController::handleInputMethodRealize(GtkComboBox *cbb) {
  cbbConfigSetActive(cbb, CONFIG_INPUTMETHOD);
}

/** @brief Load initial output-charset selection from settings. */
void SetupController::handleOutputCharsetRealize(GtkComboBox *cbb) {
  cbbConfigSetActive(cbb, CONFIG_OUTPUTCHARSET);
}

/**
 * @brief Select combo row whose column-0 id equals the string stored for `key`.
 * @param cbb Combo box using column 0 as the config id.
 * @param key GSettings (or config backend) key producing a heap string via `ibus_unikey_config_get_string`.
 *
 * Progression:
 * 1. Read stored id; abort when config has no string value.
 * 2. `activateComboBoxRowIfStoredIdMatches`; `g_free` the config string.
 */
void SetupController::cbbConfigSetActive(GtkComboBox *cbb, const char *key) {
  gchar *stored_row_id = nullptr;
  const bool have_config_label =
      ibus_unikey_config_get_string(key, &stored_row_id);
  if (!have_config_label)
    return;

  activateComboBoxRowIfStoredIdMatches(cbb, stored_row_id);
  g_free(stored_row_id);
}

/**
 * @brief Activate the first combo row whose column-0 id string matches the config value.
 * @param cbb Combo box whose model column 0 stores option ids.
 * @param expected_row_id Stored config id to match (NUL-terminated).
 *
 * Progression:
 * 1. Require at least one model row (empty model is a no-op).
 * 2. Walk rows; compare column 0 to `expected_row_id` with null-safe strcmp.
 * 3. On first match, set active iter and return.
 */
void SetupController::activateComboBoxRowIfStoredIdMatches(
    GtkComboBox *cbb,
    const gchar *expected_row_id) {
  GtkTreeModel *model = gtk_combo_box_get_model(cbb);
  GtkTreeIter iter;

  const bool model_has_at_least_one_row =
      gtk_tree_model_get_iter_first(model, &iter);
  if (!model_has_at_least_one_row)
    return;

  GValue val = {0};
  do {
    gtk_tree_model_get_value(model, &iter, 0, &val);
    const gchar *cell_id = g_value_get_string(&val);
    const bool row_matches_stored_id =
        (cell_id != NULL && strcmp(expected_row_id, cell_id) == 0);
    if (row_matches_stored_id) {
      gtk_combo_box_set_active_iter(cbb, &iter);
      g_value_unset(&val);
      return;
    }
    g_value_unset(&val);
  } while (gtk_tree_model_iter_next(model, &iter));
}

/**
 * @brief Persist a toggle-derived boolean to GSettings using widget name
 * `cfg_<key>`.
 *
 * Progression: read widget base name strip `cfg_` prefix -> toggle state ->
 * persist.
 */
void SetupController::handleSettingToggled(GtkToggleButton *btn) {

  // get the key from the widget
  const gchar *key = gtk_widget_get_name(GTK_WIDGET(btn));
  key = key + 4; // skip "cfg_"

  // get the state of the toggle button
  gboolean b = gtk_toggle_button_get_active(btn);

  // set the boolean to the key
  ibus_unikey_config_set_boolean(key, b);
}

/**
 * @brief Initialise toggle active state when the widget is realised.
 *
 * Progression: resolve `cfg_` suffix key -> read boolean if present ->
 * `gtk_toggle_button_set_active`.
 */
void SetupController::handleSettingRealize(GtkToggleButton *btn) {

  // get the key from the widget
  const gchar *key = gtk_widget_get_name(GTK_WIDGET(btn));
  key = key + 4; // skip "cfg_"

  // get the boolean from the key
  gboolean b;

  const bool loadedStoredToggle =
      ibus_unikey_config_get_boolean(key, &b);
  if (loadedStoredToggle) {
    gtk_toggle_button_set_active(btn, b);
  }
}

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

/** @brief Underlying `GtkListStore` bound to the macro tree view. */
GtkListStore *SetupController::macroStore() const {
  return GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));
}

/** @brief Tree selection handle for the macro view (read selected row). */
GtkTreeSelection *SetupController::macroSelection() const {
  return gtk_tree_view_get_selection(m_view.getMacroTree());
}

/** @brief True when search projection (subset of indices) is active. */
bool SetupController::isSearchMode() const { return m_searchMode; }

/** @brief Raw UTF-8 query from the search entry (empty string if null). */
std::string SetupController::currentSearchQuery() const {
  const gchar *text = gtk_entry_get_text(m_view.getMacroSearchEntry());
  return text ? text : "";
}

/**
 * @brief Map selected GTK row to `m_defaultEntries` index via
 * `COL_CANONICAL_INDEX`.
 * @return Canonical index or -1 when nothing selected.
 */
int SetupController::selectedCanonicalIndex() const {
  GtkTreeIter iter;
  const bool macroRowSelected =
      gtk_tree_selection_get_selected(macroSelection(), NULL, &iter);
  if (!macroRowSelected)
    return -1;
  // get the canonical index from the iterator
  int idx = -1;
  // get the canonical index from the iterator
  gtk_tree_model_get(GTK_TREE_MODEL(macroStore()), &iter, COL_CANONICAL_INDEX,
                     &idx, -1);
  // return the canonical index
  return idx;
}

/**
 * @brief Substring match using cached `key_folded`/`value_folded` vs folded query.
 */
bool SetupController::rowMatchesQuery(const MacroUiRow &row,
                                      const std::string &queryFolded) const {
  const bool keyContainsQuery =
      (row.key_folded.find(queryFolded) != std::string::npos);
  const bool valueContainsQuery =
      (row.value_folded.find(queryFolded) != std::string::npos);
  return keyContainsQuery || valueContainsQuery;
}

void SetupController::refreshMacroUiRowSearchFoldCaches(MacroUiRow &row) {
  row.key_folded = utf8_casefold_string(row.key.c_str());
  row.value_folded = utf8_casefold_string(row.value.c_str());
}

/**
 * @brief One-time wiring of GTK sort-column ids to `COL_KEY` / `COL_VALUE`.
 *
 * Progression: if already configured noop else fetch columns attach ids free
 * list flag true.
 */
void SetupController::ensureSortColumns() {
  if (m_sortConfigured)
    return;
  auto cols = gtk_tree_view_get_columns(m_view.getMacroTree());
  const bool gotColumnListFromTree = (cols != NULL);
  if (gotColumnListFromTree) {
    GtkTreeViewColumn *word = GTK_TREE_VIEW_COLUMN(g_list_nth_data(cols, 0));
    GtkTreeViewColumn *replace = GTK_TREE_VIEW_COLUMN(g_list_nth_data(cols, 1));
    const bool replaceColumnResolved = (replace != nullptr);
    const bool triggerColumnResolved = (word != nullptr);
    if (triggerColumnResolved)
      gtk_tree_view_column_set_sort_column_id(word, COL_KEY);
    if (replaceColumnResolved)
      gtk_tree_view_column_set_sort_column_id(replace, COL_VALUE);
  }
  // free the columns
  g_list_free(cols);
  // set the sort configured to true
  m_sortConfigured = true;
}

/**
 * @brief Parse `store` rows into `m_defaultEntries`, skipping `(null)`
 * placeholders.
 *
 * Progression: clear canonical walk iter skip `STR_NULL_ITEM` keys push
 * MacroUiRow.
 */
void SetupController::loadMacroRowsFromStore(GtkListStore *store) {
  m_defaultEntries.clear();
  GtkTreeIter iter;
  gboolean iterWalkValid =
      gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
  while (iterWalkValid == TRUE) {
    gchar *key = NULL;   // initialize the key
    gchar *value = NULL; // initialize the value
    // get the key and value from the store
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_KEY, &key, COL_VALUE,
                       &value, -1);
    // check if the key is a placeholder
    const bool isPlaceholder = (key != NULL && strcmp(key, STR_NULL_ITEM) == 0);
    // if the key is not a placeholder push the key and value to the default
    // entries
    if (!isPlaceholder) {
      MacroUiRow row;
      row.key = key ? key : "";
      row.value = value ? value : "";
      refreshMacroUiRowSearchFoldCaches(row);
      m_defaultEntries.push_back(std::move(row));
    }
    // free the key and value
    g_free(key);
    g_free(value);
    iterWalkValid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
  }
}

/**
 * @brief Fill `store` from the full canonical list plus trailing null placeholder row.
 * @param store Cleared/populated macro list store for default (non-search) view.
 *
 * Progression:
 * 1. Append each `m_defaultEntries` row with stable canonical index via `append_macro_list_row`.
 * 2. `list_store_add_null_item` sentinel row as in UI convention.
 */
void SetupController::renderFullCanonicalMacroListInto(GtkListStore *store) {
  for (size_t i = 0; i < m_defaultEntries.size(); i++) {
    const MacroUiRow &row = m_defaultEntries[i];
    append_macro_list_row(store, row.key.c_str(), row.value.c_str(),
                          static_cast<int>(i));
  }
  list_store_add_null_item(store);
}

/**
 * @brief Fill `store` from `m_searchedIndices` projection (skipped indices are stale).
 * @param store Macro list store for search-hit view.
 *
 * Progression:
 * 1. For each hit index in range of `m_defaultEntries`, append that canonical row with its index.
 * 2. Out-of-range indices are ignored (robust against desync).
 */
void SetupController::renderSearchProjectionInto(GtkListStore *store) {
  for (size_t i = 0; i < m_searchedIndices.size(); i++) {
    const int cidx = m_searchedIndices[i];
    const bool projectionRowMapsToStoredMacro =
        (cidx >= 0 &&
         cidx < static_cast<int>(m_defaultEntries.size()));
    if (!projectionRowMapsToStoredMacro)
      continue;
    const MacroUiRow &row = m_defaultEntries[static_cast<size_t>(cidx)];
    append_macro_list_row(store, row.key.c_str(), row.value.c_str(), cidx);
  }
}

/**
 * @brief Refresh list store from either full canonical list or search indices.
 *
 * Delegates filling the store to `renderFullCanonicalMacroListInto` or
 * `renderSearchProjectionInto`, then updates return-button visibility.
 */
void SetupController::renderActiveMacroRows() {
  GtkListStore *store = macroStore();
  gtk_list_store_clear(store);

  const bool displayingFullCanonicalMacroList = !m_searchMode;
  if (displayingFullCanonicalMacroList)
    renderFullCanonicalMacroListInto(store);
  else
    renderSearchProjectionInto(store);

  updateReturnButtonVisibility();
}

/**
 * @brief Show Return-to-list whenever search projection mode is active (including zero hits).
 *
 * Hiding when `m_searchedIndices` is empty left users stuck after a no-hit query.
 */
void SetupController::updateReturnButtonVisibility() {
  if (m_searchMode)
    gtk_widget_show(GTK_WIDGET(m_view.getMacroReturnButton())); // show the return button
  else
    gtk_widget_hide(GTK_WIDGET(m_view.getMacroReturnButton())); // hide the return button
}

/**
 * @brief Rebuild `m_searchedIndices` from `currentSearchQuery` casefolded.
 *
 * Progression: read query fold if empty clear search mode else set
 * `m_searchMode` scan `m_defaultEntries` with `rowMatchesQuery`.
 */
void SetupController::refreshSearchProjection() {
  // get the query from the current search query
  const std::string query = currentSearchQuery();
  // fold the Query
  const std::string queryFolded = utf8_casefold_string(query.c_str());
  const bool trimmedQueryProducesNoHits = queryFolded.empty();
  if (trimmedQueryProducesNoHits) {
    clearSearchMode();
    return;
  }

  m_searchMode = true; // set the search mode to true
  m_searchedIndices.clear(); // clear the searched indices
  // for each default entry check if the row matches the query folded
  for (size_t i = 0; i < m_defaultEntries.size(); i++) {
    const bool canonicalRowMatchesFoldedQuery =
        rowMatchesQuery(m_defaultEntries[i], queryFolded);
    if (canonicalRowMatchesFoldedQuery)
      m_searchedIndices.push_back((int)i);
  }
}

/**
 * @brief Exit search mode: clear indices empty entry text (with re-entrancy
 * guard).
 */
void SetupController::clearSearchMode() {
  m_searchMode = false;
  m_searchedIndices.clear();
  m_syncingSearchText = true;
  gtk_entry_set_text(m_view.getMacroSearchEntry(), "");
  m_syncingSearchText = false;
}

/**
 * @brief Programmatic search text used when leaving search or external sync.
 */
void SetupController::setSearchText(const char *text) {
  m_syncingSearchText = true;
  gtk_entry_set_text(m_view.getMacroSearchEntry(), text ? text : "");
  m_syncingSearchText = false;
}

/**
 * @brief Copy one canonical macro row into the Replace/With widgets before dialog run.
 * @param entryKey Key field `GtkEntry`.
 * @param txtValue Value field `GtkTextView`.
 * @param canonicalIndex Row index into `m_defaultEntries` (caller ensures in range).
 */
void SetupController::prefillMacroEditorFromCanonical(GtkWidget *entryKey,
                                                      GtkWidget *txtValue,
                                                      int canonicalIndex) {
  gtk_entry_set_text(GTK_ENTRY(entryKey),
                     m_defaultEntries[(size_t)canonicalIndex].key.c_str());
  GtkTextBuffer *buf =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(txtValue));
  gtk_text_buffer_set_text(
      buf, m_defaultEntries[(size_t)canonicalIndex].value.c_str(), -1);
}

/**
 * @brief Apply saved editor key/value into `m_defaultEntries` and redraw list/search.
 * @param key_utf8 NUL-terminated trigger from the entry (owned by GTK).
 * @param value_utf8 NUL-terminated replacement (may be empty string).
 * @param addMode When true, append new row else update existing when in range.
 * @param canonicalIndex Edit-mode row index (`addMode` ignores for append path).
 * @param canonicalIndexInRange When false in edit mode, row body is not updated.
 *
 * Progression:
 * 1. Push or assign vectors in `m_defaultEntries`.
 * 2. Optionally `refreshSearchProjection` when searching.
 * 3. `renderActiveMacroRows`.
 */
void SetupController::commitMacroEditorSavePayload(
    const gchar *key_utf8, const gchar *value_utf8, bool addMode,
    int canonicalIndex, bool canonicalIndexInRange) {
  const std::string value = value_utf8 ? value_utf8 : "";
  if (addMode) {
    MacroUiRow row;
    row.key = key_utf8;
    row.value = value;
    refreshMacroUiRowSearchFoldCaches(row);
    m_defaultEntries.push_back(std::move(row));
  } else if (canonicalIndexInRange) {
    m_defaultEntries[(size_t)canonicalIndex].key = key_utf8;
    m_defaultEntries[(size_t)canonicalIndex].value = value;
    refreshMacroUiRowSearchFoldCaches(m_defaultEntries[(size_t)canonicalIndex]);
  }

  const bool needToRefreshSearchHits = isSearchMode();
  if (needToRefreshSearchHits)
    refreshSearchProjection();
  renderActiveMacroRows();
}

/**
 * @brief Non-modal add/edit macro dialog: build shell, run it, apply Save.
 * @param addMode When true, add flow; when false, edit `canonicalIndex`.
 * @param canonicalIndex Row index for edit mode (ignored when `addMode`).
 * @return True when user saved with a non-empty trigger key and lists refreshed.
 *
 * Progression:
 * 1. `build_macro_editor_shell`; optional `prefillMacroEditorFromCanonical`.
 * 2. `gtk_dialog_run`; Cancel or empty key → destroy dialog and return false.
 * 3. Else `commitMacroEditorSavePayload`, free multiline buffer, destroy dialog.
 */
bool SetupController::openMacroEditor(bool addMode, int canonicalIndex) {
  MacroEditorShell shell =
      build_macro_editor_shell(GTK_WINDOW(m_view.getMacroDialog()), addMode);

  const bool editingExistingMacro = !addMode;
  const bool canonicalIndexInRange =
      canonicalIndex >= 0 &&
      canonicalIndex < static_cast<int>(m_defaultEntries.size());

  if (editingExistingMacro && canonicalIndexInRange)
    prefillMacroEditorFromCanonical(shell.entryKey, shell.txtValue,
                                    canonicalIndex);

  gtk_widget_show_all(shell.dialog);
  const int ret = gtk_dialog_run(GTK_DIALOG(shell.dialog));

  const bool userSavedMacroEditor = (ret == GTK_RESPONSE_OK);
  if (!userSavedMacroEditor) {
    gtk_widget_destroy(shell.dialog);
    return false;
  }

  const gchar *key = gtk_entry_get_text(GTK_ENTRY(shell.entryKey));
  const bool keyHasNonEmptyUtf8Text =
      (key != NULL && key[0] != '\0');

  gchar *value = read_macro_editor_multiline(shell.txtValue);

  if (!keyHasNonEmptyUtf8Text) {
    g_free(value);
    gtk_widget_destroy(shell.dialog);
    return false;
  }

  commitMacroEditorSavePayload(key, value ? value : "", addMode, canonicalIndex,
                               canonicalIndexInRange);
  g_free(value);
  gtk_widget_destroy(shell.dialog);
  return true;
}

/**
 * @brief Warn then clear entire canonical macro list on user confirmation only.
 */
void SetupController::showConfirmAndClearAll() {
  GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(m_view.getMacroDialog()),
                                          GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
                                          GTK_BUTTONS_OK_CANCEL, "%s",
                                          _("Clear all macro items?"));
  const int ret = gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
  const bool confirmedDestructiveClearAll =
      (ret == GTK_RESPONSE_OK);
  if (confirmedDestructiveClearAll) {
    m_defaultEntries.clear();
    renderActiveMacroRows();
  }
}
