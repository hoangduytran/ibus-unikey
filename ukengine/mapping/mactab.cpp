// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4;
// indent-tabs-mode:nil -*-

/**
 * @file mactab.cpp
 * @brief In-memory macro table (`CMacroTable`): VNSTANDARD row storage and
 * folded-key lookup.
 *
 * On-disk codecs live in `TextMacroFormat`; optional binary hydrate/persist
 * lives in `CacheManagement`. This TU keeps conversion helpers for `addItem` /
 * `lookup` and wires canonical load/save orchestration here.
 *
 * **Progression — `writeToFile` (canonical save):**
 * 1. `TextMacroFormat::exportToPath` writes the UTF-8 TEXT file only (sorted
 * keys, plain text).
 * 2. `CacheManagement::persist` writes/renames `{path}.ukmcache` if the
 * fingerprint of the TEXT file can be read.
 *
 * **Progression — `loadFromFile` (canonical load):**
 * 1. Clear table state.
 * 2. Try `CacheManagement::tryLoad`: if magic/version/FNV fingerprint match the
 * TEXT bytes, replace `m_entries` and `rebuildLookupMap`, done.
 * 3. Else clear again, `TextMacroFormat::importFromPath` parses TEXT (VIQR
 * legacy or UTF-8).
 * 4. If header version was not UTF-8, `writeToFile` upgrades disk to UTF-8 and
 * refreshes cache.
 * 5. If already UTF-8, `CacheManagement::persist` only (TEXT unchanged).
 */

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include <ukengine/mapping/macro_cache.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/text_macro_format.h>
#include <ukengine/mapping/vnconv.h>

namespace {

/**
 * Tone/case folding for Vietnamese StdVnChar ranges (pairs of upper/lower in
 * table layout). Outside that range leaves the code unit unchanged — must stay
 * aligned with `TextMacroFormat` load semantics for consistent last-wins /
 * lookup behaviour.
 */
#define STD_TO_LOWER(x)                                                        \
  (((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && \
    !((x) & 1))                                                                \
       ? (x + 1)                                                               \
       : (x))

/**
 * @brief Build the byte string used as `unordered_map` keys: folded `StdVnChar`
 * sequence (each unit stored little-endian across `sizeof(StdVnChar)` bytes).
 * @param s NUL-terminated trigger in VNSTANDARD; null yields empty key.
 *
 * Progression:
 * 1. Walk NUL-terminated units.
 * 2. Fold each unit with `STD_TO_LOWER`.
 * 3. Append raw little-endian bytes of each folded unit to the output string.
 */
std::string foldedLookupKeyBytes(const StdVnChar *s) {
  std::string out; // output string
  if (!s)
    return out; // return empty string if input is null
  for (int i = 0; s[i] != 0; i++) {
    StdVnChar lc = STD_TO_LOWER(s[i]); // fold each unit with STD_TO_LOWER
    const unsigned char *p =
        (const unsigned char
             *)&lc; // get the little-endian bytes of the folded unit
    for (size_t b = 0; b < sizeof(StdVnChar);
         b++)                    // append the bytes to the output string
      out.push_back((char)p[b]); // append the byte to the output string
  }
  return out; // return the output string
}

/**
 * @brief Run `VnConvert` into a growable buffer until it fits or caps are exceeded.
 * @param[out] buf Receives `CONV_CHARSET_VNSTANDARD` payload (terminator layout as produced by `VnConvert`).
 * @param charsetIn Source charset selector for `VnConvert`.
 * @param src Source buffer interpreted per `charsetIn`.
 * @param[out] ok Set true when `VnConvert` returns success (`ret == 0`).
 * @return True when conversion succeeds (`ret == 0`).
 *
 * Progression:
 * 1. Start at 256 bytes, clear `ok`.
 * 2. Call `VnConvert(charsetIn -> VNSTANDARD)` with NUL-terminated-ish input
 * semantics (`inLen = -1`).
 * 3. On overflow, double `buf` up to ~64 MiB cap; bounded attempts.
 */
bool vnConvertGrow(std::vector<char> &buf, int charsetIn, const void *src,
                   bool *ok) {
  *ok = false;     // set ok to false
  buf.resize(256); // resize the buffer to 256 bytes

  // loop through 24 attempts
  for (int attempt = 0; attempt < 24; attempt++) {
    int inLen = -1;               // set inLen to -1
    int maxOut = (int)buf.size(); // set maxOut to the size of the buffer
    // call VnConvert to convert the source to VNSTANDARD
    int ret = VnConvert(charsetIn, CONV_CHARSET_VNSTANDARD, (UKBYTE *)src,
                        (UKBYTE *)buf.data(), &inLen, &maxOut);
    // if the conversion is successful, resize the buffer to the size of the
    // output and set ok to true and return true
    if (ret == 0) {
      buf.resize((size_t)maxOut); // resize the buffer to the size of the output
      *ok = true;                 // set ok to true
      return true;                // return true
    }
    // if the buffer size is greater than 64 MiB, return false
    if (buf.size() > (size_t)64 * 1024 * 1024)
      return false; // return false
    buf.resize(buf.size() *
               2); // resize the buffer to the size of the buffer * 2
  }
  return false; // if the loop completes without success, return false
}

/**
 * @brief Decode external UTF-8/VIQR (per `charset`) into a `StdVnChar` vector.
 * @param utf8 Source payload (typically NUL-terminated text).
 * @param charset Charset selector passed through to `vnConvertGrow`.
 * @param[out] out Reconstructed `StdVnChar` run from VNSTANDARD bytes.
 * @return True when conversion output length is aligned to `StdVnChar` units.
 *
 * Progression:
 * 1. `vnConvertGrow` fills `STD` bytes.
 * 2. Interpret buffer length as multiple of `sizeof(StdVnChar)` and `assign`
 * into `out`.
 */
bool utf8ToStdVnVector(const void *utf8, int charset,
                       std::vector<StdVnChar> &out) {
  std::vector<char> raw; // raw buffer
  bool ok = false;       // ok flag
  // call vnConvertGrow to convert the UTF-8/VIQR to VNSTANDARD
  if (!vnConvertGrow(raw, charset, utf8, &ok) || !ok)
    return false; // return false if the conversion fails or ok is false
  if (raw.empty())
    return false;                  // return false if the raw buffer is empty
  const size_t nByte = raw.size(); // set nByte to the size of the raw buffer
  const size_t nUnits =
      nByte /
      sizeof(StdVnChar); // set nUnits to the number of units in the raw buffer
  const StdVnChar *p = (const StdVnChar *)raw.data();
  out.assign(p, p + nUnits); // assign the units to the output vector
  return true;               // return true if the conversion is successful
}

} // namespace

// Progression: stash numeric code -> replace or clear detailed message string.
/**
 * @brief Set the last error code and message.
 * @param code The error code.
 * @param msg The error message.
 *
 * Progression: set the last error code and message.
 */
void CMacroTable::setLastError(int code, const char *msg) {
  m_lastError = code;                              // set the last error code
  m_lastErrorMessage = (msg && msg[0]) ? msg : ""; // set the last error message
  // if the message is not null and the first character is not null, set the
  // last error message to the message otherwise, set the last error message to
  // an empty string
}

/** Progression: reset table to empty, clear last error flags (via
 * `resetContent`). */
void CMacroTable::init() { resetContent(); }

/**
 * @brief Drop all macro rows and the lookup map.
 *
 * Progression: clear `m_entries` -> clear `m_lookup` -> `MACTAB_ERR_OK`.
 */
void CMacroTable::resetContent() {
  m_entries.clear();               // clear the entries
  m_lookup.clear();                // clear the lookup map
  setLastError(MACTAB_ERR_OK, ""); // set the last error code and message to OK
}

/**
 * @brief Rebuild `m_lookup` from `m_entries` (e.g. after `CacheManagement`
 * hydrates payloads).
 *
 * Progression:
 * 1. Clear the map.
 * 2. For each entry with a non-empty key vector, compute
 * `foldedLookupKeyBytes(pk)`.
 * 3. Map fold -> linear index (`last wins` when regenerating naïvely from
 * sequential rows).
 */
void CMacroTable::rebuildLookupMap() {
  m_lookup.clear(); // clear the lookup map
  // loop through the entries
  for (size_t i = 0; i < m_entries.size(); i++) {
    // get the key
    const StdVnChar *pk =
        m_entries[i].key.empty() ? nullptr : m_entries[i].key.data();
    if (!pk)
      continue;                                  // continue if the key is null
    std::string fold = foldedLookupKeyBytes(pk); // get the fold
    m_lookup[std::move(fold)] = i;               // map the fold to the index
  }
}

/** Progression: sum `key`/`text` vector sizes × `sizeof(StdVnChar)` for all
 * rows. */
size_t CMacroTable::getOccupiedBytes() const {
  size_t n = 0; // set n to 0
  // loop through the entries
  for (const MacroEntry &e : m_entries) {
    n += e.key.size() * sizeof(StdVnChar) +
         e.text.size() *
             sizeof(StdVnChar); // add the size of the key and text to n
  }
  return n; // return the number of occupied bytes
}

/**
 * @brief Macro expansion lookup from a composed trigger buffer.
 * @param key NUL-terminated VNSTANDARD sequence (typically built in
 * `UkEngine::macroMatch`).
 * @return pointer to matching row's replacement text (`StdVnChar` vector data),
 * or nullptr.
 *
 * Progression:
 * 1. Reject null/empty table/key.
 * 2. Fold `key` → map lookup string (same folding as inserts).
 * 3. If found, return address of replacement vector's first unit (still
 * NUL-terminated in storage).
 */
const StdVnChar *CMacroTable::lookup(StdVnChar *key) {
  if (!key || m_entries.empty())
    return nullptr; // return nullptr if the key is null or the table is empty
  std::string fold = foldedLookupKeyBytes(key);
  auto it = m_lookup.find(fold);
  if (it == m_lookup.end()) // if the fold is not found
    return nullptr;         // return nullptr if the fold is not found
  return m_entries[it->second].text.data(); // return the text data
}

/**
 * @brief Key column data pointer for macro row `idx`.
 * @param idx Linear row index into `m_entries`.
 * @return Pointer to key vector data (NUL-terminated in storage), or nullptr if out of range.
 *
 * Progression:
 * 1. Bounds-check against `m_entries.size()`.
 * 2. Return `key.data()` for the row.
 */
const StdVnChar *CMacroTable::getKey(int idx) const {
  // check if the index is out of bounds
  if (idx < 0 || (size_t)idx >= m_entries.size())
    return nullptr; // return nullptr if the index is out of bounds
  return m_entries[(size_t)idx].key.data(); // return the key data
}

/**
 * @brief Replacement text pointer for macro row `idx`.
 * @param idx Linear row index into `m_entries`.
 * @return Pointer to NUL-terminated replacement vector data, or nullptr if out of range.
 *
 * Progression:
 * 1. Bounds-check against `m_entries.size()`.
 * 2. Return `text.data()` for the row.
 */
const StdVnChar *CMacroTable::getText(int idx) const {
  // check if the index is out of bounds
  if (idx < 0 || (size_t)idx >= m_entries.size())
    return nullptr; // return nullptr if the index is out of bounds
  return m_entries[(size_t)idx].text.data(); // return the text data
}

/**
 * @brief Decode `key`/`text` from caller charset into VNSTANDARD row vectors for `addItem`.
 * @param key External-encoded trigger pointer (typically UTF-8 C string).
 * @param text External-encoded expansion pointer (typically UTF-8 C string).
 * @param charset `VnConvert` source charset consistent with caller encoding.
 * @param[out] outKey VNSTANDARD key units (feed `foldedLookupKeyBytes` / storage).
 * @param[out] outText Expansion buffer paired with key.
 * @return True when both sides convert; sets `MACTAB_ERR_CONVERT` on failure.
 *
 * Progression:
 * 1. `utf8ToStdVnVector` for trigger and expansion through `VNSTANDARD`.
 * 2. On either failure call `setLastError` CONVERT.
 */
bool CMacroTable::decodeMacroKeyAndText(const void *key, const void *text,
                                         int charset,
                                         std::vector<StdVnChar> &outKey,
                                         std::vector<StdVnChar> &outText) {
  if (!utf8ToStdVnVector(key, charset, outKey) ||
      !utf8ToStdVnVector(text, charset, outText)) {
    setLastError(MACTAB_ERR_CONVERT, "Conversion failed");
    return false;
  }
  return true;
}

/**
 * @brief Insert macro row under `foldLookupBytes`, or overwrite prior row mapped to same fold (last wins).
 * @param foldLookupBytes Serialized folded trigger key (same convention as lookup map keys).
 * @param keyVec VNSTANDARD key units (ownership transferred).
 * @param textVec VNSTANDARD expansion units (ownership transferred).
 * @return New or existing linear index into `m_entries`; sets `MACTAB_ERR_OK` on success path.
 *
 * Progression:
 * 1. `m_lookup.find` distinguishes append vs overwrite.
 * 2. Append: push back `MacroEntry`, map fold string to trailing index.
 * 3. Overwrite: move-assign into stored entry at mapped index.
 */
int CMacroTable::upsertMacroByFoldKey(std::string foldLookupBytes,
                                       std::vector<StdVnChar> keyVec,
                                       std::vector<StdVnChar> textVec) {
  auto it = m_lookup.find(foldLookupBytes);
  if (it == m_lookup.end()) {
    MacroEntry e;
    e.key = std::move(keyVec);
    e.text = std::move(textVec);
    m_entries.push_back(std::move(e));
    m_lookup[foldLookupBytes] = m_entries.size() - 1;
    setLastError(MACTAB_ERR_OK, "");
    return (int)(m_entries.size() - 1);
  }
  const size_t idx = it->second;
  m_entries[idx].key = std::move(keyVec);
  m_entries[idx].text = std::move(textVec);
  setLastError(MACTAB_ERR_OK, "");
  return (int)idx;
}

/**
 * @brief Insert or overwrite one macro row.
 * @param key/text input strings interpreted per `charset`
 * (`CONV_CHARSET_UNIUTF8` or VIQR, etc.).
 * @return row index on success; -1 with `MACTAB_ERR_*` on failure.
 *
 * Progression:
 * 1. Reject null `key`/`text` (`MACTAB_ERR_PARSE`).
 * 2. `decodeMacroKeyAndText` → VNSTANDARD vectors or `MACTAB_ERR_CONVERT`.
 * 3. `upsertMacroByFoldKey` — append or overwrite by folded key (`MACTAB_ERR_OK`; `bad_alloc` → OOM).
 */
int CMacroTable::addItem(const void *key, const void *text, int charset) {
  if (!key || !text) {
    setLastError(MACTAB_ERR_PARSE, "Key or text is null");
    return -1;
  }

  try {
    std::vector<StdVnChar> keyVec;
    std::vector<StdVnChar> textVec;
    if (!decodeMacroKeyAndText(key, text, charset, keyVec, textVec))
      return -1;
    std::string foldLookup = foldedLookupKeyBytes(keyVec.data());
    return upsertMacroByFoldKey(std::move(foldLookup), std::move(keyVec),
                               std::move(textVec));
  } catch (const std::bad_alloc &) {
    setLastError(MACTAB_ERR_OOM, "Out of memory for macro table");
    return -1;
  }
}

/**
 * @brief Parse one `key:text` source line (`charset` selects encoding of both
 * sides).
 *
 * Progression: find first `':'` delimiter -> split substring -> delegate to
 * two-arg `addItem`.
 */
int CMacroTable::addItem(const char *item, int charset) {
  // find the colon in the item
  const char *colon = strchr(item, ':');
  // check if the colon is not found
  if (!colon) { // if the colon is not found
    setLastError(MACTAB_ERR_PARSE, "No ':' in macro line");
    return -1; // return -1 if the colon is not found
  }
  std::string key(item, colon - item);             // get the key
  return addItem(key.c_str(), colon + 1, charset); // add the item
}

/**
 * @brief Persist table to canonical `fname`: TEXT export then cache persist.
 * @return 1 on success, 0 when `exportToPath` fails (inspect `getLastError()`).
 *
 * Progression:
 * 1. `TextMacroFormat::exportToPath` writes sorted UTF-8 rows + header only.
 * 2. On success, `CacheManagement::persist` updates `{fname}.ukmcache` from the
 * TEXT bytes on disk.
 */
int CMacroTable::writeToFile(const char *fname) {
  TextMacroFormat textFmt; // create a new text format
  // export the table to the file
  if (textFmt.exportToPath(fname, *this) != 1) // if the export fails
    return 0;                                  // return 0 if the export fails
  CacheManagement::persist(fname, *this);      // persist the table
  // return 1 if the export is successful
  return 1; // return 1 if the export is successful
}

/**
 * @brief Load table from canonical `fname`: sidecar hydrate or TEXT import +
 * persist policy.
 * @return 1 on success, 0 when both cache miss and TEXT import fail.
 *
 * Progression:
 * 1. `resetContent()` for a cold table.
 * 2. Cache hit path: `CacheManagement::tryLoad` fills entries + rebuilds map →
 * return 1.
 * 3. Cache miss path: `resetContent()` again, `TextMacroFormat::importFromPath`
 * parses TEXT.
 * 4. If `version != kUtf8Version`, rewrite/upgraded disk via full `writeToFile`
 * → return its result.
 * 5. Already UTF-8: refresh sidecar with `persist` only, return 1.
 */
int CMacroTable::loadFromFile(const char *fname) {
  resetContent(); // reset the table

  // try to load the table from the file
  if (CacheManagement::tryLoad(fname, *this)) { // if the load is successful
    setLastError(MACTAB_ERR_OK,
                 ""); // set the last error code and message to OK
    return 1;         // return 1 if the load is successful
  }

  resetContent(); // reset the table

  int version = 0;         // set the version to 0
  TextMacroFormat textFmt; // create a new text format
  // import the table from the file
  if (textFmt.importFromPath(fname, *this, &version) !=
      1)      // if the import fails
    return 0; // return 0 if the import fails

  setLastError(MACTAB_ERR_OK, ""); // set the last error code and message to OK

  if (version != TextMacroFormat::kUtf8Version) // if the version is not UTF-8
    return writeToFile(fname); // return the result of the write to file

  CacheManagement::persist(fname, *this); // persist the table
  return 1; // return 1 if the import is successful
}
