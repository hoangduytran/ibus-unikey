// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_EXPORT_FILENAME_H
#define IBUS_UNIKEY_MACRO_EXPORT_FILENAME_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file macro_export_filename.h
 * @brief Helpers to complete macro export basenames with a format extension and detect conflicts.
 *
 * Used when the user picks an export filter with a preferred extension (e.g. `.yaml`) while the
 * typed basename already has another extension (e.g. `macro.plist` -> `macro.plist.yaml`).
 */

/**
 * @brief True if @a basename_utf8 has a non-matching extension relative to @a preferred_extension_utf8.
 *
 * Example: base `out.yaml` with preferred `.json` is a conflict; `out` with preferred `.json` is not
 * (no extension to disagree).
 *
 * @param basename_utf8 Filename or basename only (last path segment behavior in implementation).
 * @param preferred_extension_utf8 Lowercase extension including dot, e.g. `.yaml`.
 */
gboolean macro_export_filename_has_extension_conflict(const gchar *basename_utf8,
                                                      const gchar *preferred_extension_utf8);

/**
 * @brief Returns a path whose basename gains @a preferred_extension_utf8 when needed.
 *
 * If the basename already ends with @a preferred_extension_utf8, returns a copy of @a path_utf8
 * unchanged. If it has a different extension, appends the preferred one (double extension).
 * Otherwise appends the preferred extension to an extensionless basename.
 *
 * @param path_utf8 User-entered path (may be bare filename or include directories).
 * @param preferred_extension_utf8 Extension including leading dot.
 * @return Newly allocated UTF-8 path; caller must `g_free()`, or `nullptr` if @a path_utf8 is null.
 */
gchar *macro_export_filename_complete(const gchar *path_utf8, const gchar *preferred_extension_utf8);

#ifdef __cplusplus
}
#endif

#endif /* IBUS_UNIKEY_MACRO_EXPORT_FILENAME_H */
