// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file macro_handler_json.cpp
 * @brief Generic macro JSON interchange (array of string objects or flat string map) via json-glib.
 *
 * Import uses `JsonParser` on the full file bytes; `JsonNode` values are valid only while the parser
 * is alive, so traversal finishes before `g_object_unref(parser)`.
 */

#include <setup/macro_handler_json.h>

#include <memory>
#include <string>
#include <vector>

#include <json-glib/json-glib.h>

#include <setup/macro_format_handler_registry.h>
#include <setup/macro_interchange_common.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

namespace {

/**
 * @brief True when @a text is non-null and not the empty string (first byte is not `\\0`).
 *
 * Used for JSON map keys and for `trigger`/`content` pair completeness.
 */
bool utf8_label_nonempty(const gchar *text) {
  return text != nullptr && text[0] != '\0';
}

/**
 * @brief True when @a node is a JSON string value.
 *
 * json-glib stores scalars as `JSON_NODE_VALUE`; `G_TYPE_STRING` distinguishes strings from
 * numbers and booleans.
 *
 * @param node Candidate node (may be null).
 * @return True if @a node holds a JSON string.
 */
bool node_holds_json_string(JsonNode *node) {
  return node && JSON_NODE_HOLDS_VALUE(node) && json_node_get_value_type(node) == G_TYPE_STRING;
}

/**
 * @brief Interprets one JSON object as a macro row when both `trigger` and `content` exist as strings.
 *
 * Extra object members are ignored. If either key is present but not a string, sets `GError` and
 * returns `FALSE` (strict interchange profile).
 *
 * @param obj Object node member values.
 * @param table Destination macro table.
 * @param stats Optional import counters.
 * @param err Interchange error sink.
 * @return `FALSE` when a string-typed field is required but missing or wrong type.
 */
gboolean import_one_macro_object(JsonObject *obj, CMacroTable *table, MacroInterchangeStatistics *stats, GError **err) {
  const gchar *trigger_text = nullptr;
  const gchar *phrase_text = nullptr;

  // Progression: 1) resolve optional "trigger" member; reject if present but not a JSON string.
  if (json_object_has_member(obj, "trigger")) {
    JsonNode *trigger_member = json_object_get_member(obj, "trigger");
    if (!node_holds_json_string(trigger_member)) {
      macro_interchange::fail(err, "JSON macro values must be strings in this interchange profile");
      return FALSE;
    }
    trigger_text = json_node_get_string(trigger_member);
  }

  // Progression: 2) same for "content".
  if (json_object_has_member(obj, "content")) {
    JsonNode *content_member = json_object_get_member(obj, "content");
    if (!node_holds_json_string(content_member)) {
      macro_interchange::fail(err, "JSON macro values must be strings in this interchange profile");
      return FALSE;
    }
    phrase_text = json_node_get_string(content_member);
  }

  // Progression: 3) import only when both sides are non-empty (sparse objects are skipped, not errors).
  const bool trigger_has_text = utf8_label_nonempty(trigger_text);
  const bool phrase_has_text = utf8_label_nonempty(phrase_text);
  const bool both_have_text = trigger_has_text && phrase_has_text;
  if (both_have_text) {
    if (stats)
      stats->attempted++;
    const int add_item_result = table->addItem(trigger_text, phrase_text, CONV_CHARSET_UNIUTF8);
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
 * @brief Imports `[{ "trigger": "...", "content": "..." }, ...]`; each element must be an object.
 * @param arr Parsed root array (not owned).
 */
gboolean import_json_array(JsonArray *arr, CMacroTable *table, MacroInterchangeStatistics *stats, GError **err) {
  const guint element_count = json_array_get_length(arr);
  for (guint element_index = 0; element_index < element_count; element_index++) {
    // Progression per element: require JSON object, then parse macro fields inside it.
    JsonNode *array_element = json_array_get_element(arr, element_index);
    const bool element_is_object = JSON_NODE_HOLDS_OBJECT(array_element);
    if (!element_is_object) {
      macro_interchange::fail(err, "Invalid JSON macro array (non-object element)");
      return FALSE;
    }
    if (!import_one_macro_object(json_node_get_object(array_element), table, stats, err))
      return FALSE;
  }
  return TRUE;
}

/**
 * @brief Imports a flat `{ "shortcut": "expansion", ... }` object; values must be JSON strings.
 * @param obj Parsed root object (not owned).
 */
gboolean import_json_map(JsonObject *obj, CMacroTable *table, MacroInterchangeStatistics *stats, GError **err) {
  JsonObjectIter iter;
  const gchar *member_name = nullptr;
  JsonNode *value_node = nullptr;
  json_object_iter_init(&iter, obj);
  while (json_object_iter_next(&iter, &member_name, &value_node)) {
    // Progression: 1) every map value must be a JSON string.
    if (!node_holds_json_string(value_node)) {
      macro_interchange::fail(err, "JSON macro map values must be strings");
      return FALSE;
    }
    const gchar *phrase_text = json_node_get_string(value_node);

    // Progression: 2) skip empty keys; otherwise count and add trigger (key) / phrase (value).
    const bool map_key_has_text = utf8_label_nonempty(member_name);
    if (map_key_has_text) {
      if (stats)
        stats->attempted++;
      const int add_item_result = table->addItem(member_name, phrase_text, CONV_CHARSET_UNIUTF8);
      if (add_item_result >= 0) {
        if (stats)
          stats->imported++;
      } else if (stats) {
        stats->skipped_malformed++;
      }
    }
  }
  return TRUE;
}

} // namespace

MacroInterchangeForcedFormat JsonMacroInterchangeHandler::forced_format() const {
  return MACRO_INTERCHANGE_FORMAT_JSON;
}

/**
 * @brief Parses UTF-8 JSON from disk; root must be an array or object per `import_json_*` helpers.
 *
 * Parser parse errors are mapped into @a err via `macro_interchange::fail`. On success the parser is
 * always unreffed; borrowed `JsonNode` pointers are not used afterward.
 */
gboolean JsonMacroInterchangeHandler::import_from_path(const gchar *path_utf8, CMacroTable *table,
                                                       MacroInterchangeStatistics *stats, GError **err) {
  std::string raw;
  if (!macro_interchange::read_file_utf8(path_utf8, &raw, err))
    return FALSE;

  const bool file_has_bytes = !raw.empty();
  if (!file_has_bytes) {
    macro_interchange::fail(err, "Empty JSON macro file");
    return FALSE;
  }

  // Progression: 1) parse JSON text into an owned tree on the parser.
  JsonParser *parser = json_parser_new();
  GError *parser_error = nullptr;
  const bool parse_succeeded =
      json_parser_load_from_data(parser, raw.data(), (gssize)raw.size(), &parser_error) == TRUE;
  if (!parse_succeeded) {
    macro_interchange::fail(err, parser_error ? parser_error->message : "Could not parse JSON macro file");
    if (parser_error)
      g_error_free(parser_error);
    g_object_unref(parser);
    return FALSE;
  }

  // Progression: 2) root node must exist (defensive; empty file already rejected).
  JsonNode *document_root = json_parser_get_root(parser);
  const bool root_exists = (document_root != nullptr);
  if (!root_exists) {
    macro_interchange::fail(err, "Empty JSON macro file");
    g_object_unref(parser);
    return FALSE;
  }

  // Progression: 3) dispatch by top-level JSON kind, then destroy the parser.
  const bool root_is_array = JSON_NODE_HOLDS_ARRAY(document_root);
  const bool root_is_object = JSON_NODE_HOLDS_OBJECT(document_root);
  gboolean import_succeeded = FALSE;
  if (root_is_array)
    import_succeeded = import_json_array(json_node_get_array(document_root), table, stats, err);
  else if (root_is_object)
    import_succeeded = import_json_map(json_node_get_object(document_root), table, stats, err);
  else
    macro_interchange::fail(err, "Unsupported JSON shape for macro interchange (expected array or object)");

  g_object_unref(parser);
  return import_succeeded;
}

/**
 * @brief Builds a pretty-printed JSON array via `JsonBuilder` / `JsonGenerator`; writes with `g_file_set_contents`.
 *
 * @note Export does not use @a stats (`MACRO_INTERCHANGE_FORMAT_JSON` symmetry with other handlers).
 */
gboolean JsonMacroInterchangeHandler::export_to_path(const gchar *path_utf8, CMacroTable *table,
                                                     MacroInterchangeStatistics *stats, GError **err) {
  (void)stats;
  const std::vector<int> order = macro_interchange::sorted_macro_row_indices(table);
  const int row_count = table->getCount();

  // Progression: 1) build JSON array of { trigger, content } objects in export sort order.
  JsonBuilder *builder = json_builder_new();
  json_builder_begin_array(builder);
  for (int row_index = 0; row_index < row_count; row_index++) {
    const int sorted_row_index = order[(size_t)row_index];
    std::string trigger_utf8, phrase_utf8;
    const bool trigger_encodes_ok =
        macro_interchange::utf8_from_std_keytext(table->getKey(sorted_row_index), &trigger_utf8);
    const bool phrase_encodes_ok =
        macro_interchange::utf8_from_std_keytext(table->getText(sorted_row_index), &phrase_utf8);
    const bool row_encodes_to_utf8 = trigger_encodes_ok && phrase_encodes_ok;
    if (!row_encodes_to_utf8) {
      g_object_unref(builder);
      macro_interchange::fail(err, "Could not encode macro row for JSON export");
      return FALSE;
    }
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "trigger");
    json_builder_add_string_value(builder, trigger_utf8.c_str());
    json_builder_set_member_name(builder, "content");
    json_builder_add_string_value(builder, phrase_utf8.c_str());
    json_builder_end_object(builder);
  }
  json_builder_end_array(builder);

  // Progression: 2) turn the builder tree into UTF-8 text (pretty).
  JsonNode *document_root = json_builder_get_root(builder);
  g_object_unref(builder);
  const bool built_document_tree = (document_root != nullptr);
  if (!built_document_tree) {
    macro_interchange::fail(err, "Could not build JSON macro document");
    return FALSE;
  }

  JsonGenerator *json_generator = json_generator_new();
  json_generator_set_root(json_generator, document_root);
  json_node_unref(document_root);
  json_generator_set_pretty(json_generator, TRUE);

  gsize utf8_byte_length = 0;
  gchar *serialized_utf8 = json_generator_to_data(json_generator, &utf8_byte_length);
  g_object_unref(json_generator);

  const bool got_text_buffer = (serialized_utf8 != nullptr);
  if (!got_text_buffer) {
    macro_interchange::fail(err, "Could not serialize JSON macro file");
    return FALSE;
  }

  // Progression: 3) write bytes to path and release the serializer buffer.
  const gboolean write_ok =
      g_file_set_contents(path_utf8, serialized_utf8, (gssize)utf8_byte_length, err) ? TRUE : FALSE;
  g_free(serialized_utf8);
  return write_ok;
}

namespace {

/** Registers `JsonMacroInterchangeHandler` for `MACRO_INTERCHANGE_FORMAT_JSON` and `.json` (AUTO). */
struct JsonMacroInterchangeHandlerRegistrar {
  JsonMacroInterchangeHandlerRegistrar() {
    MacroFormatHandlerRegistry::register_handler(
        MACRO_INTERCHANGE_FORMAT_JSON,
        []() { return std::unique_ptr<MacroFormatHandler>(new JsonMacroInterchangeHandler()); },
        {".json"});
  }
};

static JsonMacroInterchangeHandlerRegistrar g_json_macro_interchange_handler_registrar;

} // namespace
