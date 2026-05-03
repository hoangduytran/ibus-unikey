// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file macro_format_handler_registry.cpp
 * @brief Implements `MacroFormatHandlerRegistry`: static factories, suffix routing, and mutex-backed access.
 *
 * Registration runs from static initializers in each `macro_handler_*.cpp`; the mutex serializes concurrent
 * `register_handler` / `create` / `detect_from_path` calls (e.g. tests or future dynamic registration).
 */

#include <setup/macro_format_handler_registry.h>

#include <map>
#include <mutex>
#include <string>

#include <setup/macro_interchange_common.h>

namespace MacroFormatHandlerRegistry {

namespace {

/**
 * @brief Process-wide mutex guarding @ref factories and @ref suffix_to_format.
 * @return Reference to the singleton mutex.
 */
std::mutex &registry_mutex() {
  static std::mutex registry_mutex_instance;
  return registry_mutex_instance;
}

/**
 * @brief Map from interchange format enum to factory callable (one entry per registered codec).
 * @return Reference to the static map.
 */
std::map<MacroInterchangeForcedFormat, HandlerFactory> &factories() {
  static std::map<MacroInterchangeForcedFormat, HandlerFactory> factory_map;
  return factory_map;
}

/**
 * @brief Maps lowercase path suffix (including leading dot) to a forced format for `AUTO` dispatch.
 * @return Reference to the static map.
 */
std::map<std::string, MacroInterchangeForcedFormat> &suffix_to_format() {
  static std::map<std::string, MacroInterchangeForcedFormat> suffix_map;
  return suffix_map;
}

} // namespace

/**
 * @copydoc MacroFormatHandlerRegistry::register_handler
 *
 * @details Holds `registry_mutex` for the full update. Empty or null suffix entries are skipped.
 * Last registration wins for a given @a format; suffix keys are overwritten if two handlers register
 * the same extension (avoid duplicate suffixes across handlers).
 */
void register_handler(MacroInterchangeForcedFormat format, HandlerFactory factory,
                      std::initializer_list<const char *> suffixes_lower) {
  std::lock_guard<std::mutex> lock(registry_mutex());
  factories()[format] = std::move(factory);
  for (const char *extension_suffix : suffixes_lower) {
    const bool suffix_nonempty = extension_suffix && extension_suffix[0];
    if (suffix_nonempty)
      suffix_to_format()[std::string(extension_suffix)] = format;
  }
}

/**
 * @copydoc MacroFormatHandlerRegistry::create
 *
 * @details Looks up the factory under the mutex, then invokes it **outside** the lock so handler
 * construction cannot deadlock if a factory re-enters the registry.
 */
std::unique_ptr<MacroFormatHandler> create(MacroInterchangeForcedFormat format) {
  HandlerFactory handler_factory;
  {
    std::lock_guard<std::mutex> lock(registry_mutex());
    const auto registered = factories().find(format);
    const bool format_registered = registered != factories().end() && registered->second;
    if (!format_registered)
      return nullptr;
    handler_factory = registered->second;
  }
  return handler_factory();
}

/**
 * @copydoc MacroFormatHandlerRegistry::detect_from_path
 *
 * @details Uses `macro_interchange::path_suffix_lower`. No extension or unknown suffix yields
 * `MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY` (native UniKey macro text).
 */
MacroInterchangeForcedFormat detect_from_path(const gchar *path_utf8) {
  const std::string lower_suffix = macro_interchange::path_suffix_lower(path_utf8);
  if (lower_suffix.empty())
    return MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY;

  std::lock_guard<std::mutex> lock(registry_mutex());
  const auto suffix_match = suffix_to_format().find(lower_suffix);
  const bool suffix_known = suffix_match != suffix_to_format().end();
  if (!suffix_known)
    return MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY;
  return suffix_match->second;
}

} // namespace MacroFormatHandlerRegistry
