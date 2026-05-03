// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <setup/macro_handler_json.h>

#include <sstream>
#include <string>

#include <setup/macro_interchange_common.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

namespace {

void json_skip_ws(const std::string &s, size_t *i) {
  while (*i < s.size() && (s[*i] == ' ' || s[*i] == '\t' || s[*i] == '\n' || s[*i] == '\r'))
    (*i)++;
}

bool json_read_string(const std::string &s, size_t *i, std::string *out) {
  json_skip_ws(s, i);
  if (*i >= s.size() || s[*i] != '"')
    return false;
  (*i)++;
  out->clear();
  while (*i < s.size() && s[*i] != '"') {
    if (s[*i] == '\\' && *i + 1 < s.size()) {
      char n = s[*i + 1];
      if (n == 'n')
        out->push_back('\n');
      else if (n == 'r')
        out->push_back('\r');
      else if (n == 't')
        out->push_back('\t');
      else if (n == '"' || n == '\\')
        out->push_back(n);
      else
        out->push_back(n);
      *i += 2;
      continue;
    }
    out->push_back(s[*i]);
    (*i)++;
  }
  if (*i >= s.size() || s[*i] != '"')
    return false;
  (*i)++;
  return true;
}

void json_escape_string_to_stream(std::ostringstream &oss, const std::string &s) {
  oss << '"';
  for (unsigned char c : s) {
    if (c == '"' || c == '\\')
      oss << '\\';
    if (c == '\n') {
      oss << "\\n";
      continue;
    }
    if (c == '\r') {
      oss << "\\r";
      continue;
    }
    oss << (char)c;
  }
  oss << '"';
}

} // namespace

MacroInterchangeForcedFormat JsonMacroInterchangeHandler::forced_format() const {
  return MACRO_INTERCHANGE_FORMAT_JSON;
}

gboolean JsonMacroInterchangeHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                                       MacroInterchangeStatistics *stats, GError **err) {
  std::string raw;
  if (!macro_interchange::read_file_utf8(path_utf8, &raw, err))
    return FALSE;

  size_t i = 0;
  json_skip_ws(raw, &i);
  if (i >= raw.size()) {
    macro_interchange::fail(err, "Empty JSON macro file");
    return FALSE;
  }

  if (raw[i] == '[') {
    i++;
    while (true) {
      json_skip_ws(raw, &i);
      if (i < raw.size() && raw[i] == ']')
        break;
      if (i >= raw.size() || raw[i] != '{') {
        macro_interchange::fail(err, "Invalid JSON macro array");
        return FALSE;
      }
      i++;
      std::string trig, cont;
      while (true) {
        json_skip_ws(raw, &i);
        if (i < raw.size() && raw[i] == '}') {
          i++;
          break;
        }
        std::string key;
        if (!json_read_string(raw, &i, &key)) {
          macro_interchange::fail(err, "Invalid JSON macro object (expected string key)");
          return FALSE;
        }
        json_skip_ws(raw, &i);
        if (i >= raw.size() || raw[i] != ':') {
          macro_interchange::fail(err, "Invalid JSON macro object (expected ':' after key)");
          return FALSE;
        }
        i++;
        json_skip_ws(raw, &i);
        std::string val;
        if (i < raw.size() && raw[i] == '"') {
          if (!json_read_string(raw, &i, &val)) {
            macro_interchange::fail(err, "Invalid JSON macro object (expected string value)");
            return FALSE;
          }
        } else {
          macro_interchange::fail(err, "JSON macro values must be strings in this interchange profile");
          return FALSE;
        }
        if (key == "trigger")
          trig = val;
        else if (key == "content")
          cont = val;
        json_skip_ws(raw, &i);
        if (i < raw.size() && raw[i] == ',')
          i++;
      }
      if (!trig.empty() && !cont.empty()) {
        if (stats)
          stats->attempted++;
        const int r = table->addItem(trig.c_str(), cont.c_str(), CONV_CHARSET_UNIUTF8);
        if (r >= 0) {
          if (stats)
            stats->imported++;
        } else if (stats)
          stats->skipped_malformed++;
      }
      json_skip_ws(raw, &i);
      if (i < raw.size() && raw[i] == ',')
        i++;
    }
    return TRUE;
  }

  if (raw[i] == '{') {
    i++;
    while (true) {
      json_skip_ws(raw, &i);
      if (i < raw.size() && raw[i] == '}')
        break;
      std::string key;
      if (!json_read_string(raw, &i, &key)) {
        macro_interchange::fail(err, "Invalid JSON macro map (expected string key)");
        return FALSE;
      }
      json_skip_ws(raw, &i);
      if (i >= raw.size() || raw[i] != ':') {
        macro_interchange::fail(err, "Invalid JSON macro map (expected ':' after key)");
        return FALSE;
      }
      i++;
      json_skip_ws(raw, &i);
      std::string val;
      if (i < raw.size() && raw[i] == '"') {
        if (!json_read_string(raw, &i, &val)) {
          macro_interchange::fail(err, "Invalid JSON macro map (expected string value)");
          return FALSE;
        }
      } else {
        macro_interchange::fail(err, "JSON macro map values must be strings");
        return FALSE;
      }
      if (!key.empty()) {
        if (stats)
          stats->attempted++;
        const int r = table->addItem(key.c_str(), val.c_str(), CONV_CHARSET_UNIUTF8);
        if (r >= 0) {
          if (stats)
            stats->imported++;
        } else if (stats)
          stats->skipped_malformed++;
      }
      json_skip_ws(raw, &i);
      if (i < raw.size() && raw[i] == ',')
        i++;
    }
    if (i < raw.size() && raw[i] == '}')
      i++;
    return TRUE;
  }

  macro_interchange::fail(err, "Unsupported JSON shape for macro interchange (expected array or object)");
  return FALSE;
}

gboolean JsonMacroInterchangeHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                                    MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  const std::vector<int> order = macro_interchange::sorted_macro_row_indices(table);
  const int n = table->getCount();

  std::ostringstream oss;
  oss << "[\n";
  for (int i = 0; i < n; i++) {
    const int ix = order[(size_t)i];
    std::string keyu, valu;
    if (!macro_interchange::utf8_from_std_keytext(table->getKey(ix), &keyu) ||
        !macro_interchange::utf8_from_std_keytext(table->getText(ix), &valu)) {
      macro_interchange::fail(err, "Could not encode macro row for JSON export");
      return FALSE;
    }
    oss << "  {\"trigger\": ";
    json_escape_string_to_stream(oss, keyu);
    oss << ", \"content\": ";
    json_escape_string_to_stream(oss, valu);
    oss << "}";
    if (i + 1 < n)
      oss << ",";
    oss << "\n";
  }
  oss << "]\n";
  const std::string data = oss.str();
  if (!g_file_set_contents(path_utf8, data.data(), (gssize)data.size(), err))
    return FALSE;
  return TRUE;
}
