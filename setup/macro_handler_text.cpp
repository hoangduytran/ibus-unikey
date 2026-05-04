// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file macro_handler_text.cpp
 * @brief Setup-side interchange adapter for UniKey native macro text (`.txt`, `.macro`).
 *
 * Delegates parse and serialize work to `TextMacroFormat` in ukengine. This path is used
 * for **interchange** import/export (GTK chooser, tests) and intentionally avoids
 * `CMacroTable::loadFromFile` / `writeToFile`, which also manage the `.ukmcache` sidecar
 * used by the IBus engine’s canonical macro file.
 */

#include <setup/macro_handler_text.h>

#include <cstring>
#include <memory>

#include <setup/macro_format_handler_registry.h>
#include <setup/macro_interchange_common.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/text_macro_format.h>

MacroInterchangeForcedFormat TextMacroHandler::forced_format() const {
  return MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY;
}

/**
 * @brief Load macro rows from a UniKey text file into @a table (replace semantics; caller
 *        typically called `CMacroTable::resetContent()` first).
 *
 * On success, fills @a stats with the final row count when @a stats is non-null. On failure,
 * sets `GError` via `macro_interchange::fail` and leaves detailed diagnostics on @a table
 * (`getLastErrorMessage()`), which the UI may show alongside the summary.
 *
 * @param path_utf8 Path to the macro file (UTF-8 path on POSIX).
 * @param table Target table; must be initialized by the caller.
 * @param stats Optional aggregate counters for partial-import telemetry.
 * @param err Optional `GError` location; safe to pass `nullptr`.
 * @return `TRUE` when `TextMacroFormat::importFromPath` reports success.
 *
 * @note Does not read or write `.ukmcache`; that remains the engine canonical path only.
 */
gboolean TextMacroHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                            MacroInterchangeStatistics *stats, GError **err) {
  TextMacroFormat text_format;
  int file_version = 0;
  if (text_format.importFromPath(path_utf8, *table, &file_version) != 1) {
    const char *detail_message = table->getLastErrorMessage();
    macro_interchange::fail(err, (detail_message && detail_message[0]) ? detail_message
                                                                        : "Text macro import failed");
    return FALSE;
  }
  if (stats) {
    stats->imported = table->getCount();
    stats->attempted = stats->imported;
  }
  return TRUE;
}

/**
 * @brief Write @a table to a UniKey UTF-8 text macro file (version header and sorted rows).
 *
 * @param path_utf8 Destination path for the text file.
 * @param table Source table populated by the UI or prior import.
 * @param stats Reserved for interchange statistics; unused for this codec (no partial rows).
 * @param err Optional `GError` location.
 * @return `TRUE` when `TextMacroFormat::exportToPath` reports success.
 *
 * @note Like import, this writes **human-readable text only**; no `.ukmcache` is emitted here.
 */
gboolean TextMacroHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                         MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  TextMacroFormat text_format;
  if (text_format.exportToPath(path_utf8, *table) != 1) {
    const char *detail_message = table->getLastErrorMessage();
    macro_interchange::fail(err, (detail_message && detail_message[0]) ? detail_message
                                                                       : "Text macro export failed");
    return FALSE;
  }
  return TRUE;
}

namespace {

/**
 * @brief Registers this handler with `MacroFormatHandlerRegistry` before `main`.
 *
 * Maps `.txt` and `.macro` suffixes to `MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY` for AUTO
 * detection. Consumers that link `macro_interchange` as a static library must pull every
 * object file (see `setup/CMakeLists.txt`) so this constructor runs.
 */
struct TextMacroHandlerRegistrar {
  TextMacroHandlerRegistrar() {
    MacroFormatHandlerRegistry::register_handler(
        MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY,
        []() { return std::unique_ptr<MacroFormatHandler>(new TextMacroHandler()); },
        {".txt", ".macro"});
  }
};

static TextMacroHandlerRegistrar g_text_macro_handler_registrar;

} // namespace
