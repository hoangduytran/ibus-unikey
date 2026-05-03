// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file macro_export_filename.cpp
 * @brief Completes export paths with a filter-selected extension and detects basename conflicts.
 */

#include <setup/macro_export_filename.h>

#include <cstring>

static bool hidden_dotfile_extensionless(const gchar *basename_utf8) {
  return basename_utf8 && basename_utf8[0] == '.' && strchr(basename_utf8 + 1, '.') == nullptr;
}

static gchar *take_extension(const gchar *basename_utf8) {
  if (!basename_utf8 || !basename_utf8[0])
    return nullptr;
  if (hidden_dotfile_extensionless(basename_utf8))
    return nullptr;
  const char *last_dot = strrchr(basename_utf8, '.');
  if (!last_dot || last_dot == basename_utf8)
    return nullptr;
  return g_strdup(last_dot);
}

static gboolean extensions_match(const gchar *basename_utf8, const gchar *preferred_extension_utf8) {
  if (!preferred_extension_utf8 || !preferred_extension_utf8[0])
    return FALSE;
  gchar *current_extension = take_extension(basename_utf8);
  if (!current_extension)
    return FALSE;
  const gboolean extensions_equal = g_ascii_strcasecmp(current_extension, preferred_extension_utf8) == 0;
  g_free(current_extension);
  return extensions_equal;
}

gboolean macro_export_filename_has_extension_conflict(const gchar *basename_utf8,
                                                      const gchar *preferred_extension_utf8) {
  if (!preferred_extension_utf8 || !preferred_extension_utf8[0])
    return FALSE;
  gchar *current_extension = take_extension(basename_utf8);
  if (!current_extension)
    return FALSE;
  const gboolean has_mismatch = g_ascii_strcasecmp(current_extension, preferred_extension_utf8) != 0;
  g_free(current_extension);
  return has_mismatch;
}

gchar *macro_export_filename_finalize_for_save(const gchar *path_utf8, const gchar *preferred_extension_utf8,
                                               const gchar *default_stem_utf8) {
  if (!path_utf8)
    return nullptr;
  if (!preferred_extension_utf8 || !preferred_extension_utf8[0])
    return g_strdup(path_utf8);

  const gchar *stem = (default_stem_utf8 && default_stem_utf8[0]) ? default_stem_utf8 : "macro";

  gchar *directory_utf8 = nullptr;
  gchar *basename_utf8 = nullptr;
  const gsize path_len = strlen(path_utf8);
  const gboolean path_ends_with_dir_sep =
      path_len > 1 && (path_utf8[path_len - 1] == '/' || path_utf8[path_len - 1] == '\\');

  if (path_ends_with_dir_sep) {
    directory_utf8 = g_strdup(path_utf8);
    gsize dir_len = path_len;
    while (dir_len > 1 && (directory_utf8[dir_len - 1] == '/' || directory_utf8[dir_len - 1] == '\\'))
      directory_utf8[--dir_len] = '\0';
    basename_utf8 = g_strdup(stem);
  } else {
    directory_utf8 = g_path_get_dirname(path_utf8);
    basename_utf8 = g_path_get_basename(path_utf8);
    const gboolean basename_missing =
        !basename_utf8 || !basename_utf8[0] || g_strcmp0(basename_utf8, ".") == 0;
    if (basename_missing) {
      g_free(basename_utf8);
      basename_utf8 = g_strdup(stem);
    }
  }

  const gboolean path_is_basename_only =
      (strchr(path_utf8, '/') == nullptr && strchr(path_utf8, '\\') == nullptr);
  gchar *path_for_complete = nullptr;
  if (path_is_basename_only)
    path_for_complete = g_strdup(basename_utf8);
  else
    path_for_complete = g_build_filename(directory_utf8, basename_utf8, nullptr);
  g_free(basename_utf8);
  g_free(directory_utf8);

  gchar *completed = macro_export_filename_complete(path_for_complete, preferred_extension_utf8);
  g_free(path_for_complete);
  return completed;
}

gchar *macro_export_filename_complete(const gchar *path_utf8, const gchar *preferred_extension_utf8) {
  if (!path_utf8)
    return nullptr;
  if (!preferred_extension_utf8 || !preferred_extension_utf8[0])
    return g_strdup(path_utf8);

  gchar *directory_utf8 = g_path_get_dirname(path_utf8);
  gchar *basename_utf8 = g_path_get_basename(path_utf8);

  gchar *completed_basename = extensions_match(basename_utf8, preferred_extension_utf8)
                                  ? g_strdup(basename_utf8)
                                  : g_strconcat(basename_utf8, preferred_extension_utf8, nullptr);

  const gboolean path_is_basename_only =
      (strchr(path_utf8, '/') == nullptr && strchr(path_utf8, '\\') == nullptr);
  gchar *completed_path = nullptr;
  if (path_is_basename_only)
    completed_path = completed_basename;
  else {
    completed_path = g_build_filename(directory_utf8, completed_basename, nullptr);
    g_free(completed_basename);
  }
  g_free(directory_utf8);
  g_free(basename_utf8);
  return completed_path;
}
