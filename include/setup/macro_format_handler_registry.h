// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_FORMAT_HANDLER_REGISTRY_H
#define IBUS_UNIKEY_MACRO_FORMAT_HANDLER_REGISTRY_H

#include <functional>
#include <initializer_list>
#include <memory>

#include <glib.h>

#include <setup/macro_file_io.h>
#include <setup/macro_format_handler.h>

/**
 * @file macro_format_handler_registry.h
 * @brief Factory registry keyed by `MacroInterchangeForcedFormat` and file suffix (AUTO).
 */

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

/**
 * @brief Register or replace the factory for @a format and map suffixes to it for AUTO.
 * @param format Dispatcher id (not `MACRO_INTERCHANGE_FORMAT_AUTO`).
 * @param factory Returns a new handler instance per call.
 * @param suffixes_lower Lowercase extensions with leading dot, e.g. `.yaml`, `.yml`.
 */
void register_handler(MacroInterchangeForcedFormat format, HandlerFactory factory,
                      std::initializer_list<const char *> suffixes_lower);

/**
 * @brief Instantiates a handler for @a format, or null if none registered.
 * @param format Explicit format (not usually `AUTO`).
 */
std::unique_ptr<MacroFormatHandler> create(MacroInterchangeForcedFormat format);

/**
 * @brief Picks a format from the path suffix; falls back in implementation when unknown.
 * @param path_utf8 UTF-8 filesystem path (extension is inspected).
 */
MacroInterchangeForcedFormat detect_from_path(const gchar *path_utf8);

} // namespace MacroFormatHandlerRegistry

#endif /* IBUS_UNIKEY_MACRO_FORMAT_HANDLER_REGISTRY_H */
