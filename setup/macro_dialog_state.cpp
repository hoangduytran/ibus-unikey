// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <setup/macro_dialog_state.h>

#include <glib/gstdio.h>

#include "unikey_config.h"

namespace {

gchar *ensure_documents_default_persisted(void) {
  gchar *def = g_build_filename(g_get_home_dir(), "Documents", NULL);
  if (!g_file_test(def, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents(def, 0700);
  ibus_unikey_config_set_string(CONFIG_MACRO_LASTWORKINGDIR, def);
  return def;
}

gchar *resolve_last_dir(void) {
  gchar *saved = nullptr;
  if (ibus_unikey_config_get_string(CONFIG_MACRO_LASTWORKINGDIR, &saved) && saved && saved[0]) {
    if (!g_file_test(saved, G_FILE_TEST_IS_DIR)) {
      g_free(saved);
      return ensure_documents_default_persisted();
    }
    return saved;
  }
  g_free(saved);
  return ensure_documents_default_persisted();
}

void remember_parent_dir(const gchar *path) {
  if (!path || !path[0])
    return;
  gchar *dir = g_path_get_dirname(path);
  if (dir && dir[0])
    ibus_unikey_config_set_string(CONFIG_MACRO_LASTWORKINGDIR, dir);
  g_free(dir);
}

} // namespace

gchar *macro_dialog_get_last_import_dir(void) { return resolve_last_dir(); }

gchar *macro_dialog_get_last_export_dir(void) { return resolve_last_dir(); }

void macro_dialog_remember_import_file(const gchar *path) { remember_parent_dir(path); }

void macro_dialog_remember_export_file(const gchar *path) { remember_parent_dir(path); }
