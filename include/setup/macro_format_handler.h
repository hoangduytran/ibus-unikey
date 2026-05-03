// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_FORMAT_HANDLER_H
#define IBUS_UNIKEY_MACRO_FORMAT_HANDLER_H

#include <glib.h>

#include <setup/macro_file_io.h>

class CMacroTable;

/**
 * @brief Pluggable macro interchange codec (import/export by path).
 *
 * Implementations live under `setup/macro_handler_*.cpp` and are selected by
 * `macro_interchange_import_path` / `macro_interchange_export_path`.
 */
class MacroFormatHandler {
public:
  virtual ~MacroFormatHandler() = default;

  virtual MacroInterchangeForcedFormat forced_format() const = 0;

  virtual gboolean import_from_path(const gchar *path_utf8, CMacroTable *table,
                                      MacroInterchangeStatistics *stats, GError **err) = 0;

  virtual gboolean export_to_path(const gchar *path_utf8, CMacroTable *table,
                                    MacroInterchangeStatistics *stats, GError **err) = 0;
};

#endif /* IBUS_UNIKEY_MACRO_FORMAT_HANDLER_H */
