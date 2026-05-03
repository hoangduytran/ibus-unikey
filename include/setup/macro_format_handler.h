// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_FORMAT_HANDLER_H
#define IBUS_UNIKEY_MACRO_FORMAT_HANDLER_H

#include <glib.h>

#include <setup/macro_file_io.h>

class CMacroTable;

/**
 * @file macro_format_handler.h
 * @brief Abstract interchange codec: read/write `CMacroTable` by UTF-8 path.
 */

/**
 * @brief Pluggable macro interchange codec (import/export by path).
 *
 * Implementations live under `setup/macro_handler_*.cpp` and are selected by
 * `macro_interchange_import_path` / `macro_interchange_export_path` and the
 * `MacroFormatHandlerRegistry`.
 */
class MacroFormatHandler {
public:
  virtual ~MacroFormatHandler() = default;

  /** @brief Format id this instance handles (must match registry registration). */
  virtual MacroInterchangeForcedFormat forced_format() const = 0;

  /**
   * @brief Parses @a path_utf8 and fills @a table (dispatcher clears the table before calling).
   * @param stats Optional per-codec statistics.
   * @param err Set via `macro_interchange::fail` on failure when non-null.
   */
  virtual gboolean import_from_path(const gchar *path_utf8, CMacroTable *table,
                                      MacroInterchangeStatistics *stats, GError **err) = 0;

  /**
   * @brief Serializes @a table to @a path_utf8 in this codec's format.
   * @param stats Optional; many exporters ignore it.
   * @param err Set on failure when non-null.
   */
  virtual gboolean export_to_path(const gchar *path_utf8, CMacroTable *table,
                                    MacroInterchangeStatistics *stats, GError **err) = 0;
};

#endif /* IBUS_UNIKEY_MACRO_FORMAT_HANDLER_H */
