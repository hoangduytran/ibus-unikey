// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_HANDLER_JSON_H
#define IBUS_UNIKEY_MACRO_HANDLER_JSON_H

#include <setup/macro_format_handler.h>

/**
 * @file macro_handler_json.h
 * @brief Declaration of the generic JSON macro interchange handler.
 */

/**
 * @brief Imports and exports macros from a constrained JSON interchange format.
 *
 * Import accepts either:
 * - a JSON array of objects with string fields `trigger` (shortcut) and `content` (phrase), or
 * - a JSON object whose keys are triggers and values are string expansions (flat map).
 *
 * Parsing and serialization use json-glib; only this interchange profile is supported.
 *
 * Registered for `MACRO_INTERCHANGE_FORMAT_JSON` and extension `.json` in the implementation TU.
 */
class JsonMacroInterchangeHandler : public MacroFormatHandler {
public:
  MacroInterchangeForcedFormat forced_format() const override;

  /**
   * @brief Parses JSON from @a path_utf8 and appends valid pairs into @a table.
   * @param path_utf8 UTF-8 filesystem path.
   * @param table Destination macro table (cleared by the dispatcher before import).
   * @param stats Optional aggregate counters for rows attempted, imported, or skipped.
   * @param err GLib error location.
   */
  gboolean import_from_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                            GError **err) override;

  /**
   * @brief Writes @a table as a JSON array of `{"trigger", "content"}` objects.
   * @param path_utf8 UTF-8 filesystem path.
   * @param table Source macro table.
   * @param stats Reserved for parity with other handlers; currently unused.
   * @param err GLib error location.
   */
  gboolean export_to_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                          GError **err) override;
};

#endif /* IBUS_UNIKEY_MACRO_HANDLER_JSON_H */
