// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file macro_dialog_state.cpp
 * @brief Persists the GTK macro chooser "last working directory" in GSettings-backed config.
 *
 * Uses `CONFIG_MACRO_LASTWORKINGDIR` (see `unikey_config.h`). When the saved path is not a
 * directory, falls back to `~/Documents` and re-saves that default.
 */

#include <setup/macro_dialog_state.h>

#include <glib/gstdio.h>

#include "unikey_config.h"

namespace {

/**
 * @brief Ensures `~/Documents` exists and stores it as the macro working directory.
 * @return Newly allocated path to Documents; caller owns the string.
 */
gchar *ensure_documents_default_persisted(void) {
  gchar *documents_path = g_build_filename(g_get_home_dir(), "Documents", NULL);
  if (!g_file_test(documents_path, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents(documents_path, 0700);
  ibus_unikey_config_set_string(CONFIG_MACRO_LASTWORKINGDIR, documents_path);
  return documents_path;
}

/**
 * @brief Reads the saved macro working dir, or migrates to Documents when stale.
 * @return Directory path to show in file choosers; caller must `g_free()`.
 */
gchar *resolve_last_dir(void) {
  gchar *stored_path_utf8 = nullptr;
  const bool got_config =
      ibus_unikey_config_get_string(CONFIG_MACRO_LASTWORKINGDIR, &stored_path_utf8);
  const bool stored_path_nonempty = stored_path_utf8 && stored_path_utf8[0];
  if (got_config && stored_path_nonempty) {
    if (!g_file_test(stored_path_utf8, G_FILE_TEST_IS_DIR)) {
      g_free(stored_path_utf8);
      return ensure_documents_default_persisted();
    }
    return stored_path_utf8;
  }
  g_free(stored_path_utf8);
  return ensure_documents_default_persisted();
}

/**
 * @brief Writes the parent of @a path to `CONFIG_MACRO_LASTWORKINGDIR`.
 * @param path UTF-8 path to a file that was just chosen (import or export).
 */
void remember_parent_dir(const gchar *path) {
  if (!path || !path[0])
    return;
  gchar *parent_dir_utf8 = g_path_get_dirname(path);
  if (parent_dir_utf8 && parent_dir_utf8[0])
    ibus_unikey_config_set_string(CONFIG_MACRO_LASTWORKINGDIR, parent_dir_utf8);
  g_free(parent_dir_utf8);
}

} // namespace

gchar *macro_dialog_get_last_import_dir(void) { return resolve_last_dir(); }

gchar *macro_dialog_get_last_export_dir(void) { return resolve_last_dir(); }

void macro_dialog_remember_import_file(const gchar *path) { remember_parent_dir(path); }

void macro_dialog_remember_export_file(const gchar *path) { remember_parent_dir(path); }
