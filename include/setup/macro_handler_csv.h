// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_HANDLER_CSV_H
#define IBUS_UNIKEY_MACRO_HANDLER_CSV_H

#include <setup/macro_format_handler.h>

/** RFC-style tabular macros: comma or tab delimiter; optional `trigger`/`content` header row. */
class DelimitedTextMacroHandler : public MacroFormatHandler {
public:
  DelimitedTextMacroHandler(char field_delimiter, MacroInterchangeForcedFormat format_id);

  MacroInterchangeForcedFormat forced_format() const override;
  gboolean import_from_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                            GError **err) override;
  gboolean export_to_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                          GError **err) override;

private:
  char delim_;
  MacroInterchangeForcedFormat format_id_;
};

#endif /* IBUS_UNIKEY_MACRO_HANDLER_CSV_H */
