// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/**
 * @file macro_file_io.cpp
 * @brief Macro interchange import/export dispatch (YAML, plist, JSON, CSV/TSV, UniKey text).
 */

#include <setup/macro_file_io.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/text_macro_format.h>
#include <ukengine/mapping/vnconv.h>

namespace {

#define STD_TO_LOWER(x)                                                                            \
  (((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && !((x) & 1))         \
       ? (x + 1)                                                                                   \
       : (x))

int compareStdVnKeys(const StdVnChar *a, const StdVnChar *b) {
  int i = 0;
  StdVnChar ls1, ls2;
  for (;; i++) {
    ls1 = STD_TO_LOWER(a[i]);
    ls2 = STD_TO_LOWER(b[i]);
    if (ls1 > ls2)
      return 1;
    if (ls1 < ls2)
      return -1;
    if (a[i] == 0)
      return (b[i] == 0) ? 0 : -1;
  }
}

bool vnStdToUtf8Grow(const StdVnChar *src, std::vector<char> &buf, bool *ok) {
  *ok = false;
  buf.resize(256);
  for (int attempt = 0; attempt < 24; attempt++) {
    int inLen = -1;
    int maxOut = (int)buf.size();
    int ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8, (UKBYTE *)src, (UKBYTE *)buf.data(),
                        &inLen, &maxOut);
    if (ret == 0) {
      buf.resize((size_t)maxOut);
      *ok = true;
      return true;
    }
    if (buf.size() > (size_t)64 * 1024 * 1024)
      return false;
    buf.resize(buf.size() * 2);
  }
  return false;
}

static GQuark interchange_error_quark(void) {
  return g_quark_from_static_string("ibus-unikey-macro-interchange-error");
}

static void interchange_fail(GError **err, const char *msg) {
  if (err)
    g_set_error(err, interchange_error_quark(), 0, "%s", msg);
}

static std::string path_suffix_lower(const gchar *path) {
  if (!path)
    return "";
  const char *dot = strrchr(path, '.');
  if (!dot)
    return "";
  std::string s(dot);
  for (char &c : s)
    c = (char)tolower((unsigned char)c);
  return s;
}

static bool read_file_utf8(const gchar *path, std::string *out, GError **err) {
  gchar *contents = nullptr;
  gsize len = 0;
  if (!g_file_get_contents(path, &contents, &len, err))
    return false;
  out->assign(contents, len);
  g_free(contents);
  return true;
}

static bool is_binary_plist_prefix(const std::string &bytes) {
  return bytes.size() >= 8 && std::memcmp(bytes.data(), "bplist00", 8) == 0;
}

static void trim_inline(std::string *s) {
  while (!s->empty() && ((*s)[0] == ' ' || (*s)[0] == '\t'))
    s->erase(s->begin());
  while (!s->empty() && (s->back() == ' ' || s->back() == '\t' || s->back() == '\r'))
    s->pop_back();
}

static std::string yaml_extract_scalar(const std::string &after_colon) {
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

static bool split_yaml_key_value(const std::string &line, const char *key, std::string *value_out) {
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

static gboolean load_yaml_espanso(const gchar *path, CMacroTable *table, MacroInterchangeStatistics *stats,
                                  GError **err) {
  std::string raw;
  if (!read_file_utf8(path, &raw, err))
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
    interchange_fail(err, "YAML macro interchange expects a top-level matches: block");
    return FALSE;
  }

  return TRUE;
}

static void xml_unescape_simple(std::string *s) {
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

static bool plist_dict_pair_strings(const std::string &dictxml, std::string *shortcut_out,
                                    std::string *phrase_out) {
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

static bool plist_extract_next_dict(const std::string &raw, size_t *pos_inout, std::string *dict_out) {
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

static gboolean load_plist_xml_triggers(const gchar *path, CMacroTable *table, MacroInterchangeStatistics *stats,
                                        GError **err) {
  std::string raw;
  if (!read_file_utf8(path, &raw, err))
    return FALSE;

  if (is_binary_plist_prefix(raw)) {
    interchange_fail(err, "Binary property lists are not supported for macro import");
    return FALSE;
  }

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

static void json_skip_ws(const std::string &s, size_t *i) {
  while (*i < s.size() && (s[*i] == ' ' || s[*i] == '\t' || s[*i] == '\n' || s[*i] == '\r'))
    (*i)++;
}

static bool json_read_string(const std::string &s, size_t *i, std::string *out) {
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

static gboolean load_json_interchange(const gchar *path, CMacroTable *table, MacroInterchangeStatistics *stats,
                                      GError **err) {
  std::string raw;
  if (!read_file_utf8(path, &raw, err))
    return FALSE;

  size_t i = 0;
  json_skip_ws(raw, &i);
  if (i >= raw.size()) {
    interchange_fail(err, "Empty JSON macro file");
    return FALSE;
  }

  if (raw[i] == '[') {
    i++;
    while (true) {
      json_skip_ws(raw, &i);
      if (i < raw.size() && raw[i] == ']')
        break;
      if (i >= raw.size() || raw[i] != '{') {
        interchange_fail(err, "Invalid JSON macro array");
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
        if (!json_read_string(raw, &i, &key))
          return FALSE;
        json_skip_ws(raw, &i);
        if (i >= raw.size() || raw[i] != ':')
          return FALSE;
        i++;
        json_skip_ws(raw, &i);
        std::string val;
        if (i < raw.size() && raw[i] == '"') {
          if (!json_read_string(raw, &i, &val))
            return FALSE;
        } else {
          interchange_fail(err, "JSON macro values must be strings in this interchange profile");
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
      if (!json_read_string(raw, &i, &key))
        return FALSE;
      json_skip_ws(raw, &i);
      if (i >= raw.size() || raw[i] != ':')
        return FALSE;
      i++;
      json_skip_ws(raw, &i);
      std::string val;
      if (i < raw.size() && raw[i] == '"') {
        if (!json_read_string(raw, &i, &val))
          return FALSE;
      } else {
        interchange_fail(err, "JSON macro map values must be strings");
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

  interchange_fail(err, "Unsupported JSON shape for macro interchange (expected array or object)");
  return FALSE;
}

static void csv_unescape_field(std::string *f) {
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

static gboolean load_csv_like(const gchar *path, CMacroTable *table, char delim, MacroInterchangeStatistics *stats,
                               GError **err) {
  std::string raw;
  if (!read_file_utf8(path, &raw, err))
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
        if (c == delim) {
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

    std::string trig = fields[tri_col];
    std::string cont = fields[con_col];
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

static MacroInterchangeForcedFormat detect_from_path(const gchar *path) {
  const std::string suf = path_suffix_lower(path);
  if (suf == ".yaml" || suf == ".yml")
    return MACRO_INTERCHANGE_FORMAT_YAML;
  if (suf == ".plist")
    return MACRO_INTERCHANGE_FORMAT_PLIST;
  if (suf == ".json")
    return MACRO_INTERCHANGE_FORMAT_JSON;
  if (suf == ".csv")
    return MACRO_INTERCHANGE_FORMAT_CSV;
  if (suf == ".tsv")
    return MACRO_INTERCHANGE_FORMAT_TSV;
  return MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY;
}

static std::string xml_escape_attr(const std::string &s) {
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

static std::string yaml_escape_double(const std::string &s) {
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

static gboolean export_text_unikey(const gchar *path, CMacroTable *table, GError **err) {
  TextMacroFormat fmt;
  if (fmt.exportToPath(path, *table) != 1) {
    const char *d = table->getLastErrorMessage();
    interchange_fail(err, (d && d[0]) ? d : "Text macro export failed");
    return FALSE;
  }
  return TRUE;
}

static gboolean utf8_from_std_keytext(const StdVnChar *vn, std::string *utf8_out) {
  std::vector<char> buf;
  bool ok = false;
  if (!vnStdToUtf8Grow(vn, buf, &ok) || !ok)
    return false;
  while (!buf.empty() && buf.back() == '\0')
    buf.pop_back();
  utf8_out->assign(buf.data(), buf.size());
  return true;
}

static gboolean export_yaml_espanso(const gchar *path, CMacroTable *table, GError **err) {
  const int n = table->getCount();
  std::vector<int> order((size_t)n);
  for (int i = 0; i < n; i++)
    order[(size_t)i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return compareStdVnKeys(table->getKey(a), table->getKey(b)) < 0;
  });

  std::ostringstream oss;
  oss << "matches:\n";
  for (int i = 0; i < n; i++) {
    const int ix = order[(size_t)i];
    std::string keyu, valu;
    if (!utf8_from_std_keytext(table->getKey(ix), &keyu) ||
        !utf8_from_std_keytext(table->getText(ix), &valu)) {
      interchange_fail(err, "Could not encode macro row for YAML export");
      return FALSE;
    }
    oss << "  - trigger: " << yaml_escape_double(keyu) << "\n";
    oss << "    replace: " << yaml_escape_double(valu) << "\n";
    oss << "\n";
  }

  const std::string data = oss.str();
  if (!g_file_set_contents(path, data.data(), (gssize)data.size(), err))
    return FALSE;
  return TRUE;
}

static gboolean export_plist_xml(const gchar *path, CMacroTable *table, GError **err) {
  const int n = table->getCount();
  std::vector<int> order((size_t)n);
  for (int i = 0; i < n; i++)
    order[(size_t)i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return compareStdVnKeys(table->getKey(a), table->getKey(b)) < 0;
  });

  std::ostringstream oss;
  oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  oss << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
         "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
  oss << "<plist version=\"1.0\">\n<array>\n";

  for (int i = 0; i < n; i++) {
    const int ix = order[(size_t)i];
    std::string keyu, valu;
    if (!utf8_from_std_keytext(table->getKey(ix), &keyu) ||
        !utf8_from_std_keytext(table->getText(ix), &valu)) {
      interchange_fail(err, "Could not encode macro row for plist export");
      return FALSE;
    }
    oss << "<dict>\n";
    oss << "  <key>phrase</key>\n";
    oss << "  <string>" << xml_escape_attr(valu) << "</string>\n";
    oss << "  <key>shortcut</key>\n";
    oss << "  <string>" << xml_escape_attr(keyu) << "</string>\n";
    oss << "</dict>\n";
  }

  oss << "</array>\n</plist>\n";
  const std::string data = oss.str();
  if (!g_file_set_contents(path, data.data(), (gssize)data.size(), err))
    return FALSE;
  return TRUE;
}

static void json_escape_string_to_stream(std::ostringstream &oss, const std::string &s) {
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

static gboolean export_json_generic(const gchar *path, CMacroTable *table, GError **err) {
  const int n = table->getCount();
  std::vector<int> order((size_t)n);
  for (int i = 0; i < n; i++)
    order[(size_t)i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return compareStdVnKeys(table->getKey(a), table->getKey(b)) < 0;
  });

  std::ostringstream oss;
  oss << "[\n";
  for (int i = 0; i < n; i++) {
    const int ix = order[(size_t)i];
    std::string keyu, valu;
    if (!utf8_from_std_keytext(table->getKey(ix), &keyu) ||
        !utf8_from_std_keytext(table->getText(ix), &valu)) {
      interchange_fail(err, "Could not encode macro row for JSON export");
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
  if (!g_file_set_contents(path, data.data(), (gssize)data.size(), err))
    return FALSE;
  return TRUE;
}

static gboolean export_csv(const gchar *path, CMacroTable *table, char delim, GError **err) {
  const int n = table->getCount();
  std::vector<int> order((size_t)n);
  for (int i = 0; i < n; i++)
    order[(size_t)i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return compareStdVnKeys(table->getKey(a), table->getKey(b)) < 0;
  });

  std::ostringstream oss;
  oss << "trigger" << delim << "content\n";
  for (int i = 0; i < n; i++) {
    const int ix = order[(size_t)i];
    std::string keyu, valu;
    if (!utf8_from_std_keytext(table->getKey(ix), &keyu) ||
        !utf8_from_std_keytext(table->getText(ix), &valu)) {
      interchange_fail(err, "Could not encode macro row for CSV export");
      return FALSE;
    }
    auto quote = [&](const std::string &s) -> std::string {
      bool need = s.find_first_of(",\"\n\r") != std::string::npos || (delim != ',' && s.find(delim) != std::string::npos);
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
    oss << quote(keyu) << delim << quote(valu) << "\n";
  }
  const std::string data = oss.str();
  if (!g_file_set_contents(path, data.data(), (gssize)data.size(), err))
    return FALSE;
  return TRUE;
}

} // namespace

gboolean macro_table_load_any_format(const gchar *filename, CMacroTable *table, GError **error) {
  return macro_interchange_import_path(filename, table, MACRO_INTERCHANGE_FORMAT_AUTO, nullptr, error);
}

gboolean macro_interchange_import_path(const gchar *filename, CMacroTable *table,
                                       MacroInterchangeForcedFormat forced, MacroInterchangeStatistics *stats,
                                       GError **err) {
  if (!filename || !table) {
    interchange_fail(err, "Invalid macro interchange import arguments");
    return FALSE;
  }

  if (stats)
    memset(stats, 0, sizeof(*stats));

  table->resetContent();

  MacroInterchangeForcedFormat fmt = forced;
  if (fmt == MACRO_INTERCHANGE_FORMAT_AUTO)
    fmt = detect_from_path(filename);

  switch (fmt) {
  case MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY: {
    TextMacroFormat tf;
    int ver = 0;
    if (tf.importFromPath(filename, *table, &ver) != 1) {
      interchange_fail(err, table->getLastErrorMessage());
      return FALSE;
    }
    if (stats) {
      stats->imported = table->getCount();
      stats->attempted = stats->imported;
    }
    return TRUE;
  }
  case MACRO_INTERCHANGE_FORMAT_YAML:
    return load_yaml_espanso(filename, table, stats, err);
  case MACRO_INTERCHANGE_FORMAT_PLIST:
    return load_plist_xml_triggers(filename, table, stats, err);
  case MACRO_INTERCHANGE_FORMAT_JSON:
    return load_json_interchange(filename, table, stats, err);
  case MACRO_INTERCHANGE_FORMAT_CSV:
    return load_csv_like(filename, table, ',', stats, err);
  case MACRO_INTERCHANGE_FORMAT_TSV:
    return load_csv_like(filename, table, '\t', stats, err);
  default:
    interchange_fail(err, "Unsupported macro interchange format");
    return FALSE;
  }
}

gboolean macro_interchange_export_path(const gchar *filename, CMacroTable *table,
                                       MacroInterchangeForcedFormat forced, MacroInterchangeStatistics *stats,
                                       GError **err) {
  if (!filename || !table) {
    interchange_fail(err, "Invalid macro interchange export arguments");
    return FALSE;
  }

  if (stats)
    memset(stats, 0, sizeof(*stats));

  MacroInterchangeForcedFormat fmt = forced;
  if (fmt == MACRO_INTERCHANGE_FORMAT_AUTO)
    fmt = detect_from_path(filename);

  gboolean ok = FALSE;
  switch (fmt) {
  case MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY:
    ok = export_text_unikey(filename, table, err);
    break;
  case MACRO_INTERCHANGE_FORMAT_YAML:
    ok = export_yaml_espanso(filename, table, err);
    break;
  case MACRO_INTERCHANGE_FORMAT_PLIST:
    ok = export_plist_xml(filename, table, err);
    break;
  case MACRO_INTERCHANGE_FORMAT_JSON:
    ok = export_json_generic(filename, table, err);
    break;
  case MACRO_INTERCHANGE_FORMAT_CSV:
    ok = export_csv(filename, table, ',', err);
    break;
  case MACRO_INTERCHANGE_FORMAT_TSV:
    ok = export_csv(filename, table, '\t', err);
    break;
  default:
    interchange_fail(err, "Unsupported macro interchange export format");
    return FALSE;
  }

  if (ok && stats) {
    stats->imported = table->getCount();
    stats->attempted = stats->imported;
  }
  return ok;
}

void macro_file_chooser_attach_import_filters(GtkFileChooser *chooser) {
  GtkFileFilter *all = gtk_file_filter_new();
  gtk_file_filter_set_name(all, _("All supported formats"));
  gtk_file_filter_add_pattern(all, "*.txt");
  gtk_file_filter_add_pattern(all, "*.macro");
  gtk_file_filter_add_pattern(all, "*.json");
  gtk_file_filter_add_pattern(all, "*.yaml");
  gtk_file_filter_add_pattern(all, "*.yml");
  gtk_file_filter_add_pattern(all, "*.plist");
  gtk_file_filter_add_pattern(all, "*.csv");
  gtk_file_filter_add_pattern(all, "*.tsv");
  gtk_file_chooser_add_filter(chooser, all);

  GtkFileFilter *uni = gtk_file_filter_new();
  gtk_file_filter_set_name(uni, _("UniKey macro text"));
  gtk_file_filter_add_pattern(uni, "*.txt");
  gtk_file_filter_add_pattern(uni, "*.macro");
  gtk_file_chooser_add_filter(chooser, uni);

  GtkFileFilter *json = gtk_file_filter_new();
  gtk_file_filter_set_name(json, _("JSON macros"));
  gtk_file_filter_add_pattern(json, "*.json");
  gtk_file_chooser_add_filter(chooser, json);

  GtkFileFilter *yaml = gtk_file_filter_new();
  gtk_file_filter_set_name(yaml, _("YAML macros"));
  gtk_file_filter_add_pattern(yaml, "*.yaml");
  gtk_file_filter_add_pattern(yaml, "*.yml");
  gtk_file_chooser_add_filter(chooser, yaml);

  GtkFileFilter *plist = gtk_file_filter_new();
  gtk_file_filter_set_name(plist, _("macOS plist (text replacements)"));
  gtk_file_filter_add_pattern(plist, "*.plist");
  gtk_file_chooser_add_filter(chooser, plist);

  GtkFileFilter *csv = gtk_file_filter_new();
  gtk_file_filter_set_name(csv, _("CSV macros"));
  gtk_file_filter_add_pattern(csv, "*.csv");
  gtk_file_chooser_add_filter(chooser, csv);

  GtkFileFilter *tsv = gtk_file_filter_new();
  gtk_file_filter_set_name(tsv, _("TSV macros"));
  gtk_file_filter_add_pattern(tsv, "*.tsv");
  gtk_file_chooser_add_filter(chooser, tsv);

  gtk_file_chooser_set_filter(chooser, all);
}

void macro_file_chooser_attach_export_filters(GtkFileChooser *chooser) {
  macro_file_chooser_attach_import_filters(chooser);
}
