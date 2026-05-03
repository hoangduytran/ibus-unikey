// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <setup/macro_handler_yaml.h>

#include <sstream>
#include <string>

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

std::string yaml_extract_scalar(const std::string &after_colon) {
  std::string s = after_colon;
  trim_inline(&s);
  if (s.empty())
    return "";
  if (s[0] == '"' || s[0] == '\'') {
    char q = s[0];
    std::string out;
    for (size_t i = 1; i < s.size(); i++) {
      if (s[i] == '\\' && i + 1 < s.size()) {
        out.push_back(s[i + 1]);
        i++;
        continue;
      }
      if (s[i] == q)
        break;
      out.push_back(s[i]);
    }
    return out;
  }
  return s;
}

bool split_yaml_key_value(const std::string &line, const char *key, std::string *value_out) {
  const std::string needle = std::string(key) + ":";
  size_t pos = line.find(needle);
  if (pos == std::string::npos)
    return false;
  size_t start = pos + needle.size();
  if (start > line.size())
    return false;
  *value_out = yaml_extract_scalar(line.substr(start));
  return true;
}

std::string yaml_escape_double(const std::string &s) {
  std::string o;
  o.push_back('"');
  for (unsigned char c : s) {
    if (c == '"' || c == '\\')
      o.push_back('\\');
    if (c == '\n') {
      o += "\\n";
      continue;
    }
    o.push_back((char)c);
  }
  o.push_back('"');
  return o;
}

} // namespace

MacroInterchangeForcedFormat YamlEspansoMacroHandler::forced_format() const {
  return MACRO_INTERCHANGE_FORMAT_YAML;
}

gboolean YamlEspansoMacroHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                                  MacroInterchangeStatistics *stats, GError **err) {
  std::string raw;
  if (!macro_interchange::read_file_utf8(path_utf8, &raw, err))
    return FALSE;

  std::istringstream iss(raw);
  std::string line;
  enum { SEEK_MATCHES, IN_MATCHES } phase = SEEK_MATCHES;
  std::string pending_trigger;

  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    std::string t = line;
    trim_inline(&t);

    if (phase == SEEK_MATCHES) {
      if (t.rfind("matches:", 0) == 0)
        phase = IN_MATCHES;
      continue;
    }

    if (phase == IN_MATCHES) {
      if (t.find("- trigger:") != std::string::npos || t.find("-trigger:") != std::string::npos) {
        std::string v;
        if (split_yaml_key_value(line, "trigger", &v)) {
          pending_trigger = v;
          if (stats)
            stats->attempted++;
        } else {
          pending_trigger.clear();
          if (stats)
            stats->skipped_malformed++;
        }
        continue;
      }
      if (!pending_trigger.empty() && t.rfind("replace:", 0) == 0) {
        std::string rep;
        if (split_yaml_key_value(line, "replace", &rep)) {
          const int r = table->addItem(pending_trigger.c_str(), rep.c_str(), CONV_CHARSET_UNIUTF8);
          if (r >= 0) {
            if (stats)
              stats->imported++;
          } else {
            if (stats)
              stats->skipped_malformed++;
          }
          pending_trigger.clear();
        }
        continue;
      }
    }
  }

  if (phase != IN_MATCHES) {
    macro_interchange::fail(err, "YAML macro interchange expects a top-level matches: block");
    return FALSE;
  }

  return TRUE;
}

gboolean YamlEspansoMacroHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                                 MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  const std::vector<int> order = macro_interchange::sorted_macro_row_indices(table);
  const int n = table->getCount();

  std::ostringstream oss;
  oss << "matches:\n";
  for (int i = 0; i < n; i++) {
    const int ix = order[(size_t)i];
    std::string keyu, valu;
    if (!macro_interchange::utf8_from_std_keytext(table->getKey(ix), &keyu) ||
        !macro_interchange::utf8_from_std_keytext(table->getText(ix), &valu)) {
      macro_interchange::fail(err, "Could not encode macro row for YAML export");
      return FALSE;
    }
    oss << "  - trigger: " << yaml_escape_double(keyu) << "\n";
    oss << "    replace: " << yaml_escape_double(valu) << "\n";
    oss << "\n";
  }

  const std::string data = oss.str();
  if (!g_file_set_contents(path_utf8, data.data(), (gssize)data.size(), err))
    return FALSE;
  return TRUE;
}
