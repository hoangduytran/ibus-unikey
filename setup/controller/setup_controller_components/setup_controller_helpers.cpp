/**
 * @file setup_controller_helpers.cpp
 * @brief Shared GTK/helpers for macro editor UI, list rows, file choosers, and
 * error dialogs (used across setup_controller translation units).
 */

#include "setup_controller_components/setup_controller_internal.h"

#include <libintl.h>

#include <setup/macro_file_io.h>
#include "macro_utils.h"

#define _(str) gettext(str)

void run_macro_error_dialog(GtkWindow *parent, CMacroTable *macro,
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

std::string utf8_casefold_string(const char *utf8) {
  gchar *f = g_utf8_casefold(utf8 ? utf8 : "", -1);
  std::string out(f ? f : "");
  g_free(f);
  return out;
}

static GtkWidget *macro_editor_label_start_aligned(const char *utf8_label_text) {
  GtkWidget *label = gtk_label_new(utf8_label_text);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  return label;
}

static void macro_editor_configure_form_grid(GtkWidget *grid) {
  GtkGrid *g = GTK_GRID(grid);
  gtk_grid_set_row_spacing(g, 6);
  gtk_grid_set_column_spacing(g, 6);
  gtk_container_set_border_width(GTK_CONTAINER(grid), 8);
}

static GtkWidget *macro_editor_build_value_scrolled(GtkWidget **out_txtValue) {
  GtkWidget *scroll = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_widget_set_size_request(scroll, 360, 180);
  *out_txtValue = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(*out_txtValue),
                              GTK_WRAP_WORD_CHAR);
  gtk_container_add(GTK_CONTAINER(scroll), *out_txtValue);
  return scroll;
}

static void macro_editor_attach_form_rows(GtkGrid *grid,
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

gchar *read_macro_editor_multiline(GtkWidget *txtValue) {
  GtkTextBuffer *buf =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(txtValue));
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buf, &start, &end);
  return gtk_text_buffer_get_text(buf, &start, &end, FALSE);
}

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

GtkWidget *build_macro_import_open_chooser(GtkWindow *parent) {
  GtkWidget *dlg = gtk_file_chooser_dialog_new(
      _("Import macro"), parent,
      GTK_FILE_CHOOSER_ACTION_OPEN, dgettext("gtk30", "_Cancel"),
      GTK_RESPONSE_CANCEL, dgettext("gtk30", "_Open"), GTK_RESPONSE_OK,
      nullptr);
  macro_file_chooser_attach_import_filters(GTK_FILE_CHOOSER(dlg));
  return dlg;
}
