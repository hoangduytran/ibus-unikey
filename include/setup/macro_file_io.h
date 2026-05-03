// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_FILE_IO_H
#define IBUS_UNIKEY_MACRO_FILE_IO_H

#include <glib.h>

#ifdef __cplusplus

class CMacroTable;

/**
 * Optional counters for interchange import/export (any field may stay zero if
 * not tracked for a given codec).
 */
typedef struct MacroInterchangeStatistics {
  int attempted;
  int imported;
  int skipped_malformed;
  int truncated;
  int duplicate_collapsed;
} MacroInterchangeStatistics;

typedef enum {
  MACRO_INTERCHANGE_FORMAT_AUTO = 0,
  MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY,
  MACRO_INTERCHANGE_FORMAT_YAML,
  MACRO_INTERCHANGE_FORMAT_JSON,
  MACRO_INTERCHANGE_FORMAT_PLIST,
  MACRO_INTERCHANGE_FORMAT_CSV,
  MACRO_INTERCHANGE_FORMAT_TSV,
} MacroInterchangeForcedFormat;

gboolean macro_table_load_any_format(const gchar *filename, CMacroTable *table,
                                     GError **error);

gboolean macro_interchange_import_path(const gchar *filename, CMacroTable *table,
                                       MacroInterchangeForcedFormat forced,
                                       MacroInterchangeStatistics *stats,
                                       GError **error);

gboolean macro_interchange_export_path(const gchar *filename, CMacroTable *table,
                                       MacroInterchangeForcedFormat forced,
                                       MacroInterchangeStatistics *stats,
                                       GError **error);

typedef struct _GtkFileChooser GtkFileChooser;

void macro_file_chooser_attach_import_filters(GtkFileChooser *chooser);
void macro_file_chooser_attach_export_filters(GtkFileChooser *chooser);

#endif /* __cplusplus */

#endif /* IBUS_UNIKEY_MACRO_FILE_IO_H */
