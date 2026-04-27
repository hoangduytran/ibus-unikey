#include "setup_controller.h"

#include <cstring>
#include <gdk/gdkkeysyms.h>
#include <libintl.h>

#include "macro_utils.h"
#include "unikey_config.h"
#include <ukengine/mapping/mactab.h>

static SetupController *s_global_controller = nullptr;

// Return the globally registered setup controller instance.
// @return registered global SetupController pointer
SetupController *global_setup_controller() { return s_global_controller; }

// Register the global setup controller instance.
// @param c controller instance to register globally
void global_setup_controller_set(SetupController *c) {
  s_global_controller = c;
}

#define _(str) gettext(str)

/**
 * @brief Modal error dialog for macro load/save/export failures.
 * @param parent transient parent (usually the macro dialog window)
 * @param macro source of getLastErrorMessage() detail text
 * @param summary one-line user-facing lead (translated)
 */
static void run_macro_error_dialog(GtkWindow *parent, CMacroTable *macro,
                                   const char *summary) {
  const char *d = macro->getLastErrorMessage();
  if (d == NULL || d[0] == '\0')
    d = _("(no details)");
  gchar *text = g_strdup_printf("%s\n%s", summary, d);
  GtkWidget *dlg = gtk_message_dialog_new(
      parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", text);
  g_free(text);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

static std::string utf8_casefold_string(const char *utf8) {
  // Progression: normalize null-safe input -> casefold with GLib -> return
  // std::string copy.
  gchar *f = g_utf8_casefold(utf8 ? utf8 : "", -1);
  std::string out(f ? f : "");
  g_free(f);
  return out;
}

// Construct the setup controller using the UI view and settings store.
// @param view reference to the view wrapper used to access UI widgets
// @param store reference to persistent settings storage backend
SetupController::SetupController(SetupView &view, SettingsStore &store)
    : m_view(view), m_store(store) {}

// Initialize the controller. This method is currently a placeholder
// for any future setup-specific initialization.
void SetupController::init() {}

// Handle key press events in the main setup window.
// @param widget widget receiving the key event
// @param event key event details
// @return TRUE when the event is handled and should not be propagated
//         FALSE otherwise
gboolean SetupController::handleMainWindowKeyPress(GtkWidget *widget,
                                                   GdkEventKey *event) {
  if (event->keyval == GDK_KEY_Escape) {
    gtk_main_quit();
    return true;
  }
  return false;
}

// Handle the main window being destroyed by terminating the GTK main loop.
void SetupController::handleMainWindowDestroy() { gtk_main_quit(); }

// Handle the close button being pressed by terminating the GTK main loop.
void SetupController::handleBtnClose() { gtk_main_quit(); }

// Update the stored configuration for the selected combo box value.
// @param cbb combo box containing the selected value
// @param key configuration key to update
void SetupController::cbbConfigUpdate(GtkComboBox *cbb, const char *key) {
  GValue val = {0};
  GtkTreeIter iter;
  GtkTreeModel *model;

  model = gtk_combo_box_get_model(cbb);
  gtk_combo_box_get_active_iter(cbb, &iter);
  gtk_tree_model_get_value(model, &iter, 0, &val);

  ibus_unikey_config_set_string(key, g_value_get_string(&val));

  g_value_unset(&val);
}

// Handle changes to the input method selection.
// @param cbb combo box containing input method choices
void SetupController::handleInputMethodChanged(GtkComboBox *cbb) {
  cbbConfigUpdate(cbb, CONFIG_INPUTMETHOD);
}

// Handle changes to the output charset selection.
// @param cbb combo box containing output charset choices
void SetupController::handleOutputCharsetChanged(GtkComboBox *cbb) {
  cbbConfigUpdate(cbb, CONFIG_OUTPUTCHARSET);
}

// Initialize the input method combo box from stored settings.
// @param cbb combo box containing input method choices
void SetupController::handleInputMethodRealize(GtkComboBox *cbb) {
  cbbConfigSetActive(cbb, CONFIG_INPUTMETHOD);
}

// Initialize the output charset combo box from stored settings.
// @param cbb combo box containing output charset choices
void SetupController::handleOutputCharsetRealize(GtkComboBox *cbb) {
  cbbConfigSetActive(cbb, CONFIG_OUTPUTCHARSET);
}

// Set the active combo box item based on a stored configuration value.
// @param cbb combo box to update
// @param key configuration key to read the value from
void SetupController::cbbConfigSetActive(GtkComboBox *cbb, const char *key) {
  GValue val = {0};
  GtkTreeIter iter;
  GtkTreeModel *model;

  gchar *im;
  if (!ibus_unikey_config_get_string(key, &im)) {
    return;
  }

  model = gtk_combo_box_get_model(cbb);
  gtk_tree_model_get_iter_first(model, &iter);
  do {
    gtk_tree_model_get_value(model, &iter, 0, &val);
    if (strcmp(im, g_value_get_string(&val)) == 0) {
      gtk_combo_box_set_active_iter(cbb, &iter);
      g_value_unset(&val);
      break;
    }
    g_value_unset(&val);
  } while (gtk_tree_model_iter_next(model, &iter));

  g_free(im);
}

// Persist a GUI toggle setting to configuration storage.
// @param btn toggle button for the setting
void SetupController::handleSettingToggled(GtkToggleButton *btn) {
  const gchar *key = gtk_widget_get_name(GTK_WIDGET(btn));
  key = key + 4; // skip "cfg_"

  gboolean b = gtk_toggle_button_get_active(btn);
  ibus_unikey_config_set_boolean(key, b);
}

// Initialize a GUI toggle button from stored configuration.
// @param btn toggle button for the setting
void SetupController::handleSettingRealize(GtkToggleButton *btn) {
  const gchar *key = gtk_widget_get_name(GTK_WIDGET(btn));
  key = key + 4; // skip "cfg_"

  gboolean b;
  if (ibus_unikey_config_get_boolean(key, &b)) {
    gtk_toggle_button_set_active(btn, b);
  }
}

// Show the macro editor dialog and save macro definitions if confirmed.
void SetupController::handleMacroEdit() { // Progression:
  // 1) load engine macros from disk,
  // 2) convert/store as canonical UI rows,
  // 3) run dialog with default/search rendering,
  // 4) on Save, rebuild engine table from canonical rows and persist.
  gchar *macrofile = get_macro_file();

  CMacroTable macro;
  macro.init();
  macro.loadFromFile(macrofile);

  auto store = GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));
  unikey_macro_to_store(&macro, store);
  loadMacroRowsFromStore(store);
  ensureSortColumns();
  clearSearchMode();
  renderActiveMacroRows();

  gtk_widget_show_all(m_view.getMacroDialog());
  gtk_window_present(GTK_WINDOW(m_view.getMacroDialog()));

  int ret = gtk_dialog_run(GTK_DIALOG(m_view.getMacroDialog()));
  if (ret == GTK_RESPONSE_OK) {
    gtk_list_store_clear(store);
    for (size_t i = 0; i < m_defaultEntries.size(); i++) {
      GtkTreeIter iter;
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter, COL_KEY, m_defaultEntries[i].key.c_str(),
                         COL_VALUE, m_defaultEntries[i].value.c_str(),
                         COL_CANONICAL_INDEX, (int)i, -1);
    }

    UnikeyMacroTableFillResult sync = unikey_store_to_macro(store, &macro);
    if (sync.failed > 0) {
      run_macro_error_dialog(
          GTK_WINDOW(m_view.getMacroDialog()), &macro,
          _("Not all macros could be saved. The macro file was not written."));
    } else {
      GFile *f = g_file_get_parent(g_file_new_for_path(macrofile));
      if (g_file_query_exists(f, NULL) == FALSE) {
        g_file_make_directory_with_parents(f, NULL, NULL);
      }
      g_object_unref(f);

      if (!macro.writeToFile(macrofile)) {
        run_macro_error_dialog(GTK_WINDOW(m_view.getMacroDialog()), &macro,
                               _("Could not write the macro file."));
      }
    }
  }

  g_free(macrofile);
}

// Hide the macro dialog and report the delete event handled.
// @return TRUE always to indicate event handling
gboolean SetupController::handleMacroDialogDelete() {
  gtk_widget_hide(m_view.getMacroDialog());
  return true;
}

// Hide the macro dialog without action.
void SetupController::handleMacroDialogHide() {
  gtk_widget_hide(m_view.getMacroDialog());
}

// Handle editing a macro key cell.
// @param celltext cell renderer for the edited text cell
// @param string_path string path to the edited row
// @param newkey new key text entered by user
void SetupController::handleCellKeyEdited(GtkCellRendererText *celltext,
                                          const gchar *string_path,
                                          const gchar *newkey) {
  (void)celltext;
  (void)string_path;
  (void)newkey;
}

// Handle editing a macro value cell.
// @param celltext cell renderer for the edited text cell
// @param string_path string path to the edited row
// @param newvalue new value text entered by user
void SetupController::handleCellValueEdited(GtkCellRendererText *celltext,
                                            const gchar *string_path,
                                            const gchar *newvalue) {
  (void)celltext;
  (void)string_path;
  (void)newvalue;
}

void SetupController::handleMacroAdd() {
  // Progression: open editor in add mode -> append canonical row on Save ->
  // rerender list.
  openMacroEditor(true, -1);
}

void SetupController::handleMacroEditSelected() {
  // Progression: resolve selected canonical row -> open editor in edit mode.
  const int idx = selectedCanonicalIndex();
  if (idx < 0 || idx >= (int)m_defaultEntries.size())
    return;
  openMacroEditor(false, idx);
}

// Remove the currently selected macro entry if present.
void SetupController::handleMacroDel() {
  // Progression: map selection -> remove canonical row -> refresh search
  // projection when active -> rerender.
  const int idx = selectedCanonicalIndex();
  if (idx < 0 || idx >= (int)m_defaultEntries.size())
    return;
  m_defaultEntries.erase(m_defaultEntries.begin() + idx);
  if (isSearchMode())
    refreshSearchProjection();
  renderActiveMacroRows();
}

// Clear all macro entries and restore the empty-placeholder row.
void SetupController::handleMacroClear() {
  // Mode-specific clear policy:
  // - Search mode: clear search only (never delete canonical data),
  // - Default mode: confirm then clear all canonical entries.
  if (isSearchMode()) {
    clearSearchMode();
    renderActiveMacroRows();
    return;
  }
  showConfirmAndClearAll();
}

void SetupController::handleMacroSearchActivate() {
  // Progression: ignore programmatic entry updates -> rebuild search projection
  // -> rerender active rows.
  if (m_syncingSearchText)
    return;
  refreshSearchProjection();
  renderActiveMacroRows();
}

void SetupController::handleMacroReturnToDefaultList() {
  // Explicit search-list exit action from dedicated Return button.
  clearSearchMode();
  renderActiveMacroRows();
}

gboolean SetupController::handleMacroSearchKeyPress(GtkWidget *widget,
                                                    GdkEventKey *event) {
  (void)widget;
  if (isSearchMode() && event->keyval == GDK_KEY_Escape) {
    // Esc is no longer used to leave search-list mode.
    return TRUE;
  }
  if (event->keyval == GDK_KEY_Escape) {
    clearSearchMode();
    renderActiveMacroRows();
    return TRUE;
  }
  return FALSE;
}

void SetupController::handleMacroTreeRowActivated(GtkTreePath *path) {
  // Double-click/Enter on row opens dedicated editor for selected canonical
  // row.
  (void)path;
  handleMacroEditSelected();
}

// Show a file chooser to import macro definitions from disk.
void SetupController::handleMacroImport() {
  // Progression:
  // 1) load imported file via engine table parser,
  // 2) append imported rows into canonical UI list,
  // 3) recompute search projection when needed,
  // 4) rerender active table view.
  auto file = gtk_file_chooser_dialog_new(
      _("Import macro"), GTK_WINDOW(m_view.getMacroDialog()),
      GTK_FILE_CHOOSER_ACTION_OPEN, dgettext("gtk30", "_Cancel"),
      GTK_RESPONSE_CANCEL, dgettext("gtk30", "_Open"), GTK_RESPONSE_OK, NULL);

  if (gtk_dialog_run(GTK_DIALOG(file)) == GTK_RESPONSE_OK) {

    auto fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file));
    CMacroTable macro;
    macro.init();
    if (!macro.loadFromFile(fn)) {
      run_macro_error_dialog(GTK_WINDOW(m_view.getMacroDialog()), &macro,
                             _("The macro file could not be fully imported."));
    }
    g_free(fn);

    auto tmp = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    list_store_append(tmp, &macro);

    GtkTreeIter iter;
    gboolean b = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tmp), &iter);
    while (b == TRUE) {
      gchar *key = NULL;
      gchar *value = NULL;
      gtk_tree_model_get(GTK_TREE_MODEL(tmp), &iter, COL_KEY, &key, COL_VALUE,
                         &value, -1);
      m_defaultEntries.push_back(
          MacroUiRow{key ? key : "", value ? value : ""});
      g_free(key);
      g_free(value);
      b = gtk_tree_model_iter_next(GTK_TREE_MODEL(tmp), &iter);
    }
    g_object_unref(tmp);

    if (isSearchMode())
      refreshSearchProjection();
    renderActiveMacroRows();
  }

  gtk_widget_destroy(file);
}

// Show a file chooser to export macro definitions to disk.
void SetupController::handleMacroExport() {
  // Progression: convert active UI model -> engine table -> write selected
  // file.
  auto file = gtk_file_chooser_dialog_new(
      _("Export macro"), GTK_WINDOW(m_view.getMacroDialog()),
      GTK_FILE_CHOOSER_ACTION_SAVE, dgettext("gtk30", "_Cancel"),
      GTK_RESPONSE_CANCEL, dgettext("gtk30", "_Save"), GTK_RESPONSE_OK, NULL);

  if (gtk_dialog_run(GTK_DIALOG(file)) == GTK_RESPONSE_OK) {
    CMacroTable macro;
    macro.init();

    auto model = GTK_TREE_MODEL(gtk_tree_view_get_model(m_view.getMacroTree()));
    UnikeyMacroTableFillResult fill =
        unikey_gtk_model_fill_macro_table(model, &macro, FALSE);

    auto fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file));
    if (fill.failed > 0) {
      run_macro_error_dialog(
          GTK_WINDOW(m_view.getMacroDialog()), &macro,
          _("Not all macros could be written to the export file."));
    } else if (!macro.writeToFile(fn)) {
      run_macro_error_dialog(GTK_WINDOW(m_view.getMacroDialog()), &macro,
                             _("Could not write the export file."));
    }
    g_free(fn);
  }

  gtk_widget_destroy(file);
}

GtkListStore *SetupController::macroStore() const {
  return GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));
}

GtkTreeSelection *SetupController::macroSelection() const {
  return gtk_tree_view_get_selection(m_view.getMacroTree());
}

bool SetupController::isSearchMode() const { return m_searchMode; }

std::string SetupController::currentSearchQuery() const {
  const gchar *text = gtk_entry_get_text(m_view.getMacroSearchEntry());
  return text ? text : "";
}

int SetupController::selectedCanonicalIndex() const {
  // Every rendered row carries COL_CANONICAL_INDEX for default/search unified
  // edits.
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(macroSelection(), NULL, &iter))
    return -1;
  int idx = -1;
  gtk_tree_model_get(GTK_TREE_MODEL(macroStore()), &iter, COL_CANONICAL_INDEX,
                     &idx, -1);
  return idx;
}

bool SetupController::rowMatchesQuery(const MacroUiRow &row,
                                      const std::string &queryFolded) const {
  const std::string keyFold = utf8_casefold_string(row.key.c_str());
  const std::string valueFold = utf8_casefold_string(row.value.c_str());
  return keyFold.find(queryFolded) != std::string::npos ||
         valueFold.find(queryFolded) != std::string::npos;
}

void SetupController::ensureSortColumns() {
  // Configure column sort IDs once; tree sort applies to whichever list is
  // rendered.
  if (m_sortConfigured)
    return;
  auto cols = gtk_tree_view_get_columns(m_view.getMacroTree());
  if (cols != NULL) {
    GtkTreeViewColumn *word = GTK_TREE_VIEW_COLUMN(g_list_nth_data(cols, 0));
    GtkTreeViewColumn *replace = GTK_TREE_VIEW_COLUMN(g_list_nth_data(cols, 1));
    if (word)
      gtk_tree_view_column_set_sort_column_id(word, COL_KEY);
    if (replace)
      gtk_tree_view_column_set_sort_column_id(replace, COL_VALUE);
  }
  g_list_free(cols);
  m_sortConfigured = true;
}

void SetupController::loadMacroRowsFromStore(GtkListStore *store) {
  // Rehydrate canonical rows from GtkListStore, skipping placeholder row
  // markers.
  m_defaultEntries.clear();
  GtkTreeIter iter;
  gboolean b = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
  while (b == TRUE) {
    gchar *key = NULL;
    gchar *value = NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_KEY, &key, COL_VALUE,
                       &value, -1);
    const bool isPlaceholder = (key != NULL && strcmp(key, STR_NULL_ITEM) == 0);
    if (!isPlaceholder)
      m_defaultEntries.push_back(
          MacroUiRow{key ? key : "", value ? value : ""});
    g_free(key);
    g_free(value);
    b = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
  }
}

void SetupController::renderActiveMacroRows() {
  // Render from canonical rows:
  // - default mode: all canonical rows in table order,
  // - search mode: projection rows by canonical indices.
  auto store = macroStore();
  gtk_list_store_clear(store);

  if (!m_searchMode) {
    for (size_t i = 0; i < m_defaultEntries.size(); i++) {
      GtkTreeIter iter;
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter, COL_KEY, m_defaultEntries[i].key.c_str(),
                         COL_VALUE, m_defaultEntries[i].value.c_str(),
                         COL_CANONICAL_INDEX, (int)i, -1);
    }
  } else {
    for (size_t i = 0; i < m_searchedIndices.size(); i++) {
      const int cidx = m_searchedIndices[i];
      if (cidx < 0 || cidx >= (int)m_defaultEntries.size())
        continue;
      GtkTreeIter iter;
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter, COL_KEY,
                         m_defaultEntries[(size_t)cidx].key.c_str(), COL_VALUE,
                         m_defaultEntries[(size_t)cidx].value.c_str(),
                         COL_CANONICAL_INDEX, cidx, -1);
    }
  }
  updateReturnButtonVisibility();
}

void SetupController::updateReturnButtonVisibility() {
  const bool showReturn = m_searchMode && !m_searchedIndices.empty();
  if (showReturn)
    gtk_widget_show(GTK_WIDGET(m_view.getMacroReturnButton()));
  else
    gtk_widget_hide(GTK_WIDGET(m_view.getMacroReturnButton()));
}

void SetupController::refreshSearchProjection() {
  // Projection builder:
  // 1) casefold query,
  // 2) empty query exits search mode,
  // 3) otherwise rebuild searched index vector from canonical rows.
  const std::string query = currentSearchQuery();
  const std::string queryFolded = utf8_casefold_string(query.c_str());

  if (queryFolded.empty()) {
    clearSearchMode();
    return;
  }

  m_searchMode = true;
  m_searchedIndices.clear();
  for (size_t i = 0; i < m_defaultEntries.size(); i++) {
    if (rowMatchesQuery(m_defaultEntries[i], queryFolded))
      m_searchedIndices.push_back((int)i);
  }
}

void SetupController::clearSearchMode() {
  // Ensure search-list exit is explicit and non-destructive to canonical data.
  m_searchMode = false;
  m_searchedIndices.clear();
  m_syncingSearchText = true;
  gtk_entry_set_text(m_view.getMacroSearchEntry(), "");
  m_syncingSearchText = false;
}

void SetupController::setSearchText(const char *text) {
  // Guard to avoid firing search actions during programmatic entry updates.
  m_syncingSearchText = true;
  gtk_entry_set_text(m_view.getMacroSearchEntry(), text ? text : "");
  m_syncingSearchText = false;
}

bool SetupController::openMacroEditor(bool addMode, int canonicalIndex) {
  // Non-modal dedicated editor (design requirement):
  // 1) build dialog + Replace/With controls,
  // 2) prefill in edit mode,
  // 3) validate/apply into canonical rows on Save,
  // 4) refresh active rendering.
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      addMode ? _("Add macro") : _("Edit macro"),
      GTK_WINDOW(m_view.getMacroDialog()), GTK_DIALOG_DESTROY_WITH_PARENT,
      _("_Close"), GTK_RESPONSE_CANCEL, _("_Save"), GTK_RESPONSE_OK, NULL);
  gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
  gtk_container_set_border_width(GTK_CONTAINER(grid), 8);

  GtkWidget *lblKey = gtk_label_new(_("Replace"));
  gtk_widget_set_halign(lblKey, GTK_ALIGN_START);
  GtkWidget *lblValue = gtk_label_new(_("With"));
  gtk_widget_set_halign(lblValue, GTK_ALIGN_START);
  GtkWidget *entryKey = gtk_entry_new();
  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_size_request(scroll, 360, 180);
  GtkWidget *txtValue = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txtValue), GTK_WRAP_WORD_CHAR);
  gtk_container_add(GTK_CONTAINER(scroll), txtValue);
  gtk_grid_attach(GTK_GRID(grid), lblKey, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), entryKey, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), lblValue, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), scroll, 0, 3, 1, 1);
  gtk_container_add(GTK_CONTAINER(content), grid);

  if (!addMode && canonicalIndex >= 0 &&
      canonicalIndex < (int)m_defaultEntries.size()) {
    gtk_entry_set_text(GTK_ENTRY(entryKey),
                       m_defaultEntries[(size_t)canonicalIndex].key.c_str());
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txtValue));
    gtk_text_buffer_set_text(
        buf, m_defaultEntries[(size_t)canonicalIndex].value.c_str(), -1);
  }

  gtk_widget_show_all(dialog);
  const int ret = gtk_dialog_run(GTK_DIALOG(dialog));
  if (ret == GTK_RESPONSE_OK) {
    const gchar *key = gtk_entry_get_text(GTK_ENTRY(entryKey));
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txtValue));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    gchar *value = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    if (key != NULL && key[0] != '\0') {
      if (addMode)
        m_defaultEntries.push_back(MacroUiRow{key, value ? value : ""});
      else if (canonicalIndex >= 0 &&
               canonicalIndex < (int)m_defaultEntries.size()) {
        m_defaultEntries[(size_t)canonicalIndex].key = key;
        m_defaultEntries[(size_t)canonicalIndex].value = value ? value : "";
      }
      if (isSearchMode())
        refreshSearchProjection();
      renderActiveMacroRows();
      g_free(value);
      gtk_widget_destroy(dialog);
      return true;
    }
    g_free(value);
  }
  gtk_widget_destroy(dialog);
  return false;
}

void SetupController::showConfirmAndClearAll() {
  // Destructive clear is only allowed in default mode and always asks
  // confirmation.
  GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(m_view.getMacroDialog()),
                                          GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
                                          GTK_BUTTONS_OK_CANCEL, "%s",
                                          _("Clear all macro items?"));
  const int ret = gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
  if (ret == GTK_RESPONSE_OK) {
    m_defaultEntries.clear();
    renderActiveMacroRows();
  }
}
