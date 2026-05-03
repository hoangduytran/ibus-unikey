// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file macro_interchange_common.cpp
 * @brief Implementations of shared macro interchange helpers (errors, file I/O, paths, VN key order, UTF-8).
 */

#include <setup/macro_interchange_common.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

namespace macro_interchange {

/**
 * @brief Fold one `StdVnChar` code unit for case-insensitive lexical order (Vietnamese pair rule).
 *
 * Matches the folding used elsewhere for macro text (e.g. `STD_TO_LOWER` in `text_macro_format.cpp`):
 * alphabetic VN standard units in a case pair map to the “lower” member of the pair.
 *
 * @param x Input code unit (`StdVnChar` / `UKWORD`-sized).
 * @return Folded code unit (may equal @a x when no fold applies).
 */
#define STD_TO_LOWER(x)                                                                            \
  (((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && !((x) & 1))         \
       ? (x + 1)                                                                                   \
       : (x))

/**
 * @brief Returns the `GQuark` for `macro_interchange::fail()` errors.
 * @return Static quark string `ibus-unikey-macro-interchange-error`.
 */
GQuark error_quark(void) { return g_quark_from_static_string("ibus-unikey-macro-interchange-error"); }

/**
 * @brief Populates @a err with a message in `error_quark()` when @a err is non-null.
 * @param err Optional `GError**` from the caller.
 * @param msg Short English message (not translated).
 */
void fail(GError **err, const char *msg) {
  if (err)
    g_set_error(err, error_quark(), 0, "%s", msg);
}

/**
 * @brief Reads the whole file into @a out without interpreting encoding.
 * @param path Filesystem path in UTF-8 (GLib convention).
 * @param out Receives raw bytes (length equals file size).
 * @param err Set by `g_file_get_contents` on failure; left untouched on success.
 * @return `true` if the file was read; `false` if @a err was set.
 */
bool read_file_utf8(const gchar *path, std::string *out, GError **err) {
  gchar *contents = nullptr;
  gsize byte_length = 0;
  if (!g_file_get_contents(path, &contents, &byte_length, err))
    return false;
  out->assign(contents, byte_length);
  g_free(contents);
  return true;
}

/**
 * @brief Returns the substring from the last `.` in @a path through the end, ASCII-lowercased.
 *
 * Used for extension-based format detection (`AUTO`). Leading dot is included (e.g. `.yaml`).
 *
 * @param path UTF-8 path or `nullptr`.
 * @return Suffix including dot, or empty string if there is no `.` or @a path is null.
 */
std::string path_suffix_lower(const gchar *path) {
  if (!path)
    return "";
  const char *dot = strrchr(path, '.');
  if (!dot)
    return "";
  std::string lower_suffix(dot);
  for (char &ch : lower_suffix)
    ch = (char)tolower((unsigned char)ch);
  return lower_suffix;
}

/**
 * @brief Lexicographic compare of NUL-terminated `StdVnChar` keys using `STD_TO_LOWER` per unit.
 *
 * Ordering matches folded trigger comparison used for deterministic export sort (consistent with
 * `TextMacroFormat` / `CMacroTable` documentation).
 *
 * @param left_key First key (NUL-terminated).
 * @param right_key Second key (NUL-terminated).
 * @return Negative if @a left_key precedes @a right_key, positive if after, zero if equal.
 */
int compare_std_vn_keys(const StdVnChar *left_key, const StdVnChar *right_key) {
  int position = 0;
  StdVnChar folded_left_unit, folded_right_unit;
  for (;; position++) {
    folded_left_unit = STD_TO_LOWER(left_key[position]);
    folded_right_unit = STD_TO_LOWER(right_key[position]);
    if (folded_left_unit > folded_right_unit)
      return 1;
    if (folded_left_unit < folded_right_unit)
      return -1;
    if (left_key[position] == 0)
      return (right_key[position] == 0) ? 0 : -1;
  }
}

/**
 * @brief Converts a NUL-terminated `StdVnChar` buffer to UTF-8 using `VnConvert`, reallocating @a buf.
 *
 * On success, @a buf holds the converted bytes (length from `VnConvert`’s output size); may include
 * embedded `NUL` bytes before trailing padding—callers that need a C string should trim.
 *
 * @param src Source string in `CONV_CHARSET_VNSTANDARD` (NUL-terminated).
 * @param buf Growable byte buffer; reinitialized from 256 bytes upward on each call.
 * @param[out] ok Set to `true` only when `VnConvert` returns success (`0`).
 * @return `false` if conversion never succeeds or the buffer would exceed an internal cap (~64 MiB growth).
 */
bool vn_std_to_utf8_grow(const StdVnChar *src, std::vector<char> &buf, bool *ok) {
  *ok = false;
  buf.resize(256);
  for (int resize_attempt = 0; resize_attempt < 24; resize_attempt++) {
    int input_length = -1;
    int max_output = (int)buf.size();
    const int convert_result =
        VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8, (UKBYTE *)src, (UKBYTE *)buf.data(),
                  &input_length, &max_output);
    if (convert_result == 0) {
      buf.resize((size_t)max_output);
      *ok = true;
      return true;
    }
    if (buf.size() > (size_t)64 * 1024 * 1024)
      return false;
    buf.resize(buf.size() * 2);
  }
  return false;
}

/**
 * @brief Encodes one macro cell (`StdVnChar` key or phrase) as a UTF-8 `std::string` for export.
 *
 * Trims trailing `NUL` code units from the conversion buffer so `*utf8_out` is a normal UTF-8 byte
 * string suitable for codecs and UI.
 *
 * @param vn NUL-terminated VN standard text from `CMacroTable::getKey` / `getText`.
 * @param[out] utf8_out Receives UTF-8 octets (no embedded `NUL` after trim).
 * @return `false` if `vn_std_to_utf8_grow` fails.
 */
bool utf8_from_std_keytext(const StdVnChar *vn, std::string *utf8_out) {
  std::vector<char> buf;
  bool decode_ok = false;
  if (!vn_std_to_utf8_grow(vn, buf, &decode_ok) || !decode_ok)
    return false;
  while (!buf.empty() && buf.back() == '\0')
    buf.pop_back();
  utf8_out->assign(buf.data(), buf.size());
  return true;
}

/**
 * @brief Permutation `0 .. row_count-1` sorted by folded `StdVnChar` trigger key.
 *
 * Used so JSON/XML/YAML/CSV exports list rows in the same order as the native text macro exporter.
 *
 * @param table Macro table (must be non-null; caller ensures).
 * @return Indices such that `compare_std_vn_keys(getKey[i], getKey[j]) < 0` implies `i` appears before `j`.
 */
std::vector<int> sorted_macro_row_indices(const CMacroTable *table) {
  const int row_count = table->getCount();
  std::vector<int> sorted_indices((size_t)row_count);
  for (int row_index = 0; row_index < row_count; row_index++)
    sorted_indices[(size_t)row_index] = row_index;
  std::sort(sorted_indices.begin(), sorted_indices.end(), [&](int row_index_a, int row_index_b) {
    return compare_std_vn_keys(table->getKey(row_index_a), table->getKey(row_index_b)) < 0;
  });
  return sorted_indices;
}

} // namespace macro_interchange
