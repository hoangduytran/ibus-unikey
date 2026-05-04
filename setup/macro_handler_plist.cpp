// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file macro_handler_plist.cpp
 * @brief Import and export macro tables using Apple property lists (macOS text replacements).
 *
 * Uses libplist for XML and binary plist parsing and for XML serialization on export.
 * Shortcut/phrase keys mirror common macOS naming (`shortcut`/`phrase` plus `replace`/`with` aliases).
 */

#include <setup/macro_handler_plist.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include <plist/plist.h>

#include <setup/macro_format_handler_registry.h>
#include <setup/macro_interchange_common.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

namespace {

/**
 * @brief Detects the binary plist magic prefix used by Apple and libplist.
 *
 * XML plists are parsed separately; this avoids mis-routing UTF-8/XML text that happens to contain
 * similar bytes at the start of the buffer.
 */
bool is_binary_plist_prefix(const std::string &bytes) {
  return bytes.size() >= 8 && std::memcmp(bytes.data(), "bplist00", 8) == 0;
}

/**
 * @brief Converts a plist scalar node to UTF-8 text for macro shortcut or phrase fields.
 *
 * Integer and real nodes are stringified so legacy plist exports that use `<integer>` still load.
 *
 * @param node Property list node (must not be null for a successful conversion).
 * @param out Receives decoded text on success.
 * @return True if @a node was a supported scalar type; false if the type cannot be represented as text here.
 */
bool node_to_utf8_text(plist_t node, std::string *out) {
  if (!node)
    return false;
  const plist_type node_type = plist_get_node_type(node);
  if (node_type == PLIST_STRING) {
    uint64_t byte_length = 0;
    const char *string_ptr = plist_get_string_ptr(node, &byte_length);
    if (string_ptr)
      out->assign(string_ptr, byte_length);
    else
      out->clear();
    return true;
  }
  if (node_type == PLIST_INT || node_type == PLIST_UINT) {
    int64_t signed_int_value = 0;
    plist_get_int_val(node, &signed_int_value);
    *out = std::to_string(signed_int_value);
    return true;
  }
  if (node_type == PLIST_REAL) {
    double real_value = 0;
    plist_get_real_val(node, &real_value);
    std::ostringstream oss;
    oss << real_value;
    *out = oss.str();
    return true;
  }
  return false;
}

/**
 * @brief Reads shortcut and phrase strings from one plist dictionary entry.
 *
 * Accepts macOS field names and common synonyms; both fields must decode to non-empty UTF-8.
 *
 * @param dict Dictionary node.
 * @param shortcut Output shortcut (trigger) text.
 * @param phrase Output expansion text.
 * @return True if both fields were found, decoded, and are non-empty.
 */
bool dict_get_macro_pair(plist_t dict, std::string *shortcut, std::string *phrase) {
  shortcut->clear();
  phrase->clear();
  if (!dict || plist_get_node_type(dict) != PLIST_DICT)
    return false;

  plist_t shortcut_node = plist_dict_get_item(dict, "shortcut");
  if (!shortcut_node)
    shortcut_node = plist_dict_get_item(dict, "replace");
  plist_t phrase_node = plist_dict_get_item(dict, "phrase");
  if (!phrase_node)
    phrase_node = plist_dict_get_item(dict, "with");
  if (!shortcut_node || !phrase_node)
    return false;

  if (!node_to_utf8_text(shortcut_node, shortcut) || !node_to_utf8_text(phrase_node, phrase))
    return false;
  return !shortcut->empty() && !phrase->empty();
}

/**
 * @brief Walks the plist root and appends matching entries into @a table.
 *
 * Supports a root array of replacement dicts (standard macOS export) or a single root dict for a
 * one-row file. Dicts that lack a usable pair are skipped without failing the whole import.
 *
 * @param root Parsed plist root; ownership remains with the caller until plist_free.
 * @param table Macro table to fill (already cleared by the importer).
 * @param stats Stats counters when non-null; attempted/imported/skipped are updated like other handlers.
 * @return False if @a root is neither an array nor a dictionary.
 */
bool import_macros_from_plist_root(plist_t root, CMacroTable *table, MacroInterchangeStatistics *stats) {
  if (PLIST_IS_DICT(root)) {
    std::string shortcut_text, phrase_text;
    if (!dict_get_macro_pair(root, &shortcut_text, &phrase_text))
      return true;
    if (stats)
      stats->attempted++;
    const int add_item_result =
        table->addItem(shortcut_text.c_str(), phrase_text.c_str(), CONV_CHARSET_UNIUTF8);
    if (add_item_result >= 0) {
      if (stats)
        stats->imported++;
    } else {
      if (stats)
        stats->skipped_malformed++;
    }
    return true;
  }

  if (!PLIST_IS_ARRAY(root))
    return false;

  const uint32_t entry_count = plist_array_get_size(root);
  for (uint32_t entry_index = 0; entry_index < entry_count; entry_index++) {
    plist_t array_item = plist_array_get_item(root, entry_index);
    std::string shortcut_text, phrase_text;
    if (!dict_get_macro_pair(array_item, &shortcut_text, &phrase_text))
      continue;
    if (stats)
      stats->attempted++;
    const int add_item_result =
        table->addItem(shortcut_text.c_str(), phrase_text.c_str(), CONV_CHARSET_UNIUTF8);
    if (add_item_result >= 0) {
      if (stats)
        stats->imported++;
    } else {
      if (stats)
        stats->skipped_malformed++;
    }
  }
  return true;
}

} // namespace

MacroInterchangeForcedFormat PlistTextReplacementMacroHandler::forced_format() const {
  return MACRO_INTERCHANGE_FORMAT_PLIST;
}

/**
 * @brief Loads replacement shortcuts from an XML or binary plist file.
 *
 * libplist APIs take a uint32 length; oversize files are rejected. On success the parsed tree is
 * freed before return; on parse error @a err is set and any partial tree is released.
 */
gboolean PlistTextReplacementMacroHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                                            MacroInterchangeStatistics *stats, GError **err) {
  std::string raw;
  if (!macro_interchange::read_file_utf8(path_utf8, &raw, err))
    return FALSE;

  if (raw.size() > (size_t)UINT32_MAX) {
    macro_interchange::fail(err, "Property list is too large to load");
    return FALSE;
  }

  // Strip UTF-8 BOM so plist_from_xml sees a decl or plist element first.
  if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB &&
      (unsigned char)raw[2] == 0xBF)
    raw.erase(0, 3);

  plist_t root = nullptr;
  plist_err_t plist_error;
  if (is_binary_plist_prefix(raw))
    plist_error = plist_from_bin(raw.data(), (uint32_t)raw.size(), &root);
  else
    plist_error = plist_from_xml(raw.data(), (uint32_t)raw.size(), &root);

  if (plist_error != PLIST_ERR_SUCCESS || !root) {
    if (root)
      plist_free(root);
    macro_interchange::fail(err, "Could not parse property list (XML or binary)");
    return FALSE;
  }

  if (!import_macros_from_plist_root(root, table, stats)) {
    plist_free(root);
    macro_interchange::fail(err,
                            "Plist macro file must contain an array (or one replacement dictionary) at the root");
    return FALSE;
  }

  plist_free(root);
  return TRUE;
}

/**
 * @brief Writes a UTF-8 XML plist with an array of replacement dicts (phrase + shortcut).
 *
 * Row order matches other exporters via `sorted_macro_row_indices`. The plist tree owns child nodes;
 * `plist_to_xml` allocates @a xml_out with malloc; caller must free that buffer after writing the file.
 */
gboolean PlistTextReplacementMacroHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                                         MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  const std::vector<int> order = macro_interchange::sorted_macro_row_indices(table);
  const int row_count = table->getCount();

  plist_t root = plist_new_array();
  if (!root) {
    macro_interchange::fail(err, "Could not allocate plist for export");
    return FALSE;
  }

  for (int row_index = 0; row_index < row_count; row_index++) {
    const int sorted_row_index = order[(size_t)row_index];
    std::string trigger_utf8, phrase_utf8;
    if (!macro_interchange::utf8_from_std_keytext(table->getKey(sorted_row_index), &trigger_utf8) ||
        !macro_interchange::utf8_from_std_keytext(table->getText(sorted_row_index), &phrase_utf8)) {
      plist_free(root);
      macro_interchange::fail(err, "Could not encode macro row for plist export");
      return FALSE;
    }

    plist_t entry = plist_new_dict();
    if (!entry) {
      plist_free(root);
      macro_interchange::fail(err, "Could not allocate plist dictionary for export");
      return FALSE;
    }
    plist_dict_set_item(entry, "phrase", plist_new_string(phrase_utf8.c_str()));
    plist_dict_set_item(entry, "shortcut", plist_new_string(trigger_utf8.c_str()));
    plist_array_append_item(root, entry);
  }

  char *xml_out = nullptr;
  uint32_t xml_len = 0;
  const plist_err_t serialize_error = plist_to_xml(root, &xml_out, &xml_len);
  plist_free(root);
  root = nullptr;

  if (serialize_error != PLIST_ERR_SUCCESS || !xml_out) {
    if (xml_out)
      free(xml_out);
    macro_interchange::fail(err, "Could not serialize property list to XML");
    return FALSE;
  }

  const gboolean write_succeeded = g_file_set_contents(path_utf8, xml_out, (gssize)xml_len, err) ? TRUE : FALSE;
  free(xml_out);
  return write_succeeded;
}

namespace {

/** Registers `PlistTextReplacementMacroHandler` for `.plist` and `MACRO_INTERCHANGE_FORMAT_PLIST`. */
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
