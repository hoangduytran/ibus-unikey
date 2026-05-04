// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/**
 * @file macro_cache.cpp
 * @brief Binary sidecar cache for `CMacroTable` (see project_planning ukengine macro plan).
 *
 * **File layout (little-endian):**
 * - 8-byte magic `UKMCCH01`.
 * - `uint16_t` format version, `uint16_t` reserved flags.
 * - `uint64_t` FNV-1a fingerprint of the macro *text* file bytes (must match on load).
 * - `uint32_t` entry count, then per row: key length and text length in `StdVnChar`
 *   units (each count includes the trailing NUL), followed by raw `StdVnChar` payloads.
 *
 * **Progression — `tryLoad`:**
 * 1. Open `{macroTextPath}.ukmcache`; require magic and format version.
 * 2. Recompute FNV-1a of the text file; reject if mismatch or hash read failure.
 * 3. For each entry, read unit counts and byte payloads into `MacroEntry` rows.
 * 4. Replace table entries and rebuild the folded-key map.
 *
 * **Progression — `persist`:**
 * 1. Hash the text file; abort if unreadable (fingerprint 0).
 * 2. Write a `.tmp` sidecar with header and rows (NUL counts match `writeToFile`).
 * 3. Atomically replace the final `.ukmcache` via remove + rename.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <ukengine/mapping/macro_cache.h>
#include <ukengine/mapping/mactab.h>

namespace {

/** Magic bytes identifying a UniKey macro cache file (not NUL-terminated). */
const char kMagic[8] = {'U', 'K', 'M', 'C', 'C', 'H', '0', '1'};
/** On-disk record layout version; increment only when the binary format changes. */
const uint16_t kFormatVersion = 1;

/**
 * @brief FNV-1a 64-bit hash over the raw byte contents of a file.
 * @param filePath Path to the macro text file (same path used for load/save).
 * @return Hash of all bytes read, or 0 if the file could not be opened.
 */
uint64_t fnv1a64File(const char *filePath)
{
    FILE *macroTextFile = fopen(filePath, "rb");
    if (!macroTextFile)
        return 0;
    uint64_t hash = 14695981039346656037ULL;
    unsigned char chunk[8192];
    size_t bytesRead;
    while ((bytesRead = fread(chunk, 1, sizeof(chunk), macroTextFile)) > 0) {
        for (size_t byteIndex = 0; byteIndex < bytesRead; byteIndex++) {
            hash ^= (uint64_t)chunk[byteIndex];
            hash *= 1099511628211ULL;
        }
    }
    fclose(macroTextFile);
    return hash;
}

/**
 * @brief Read a little-endian `uint16_t` from @a stream.
 * @param stream Open binary file positioned at the field.
 * @param[out] valueOut Receives the decoded value on success.
 */
bool readU16(FILE *stream, uint16_t *valueOut)
{
    unsigned char rawBytes[2];
    if (fread(rawBytes, 1, 2, stream) != 2)
        return false;
    *valueOut = (uint16_t)rawBytes[0] | ((uint16_t)rawBytes[1] << 8);
    return true;
}

/** @brief Read a little-endian `uint32_t`; see `readU16`. */
bool readU32(FILE *stream, uint32_t *valueOut)
{
    unsigned char rawBytes[4];
    if (fread(rawBytes, 1, 4, stream) != 4)
        return false;
    *valueOut = (uint32_t)rawBytes[0] | ((uint32_t)rawBytes[1] << 8) |
                ((uint32_t)rawBytes[2] << 16) | ((uint32_t)rawBytes[3] << 24);
    return true;
}

/** @brief Read a little-endian `uint64_t`; see `readU16`. */
bool readU64(FILE *stream, uint64_t *valueOut)
{
    unsigned char rawBytes[8];
    if (fread(rawBytes, 1, 8, stream) != 8)
        return false;
    uint64_t value = 0;
    for (int byteIndex = 0; byteIndex < 8; byteIndex++)
        value |= (uint64_t)rawBytes[byteIndex] << (8 * byteIndex);
    *valueOut = value;
    return true;
}

/** @brief Write a little-endian `uint16_t` to @a stream. */
void writeU16(FILE *stream, uint16_t value)
{
    unsigned char rawBytes[2] = {(unsigned char)(value & 0xFF),
                                 (unsigned char)((value >> 8) & 0xFF)};
    fwrite(rawBytes, 1, 2, stream);
}

/** @brief Write a little-endian `uint32_t` to @a stream. */
void writeU32(FILE *stream, uint32_t value)
{
    unsigned char rawBytes[4] = {(unsigned char)(value & 0xFF),
                                 (unsigned char)((value >> 8) & 0xFF),
                                 (unsigned char)((value >> 16) & 0xFF),
                                 (unsigned char)((value >> 24) & 0xFF)};
    fwrite(rawBytes, 1, 4, stream);
}

/** @brief Write a little-endian `uint64_t` to @a stream. */
void writeU64(FILE *stream, uint64_t value)
{
    for (int byteIndex = 0; byteIndex < 8; byteIndex++) {
        unsigned char byteOut = (unsigned char)((value >> (8 * byteIndex)) & 0xFF);
        fwrite(&byteOut, 1, 1, stream);
    }
}

/**
 * @brief Confirm the cache file starts with the expected magic bytes.
 * @param cacheFile Positioned at the first byte of the sidecar (caller owns the handle).
 */
bool readCacheMagic(FILE *cacheFile)
{
    char fileMagic[8];
    return fread(fileMagic, 1, 8, cacheFile) == 8 &&
           memcmp(fileMagic, kMagic, 8) == 0;
}

/**
 * @brief Parse the fixed header after magic: version, flags, fingerprint, row count.
 * @return False on I/O error or if the stored format version is not `kFormatVersion`.
 */
bool readValidatedCacheHeader(FILE *cacheFile, uint64_t *storedFingerprintOut,
                              uint32_t *entryCountOut)
{
    uint16_t formatVersion = 0;
    uint16_t headerFlags = 0;
    if (!readU16(cacheFile, &formatVersion) ||
        !readU16(cacheFile, &headerFlags) ||
        !readU64(cacheFile, storedFingerprintOut) ||
        !readU32(cacheFile, entryCountOut))
        return false;
    if (formatVersion != kFormatVersion)
        return false;
    (void)headerFlags; // reserved in on-disk header; unused until a feature needs it
    return true;
}

/**
 * @brief Deserialize the next macro row (unit counts + raw key/text `StdVnChar` bytes).
 */
bool readMacroEntryFromCache(FILE *cacheFile, MacroEntry *entryOut)
{
    uint32_t keyUnitCount = 0;
    uint32_t textUnitCount = 0;
    if (!readU32(cacheFile, &keyUnitCount) ||
        !readU32(cacheFile, &textUnitCount) || keyUnitCount == 0 ||
        textUnitCount == 0)
        return false;

    const size_t keyByteCount = (size_t)keyUnitCount * sizeof(StdVnChar);
    const size_t textByteCount = (size_t)textUnitCount * sizeof(StdVnChar);
    std::vector<char> keyBytes(keyByteCount);
    std::vector<char> textBytes(textByteCount);
    if (fread(keyBytes.data(), 1, keyByteCount, cacheFile) != keyByteCount ||
        fread(textBytes.data(), 1, textByteCount, cacheFile) != textByteCount)
        return false;

    MacroEntry entry;
    entry.key.resize(keyUnitCount);
    entry.text.resize(textUnitCount);
    memcpy(entry.key.data(), keyBytes.data(), keyByteCount);
    memcpy(entry.text.data(), textBytes.data(), textByteCount);
    *entryOut = std::move(entry);
    return true;
}

/**
 * @brief Load exactly @a entryCount rows after the header; @a cacheFile must be positioned
 * at the first row.
 */
bool loadMacroEntriesFromCache(FILE *cacheFile, uint32_t entryCount,
                               std::vector<MacroEntry> *entriesOut)
{
    entriesOut->clear();
    entriesOut->reserve(entryCount);
    for (uint32_t i = 0; i < entryCount; i++) {
        MacroEntry row;
        if (!readMacroEntryFromCache(cacheFile, &row))
            return false;
        entriesOut->push_back(std::move(row));
    }
    return true;
}

/** @brief Write magic + version + flags + fingerprint + entry count (rows follow). */
void writeCacheFixedHeader(FILE *cacheFile, uint64_t textFingerprint,
                           uint32_t entryCount)
{
    fwrite(kMagic, 1, 8, cacheFile);
    writeU16(cacheFile, kFormatVersion);
    writeU16(cacheFile, 0);
    writeU64(cacheFile, textFingerprint);
    writeU32(cacheFile, entryCount);
}

/**
 * @brief Length of a NUL-terminated `StdVnChar` run in units, including the trailing 0 unit
 * (matches what we persist per row).
 */
size_t stdVnUnitsIncludingTrailingNul(const StdVnChar *run)
{
    size_t n = 0;
    while (run[n] != 0)
        n++;
    return n + 1;
}

/**
 * @brief Append all table rows in on-disk order; returns false if any key/text pointer is null.
 */
bool writeMacroTableRows(FILE *cacheFile, const CMacroTable &table,
                         uint32_t entryCount)
{
    for (int i = 0; i < (int)entryCount; i++) {
        const StdVnChar *keyRun = table.getKey(i);
        const StdVnChar *textRun = table.getText(i);
        if (!keyRun || !textRun)
            return false;

        const size_t keyUnits = stdVnUnitsIncludingTrailingNul(keyRun);
        const size_t textUnits = stdVnUnitsIncludingTrailingNul(textRun);

        writeU32(cacheFile, (uint32_t)keyUnits);
        writeU32(cacheFile, (uint32_t)textUnits);
        fwrite(keyRun, sizeof(StdVnChar), keyUnits, cacheFile);
        fwrite(textRun, sizeof(StdVnChar), textUnits, cacheFile);
    }
    return true;
}

/** @brief Replace an existing sidecar with a fully written temp file. */
void replaceSidecarFromTemp(const std::string &sidecarPath,
                            const std::string &tempSidecarPath)
{
    remove(sidecarPath.c_str());
    rename(tempSidecarPath.c_str(), sidecarPath.c_str());
}

} // namespace

std::string CacheManagement::sidecarPathFor(const char *macroTextPath)
{
    if (!macroTextPath)
        return std::string();
    return std::string(macroTextPath) + ".ukmcache";
}

bool CacheManagement::tryLoad(const char *macroTextPath, CMacroTable &table)
{
    const std::string sidecarPath = sidecarPathFor(macroTextPath);
    FILE *cacheFile = fopen(sidecarPath.c_str(), "rb");
    if (!cacheFile)
        return false;

    uint64_t storedFingerprint = 0;
    uint32_t entryCount = 0;
    const bool magicOk = readCacheMagic(cacheFile);
    const bool headerOk =
        magicOk &&
        readValidatedCacheHeader(cacheFile, &storedFingerprint, &entryCount);
    const uint64_t currentTextFingerprint =
        headerOk ? fnv1a64File(macroTextPath) : 0ULL;
    const bool fingerprintMatchesTextFile =
        headerOk && currentTextFingerprint == storedFingerprint &&
        currentTextFingerprint != 0ULL;

    std::vector<MacroEntry> loadedEntries;
    const bool entriesOk = fingerprintMatchesTextFile &&
                           loadMacroEntriesFromCache(cacheFile, entryCount,
                                                     &loadedEntries);

    fclose(cacheFile);

    if (!entriesOk)
        return false;

    table.m_entries = std::move(loadedEntries);
    table.rebuildLookupMap();
    return true;
}

void CacheManagement::persist(const char *macroTextPath, const CMacroTable &table)
{
    const uint64_t textFingerprint = fnv1a64File(macroTextPath);
    if (textFingerprint == 0)
        return;

    const std::string sidecarPath = sidecarPathFor(macroTextPath);
    const std::string tempSidecarPath = sidecarPath + ".tmp";

    FILE *cacheFile = fopen(tempSidecarPath.c_str(), "wb");
    if (!cacheFile)
        return;

    const uint32_t entryCount = (uint32_t)table.getCount();
    writeCacheFixedHeader(cacheFile, textFingerprint, entryCount);

    const bool rowsWrittenOk =
        writeMacroTableRows(cacheFile, table, entryCount);
    fclose(cacheFile);

    if (!rowsWrittenOk) {
        remove(tempSidecarPath.c_str());
        return;
    }

    replaceSidecarFromTemp(sidecarPath, tempSidecarPath);
}
