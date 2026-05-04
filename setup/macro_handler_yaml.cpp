// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file macro_handler_yaml.cpp
 * @brief Espanso-shaped YAML interchange (`matches:` / `trigger` / `replace`) via **yaml-cpp**.
 */

#include <setup/macro_handler_yaml.h>

#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>

#include <setup/macro_format_handler_registry.h>
#include <setup/macro_interchange_common.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

namespace {

/**
 * @brief Extracts a UTF-8 string from a YAML scalar node for macro trigger/phrase fields.
 * @return False if the node is missing, null, or not representable as a string.
 */
bool scalar_to_utf8(const YAML::Node &node, std::string *out) {
  if (!node.IsDefined() || node.IsNull())
    return false;
  try {
    *out = node.as<std::string>();
    return true;
  } catch (const YAML::Exception &) {
    return false;
  }
}

} // namespace

MacroInterchangeForcedFormat YamlEspansoMacroHandler::forced_format() const {
  return MACRO_INTERCHANGE_FORMAT_YAML;
}

/**
 * @brief Parses Espanso-style YAML: top-level `matches:` as a sequence of maps with `trigger` and `replace`.
 */
gboolean YamlEspansoMacroHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                                   MacroInterchangeStatistics *stats, GError **err) {
  std::string raw;
  if (!macro_interchange::read_file_utf8(path_utf8, &raw, err))
    return FALSE;

  YAML::Node root;
  try {
    root = YAML::Load(raw);
  } catch (const YAML::Exception &parse_exception) {
    macro_interchange::fail(err, parse_exception.what());
    return FALSE;
  }

  const YAML::Node matches = root["matches"];
  if (!matches.IsDefined() || !matches.IsSequence()) {
    macro_interchange::fail(err, "YAML macro interchange expects a top-level matches: sequence");
    return FALSE;
  }

  for (const YAML::Node &entry : matches) {
    if (!entry.IsMap()) {
      if (stats)
        stats->skipped_malformed++;
      continue;
    }

    std::string trigger_utf8, phrase_utf8;
    const bool got_trigger = scalar_to_utf8(entry["trigger"], &trigger_utf8);
    const bool got_phrase = scalar_to_utf8(entry["replace"], &phrase_utf8);
    if (!got_trigger || !got_phrase) {
      if (stats)
        stats->skipped_malformed++;
      continue;
    }

    const bool trigger_nonempty = !trigger_utf8.empty();
    const bool phrase_nonempty = !phrase_utf8.empty();
    if (!trigger_nonempty || !phrase_nonempty) {
      if (stats)
        stats->skipped_malformed++;
      continue;
    }

    if (stats)
      stats->attempted++;
    const int add_item_result =
        table->addItem(trigger_utf8.c_str(), phrase_utf8.c_str(), CONV_CHARSET_UNIUTF8);
    if (add_item_result >= 0) {
      if (stats)
        stats->imported++;
    } else if (stats) {
      stats->skipped_malformed++;
    }
  }

  return TRUE;
}

/**
 * @brief Writes `matches:` as a block sequence of maps with double-quoted `trigger` / `replace` scalars.
 */
gboolean YamlEspansoMacroHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                                 MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  const std::vector<int> order = macro_interchange::sorted_macro_row_indices(table);
  const int row_count = table->getCount();

  YAML::Emitter emitter;
  emitter.SetIndent(2);
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "matches";
  emitter << YAML::Value << YAML::BeginSeq;

  for (int row_index = 0; row_index < row_count; row_index++) {
    const int sorted_row_index = order[(size_t)row_index];
    std::string trigger_utf8, phrase_utf8;
    if (!macro_interchange::utf8_from_std_keytext(table->getKey(sorted_row_index), &trigger_utf8) ||
        !macro_interchange::utf8_from_std_keytext(table->getText(sorted_row_index), &phrase_utf8)) {
      macro_interchange::fail(err, "Could not encode macro row for YAML export");
      return FALSE;
    }

    emitter << YAML::BeginMap;
    emitter << YAML::Key << "trigger";
    emitter << YAML::Value << YAML::DoubleQuoted << trigger_utf8;
    emitter << YAML::Key << "replace";
    emitter << YAML::Value << YAML::DoubleQuoted << phrase_utf8;
    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;
  emitter << YAML::EndMap;

  if (!emitter.good()) {
    macro_interchange::fail(err, "Could not serialize YAML macro document");
    return FALSE;
  }

  const std::string serialized_utf8 = emitter.c_str();
  if (!g_file_set_contents(path_utf8, serialized_utf8.data(), (gssize)serialized_utf8.size(), err))
    return FALSE;
  return TRUE;
}

namespace {

struct YamlEspansoMacroHandlerRegistrar {
  YamlEspansoMacroHandlerRegistrar() {
    MacroFormatHandlerRegistry::register_handler(
        MACRO_INTERCHANGE_FORMAT_YAML,
        []() { return std::unique_ptr<MacroFormatHandler>(new YamlEspansoMacroHandler()); },
        {".yaml", ".yml"});
  }
};

static YamlEspansoMacroHandlerRegistrar g_yaml_espanso_macro_handler_registrar;

} // namespace
