// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <setup/macro_handler_plist.h>

#include <cstring>
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

void xml_unescape_simple(std::string *s) {
  std::string out;
  out.reserve(s->size());
  for (size_t i = 0; i < s->size(); i++) {
    if ((*s)[i] != '&') {
      out.push_back((*s)[i]);
      continue;
    }
    if (s->compare(i, 5, "&amp;") == 0) {
      out.push_back('&');
      i += 4;
    } else if (s->compare(i, 4, "&lt;") == 0) {
      out.push_back('<');
      i += 3;
    } else if (s->compare(i, 4, "&gt;") == 0) {
      out.push_back('>');
      i += 3;
    } else if (s->compare(i, 6, "&quot;") == 0) {
      out.push_back('"');
      i += 5;
    } else if (s->compare(i, 6, "&apos;") == 0) {
      out.push_back('\'');
      i += 5;
    } else
      out.push_back('&');
  }
  *s = std::move(out);
}

bool plist_dict_pair_strings(const std::string &dictxml, std::string *shortcut_out, std::string *phrase_out) {
  shortcut_out->clear();
  phrase_out->clear();
  size_t p = 0;
  while ((p = dictxml.find("<key>", p)) != std::string::npos) {
    size_t kstart = p + 5;
    size_t kend = dictxml.find("</key>", kstart);
    if (kend == std::string::npos)
      break;
    std::string key = dictxml.substr(kstart, kend - kstart);
    trim_inline(&key);

    size_t lt = dictxml.find('<', kend + 6);
    if (lt == std::string::npos)
      break;

    std::string val;
    if (dictxml.compare(lt, 8, "<string>") == 0) {
      size_t vs = lt + 8;
      size_t ve = dictxml.find("</string>", vs);
      if (ve == std::string::npos)
        break;
      val = dictxml.substr(vs, ve - vs);
      xml_unescape_simple(&val);
    } else if (dictxml.compare(lt, 9, "<integer>") == 0) {
      size_t vs = lt + 9;
      size_t ve = dictxml.find("</integer>", vs);
      if (ve == std::string::npos)
        break;
      val = dictxml.substr(vs, ve - vs);
      trim_inline(&val);
    } else {
      p = kend + 6;
      continue;
    }

    if (key == "shortcut" || key == "replace")
      *shortcut_out = val;
    else if (key == "phrase" || key == "with")
      *phrase_out = val;

    p = kend + 6;
  }
  return !shortcut_out->empty() && !phrase_out->empty();
}

bool plist_extract_next_dict(const std::string &raw, size_t *pos_inout, std::string *dict_out) {
  size_t pos = raw.find("<dict>", *pos_inout);
  if (pos == std::string::npos)
    return false;
  size_t depth = 1;
  size_t scan = pos + 6;
  while (depth > 0 && scan < raw.size()) {
    size_t nd = raw.find("<dict>", scan);
    size_t nc = raw.find("</dict>", scan);
    if (nc == std::string::npos)
      return false;
    if (nd != std::string::npos && nd < nc) {
      depth++;
      scan = nd + 6;
    } else {
      depth--;
      if (depth == 0) {
        *dict_out = raw.substr(pos, nc + 7 - pos);
        *pos_inout = nc + 7;
        return true;
      }
      scan = nc + 7;
    }
  }
  return false;
}

bool is_binary_plist_prefix(const std::string &bytes) {
  return bytes.size() >= 8 && std::memcmp(bytes.data(), "bplist00", 8) == 0;
}

std::string xml_escape_attr(const std::string &s) {
  std::string o;
  for (unsigned char c : s) {
    if (c == '&')
      o += "&amp;";
    else if (c == '<')
      o += "&lt;";
    else if (c == '>')
      o += "&gt;";
    else if (c == '"')
      o += "&quot;";
    else
      o += (char)c;
  }
  return o;
}

} // namespace

MacroInterchangeForcedFormat PlistTextReplacementMacroHandler::forced_format() const {
  return MACRO_INTERCHANGE_FORMAT_PLIST;
}

gboolean PlistTextReplacementMacroHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                                            MacroInterchangeStatistics *stats, GError **err) {
  std::string raw;
  if (!macro_interchange::read_file_utf8(path_utf8, &raw, err))
    return FALSE;

  if (is_binary_plist_prefix(raw)) {
    macro_interchange::fail(err, "Binary property lists are not supported for macro import");
    return FALSE;
  }

  if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB &&
      (unsigned char)raw[2] == 0xBF)
    raw.erase(0, 3);

  size_t scan_pos = 0;
  std::string dict;
  while (plist_extract_next_dict(raw, &scan_pos, &dict)) {
    std::string sc, ph;
    if (plist_dict_pair_strings(dict, &sc, &ph)) {
      if (stats)
        stats->attempted++;
      const int r = table->addItem(sc.c_str(), ph.c_str(), CONV_CHARSET_UNIUTF8);
      if (r >= 0) {
        if (stats)
          stats->imported++;
      } else {
        if (stats)
          stats->skipped_malformed++;
      }
    }
  }

  return TRUE;
}

gboolean PlistTextReplacementMacroHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                                         MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  const std::vector<int> order = macro_interchange::sorted_macro_row_indices(table);
  const int n = table->getCount();

  std::ostringstream oss;
  oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  oss << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
         "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
  oss << "<plist version=\"1.0\">\n";
  oss << "<array>\n";

  for (int i = 0; i < n; i++) {
    const int ix = order[(size_t)i];
    std::string keyu, valu;
    if (!macro_interchange::utf8_from_std_keytext(table->getKey(ix), &keyu) ||
        !macro_interchange::utf8_from_std_keytext(table->getText(ix), &valu)) {
      macro_interchange::fail(err, "Could not encode macro row for plist export");
      return FALSE;
    }
    oss << "    <dict>\n";
    oss << "        <key>phrase</key>\n";
    oss << "        <string>" << xml_escape_attr(valu) << "</string>\n";
    oss << "        <key>shortcut</key>\n";
    oss << "        <string>" << xml_escape_attr(keyu) << "</string>\n";
    oss << "    </dict>\n";
  }

  oss << "</array>\n";
  oss << "</plist>\n";
  const std::string data = oss.str();
  if (!g_file_set_contents(path_utf8, data.data(), (gssize)data.size(), err))
    return FALSE;
  return TRUE;
}

namespace {

struct PlistTextReplacementMacroHandlerRegistrar {
  PlistTextReplacementMacroHandlerRegistrar() {
    MacroFormatHandlerRegistry::register_handler(
        MACRO_INTERCHANGE_FORMAT_PLIST,
        []() { return std::unique_ptr<MacroFormatHandler>(new PlistTextReplacementMacroHandler()); },
        {".plist"});
  }
};

static PlistTextReplacementMacroHandlerRegistrar g_plist_text_replacement_macro_handler_registrar;

} // namespace
