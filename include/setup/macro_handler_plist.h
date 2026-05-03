// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_HANDLER_PLIST_H
#define IBUS_UNIKEY_MACRO_HANDLER_PLIST_H

#include <setup/macro_format_handler.h>

/**
 * @file macro_handler_plist.h
 * @brief Declaration of the Apple property list macro interchange handler.
 */

/**
 * @brief Imports and exports UniKey macros using macOS text-replacement style plists.
 *
 * Parsing and XML serialization use libplist (XML and binary on import; XML on export). Each entry is
 * a dictionary with `shortcut` and `phrase`, or the synonyms `replace` / `with`. The usual file shape
 * is a root `array` of such dictionaries; a single root `dict` with one pair is also accepted on import.
 *
 * Registered for `MACRO_INTERCHANGE_FORMAT_PLIST` and the `.plist` extension in the implementation TU.
 */
class PlistTextReplacementMacroHandler : public MacroFormatHandler {
public:
  MacroInterchangeForcedFormat forced_format() const override;

  /**
   * @brief Loads shortcuts from a plist file into @a table (replacing prior content via the dispatcher).
   * @param path_utf8 UTF-8 filesystem path.
   * @param table Destination macro table.
   * @param stats Optional counters for attempted / imported / skipped rows.
   * @param err GLib error location.
   */
  gboolean import_from_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                            GError **err) override;

  /**
   * @brief Writes @a table as an XML plist (array of `phrase` / `shortcut` dicts).
   * @param path_utf8 UTF-8 filesystem path.
   * @param table Source macro table.
   * @param stats Reserved for parity with other handlers; currently unused.
   * @param err GLib error location.
   */
  gboolean export_to_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                          GError **err) override;
};

#endif /* IBUS_UNIKEY_MACRO_HANDLER_PLIST_H */
