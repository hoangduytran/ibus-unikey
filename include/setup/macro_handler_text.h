// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_HANDLER_TEXT_H
#define IBUS_UNIKEY_MACRO_HANDLER_TEXT_H

#include <setup/macro_format_handler.h>

/**
 * @file macro_handler_text.h
 * @brief Declaration of the native UniKey text macro interchange handler.
 */

/**
 * @brief Imports and exports macros via the canonical UniKey text format (`TextMacroFormat`).
 *
 * Used for GTK/tests interchange by path (`.txt`, `.macro`). Implementation delegates to
 * `TextMacroFormat` in ukengine. Unlike `CMacroTable::loadFromFile` / `writeToFile`, this does not
 * manage the engine's `.ukmcache` sidecar.
 *
 * Registered for `MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY` and extensions `.txt` / `.macro` in the
 * implementation TU.
 */
class TextMacroHandler : public MacroFormatHandler {
public:
  MacroInterchangeForcedFormat forced_format() const override;

  /**
   * @brief Loads a UniKey UTF-8/VIQR text macro file into @a table.
   * @param path_utf8 UTF-8 filesystem path.
   * @param table Destination macro table (cleared by the dispatcher before import).
   * @param stats Optional counters; on success reflects final imported row count.
   * @param err GLib error location.
   */
  gboolean import_from_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                            GError **err) override;

  /**
   * @brief Writes @a table as a UniKey text macro file with version header and sorted rows.
   * @param path_utf8 UTF-8 filesystem path.
   * @param table Source macro table.
   * @param stats Reserved for parity with other handlers; currently unused.
   * @param err GLib error location.
   */
  gboolean export_to_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                          GError **err) override;
};

#endif /* IBUS_UNIKEY_MACRO_HANDLER_TEXT_H */
