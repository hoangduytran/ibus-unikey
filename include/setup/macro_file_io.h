// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_FILE_IO_H
#define IBUS_UNIKEY_MACRO_FILE_IO_H

#include <glib.h>

#ifdef __cplusplus

class CMacroTable;

/**
 * @file macro_file_io.h
 * @brief Macro interchange public API: format dispatch, statistics, GTK file filters.
 *
 * Import paths replace table content via `macro_interchange_import_path` (after reset inside the
 * dispatcher). Handlers are registered in `setup/macro_handler_*.cpp` via
 * `MacroFormatHandlerRegistry`.
 */

/**
 * @brief Optional counters for interchange import/export (any field may stay zero if not used).
 *
 * Semantics are codec-specific; handlers that parse row-by-row typically bump `attempted` per
 * candidate row and `imported` / `skipped_malformed` according to `CMacroTable::addItem` results.
 */
typedef struct MacroInterchangeStatistics {
  int attempted;
  int imported;
  int skipped_malformed;
  int truncated;
  int duplicate_collapsed;
} MacroInterchangeStatistics;

/**
 * @brief Selects a macro interchange codec or defers to path-based AUTO detection.
 *
 * Values correspond to registered `MacroFormatHandler` factories except `AUTO`, which chooses a
 * handler from the file extension (and possibly future heuristics).
 */
typedef enum {
  MACRO_INTERCHANGE_FORMAT_AUTO = 0,
  MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY,
  MACRO_INTERCHANGE_FORMAT_YAML,
  MACRO_INTERCHANGE_FORMAT_JSON,
  MACRO_INTERCHANGE_FORMAT_PLIST,
  MACRO_INTERCHANGE_FORMAT_CSV,
  MACRO_INTERCHANGE_FORMAT_TSV,
} MacroInterchangeForcedFormat;

/**
 * @brief Loads a macro table from @a filename using extension-based format detection.
 * @param filename UTF-8 path.
 * @param table Destination table; previous rows are cleared.
 * @param error Set on failure.
 * @return True on success.
 */
gboolean macro_table_load_any_format(const gchar *filename, CMacroTable *table,
                                     GError **error);

/**
 * @brief Loads macros from @a filename into @a table (resets table first).
 * @param forced `AUTO` or a specific `MacroInterchangeForcedFormat`.
 * @param stats Optional counters; may be null.
 * @param error Set on failure.
 */
gboolean macro_interchange_import_path(const gchar *filename, CMacroTable *table,
                                       MacroInterchangeForcedFormat forced,
                                       MacroInterchangeStatistics *stats,
                                       GError **error);

/**
 * @brief Writes @a table to @a filename using @a forced format (or AUTO from extension).
 * @param stats Optional counters for export telemetry; many codecs ignore this.
 * @param error Set on failure.
 */
gboolean macro_interchange_export_path(const gchar *filename, CMacroTable *table,
                                       MacroInterchangeForcedFormat forced,
                                       MacroInterchangeStatistics *stats,
                                       GError **error);

typedef struct _GtkFileChooser GtkFileChooser;

/** @brief Adds "all supported" and per-format patterns to a GTK import chooser. */
void macro_file_chooser_attach_import_filters(GtkFileChooser *chooser);

/** @brief Adds "all supported" and per-format patterns to a GTK export chooser. */
void macro_file_chooser_attach_export_filters(GtkFileChooser *chooser);

#endif /* __cplusplus */

#endif /* IBUS_UNIKEY_MACRO_FILE_IO_H */
