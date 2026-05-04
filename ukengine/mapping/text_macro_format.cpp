// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file text_macro_format.cpp
 * @brief UniKey TEXT macro import/export (`TextMacroFormat` / `MacroFormat`).
 *
 * Parses and writes the human-readable macro file: optional UTF-8 BOM (handled on read),
 * magic header line with `version=`, then `trigger:expansion` rows. ASCII trigger prefixes are
 * case-folded for duplicate resolution so the **last** physical line for a folded key wins,
 * matching `CMacroTable` lookup semantics documented alongside `mactab.cpp`.
 *
 * This codec touches **only** the text stream. Binary `.ukmcache` hydrate/persist is owned by
 * `CacheManagement` via `CMacroTable::loadFromFile` / `writeToFile`, not by this class.
 */

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/text_macro_format.h>
#include <ukengine/mapping/vnconv.h>

namespace {

#define STD_TO_LOWER(x)                                                                            \
    (((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && !((x) & 1))       \
         ? (x + 1)                                                                                 \
         : (x))

/**
 * @brief Lexicographic compare on NUL-terminated `StdVnChar` keys with Vietnamese pair folding.
 *
 * Used only for deterministic sort order on export; must stay aligned with folding used when
 * building the “last wins” line index during import (`mactabFoldKeyPrefix` vs `foldedLookupKeyBytes`).
 */
int compareStdVnKeys(const StdVnChar *a, const StdVnChar *b)
{
    int i = 0;
    StdVnChar ls1, ls2;
    for (;; i++) {
        ls1 = STD_TO_LOWER(a[i]);
        ls2 = STD_TO_LOWER(b[i]);
        if (ls1 > ls2)
            return 1;
        if (ls1 < ls2)
            return -1;
        if (a[i] == 0)
            return (b[i] == 0) ? 0 : -1;
    }
}

/**
 * @brief Convert a NUL-terminated `StdVnChar` run to UTF-8 via `VnConvert`, growing `buf` as needed.
 *
 * @param[out] ok Set to `true` only when `VnConvert` returns success.
 * @return `false` when conversion fails repeatedly or the buffer would exceed an internal cap.
 */
bool vnStdToUtf8Grow(const StdVnChar *src, std::vector<char> &buf, bool *ok)
{
    *ok = false;
    buf.resize(256);
    for (int attempt = 0; attempt < 24; attempt++) {
        int inLen = -1;
        int maxOut = (int)buf.size();
        int ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8, (UKBYTE *)src, (UKBYTE *)buf.data(),
                            &inLen, &maxOut);
        if (ret == 0) {
            buf.resize((size_t)maxOut);
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
 * @brief ASCII-only lowercase fold of the trigger substring before the first `:` on a source line.
 *
 * Lines without `:` are not passed here. Vietnamese bytes outside ASCII are left unchanged so
 * folding stays consistent with historical text-macro behavior for duplicate keys.
 */
std::string mactabFoldKeyPrefix(const char *line, size_t keyLen)
{
    std::string s(line, keyLen);
    for (char &c : s) {
        unsigned char u = (unsigned char)c;
        if (u < 128U)
            c = (char)std::tolower((int)u);
    }
    return s;
}

/** @brief Open macro file for read using platform-appropriate text mode. */
FILE *openMacroFileRead(const char *fname)
{
#if defined(WIN32)
    return _tfopen(fname, _TEXT("rt"));
#else
    return fopen(fname, "r");
#endif
}

/** @brief Open macro file for write using platform-appropriate text mode. */
FILE *openMacroFileWrite(const char *fname)
{
#if defined(WIN32)
    return _tfopen(fname, _TEXT("wt"));
#else
    return fopen(fname, "w");
#endif
}

/**
 * @brief Read a single logical line: accepts arbitrarily long lines; strips `\r`; `\n` ends the line.
 *
 * @return `true` when a line terminator was seen or trailing payload existed before EOF.
 */
bool readLineUnbounded(FILE *f, std::string &out)
{
    out.clear();
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\r')
            continue;
        if (c == '\n')
            return true;
        out.push_back((char)c);
    }
    return !out.empty();
}

/** @brief Append all remaining lines after the header into `lines`. */
void readRemainingLines(FILE *f, std::vector<std::string> &lines)
{
    std::string line;
    while (readLineUnbounded(f, line))
        lines.push_back(std::move(line));
}

/**
 * @brief For each `key:...` line, record the last line index whose folded key matches.
 *
 * Implements **last-wins** when several file lines share the same folded ASCII trigger prefix.
 */
void buildLastWinsLineIndex(const std::vector<std::string> &allLines,
                            std::unordered_map<std::string, size_t> &lastLineForFoldedKey)
{
    for (size_t i = 0; i < allLines.size(); i++) {
        const char *L = allLines[i].c_str();
        const char *colon = strchr(L, ':');
        if (!colon)
            continue;
        const size_t keyLen = (size_t)(colon - L);
        const std::string folded = mactabFoldKeyPrefix(L, keyLen);
        lastLineForFoldedKey[folded] = i;
    }
}

/**
 * @brief Apply `allLines` to `table`, honoring the precomputed last-wins index.
 *
 * Lines **without** `:` are passed to `addItem` whole (legacy single-field rows). Lines with `:`
 * are inserted only when their index is the recorded winner for their folded key, so earlier
 * duplicates are skipped without touching the table.
 *
 * @return `true` if any `addItem` failed (caller may still inspect `table` for OOM vs parse errors).
 */
bool applyLoadedLinesToTable(CMacroTable *table, const std::vector<std::string> &allLines, int charset,
                             const std::unordered_map<std::string, size_t> &lastLineForFoldedKey)
{
    bool anyLineFailed = false;

    for (size_t i = 0; i < allLines.size(); i++) {
        const char *L = allLines[i].c_str();
        const char *colon = strchr(L, ':');
        if (!colon) {
            const int rc = table->addItem(L, charset);
            if (rc < 0)
                anyLineFailed = true;
            continue;
        }

        const size_t keyLen = (size_t)(colon - L);
        const std::string folded = mactabFoldKeyPrefix(L, keyLen);
        const auto winnerIt = lastLineForFoldedKey.find(folded);
        const bool isWinningRow =
            (winnerIt != lastLineForFoldedKey.end()) && (winnerIt->second == i);
        if (!isWinningRow)
            continue;

        const int rc = table->addItem(L, charset);
        if (rc < 0)
            anyLineFailed = true;
    }
    return anyLineFailed;
}

/**
 * @brief Read the first line for `version=` in the UniKey header, or establish legacy (VIQR) mode.
 *
 * Strips an optional UTF-8 BOM from the scan window. If no recognizable marker is found, sets
 * `version` to 0 and rewinds the stream so the first line is re-read as body content.
 *
 * @param[out] version `kUtf8Version` when the UTF-8 header is present; `0` for legacy / absent header.
 * @return `false` only when the first read fails unexpectedly (not EOF).
 */
bool readVersionHeader(FILE *f, int &version)
{
    std::string line;
    if (!readLineUnbounded(f, line)) {
        if (feof(f)) {
            version = 0;
            return true;
        }
        return false;
    }

    const char *p = line.c_str();
    size_t len = line.size();
    if (len >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;

    const char *marker = strstr(p, "***");
    if (marker) {
        marker += 3;
        while (*marker == ' ')
            marker++;
        if (sscanf(marker, "version=%d", &version) == 1)
            return true;
    }

    version = 0;
    fseek(f, 0, SEEK_SET);
    return true;
}

/**
 * @brief Write one `key:text` row as UTF-8 bytes, trimming NUL padding from `VnConvert` output.
 *
 * @param addNl When `false`, omits the trailing newline (used for the final row on export).
 * @return 0 on success, -1 on conversion or `fwrite` failure.
 */
int writeOneRowUtf8(FILE *f, const MacroEntry &row, bool addNl)
{
    std::vector<char> keyUtf8;
    std::vector<char> textUtf8;
    bool ok = false;
    if (!vnStdToUtf8Grow(row.key.data(), keyUtf8, &ok) || !ok)
        return -1;
    ok = false;
    if (!vnStdToUtf8Grow(row.text.data(), textUtf8, &ok) || !ok)
        return -1;

    while (!keyUtf8.empty() && keyUtf8.back() == '\0')
        keyUtf8.pop_back();
    while (!textUtf8.empty() && textUtf8.back() == '\0')
        textUtf8.pop_back();

    std::string line;
    line.assign(keyUtf8.data(), keyUtf8.size());
    line.push_back(':');
    line.append(textUtf8.data(), textUtf8.size());
    if (addNl)
        line.push_back('\n');
    if (fwrite(line.data(), 1, line.size(), f) != line.size())
        return -1;
    return 0;
}

} // namespace

MacroFormatId TextMacroFormat::id() const
{
    return MacroFormatId::TextUniKey;
}

/**
 * @brief Load macro rows from disk into `table` using the text codec only.
 *
 * Progression: open file → read header to choose UTF-8 vs VIQR charset → read remaining lines →
 * build last-wins index for folded keys → apply winning lines to `table`. Sets `MACTAB_ERR_INCOMPLETE`
 * when any row fails to insert unless the failure was OOM.
 *
 * @param path Filesystem path to the macro text file.
 * @param table Destination table; caller controls whether rows are cleared beforehand.
 * @param[out] outSourceVersion When non-null, receives header `version` (`0` = legacy / absent).
 * @return `1` on full success, `0` on I/O, header, or partial line failure (`table` holds detail).
 *
 * @note Import uses only public `CMacroTable` APIs (`addItem`); does not touch `.ukmcache`.
 */
int TextMacroFormat::importFromPath(const char *path, CMacroTable &table, int *outSourceVersion)
{
    FILE *f = openMacroFileRead(path);
    const bool openedForReadOk = (f != nullptr);
    if (!openedForReadOk) {
        table.setLastError(MACTAB_ERR_IO, "Failed to open macro file for read");
        return 0;
    }

    int version = 0;
    const bool versionHeaderDecodedOk = readVersionHeader(f, version);
    if (!versionHeaderDecodedOk) {
        fclose(f);
        table.setLastError(MACTAB_ERR_IO, "Failed to read macro file header");
        return 0;
    }

    std::vector<std::string> allLines;
    readRemainingLines(f, allLines);
    fclose(f);

    if (outSourceVersion)
        *outSourceVersion = version;

    const bool isUtf8Version = (version == kUtf8Version);
    const int charset = isUtf8Version ? CONV_CHARSET_UNIUTF8 : CONV_CHARSET_VIQR;

    std::unordered_map<std::string, size_t> lastLineForFoldedKey;
    buildLastWinsLineIndex(allLines, lastLineForFoldedKey);

    const bool anyLineFailed =
        applyLoadedLinesToTable(&table, allLines, charset, lastLineForFoldedKey);

    if (anyLineFailed) {
        if (table.getLastError() != MACTAB_ERR_OOM)
            table.setLastError(MACTAB_ERR_INCOMPLETE, "One or more macro lines could not be added");
        return 0;
    }

    table.setLastError(MACTAB_ERR_OK, "");
    return 1;
}

/**
 * @brief Serialize `table` to a UTF-8 macro text file with the current header and sorted keys.
 *
 * Rows are written in ascending trigger order (`compareStdVnKeys`). Uses `table.m_entries`
 * directly because `TextMacroFormat` is a `friend` of `CMacroTable`.
 *
 * @param path Destination path (overwritten on success).
 * @param table Source macro table.
 * @return `1` on success, `0` on open/write failure (`table` carries the last error message).
 *
 * @note On Windows the header is written with a UTF-8 BOM prefix before the magic line for legacy
 *       consumers; non-Windows builds omit the BOM on the header line per existing convention.
 */
int TextMacroFormat::exportToPath(const char *path, CMacroTable &table)
{
    FILE *f = openMacroFileWrite(path);
    const bool openedForWriteOk = (f != nullptr);
    if (!openedForWriteOk) {
        table.setLastError(MACTAB_ERR_IO, "Failed to open macro file for write");
        return 0;
    }

    table.setLastError(MACTAB_ERR_OK, "");
#if defined(WIN32)
    fprintf(f, "\xEF\xBB\xBF;DO NOT DELETE THIS LINE*** version=%d ***\n", kUtf8Version);
#else
    fprintf(f, "DO NOT DELETE THIS LINE*** version=%d ***\n", kUtf8Version);
#endif

    const int n = table.getCount();
    std::vector<int> order((size_t)n);
    for (int i = 0; i < n; i++)
        order[(size_t)i] = i;

    std::sort(order.begin(), order.end(), [&table](int a, int b) {
        const StdVnChar *ka = table.getKey(a);
        const StdVnChar *kb = table.getKey(b);
        return compareStdVnKeys(ka, kb) < 0;
    });

    for (int i = 0; i < n; i++) {
        const bool last = (i == n - 1);
        const MacroEntry &row = table.m_entries[(size_t)order[(size_t)i]];
        if (writeOneRowUtf8(f, row, !last) != 0) {
            fclose(f);
            table.setLastError(MACTAB_ERR_IO, "Write macro row failed");
            return 0;
        }
    }

    fclose(f);
    return 1;
}
