// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <setup/macro_handler_text.h>

#include <cstring>

#include <setup/macro_interchange_common.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/text_macro_format.h>

MacroInterchangeForcedFormat TextMacroHandler::forced_format() const {
  return MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY;
}

gboolean TextMacroHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                            MacroInterchangeStatistics *stats, GError **err) {
  TextMacroFormat tf;
  int ver = 0;
  if (tf.importFromPath(path_utf8, *table, &ver) != 1) {
    const char *d = table->getLastErrorMessage();
    macro_interchange::fail(err, (d && d[0]) ? d : "Text macro import failed");
    return FALSE;
  }
  if (stats) {
    stats->imported = table->getCount();
    stats->attempted = stats->imported;
  }
  return TRUE;
}

gboolean TextMacroHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                         MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  TextMacroFormat fmt;
  if (fmt.exportToPath(path_utf8, *table) != 1) {
    const char *d = table->getLastErrorMessage();
    macro_interchange::fail(err, (d && d[0]) ? d : "Text macro export failed");
    return FALSE;
  }
  return TRUE;
}
