// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <setup/macro_handler_csv.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>

#include <setup/macro_format_handler_registry.h>
#include <setup/macro_interchange_common.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

namespace {

void trim_inline(std::string *s) {
  while (!s->empty() && ((*s)[0] == ' ' || (*s)[0] == '\t'))
    s->erase(s->begin());
  while (!s->empty() && (s->back() == ' ' || s->back() == '\t' || s->back() == '\r'))
    s->pop_back();
}

void csv_unescape_field(std::string *f) {
  trim_inline(f);
  if (f->size() >= 2 && (*f)[0] == '"' && f->back() == '"') {
    std::string inner = f->substr(1, f->size() - 2);
    std::string out;
    for (size_t i = 0; i < inner.size(); i++) {
      if (inner[i] == '"' && i + 1 < inner.size() && inner[i + 1] == '"') {
        out.push_back('"');
        i++;
      } else
        out.push_back(inner[i]);
    }
    *f = std::move(out);
  }
}

} // namespace

DelimitedTextMacroHandler::DelimitedTextMacroHandler(char field_delimiter, MacroInterchangeForcedFormat format_id)
    : delim_(field_delimiter), format_id_(format_id) {}

MacroInterchangeForcedFormat DelimitedTextMacroHandler::forced_format() const { return format_id_; }

gboolean DelimitedTextMacroHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                                    MacroInterchangeStatistics *stats, GError **err) {
  std::string raw;
  if (!macro_interchange::read_file_utf8(path_utf8, &raw, err))
    return FALSE;

  std::istringstream iss(raw);
  std::string line;
  bool first = true;
  int tri_col = 0, con_col = 1;

  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      continue;

    std::vector<std::string> fields;
    std::string cur;
    bool inq = false;
    for (size_t i = 0; i < line.size(); i++) {
      char c = line[i];
      if (inq) {
        if (c == '"') {
          if (i + 1 < line.size() && line[i + 1] == '"') {
            cur.push_back('"');
            i++;
          } else {
            inq = false;
          }
        } else
          cur.push_back(c);
      } else {
        if (c == delim_) {
          fields.push_back(cur);
          cur.clear();
        } else if (c == '"')
          inq = true;
        else
          cur.push_back(c);
      }
    }
    fields.push_back(cur);

    if (first && fields.size() >= 2) {
      std::string h0 = fields[0];
      std::string h1 = fields[1];
      trim_inline(&h0);
      trim_inline(&h1);
      csv_unescape_field(&h0);
      csv_unescape_field(&h1);
      for (char &c : h0)
        c = (char)tolower((unsigned char)c);
      for (char &c : h1)
        c = (char)tolower((unsigned char)c);
      if ((h0 == "trigger" || h0 == "abbreviation") && (h1 == "content" || h1 == "expansion")) {
        tri_col = 0;
        con_col = 1;
        first = false;
        continue;
      }
    }
    first = false;

    if ((int)fields.size() <= std::max(tri_col, con_col))
      continue;

    std::string trig = fields[(size_t)tri_col];
    std::string cont = fields[(size_t)con_col];
    csv_unescape_field(&trig);
    csv_unescape_field(&cont);
    if (stats)
      stats->attempted++;
    const int r = table->addItem(trig.c_str(), cont.c_str(), CONV_CHARSET_UNIUTF8);
    if (r >= 0) {
      if (stats)
        stats->imported++;
    } else if (stats)
      stats->skipped_malformed++;
  }

  return TRUE;
}

gboolean DelimitedTextMacroHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                                  MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  const std::vector<int> order = macro_interchange::sorted_macro_row_indices(table);
  const int n = table->getCount();

  std::ostringstream oss;
  oss << "trigger" << delim_ << "content\n";
  for (int i = 0; i < n; i++) {
    const int ix = order[(size_t)i];
    std::string keyu, valu;
    if (!macro_interchange::utf8_from_std_keytext(table->getKey(ix), &keyu) ||
        !macro_interchange::utf8_from_std_keytext(table->getText(ix), &valu)) {
      macro_interchange::fail(err, "Could not encode macro row for CSV/TSV export");
      return FALSE;
    }
    auto quote = [&](const std::string &s) -> std::string {
      bool need = s.find_first_of(",\"\n\r") != std::string::npos ||
                  (delim_ != ',' && s.find(delim_) != std::string::npos);
      if (!need)
        return s;
      std::string o = "\"";
      for (char c : s) {
        if (c == '"')
          o += "\"\"";
        else
          o.push_back(c);
      }
      o += '"';
      return o;
    };
    oss << quote(keyu) << delim_ << quote(valu) << "\n";
  }
  const std::string data = oss.str();
  if (!g_file_set_contents(path_utf8, data.data(), (gssize)data.size(), err))
    return FALSE;
  return TRUE;
}

namespace {

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
