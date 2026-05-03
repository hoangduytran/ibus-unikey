// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_HANDLER_JSON_H
#define IBUS_UNIKEY_MACRO_HANDLER_JSON_H

#include <setup/macro_format_handler.h>

/** Generic macro JSON: array of `{"trigger","content"}` or flat string map. */
class JsonMacroInterchangeHandler : public MacroFormatHandler {
public:
  MacroInterchangeForcedFormat forced_format() const override;
  gboolean import_from_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                            GError **err) override;
  gboolean export_to_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                          GError **err) override;
};

#endif /* IBUS_UNIKEY_MACRO_HANDLER_JSON_H */
