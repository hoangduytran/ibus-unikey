// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_HANDLER_PLIST_H
#define IBUS_UNIKEY_MACRO_HANDLER_PLIST_H

#include <setup/macro_format_handler.h>

/** Apple XML plist text replacements (`shortcut` / `phrase`, with common synonyms). */
class PlistTextReplacementMacroHandler : public MacroFormatHandler {
public:
  MacroInterchangeForcedFormat forced_format() const override;
  gboolean import_from_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                            GError **err) override;
  gboolean export_to_path(const gchar *path_utf8, CMacroTable *table, MacroInterchangeStatistics *stats,
                          GError **err) override;
};

#endif /* IBUS_UNIKEY_MACRO_HANDLER_PLIST_H */
