// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/**
 * @file macro_file_io.cpp
 * @brief Macro interchange entry points and GTK file chooser filters.
 *
 * Handler types and AUTO suffix routing are registered in each
 * `macro_handler_*.cpp` via `MacroFormatHandlerRegistry::register_handler`.
 */

#include <setup/macro_file_io.h>

#include <cstring>
#include <memory>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <setup/macro_format_handler.h>
#include <setup/macro_format_handler_registry.h>
#include <setup/macro_interchange_common.h>

#include <ukengine/mapping/mactab.h>

gboolean macro_table_load_any_format(const gchar *filename, CMacroTable *table, GError **error) {
  return macro_interchange_import_path(filename, table, MACRO_INTERCHANGE_FORMAT_AUTO, nullptr, error);
}

gboolean macro_interchange_import_path(const gchar *filename, CMacroTable *table,
                                       MacroInterchangeForcedFormat forced, MacroInterchangeStatistics *stats,
                                       GError **err) {
  if (!filename || !table) {
    macro_interchange::fail(err, "Invalid macro interchange import arguments");
    return FALSE;
  }

  if (stats)
    memset(stats, 0, sizeof(*stats));

  table->resetContent();

  MacroInterchangeForcedFormat fmt = forced;
  if (fmt == MACRO_INTERCHANGE_FORMAT_AUTO)
    fmt = MacroFormatHandlerRegistry::detect_from_path(filename);

  std::unique_ptr<MacroFormatHandler> h = MacroFormatHandlerRegistry::create(fmt);
  if (!h) {
    macro_interchange::fail(err, "Unsupported macro interchange format");
    return FALSE;
  }

  return h->import_from_path(filename, table, stats, err);
}

gboolean macro_interchange_export_path(const gchar *filename, CMacroTable *table,
                                       MacroInterchangeForcedFormat forced, MacroInterchangeStatistics *stats,
                                       GError **err) {
  if (!filename || !table) {
    macro_interchange::fail(err, "Invalid macro interchange export arguments");
    return FALSE;
  }

  if (stats)
    memset(stats, 0, sizeof(*stats));

  MacroInterchangeForcedFormat fmt = forced;
  if (fmt == MACRO_INTERCHANGE_FORMAT_AUTO)
    fmt = MacroFormatHandlerRegistry::detect_from_path(filename);

  std::unique_ptr<MacroFormatHandler> h = MacroFormatHandlerRegistry::create(fmt);
  if (!h) {
    macro_interchange::fail(err, "Unsupported macro interchange export format");
    return FALSE;
  }

  const gboolean ok = h->export_to_path(filename, table, stats, err);
  if (ok && stats) {
    stats->imported = table->getCount();
    stats->attempted = stats->imported;
  }
  return ok;
}

void macro_file_chooser_attach_import_filters(GtkFileChooser *chooser) {
  GtkFileFilter *all = gtk_file_filter_new();
  gtk_file_filter_set_name(all, _("All supported formats"));
  gtk_file_filter_add_pattern(all, "*.txt");
  gtk_file_filter_add_pattern(all, "*.macro");
  gtk_file_filter_add_pattern(all, "*.json");
  gtk_file_filter_add_pattern(all, "*.yaml");
  gtk_file_filter_add_pattern(all, "*.yml");
  gtk_file_filter_add_pattern(all, "*.plist");
  gtk_file_filter_add_pattern(all, "*.csv");
  gtk_file_filter_add_pattern(all, "*.tsv");
  gtk_file_chooser_add_filter(chooser, all);

  GtkFileFilter *uni = gtk_file_filter_new();
  gtk_file_filter_set_name(uni, _("UniKey macro text"));
  gtk_file_filter_add_pattern(uni, "*.txt");
  gtk_file_filter_add_pattern(uni, "*.macro");
  gtk_file_chooser_add_filter(chooser, uni);

  GtkFileFilter *json = gtk_file_filter_new();
  gtk_file_filter_set_name(json, _("JSON macros"));
  gtk_file_filter_add_pattern(json, "*.json");
  gtk_file_chooser_add_filter(chooser, json);

  GtkFileFilter *yaml = gtk_file_filter_new();
  gtk_file_filter_set_name(yaml, _("YAML macros"));
  gtk_file_filter_add_pattern(yaml, "*.yaml");
  gtk_file_filter_add_pattern(yaml, "*.yml");
  gtk_file_chooser_add_filter(chooser, yaml);

  GtkFileFilter *plist = gtk_file_filter_new();
  gtk_file_filter_set_name(plist, _("macOS plist (text replacements)"));
  gtk_file_filter_add_pattern(plist, "*.plist");
  gtk_file_chooser_add_filter(chooser, plist);

  GtkFileFilter *csv = gtk_file_filter_new();
  gtk_file_filter_set_name(csv, _("CSV macros"));
  gtk_file_filter_add_pattern(csv, "*.csv");
  gtk_file_chooser_add_filter(chooser, csv);

  GtkFileFilter *tsv = gtk_file_filter_new();
  gtk_file_filter_set_name(tsv, _("TSV macros"));
  gtk_file_filter_add_pattern(tsv, "*.tsv");
  gtk_file_chooser_add_filter(chooser, tsv);

  gtk_file_chooser_set_filter(chooser, all);
}

void macro_file_chooser_attach_export_filters(GtkFileChooser *chooser) {
  macro_file_chooser_attach_import_filters(chooser);
}
