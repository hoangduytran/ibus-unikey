// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_HANDLER_CSV_H
#define IBUS_UNIKEY_MACRO_HANDLER_CSV_H

#include <setup/macro_format_handler.h>

/**
 * @file macro_handler_csv.h
 * @brief Declaration of delimiter-separated (CSV/TSV) macro interchange handler.
 */

/**
 * @brief Imports and exports macros as delimiter-separated text (comma or tab).
 *
 * Supports an optional header row naming `trigger` and `content` columns (RFC-style tables).
 * Two instances are registered: CSV (comma, `MACRO_INTERCHANGE_FORMAT_CSV`, `.csv`) and TSV (tab,
 * `MACRO_INTERCHANGE_FORMAT_TSV`, `.tsv`) via static registrars in the implementation TU.
 */
class DelimitedTextMacroHandler : public MacroFormatHandler {
public:
  /**
   * @brief Binds the field delimiter and interchange format id (CSV vs TSV).
   * @param field_delimiter Comma or ASCII tab for the two supported profiles.
   * @param format_id Must match `forced_format()` and registry registration.
   */
  DelimitedTextMacroHandler(char field_delimiter, MacroInterchangeForcedFormat format_id);

  MacroInterchangeForcedFormat forced_format() const override;

  /**
   * @brief Parses delimiter-separated rows from @a path_utf8 into @a table.
   * @param stats Optional row counters.
   * @param err Set on failure.
   */
  gboolean import_from_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                            GError **err) override;

  /**
   * @brief Writes @a table with header and quoted fields as needed.
   * @param stats Reserved; currently unused.
   * @param err Set on failure.
   */
  gboolean export_to_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                          GError **err) override;

private:
  char delim_;
  MacroInterchangeForcedFormat format_id_;
};

#endif /* IBUS_UNIKEY_MACRO_HANDLER_CSV_H */
