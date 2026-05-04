/**
 * @file setup_controller_macro_model.cpp
 * @brief Part of SetupController implementation (split from setup_controller.cpp).
 */

#include "setup_controller.h"

#include "setup_controller_components/setup_controller_internal.h"
#include "macro_utils.h"
#include "unikey_config.h"

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
