// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <setup/macro_format_handler_registry.h>

#include <map>
#include <mutex>
#include <string>

#include <setup/macro_interchange_common.h>

namespace MacroFormatHandlerRegistry {

namespace {

std::mutex &registry_mutex() {
  static std::mutex m;
  return m;
}

std::map<MacroInterchangeForcedFormat, HandlerFactory> &factories() {
  static std::map<MacroInterchangeForcedFormat, HandlerFactory> m;
  return m;
}

/** Lowercase path suffix (with leading dot) -> interchange format for AUTO. */
std::map<std::string, MacroInterchangeForcedFormat> &suffix_to_format() {
  static std::map<std::string, MacroInterchangeForcedFormat> m;
  return m;
}

} // namespace

void register_handler(MacroInterchangeForcedFormat format, HandlerFactory factory,
                      std::initializer_list<const char *> suffixes_lower) {
  std::lock_guard<std::mutex> lock(registry_mutex());
  factories()[format] = std::move(factory);
  for (const char *suf : suffixes_lower) {
    if (suf && suf[0])
      suffix_to_format()[std::string(suf)] = format;
  }
}

std::unique_ptr<MacroFormatHandler> create(MacroInterchangeForcedFormat format) {
  HandlerFactory f;
  {
    std::lock_guard<std::mutex> lock(registry_mutex());
    const auto it = factories().find(format);
    if (it == factories().end() || !it->second)
      return nullptr;
    f = it->second;
  }
  return f();
}

MacroInterchangeForcedFormat detect_from_path(const gchar *path_utf8) {
  const std::string suf = macro_interchange::path_suffix_lower(path_utf8);
  if (suf.empty())
    return MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY;

  std::lock_guard<std::mutex> lock(registry_mutex());
  const auto it = suffix_to_format().find(suf);
  if (it == suffix_to_format().end())
    return MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY;
  return it->second;
}

} // namespace MacroFormatHandlerRegistry
