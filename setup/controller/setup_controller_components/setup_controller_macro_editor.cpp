/**
 * @file setup_controller_macro_editor.cpp
 * @brief Part of SetupController implementation (split from setup_controller.cpp).
 */

#include "setup_controller.h"

#include <libintl.h>

#define _(str) gettext(str)

#include "setup_controller_components/setup_controller_internal.h"

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
