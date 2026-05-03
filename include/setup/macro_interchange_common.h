// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_INTERCHANGE_COMMON_H
#define IBUS_UNIKEY_MACRO_INTERCHANGE_COMMON_H

#include <glib.h>

#include <setup/macro_file_io.h>

#include <string>
#include <vector>

#include <ukengine/mapping/charset.h>

class CMacroTable;

/**
 * @file macro_interchange_common.h
 * @brief Shared helpers for macro interchange paths (bytes, UTF-8, ordering, errors).
 *
 * Used by `setup/macro_handler_*.cpp` and related UI code to avoid duplicating file I/O and
 * key-encoding glue between codecs.
 */

namespace macro_interchange {

/** @brief Error domain for macro interchange `GError` reporting. */
GQuark error_quark();

/**
 * @brief Sets a domain error on @a err when non-null.
 * @param err `GError**` from the caller (may be `nullptr`).
 * @param msg Human-readable English message (stable for logs; not necessarily translated).
 */
void fail(GError **err, const char *msg);

/**
 * @brief Reads an entire file into @a out as a byte string (no encoding conversion).
 * @param path UTF-8 path.
 * @param out Receives raw file bytes (often UTF-8 text).
 * @param err Set on I/O failure.
 * @return False on read failure.
 */
bool read_file_utf8(const gchar *path, std::string *out, GError **err);

/**
 * @brief Lowercases the filename suffix after the final dot (for extension-based format sniffing).
 */
std::string path_suffix_lower(const gchar *path);

/** @brief Lexicographic compare on `StdVnChar` key buffers (folded ordering). */
int compare_std_vn_keys(const StdVnChar *left_key, const StdVnChar *right_key);

/**
 * @brief Converts `StdVnChar` key/phrase text to UTF-8, growing @a buf as needed.
 * @param ok Set false on invalid conversion.
 */
bool vn_std_to_utf8_grow(const StdVnChar *src, std::vector<char> &buf, bool *ok);

/** @brief Encodes one macro key or phrase cell as UTF-8 for interchange output. */
bool utf8_from_std_keytext(const StdVnChar *vn, std::string *utf8_out);

/**
 * @brief Row indices sorted by folded StdVn trigger key (same order as native text export).
 * @param table Non-null macro table.
 */
std::vector<int> sorted_macro_row_indices(const CMacroTable *table);

} // namespace macro_interchange

#endif /* IBUS_UNIKEY_MACRO_INTERCHANGE_COMMON_H */
