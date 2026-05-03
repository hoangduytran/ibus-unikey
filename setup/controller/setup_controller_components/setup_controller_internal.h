#ifndef SETUP_CONTROLLER_INTERNAL_H
#define SETUP_CONTROLLER_INTERNAL_H

#include <gtk/gtk.h>

#include <string>

class CMacroTable;

void run_macro_error_dialog(GtkWindow *parent, CMacroTable *macro,
                            const char *summary);

std::string utf8_casefold_string(const char *utf8);

struct MacroEditorShell {
  GtkWidget *dialog = nullptr;
  GtkWidget *entryKey = nullptr;
  GtkWidget *txtValue = nullptr;
};

MacroEditorShell build_macro_editor_shell(GtkWindow *parent, bool addMode);

gchar *read_macro_editor_multiline(GtkWidget *txtValue);

void append_macro_list_row(GtkListStore *store, const gchar *key_utf8,
                           const gchar *value_utf8, int canonicalIndex);

GtkWidget *build_macro_export_save_chooser(GtkWindow *parent);

GtkWidget *build_macro_import_open_chooser(GtkWindow *parent);

#endif
