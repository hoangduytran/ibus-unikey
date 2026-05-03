// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file macro_handler_csv.cpp
 * @brief CSV/TSV macro interchange using **libcsv** for parse and quoted-field write.
 *
 * **Import:** `csv_parse` / `csv_fini` with `CSV_APPEND_NULL`, one logical row assembled from libcsv
 * field and row callbacks. **Export:** `csv_write()` wraps every cell in quotes (always valid CSV/TSV;
 * more verbose than “quote only when needed”). **AUTO** registration maps `.csv` and `.tsv` in this TU.
 */

#include <setup/macro_handler_csv.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <csv.h>
}

#include <setup/macro_format_handler_registry.h>
#include <setup/macro_interchange_common.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

namespace {

/**
 * @brief Strips leading ASCII space/tab and trailing space/tab/CR from a parsed cell.
 *
 * libcsv already trims spaces defined by its own rules; this aligns optional header detection with
 * the previous hand parser (`Trigger` vs `trigger`).
 *
 * @param cell In-out string from one CSV field.
 */
void trim_ascii_edges(std::string *cell) {
  while (!cell->empty() && ((*cell)[0] == ' ' || (*cell)[0] == '\t'))
    cell->erase(cell->begin());
  while (!cell->empty() &&
         (cell->back() == ' ' || cell->back() == '\t' || cell->back() == '\r'))
    cell->pop_back();
}

/**
 * @brief Lowercases ASCII letters in @a cell for case-insensitive header matching.
 * @param cell Header cell text after @ref trim_ascii_edges.
 */
void header_ascii_lowercase(std::string *cell) {
  for (char &ch : *cell)
    ch = (char)tolower((unsigned char)ch);
}

/**
 * @brief User data for libcsv callbacks while importing one file.
 *
 * Accumulates fields into `current_row` until the row callback runs, then maps two columns into
 * `CMacroTable::addItem`. The first row may be interpreted as a header to set column indices.
 */
struct DelimitedImportContext {
  /** Destination table (owned by caller). */
  CMacroTable *table;
  /** Optional counters; may be `nullptr`. */
  MacroInterchangeStatistics *stats;
  /** Set when `csv_parse` length mismatch or `csv_fini` fails (stops further row logic). */
  bool parse_failed;
  /** Zero-based column index for trigger text (default 0; reaffirmed if header matches). */
  int trigger_column_index;
  /** Zero-based column index for phrase text (default 1). */
  int content_column_index;
  /** Until the first data row is committed, allow header sniffing on two cells. */
  bool expect_header_or_first_data_row;
  /** Fields for the row currently being assembled. */
  std::vector<std::string> current_row;
};

/**
 * @brief Completes one record: optional header row, or `addItem` from trigger/phrase columns.
 *
 * @param ctx Parser state; `current_row` is cleared before return unless `parse_failed`.
 *
 * @details Skips rows with too few columns. Header synonyms: `trigger`/`abbreviation` and
 * `content`/`expansion`. Malformed or rejected adds bump `skipped_malformed` when @a stats is set.
 */
void finish_import_row(DelimitedImportContext *ctx) {
  if (ctx->parse_failed)
    return;
  if (ctx->current_row.empty())
    return;

  std::vector<std::string> &row = ctx->current_row;

  if (ctx->expect_header_or_first_data_row && row.size() >= 2) {
    std::string header_cell_0 = row[0];
    std::string header_cell_1 = row[1];
    trim_ascii_edges(&header_cell_0);
    trim_ascii_edges(&header_cell_1);
    header_ascii_lowercase(&header_cell_0);
    header_ascii_lowercase(&header_cell_1);
    if ((header_cell_0 == "trigger" || header_cell_0 == "abbreviation") &&
        (header_cell_1 == "content" || header_cell_1 == "expansion")) {
      ctx->trigger_column_index = 0;
      ctx->content_column_index = 1;
      ctx->expect_header_or_first_data_row = false;
      row.clear();
      return;
    }
  }

  ctx->expect_header_or_first_data_row = false;

  if ((int)row.size() <= std::max(ctx->trigger_column_index, ctx->content_column_index)) {
    row.clear();
    return;
  }

  const std::string &trigger_utf8 = row[(size_t)ctx->trigger_column_index];
  const std::string &phrase_utf8 = row[(size_t)ctx->content_column_index];
  if (ctx->stats)
    ctx->stats->attempted++;
  const int add_item_result =
      ctx->table->addItem(trigger_utf8.c_str(), phrase_utf8.c_str(), CONV_CHARSET_UNIUTF8);
  if (add_item_result >= 0) {
    if (ctx->stats)
      ctx->stats->imported++;
  } else if (ctx->stats) {
    ctx->stats->skipped_malformed++;
  }

  row.clear();
}

/**
 * @brief Appends one escaped CSV field (quotes + doubled inner quotes) to @a out.
 *
 * Uses libcsv’s sizing convention: `csv_write(nullptr, 0, …)` yields required length; `SIZE_MAX`
 * means the field is too large to represent.
 *
 * @param out Growing output buffer.
 * @param field_utf8 Raw cell bytes (UTF-8 for this interchange profile).
 * @return `true` if @a out was resized and written successfully.
 */
bool append_libcsv_quoted_field(std::string *out, const std::string &field_utf8) {
  const size_t required_len = csv_write(nullptr, 0, field_utf8.data(), field_utf8.size());
  if (required_len == SIZE_MAX)
    return false;
  const size_t offset = out->size();
  out->resize(offset + required_len);
  const size_t written =
      csv_write(&(*out)[offset], required_len, field_utf8.data(), field_utf8.size());
  return written == required_len;
}

/**
 * @brief libcsv end-of-field callback; appends one field to `DelimitedImportContext::current_row`.
 *
 * @param field_bytes Field buffer (NUL-terminated when `CSV_APPEND_NULL` is set).
 * @param field_byte_count Field length in bytes (excluding added `NUL` from append-null mode).
 * @param userdata Pointer to `DelimitedImportContext`.
 */
extern "C" void delimited_import_field_cb(void *field_bytes, size_t field_byte_count, void *userdata) {
  auto *ctx = static_cast<DelimitedImportContext *>(userdata);
  if (ctx->parse_failed)
    return;
  ctx->current_row.emplace_back(static_cast<const char *>(field_bytes), field_byte_count);
}

/**
 * @brief libcsv end-of-record callback; forwards to @ref finish_import_row.
 *
 * @param terminator_char Line-ending character from libcsv (unused).
 * @param userdata Pointer to `DelimitedImportContext`.
 */
extern "C" void delimited_import_row_cb(int /*terminator_char*/, void *userdata) {
  auto *ctx = static_cast<DelimitedImportContext *>(userdata);
  finish_import_row(ctx);
}

} // namespace

/**
 * @copydoc DelimitedTextMacroHandler::DelimitedTextMacroHandler(char,MacroInterchangeForcedFormat)
 */
DelimitedTextMacroHandler::DelimitedTextMacroHandler(char field_delimiter, MacroInterchangeForcedFormat format_id)
    : delim_(field_delimiter), format_id_(format_id) {}

/** @copydoc DelimitedTextMacroHandler::forced_format */
MacroInterchangeForcedFormat DelimitedTextMacroHandler::forced_format() const { return format_id_; }

/**
 * @copydoc DelimitedTextMacroHandler::import_from_path
 *
 * @details Configures `csv_parser` with `delim_` and `CSV_APPEND_NULL`. On `csv_parse` partial
 * consumption, sets `GError` from `csv_strerror(csv_error(parser))`. Always `csv_free`s the parser.
 */
gboolean DelimitedTextMacroHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                                     MacroInterchangeStatistics *stats, GError **err) {
  std::string raw;
  if (!macro_interchange::read_file_utf8(path_utf8, &raw, err))
    return FALSE;

  struct csv_parser parser_storage;
  struct csv_parser *parser = &parser_storage;
  if (csv_init(parser, CSV_APPEND_NULL) != 0) {
    macro_interchange::fail(err, "Could not initialize CSV parser");
    return FALSE;
  }
  csv_set_delim(parser, (unsigned char)delim_);

  DelimitedImportContext ctx{};
  ctx.table = table;
  ctx.stats = stats;
  ctx.parse_failed = false;
  ctx.trigger_column_index = 0;
  ctx.content_column_index = 1;
  ctx.expect_header_or_first_data_row = true;

  const size_t consumed =
      csv_parse(parser, raw.data(), raw.size(), delimited_import_field_cb, delimited_import_row_cb, &ctx);
  if (consumed != raw.size()) {
    ctx.parse_failed = true;
    macro_interchange::fail(err, csv_strerror(csv_error(parser)));
    csv_free(parser);
    return FALSE;
  }

  if (csv_fini(parser, delimited_import_field_cb, delimited_import_row_cb, &ctx) != 0) {
    macro_interchange::fail(err, "CSV parser finalization failed");
    csv_free(parser);
    return FALSE;
  }

  csv_free(parser);
  return TRUE;
}

/**
 * @copydoc DelimitedTextMacroHandler::export_to_path
 *
 * @details Emits a header row `trigger`/`content`, then rows in `macro_interchange::sorted_macro_row_indices`
 * order. Each cell is written with `append_libcsv_quoted_field`, separated by `delim_`, terminated by `\\n`.
 */
gboolean DelimitedTextMacroHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                                   MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  const std::vector<int> order = macro_interchange::sorted_macro_row_indices(table);
  const int row_count = table->getCount();

  std::string output;
  output.reserve((size_t)row_count * 64 + 32);

  if (!append_libcsv_quoted_field(&output, "trigger"))
    goto export_fail;
  output.push_back(delim_);
  if (!append_libcsv_quoted_field(&output, "content"))
    goto export_fail;
  output.push_back('\n');

  for (int row_index = 0; row_index < row_count; row_index++) {
    const int sorted_row_index = order[(size_t)row_index];
    std::string trigger_utf8, phrase_utf8;
    if (!macro_interchange::utf8_from_std_keytext(table->getKey(sorted_row_index), &trigger_utf8) ||
        !macro_interchange::utf8_from_std_keytext(table->getText(sorted_row_index), &phrase_utf8)) {
      macro_interchange::fail(err, "Could not encode macro row for CSV/TSV export");
      return FALSE;
    }
    if (!append_libcsv_quoted_field(&output, trigger_utf8))
      goto export_fail;
    output.push_back(delim_);
    if (!append_libcsv_quoted_field(&output, phrase_utf8))
      goto export_fail;
    output.push_back('\n');
  }

  if (!g_file_set_contents(path_utf8, output.data(), (gssize)output.size(), err))
    return FALSE;
  return TRUE;

export_fail:
  macro_interchange::fail(err, "Could not encode CSV/TSV field for export");
  return FALSE;
}

namespace {

/**
 * @brief Registers `DelimitedTextMacroHandler` for comma-separated values and `.csv` (AUTO).
 *
 * Static instance ensures `MacroFormatHandlerRegistry::register_handler` runs during startup when
 * this object file is linked (see `setup/CMakeLists.txt` whole-archive notes).
 */
struct CsvMacroHandlerRegistrar {
  CsvMacroHandlerRegistrar() {
    MacroFormatHandlerRegistry::register_handler(
        MACRO_INTERCHANGE_FORMAT_CSV,
        []() {
          return std::unique_ptr<MacroFormatHandler>(
              new DelimitedTextMacroHandler(',', MACRO_INTERCHANGE_FORMAT_CSV));
        },
        {".csv"});
  }
};

/**
 * @brief Registers tab-delimited handler and `.tsv` (AUTO).
 */
struct TsvMacroHandlerRegistrar {
  TsvMacroHandlerRegistrar() {
    MacroFormatHandlerRegistry::register_handler(
        MACRO_INTERCHANGE_FORMAT_TSV,
        []() {
          return std::unique_ptr<MacroFormatHandler>(
              new DelimitedTextMacroHandler('\t', MACRO_INTERCHANGE_FORMAT_TSV));
        },
        {".tsv"});
  }
};

static CsvMacroHandlerRegistrar g_csv_macro_handler_registrar;
static TsvMacroHandlerRegistrar g_tsv_macro_handler_registrar;

} // namespace
