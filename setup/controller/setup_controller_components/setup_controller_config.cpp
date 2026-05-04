/**
 * @file setup_controller_config.cpp
 * @brief Part of SetupController implementation (split from setup_controller.cpp).
 */

#include "setup_controller.h"

#include <cstring>

#include "unikey_config.h"

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
