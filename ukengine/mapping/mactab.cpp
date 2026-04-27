// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4;
// indent-tabs-mode:nil -*-
/* Unikey Vietnamese Input Method
 * Copyright (C) 2000-2005 Pham Kim Long
 * Contact:
 *   unikey@gmail.com
 *   UniKey Project: http://unikey.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * @file mactab.cpp
 * @brief Implementation of CMacroTable: in-memory storage, file load/save,
 * sort, lookup.
 */

#include <cctype>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <ukengine/mapping/keycons.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

/**
 * @brief Macro file version identifier for UTF-8 encoded macro tables.
 * Used to distinguish between legacy VIQR and modern UTF-8 macro files.
 */
#define UKMACRO_VERSION_UTF8 1

/**
 * @brief Normalize the key prefix (bytes before `:`) for last-wins duplicate
 * detection.
 *
 * Only bytes < 128 are lowercased so UTF-8 multibyte sequences are left
 * unchanged. This matches typical Latin macro keys without adding a Unicode
 * dependency in ukengine.
 */
static std::string mactabFoldKeyPrefix(const char *line, size_t keyLen) {
    std::string s(line, keyLen);
    for (char &c : s) {
        unsigned char u = (unsigned char)c;
        if (u < 128U)
            c = (char)std::tolower((int)u);
    }
    return s;
}

namespace {

/** Clamp UTF-8 key prefix length used for fold / last-wins index. */
size_t clampMacroKeyPrefixLen(size_t rawLen) {
    const size_t maxPrefix = (size_t)(MAX_MACRO_KEY_LEN - 1);
    const bool tooLong = rawLen > maxPrefix;
    return tooLong ? maxPrefix : rawLen;
}

/** Remove optional trailing `\\n` and `\\r\\n` from a buffer filled by `fgets`. */
void stripTrailingNewline(char *line) {
    size_t len = strlen(line);
    const bool endsWithLf = len > 0 && line[len - 1] == '\n';
    if (endsWithLf) {
        line[len - 1] = 0;
        len--;
    }
    const bool hasCrBeforeNul = len > 1 && line[len - 1] == '\r';
    if (hasCrBeforeNul)
        line[len - 1] = 0;
}

FILE *openMacroFileRead(const char *fname) {
#if defined(WIN32)
    return _tfopen(fname, _TEXT("rt"));
#else
    return fopen(fname, "r");
#endif
}

FILE *openMacroFileWrite(const char *fname) {
#if defined(WIN32)
    return _tfopen(fname, _TEXT("wt"));
#else
    return fopen(fname, "w");
#endif
}

/** Read every remaining line after the header pass; strips newlines. */
void readRemainingLines(FILE *f, std::vector<std::string> &out) {
    char line[MAX_MACRO_LINE];
    while (fgets(line, sizeof(line), f)) {
        stripTrailingNewline(line);
        out.emplace_back(line);
    }
}

/**
 * First pass: for each `key:text` line, record last line index per folded key.
 */
void buildLastWinsLineIndex(const std::vector<std::string> &allLines,
                            std::unordered_map<std::string, size_t> &lastLineForFoldedKey) {
    for (size_t i = 0; i < allLines.size(); i++) {
        const char *L = allLines[i].c_str();
        const char *colon = strchr(L, ':');
        const bool hasKeyValuePair = (colon != NULL);
        if (!hasKeyValuePair)
            continue;

        const size_t keyLen = clampMacroKeyPrefixLen((size_t)(colon - L));
        const std::string folded = mactabFoldKeyPrefix(L, keyLen);
        lastLineForFoldedKey[folded] = i;
    }
}

/**
 * Second pass: add lines to the table; key:value rows use last-wins only.
 * @return true if any `addItem` failed.
 */
bool applyLoadedLinesToTable(CMacroTable *table, const std::vector<std::string> &allLines, int charset,
                             const std::unordered_map<std::string, size_t> &lastLineForFoldedKey) {
    bool anyLineFailed = false;

    for (size_t i = 0; i < allLines.size(); i++) {
        const char *L = allLines[i].c_str();
        const char *colon = strchr(L, ':');
        const bool hasKeyValuePair = (colon != NULL);

        if (!hasKeyValuePair) {
            const int rc = table->addItem(L, charset);
            const bool addedOk = (rc >= 0);
            if (!addedOk)
                anyLineFailed = true;
            continue;
        }

        const size_t keyLen = clampMacroKeyPrefixLen((size_t)(colon - L));
        const std::string folded = mactabFoldKeyPrefix(L, keyLen);
        const auto winnerIt = lastLineForFoldedKey.find(folded);
        const bool hasWinner = (winnerIt != lastLineForFoldedKey.end());
        const bool isWinningRow = hasWinner && (winnerIt->second == i);

        if (!isWinningRow)
            continue;

        const int rc = table->addItem(L, charset);
        const bool addedOk = (rc >= 0);
        if (!addedOk)
            anyLineFailed = true;
    }
    return anyLineFailed;
}

/**
 * Convert one stored macro row to UTF-8 and write `key:text` with optional newline.
 * @return 0 on success, -1 if either VnConvert failed.
 */
int appendMacroRowAsUtf8(FILE *f, const char *memBase, const MacroDef &def, bool addTrailingNewline) {
    int inLen;
    int maxOutLen;
    int ret;

    char line[MAX_MACRO_LINE * 3 + 1];
    char key[MAX_MACRO_KEY_LEN * 3];
    char text[MAX_MACRO_TEXT_LEN * 3];

    UKBYTE *keySrc = (UKBYTE *)(memBase + def.keyOffset);
    inLen = -1;
    maxOutLen = (int)sizeof(key);
    ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8, keySrc, (UKBYTE *)key, &inLen,
                    &maxOutLen);
    const bool keyOk = (ret == 0);
    if (!keyOk)
        return -1;

    UKBYTE *textSrc = (UKBYTE *)(memBase + def.textOffset);
    inLen = -1;
    maxOutLen = (int)sizeof(text);
    ret =
        VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8, textSrc, (UKBYTE *)text, &inLen,
                  &maxOutLen);
    const bool textOk = (ret == 0);
    if (!textOk)
        return -1;

    if (addTrailingNewline)
        sprintf(line, "%s:%s\n", key, text);
    else
        sprintf(line, "%s:%s", key, text);
    fputs(line, f);
    return 0;
}

} // namespace

/**
 * @brief Record error code and message for the last failed operation.
 * @param code MACTAB_ERR_* value
 * @param msg  optional null-terminated message (empty clears description)
 */
void CMacroTable::setLastError(int code, const char *msg) {
    m_lastError = code;
    m_lastErrorMessage = (msg && msg[0]) ? msg : "";
}

/**
 * @brief Clear all entries and error state; used at startup and after reset.
 */
void CMacroTable::init() {
    m_table.clear();
    m_macroMem.clear();
    m_occupied = 0;
    setLastError(MACTAB_ERR_OK, "");
}

/**
 * @brief Base address of macro key/text blob while sorting and searching.
 *
 * Filled with @ref CMacroTable::m_macroMem data pointer before qsort and
 * bsearch. Not thread-safe: assume single-threaded use during sort/lookup.
 */
static char *MacCompareStartMem;

#define STD_TO_LOWER(x)                                                                            \
    (((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && !((x) & 1))       \
         ? (x + 1)                                                                                 \
         : (x))

/**
 * @brief qsort comparison: compare two @ref MacroDef entries by their stored
 * key strings.
 * @param p1 pointer to first MacroDef
 * @param p2 pointer to second MacroDef
 * @return &lt;0, 0, or &gt;0 per qsort contract
 */
int macCompare(const void *p1, const void *p2) {
    StdVnChar *s1 = (StdVnChar *)((char *)MacCompareStartMem + ((MacroDef *)p1)->keyOffset);
    StdVnChar *s2 = (StdVnChar *)((char *)MacCompareStartMem + ((MacroDef *)p2)->keyOffset);

    int i;
    StdVnChar ls1, ls2;

    for (i = 0; s1[i] != 0 && s2[i] != 0; i++) {
        ls1 = STD_TO_LOWER(s1[i]);
        ls2 = STD_TO_LOWER(s2[i]);
        if (ls1 > ls2)
            return 1;
        if (ls1 < ls2)
            return -1;
    }
    if (s1[i] == 0)
        return (s2[i] == 0) ? 0 : -1;
    return 1;
}

/**
 * @brief bsearch comparison: compare search key to one @ref MacroDef key.
 * @param key null-terminated StdVnChar search key
 * @param ele pointer to MacroDef in the table
 * @return &lt;0, 0, or &gt;0 per bsearch contract
 */
int macKeyCompare(const void *key, const void *ele) {
    StdVnChar *s1 = (StdVnChar *)key;
    StdVnChar *s2 = (StdVnChar *)((char *)MacCompareStartMem + ((MacroDef *)ele)->keyOffset);

    StdVnChar ls1, ls2;
    int i;

    for (i = 0; s1[i] != 0 && s2[i] != 0; i++) {
        ls1 = STD_TO_LOWER(s1[i]);
        ls2 = STD_TO_LOWER(s2[i]);
        if (ls1 > ls2)
            return 1;
        if (ls1 < ls2)
            return -1;
    }
    if (s1[i] == 0)
        return (s2[i] == 0) ? 0 : -1;
    return 1;
}

/**
 * @brief Find replacement text for a key in the sorted table (binary search).
 * @param key null-terminated search key in internal encoding
 * @return pointer to replacement in @ref m_macroMem, or nullptr
 */
const StdVnChar *CMacroTable::lookup(StdVnChar *key) {
    const bool tableEmpty = m_table.empty();
    const bool memEmpty = m_macroMem.empty();
    const bool cannotSearch = tableEmpty || memEmpty;
    if (cannotSearch)
        return 0;

    MacCompareStartMem = m_macroMem.data();
    MacroDef *p = (MacroDef *)bsearch(key, m_table.data(), m_table.size(), sizeof(MacroDef),
                                      macKeyCompare);
    const bool found = (p != NULL);
    if (found)
        return (StdVnChar *)(m_macroMem.data() + p->textOffset);
    return 0;
}

/**
 * @brief Read optional UTF-8 macro file header and detect format version.
 *
 * If the first line is missing a valid `***version=n` marker, seeks to offset 0
 * and sets
 * @a version to 0 (legacy file). On empty file after EOF, also sets version 0
 * and returns true. Header line format may include BOM and text such as: `…***
 * version=1 ***`
 *
 * @param f        open FILE positioned after any initial read; first line
 * consumed when present
 * @param version  output: 0 = legacy, @ref UKMACRO_VERSION_UTF8 for UTF-8
 * tables
 * @return false on read error; true if header was handled or file is
 * empty-legacy
 */
bool CMacroTable::readHeader(FILE *f, int &version) {
    char line[MAX_MACRO_LINE];
    const bool readOk = (fgets(line, sizeof(line), f) != NULL);
    if (!readOk) {
        const bool atEof = (feof(f) != 0);
        if (atEof) {
            fseek(f, 0, SEEK_SET);
            version = 0;
            return true;
        }
        return false;
    }

    char *p = line;
    size_t len = strlen(line);
    const bool hasUtf8Bom =
        len >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF;
    if (hasUtf8Bom)
        p += 3;

    p = strstr(p, "***");
    const bool foundMarker = (p != NULL);
    if (foundMarker) {
        p += 3;
        while (*p == ' ')
            p++;
        const bool parsedVersion = (sscanf(p, "version=%d", &version) == 1);
        if (parsedVersion)
            return true;
    }

    fseek(f, 0, SEEK_SET);
    version = 0;
    return true;
}

/**
 * @brief Write the standard UTF-8 macro file header (and BOM on Windows) at
 * file start.
 * @param f open FILE in text write mode, positioned at beginning
 */
void CMacroTable::writeHeader(FILE *f) {
#if defined(WIN32)
    fprintf(f, "\xEF\xBB\xBF;DO NOT DELETE THIS LINE*** version=%d ***\n", UKMACRO_VERSION_UTF8);
#else
    fprintf(f, "DO NOT DELETE THIS LINE*** version=%d ***\n", UKMACRO_VERSION_UTF8);
#endif
}

/**
 * @brief Load macros from path: read all lines, apply last-wins for duplicate
 * keys, sort, optional rewrite to UTF-8.
 * @return 0 on failure, 1 on success (see mactab.h and @ref setLastError)
 */
int CMacroTable::loadFromFile(const char *fname) {
    // Progression: open file -> parse header -> collect lines -> last-wins merge -> sort -> finalize state
    FILE *f = openMacroFileRead(fname);
    const bool opened = (f != NULL);
    if (!opened) {
        setLastError(MACTAB_ERR_IO, "Failed to open macro file for read");
        return 0;
    }

    // Stage 1: reset table and read format version header.
    resetContent();
    int version = 0;
    const bool headerRead = readHeader(f, version);
    if (!headerRead) {
        setLastError(MACTAB_ERR_IO, "Failed to read macro file header");
        fclose(f);
        return 0;
    }

    // Stage 2: read raw lines into memory for two-pass last-wins processing.
    std::vector<std::string> allLines;
    readRemainingLines(f, allLines);
    fclose(f);

    const bool isUtf8Version = (version == UKMACRO_VERSION_UTF8);
    const int charset = isUtf8Version ? CONV_CHARSET_UNIUTF8 : CONV_CHARSET_VIQR;

    // Stage 3: build folded-key index so duplicate keys keep the last row only.
    std::unordered_map<std::string, size_t> lastLineForFoldedKey;
    buildLastWinsLineIndex(allLines, lastLineForFoldedKey);

    // Stage 4: add rows to table; non-winning duplicates are skipped by design.
    const bool anyLineFailed =
        applyLoadedLinesToTable(this, allLines, charset, lastLineForFoldedKey);

    // Stage 5: sort for deterministic binary-search lookup.
    MacCompareStartMem = m_macroMem.data();
    const bool hasEntries = !m_table.empty();
    if (hasEntries)
        qsort(m_table.data(), m_table.size(), sizeof(MacroDef), macCompare);

    if (anyLineFailed) {
        const bool wasOom = (m_lastError == MACTAB_ERR_OOM);
        if (!wasOom)
            setLastError(MACTAB_ERR_INCOMPLETE, "One or more macro lines could not be added");
        return 0;
    }

    const bool wasOomAfterSuccess = (m_lastError == MACTAB_ERR_OOM);
    if (!wasOomAfterSuccess)
        setLastError(MACTAB_ERR_OK, "");

    // Stage 6: legacy input gets rewritten to UTF-8 header format after successful load.
    const bool needsLegacyRewrite = (version != UKMACRO_VERSION_UTF8);
    if (needsLegacyRewrite)
        writeToFile(fname);
    return 1;
}

/**
 * @brief Write the current table to path as UTF-8 `key:text` lines with a
 * version header.
 * @return 0 on open/write failure, 1 on success
 */
int CMacroTable::writeToFile(const char *fname) {
    // Progression: open -> header -> row conversion loop -> close.
    FILE *f = openMacroFileWrite(fname);
    const bool opened = (f != NULL);
    if (!opened) {
        setLastError(MACTAB_ERR_IO, "Failed to open macro file for write");
        return 0;
    }

    setLastError(MACTAB_ERR_OK, "");
    writeHeader(f);

    const char *memBase = m_macroMem.data();
    const int n = (int)m_table.size();
    for (int i = 0; i < n; i++) {
        const bool isLastRow = (i == n - 1);
        const bool addNewline = !isLastRow;
        appendMacroRowAsUtf8(f, memBase, m_table[(size_t)i], addNewline);
    }

    fclose(f);
    return 1;
}

/**
 * @brief Add one macro: convert key and text to internal encoding and append to
 * @ref m_table / @ref m_macroMem.
 * @return new row index, or -1 (see mactab.h, @ref setLastError)
 */
int CMacroTable::addItem(const void *key, const void *text, int charset) {
    // Transactional progression: reserve/convert key -> reserve/convert text -> append row metadata.
    int ret;
    int inLen, maxOutLen;
    const int oldOccupied = m_occupied;
    const size_t oldRowCount = m_table.size();

    const bool nullKeyOrText = (!key || !text);
    if (nullKeyOrText) {
        setLastError(MACTAB_ERR_PARSE, "Key or text is null");
        return -1;
    }

    try {
        // Step A: convert key bytes to internal VNSTANDARD and advance occupied cursor.
        const int keyOff = m_occupied;
        {
            const size_t need =
                (size_t)keyOff + (size_t)MAX_MACRO_KEY_LEN * sizeof(StdVnChar) + 1u;
            const bool needGrow = (m_macroMem.size() < need);
            if (needGrow)
                m_macroMem.resize(need);
        }
        int maxKeyOut = (int)((size_t)MAX_MACRO_KEY_LEN * sizeof(StdVnChar));
        char *keyDst = m_macroMem.data() + (size_t)keyOff;
        inLen = -1;
        maxOutLen = maxKeyOut;
        ret =
            VnConvert(charset, CONV_CHARSET_VNSTANDARD, (UKBYTE *)key, (UKBYTE *)keyDst, &inLen,
                      &maxOutLen);
        const bool keyConvertOk = (ret == 0);
        if (!keyConvertOk) {
            m_macroMem.resize((size_t)oldOccupied);
            setLastError(MACTAB_ERR_CONVERT, "Key conversion failed");
            return -1;
        }
        m_occupied = keyOff + maxOutLen;

        // Step B: convert text bytes and keep rollback semantics on conversion failure.
        const int offAfterKey = m_occupied;
        const int textOff = offAfterKey;
        {
            const size_t need =
                (size_t)offAfterKey + (size_t)MAX_MACRO_TEXT_LEN * sizeof(StdVnChar) + 1u;
            const bool needGrow = (m_macroMem.size() < need);
            if (needGrow)
                m_macroMem.resize(need);
        }
        const int maxTextBuf = (int)((size_t)MAX_MACRO_TEXT_LEN * sizeof(StdVnChar));
        char *textDst = m_macroMem.data() + (size_t)textOff;
        inLen = -1;
        maxOutLen = maxTextBuf;
        ret = VnConvert(charset, CONV_CHARSET_VNSTANDARD, (UKBYTE *)text, (UKBYTE *)textDst,
                        &inLen, &maxOutLen);
        const bool textConvertOk = (ret == 0);
        if (!textConvertOk) {
            m_occupied = oldOccupied;
            m_macroMem.resize((size_t)oldOccupied);
            setLastError(MACTAB_ERR_CONVERT, "Text conversion failed");
            return -1;
        }
        m_occupied = offAfterKey + maxOutLen;

        // Step C: commit metadata only after both conversions succeed.
        MacroDef def;
        def.keyOffset = keyOff;
        def.textOffset = textOff;
        m_table.push_back(def);
        setLastError(MACTAB_ERR_OK, "");
        return (int)m_table.size() - 1;
    } catch (const std::bad_alloc &) {
        m_occupied = oldOccupied;
        m_table.resize(oldRowCount);
        try {
            m_macroMem.resize((size_t)oldOccupied);
        } catch (const std::bad_alloc &) {
        }
        setLastError(MACTAB_ERR_OOM, "Out of memory for macro table");
        return -1;
    }
}

/**
 * @brief Parse a single `key:text` line and delegate to @ref addItem(const
 * void*, const void*, int).
 */
int CMacroTable::addItem(const char *item, int charset) {
    char key[MAX_MACRO_KEY_LEN];

    char *pos = (char *)strchr(item, ':');
    const bool hasSeparator = (pos != NULL);
    if (!hasSeparator) {
        setLastError(MACTAB_ERR_PARSE, "No ':' in macro line");
        return -1;
    }
    int keyLen = (int)(pos - item);
    if (keyLen > MAX_MACRO_KEY_LEN - 1)
        keyLen = MAX_MACRO_KEY_LEN - 1;
    strncpy(key, item, (size_t)keyLen);
    key[keyLen] = '\0';
    return addItem(key, ++pos, charset);
}

/**
 * @brief Remove all stored macros and clear buffers (same as empty @ref init
 * state).
 */
void CMacroTable::resetContent() {
    m_occupied = 0;
    m_table.clear();
    m_macroMem.clear();
    setLastError(MACTAB_ERR_OK, "");
}

/**
 * @brief Return a pointer to the key sequence for entry @a idx in internal
 * encoding.
 * @return nullptr if @a idx is out of range or memory is empty
 */
const StdVnChar *CMacroTable::getKey(int idx) {
    const bool badIndex = (idx < 0 || (size_t)idx >= m_table.size());
    const bool memEmpty = m_macroMem.empty();
    const bool invalid = badIndex || memEmpty;
    if (invalid)
        return 0;
    return (const StdVnChar *)(m_macroMem.data() + m_table[(size_t)idx].keyOffset);
}

/**
 * @brief Return a pointer to the replacement text for entry @a idx in internal
 * encoding.
 * @return nullptr if @a idx is out of range or memory is empty
 */
const StdVnChar *CMacroTable::getText(int idx) {
    const bool badIndex = (idx < 0 || (size_t)idx >= m_table.size());
    const bool memEmpty = m_macroMem.empty();
    const bool invalid = badIndex || memEmpty;
    if (invalid)
        return 0;
    return (const StdVnChar *)(m_macroMem.data() + m_table[(size_t)idx].textOffset);
}
