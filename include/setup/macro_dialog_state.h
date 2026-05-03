// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_DIALOG_STATE_H
#define IBUS_UNIKEY_MACRO_DIALOG_STATE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file macro_dialog_state.h
 * @brief Persistent macro dialog working-directory helpers (GTK file chooser).
 *
 * Remembers the last import/export parent folder in IBus UniKey GSettings so the macro dialog
 * re-opens where the user last picked a file. If the stored path is missing, falls back to
 * `~/Documents` (implementation).
 */

/**
 * @brief Returns the directory the import chooser should start in.
 * @return Newly allocated UTF-8 path; caller must `g_free()` when done.
 */
gchar *macro_dialog_get_last_import_dir(void);

/**
 * @brief Returns the directory the export chooser should start in (same backing store as import).
 * @return Newly allocated UTF-8 path; caller must `g_free()` when done.
 */
gchar *macro_dialog_get_last_export_dir(void);

/**
 * @brief Persists the parent directory of @a path after the user picks a file to import.
 * @param path UTF-8 path to a file (directory component is stored).
 */
void macro_dialog_remember_import_file(const gchar *path);

/**
 * @brief Persists the parent directory of @a path after the user picks an export location.
 * @param path UTF-8 path to a file (directory component is stored).
 */
void macro_dialog_remember_export_file(const gchar *path);

#ifdef __cplusplus
}
#endif

#endif /* IBUS_UNIKEY_MACRO_DIALOG_STATE_H */
