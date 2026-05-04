// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_HANDLER_YAML_H
#define IBUS_UNIKEY_MACRO_HANDLER_YAML_H

#include <setup/macro_format_handler.h>

/**
 * @file macro_handler_yaml.h
 * @brief Declaration of the Espanso-style YAML macro interchange handler.
 */

/**
 * @brief Imports and exports macros using a small subset of YAML shaped like Espanso packs.
 *
 * Expects a top-level `matches:` sequence of objects with `trigger` (shortcut) and `replace`
 * (phrase) scalars. Parsing and emission use **yaml-cpp** (full YAML 1.2 syntax for the document
 * shape above; only this Espanso-like layout is treated as a macro interchange profile).
 *
 * Registered for `MACRO_INTERCHANGE_FORMAT_YAML` and extensions `.yaml` / `.yml` in the
 * implementation TU.
 */
class YamlEspansoMacroHandler : public MacroFormatHandler {
public:
  MacroInterchangeForcedFormat forced_format() const override;

  /**
   * @brief Loads YAML macro rows into @a table.
   * @param path_utf8 UTF-8 filesystem path.
   * @param table Destination macro table (cleared by the dispatcher before import).
   * @param stats Optional aggregate counters for rows attempted, imported, or skipped.
   * @param err GLib error location.
   */
  gboolean import_from_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                            GError **err) override;

  /**
   * @brief Writes @a table as Espanso-style YAML (`matches:` / `trigger` / `replace`).
   * @param path_utf8 UTF-8 filesystem path.
   * @param table Source macro table.
   * @param stats Reserved for parity with other handlers; currently unused.
   * @param err GLib error location.
   */
  gboolean export_to_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                          GError **err) override;
};

#endif /* IBUS_UNIKEY_MACRO_HANDLER_YAML_H */
