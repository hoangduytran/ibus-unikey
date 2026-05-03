// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_FORMAT_HANDLER_REGISTRY_H
#define IBUS_UNIKEY_MACRO_FORMAT_HANDLER_REGISTRY_H

#include <functional>
#include <initializer_list>
#include <memory>
#include <string>

#include <glib.h>

#include <setup/macro_file_io.h>
#include <setup/macro_format_handler.h>

/**
 * @brief Factory/registry for macro interchange handlers (polymorphic dispatch).
 *
 * Each built-in format registers a factory and path suffix hints (lowercase,
 * including the dot, e.g. `.yaml`) from its own translation unit. Call
 * `register_handler()` again at runtime to replace a factory (swap codec) or
 * register a new `MacroInterchangeForcedFormat` value plus suffixes without
 * changing `macro_file_io.cpp`.
 *
 * When linking `macro_interchange` as a **static** library, consumers must pull
 * every object file (e.g. `-Wl,--whole-archive` on GNU ld or `-force_load` on
 * macOS) so static registrars run before `main`; see `setup/CMakeLists.txt`.
 */
namespace MacroFormatHandlerRegistry {

using HandlerFactory = std::function<std::unique_ptr<MacroFormatHandler>()>;

/** Register or replace the factory for @a format and map @a suffixes_lower to it for AUTO. */
void register_handler(MacroInterchangeForcedFormat format, HandlerFactory factory,
                      std::initializer_list<const char *> suffixes_lower);

std::unique_ptr<MacroFormatHandler> create(MacroInterchangeForcedFormat format);

MacroInterchangeForcedFormat detect_from_path(const gchar *path_utf8);

} // namespace MacroFormatHandlerRegistry

#endif /* IBUS_UNIKEY_MACRO_FORMAT_HANDLER_REGISTRY_H */
