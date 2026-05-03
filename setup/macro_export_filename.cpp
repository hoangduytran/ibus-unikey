// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <setup/macro_export_filename.h>

#include <cstring>

namespace {

bool hidden_dotfile_extensionless(const gchar *base) {
  return base && base[0] == '.' && strchr(base + 1, '.') == nullptr;
}

gchar *take_extension(const gchar *base) {
  if (!base || !base[0])
    return nullptr;
  if (hidden_dotfile_extensionless(base))
    return nullptr;
  const char *dot = strrchr(base, '.');
  if (!dot || dot == base)
    return nullptr;
  return g_strdup(dot);
}

gboolean extensions_match(const gchar *base, const gchar *preferred_ext) {
  if (!preferred_ext || !preferred_ext[0])
    return FALSE;
  gchar *have = take_extension(base);
  if (!have)
    return FALSE;
  gboolean ok = g_ascii_strcasecmp(have, preferred_ext) == 0;
  g_free(have);
  return ok;
}

} // namespace

gboolean macro_export_filename_has_extension_conflict(const gchar *basename_utf8,
                                                      const gchar *preferred_extension_utf8) {
  if (!preferred_extension_utf8 || !preferred_extension_utf8[0])
    return FALSE;
  gchar *have = take_extension(basename_utf8);
  if (!have)
    return FALSE;
  gboolean conflict = g_ascii_strcasecmp(have, preferred_extension_utf8) != 0;
  g_free(have);
  return conflict;
}

gchar *macro_export_filename_complete(const gchar *path_utf8, const gchar *preferred_extension_utf8) {
  if (!path_utf8)
    return nullptr;
  if (!preferred_extension_utf8 || !preferred_extension_utf8[0])
    return g_strdup(path_utf8);

  gchar *dir = g_path_get_dirname(path_utf8);
  gchar *base = g_path_get_basename(path_utf8);

  gchar *new_base = nullptr;
  if (extensions_match(base, preferred_extension_utf8))
    new_base = g_strdup(base);
  else if (macro_export_filename_has_extension_conflict(base, preferred_extension_utf8))
    new_base = g_strconcat(base, preferred_extension_utf8, nullptr);
  else
    new_base = g_strconcat(base, preferred_extension_utf8, nullptr);

  const gboolean bare =
      (strchr(path_utf8, '/') == nullptr && strchr(path_utf8, '\\') == nullptr);
  gchar *out = nullptr;
  if (bare)
    out = new_base;
  else {
    out = g_build_filename(dir, new_base, nullptr);
    g_free(new_base);
  }
  g_free(dir);
  g_free(base);
  return out;
}
