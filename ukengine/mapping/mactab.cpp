// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file mactab.cpp
 * @brief CMacroTable: vector storage, hash lookup, optional binary cache.
 */

#include <algorithm>
#include <cctype>
#include <fstream>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <ukengine/mapping/keycons.h>
#include <ukengine/mapping/macro_cache.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

#define UKMACRO_VERSION_UTF8 1

namespace {

#define STD_TO_LOWER(x)                                                                            \
    (((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && !((x) & 1))       \
         ? (x + 1)                                                                                 \
         : (x))

std::string foldedLookupKeyBytes(const StdVnChar *s)
{
    std::string out;
    if (!s)
        return out;
    for (int i = 0; s[i] != 0; i++) {
        StdVnChar lc = STD_TO_LOWER(s[i]);
        const unsigned char *p = (const unsigned char *)&lc;
        for (size_t b = 0; b < sizeof(StdVnChar); b++)
            out.push_back((char)p[b]);
    }
    return out;
}

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

bool vnConvertGrow(std::vector<char> &buf, int charsetIn, const void *src, bool *ok)
{
    *ok = false;
    buf.resize(256);
    for (int attempt = 0; attempt < 24; attempt++) {
        int inLen = -1;
        int maxOut = (int)buf.size();
        int ret =
            VnConvert(charsetIn, CONV_CHARSET_VNSTANDARD, (UKBYTE *)src, (UKBYTE *)buf.data(), &inLen,
                      &maxOut);
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

bool utf8ToStdVnVector(const void *utf8, int charset, std::vector<StdVnChar> &out)
{
    std::vector<char> raw;
    bool ok = false;
    if (!vnConvertGrow(raw, charset, utf8, &ok) || !ok)
        return false;
    if (raw.empty())
        return false;
    const size_t nByte = raw.size();
    const size_t nUnits = nByte / sizeof(StdVnChar);
    const StdVnChar *p = (const StdVnChar *)raw.data();
    out.assign(p, p + nUnits);
    return true;
}

static std::string mactabFoldKeyPrefix(const char *line, size_t keyLen)
{
    std::string s(line, keyLen);
    for (char &c : s) {
        unsigned char u = (unsigned char)c;
        if (u < 128U)
            c = (char)std::tolower((int)u);
    }
    return s;
}

void stripTrailingNewline(char *line)
{
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = 0;
        len--;
    }
    if (len > 1 && line[len - 1] == '\r')
        line[len - 1] = 0;
}

FILE *openMacroFileRead(const char *fname)
{
#if defined(WIN32)
    return _tfopen(fname, _TEXT("rt"));
#else
    return fopen(fname, "r");
#endif
}

FILE *openMacroFileWrite(const char *fname)
{
#if defined(WIN32)
    return _tfopen(fname, _TEXT("wt"));
#else
    return fopen(fname, "w");
#endif
}

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

void readRemainingLines(FILE *f, std::vector<std::string> &lines)
{
    std::string line;
    while (readLineUnbounded(f, line))
        lines.push_back(std::move(line));
}

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

} // namespace

void CMacroTable::setLastError(int code, const char *msg)
{
    m_lastError = code;
    m_lastErrorMessage = (msg && msg[0]) ? msg : "";
}

void CMacroTable::init()
{
    resetContent();
}

void CMacroTable::resetContent()
{
    m_entries.clear();
    m_lookup.clear();
    setLastError(MACTAB_ERR_OK, "");
}

void CMacroTable::rebuildLookupMap()
{
    m_lookup.clear();
    for (size_t i = 0; i < m_entries.size(); i++) {
        const StdVnChar *pk = m_entries[i].key.empty() ? nullptr : m_entries[i].key.data();
        if (!pk)
            continue;
        std::string fold = foldedLookupKeyBytes(pk);
        m_lookup[std::move(fold)] = i;
    }
}

size_t CMacroTable::getOccupiedBytes() const
{
    size_t n = 0;
    for (const MacroEntry &e : m_entries) {
        n += e.key.size() * sizeof(StdVnChar) + e.text.size() * sizeof(StdVnChar);
    }
    return n;
}

const StdVnChar *CMacroTable::lookup(StdVnChar *key)
{
    if (!key || m_entries.empty())
        return nullptr;
    std::string fold = foldedLookupKeyBytes(key);
    auto it = m_lookup.find(fold);
    if (it == m_lookup.end())
        return nullptr;
    return m_entries[it->second].text.data();
}

const StdVnChar *CMacroTable::getKey(int idx) const
{
    if (idx < 0 || (size_t)idx >= m_entries.size())
        return nullptr;
    return m_entries[(size_t)idx].key.data();
}

const StdVnChar *CMacroTable::getText(int idx) const
{
    if (idx < 0 || (size_t)idx >= m_entries.size())
        return nullptr;
    return m_entries[(size_t)idx].text.data();
}

bool CMacroTable::readHeader(FILE *f, int &version)
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

void CMacroTable::writeHeader(FILE *f)
{
#if defined(WIN32)
    fprintf(f, "\xEF\xBB\xBF;DO NOT DELETE THIS LINE*** version=%d ***\n", UKMACRO_VERSION_UTF8);
#else
    fprintf(f, "DO NOT DELETE THIS LINE*** version=%d ***\n", UKMACRO_VERSION_UTF8);
#endif
}

int CMacroTable::addItem(const void *key, const void *text, int charset)
{
    if (!key || !text) {
        setLastError(MACTAB_ERR_PARSE, "Key or text is null");
        return -1;
    }

    try {
        std::vector<StdVnChar> keyVec;
        std::vector<StdVnChar> textVec;
        if (!utf8ToStdVnVector(key, charset, keyVec) || !utf8ToStdVnVector(text, charset, textVec)) {
            setLastError(MACTAB_ERR_CONVERT, "Conversion failed");
            return -1;
        }

        std::string fold = foldedLookupKeyBytes(keyVec.data());
        auto it = m_lookup.find(fold);
        if (it == m_lookup.end()) {
            MacroEntry e;
            e.key = std::move(keyVec);
            e.text = std::move(textVec);
            m_entries.push_back(std::move(e));
            m_lookup[fold] = m_entries.size() - 1;
            setLastError(MACTAB_ERR_OK, "");
            return (int)(m_entries.size() - 1);
        }
        const size_t idx = it->second;
        m_entries[idx].key = std::move(keyVec);
        m_entries[idx].text = std::move(textVec);
        setLastError(MACTAB_ERR_OK, "");
        return (int)idx;
    } catch (const std::bad_alloc &) {
        setLastError(MACTAB_ERR_OOM, "Out of memory for macro table");
        return -1;
    }
}

int CMacroTable::addItem(const char *item, int charset)
{
    const char *colon = strchr(item, ':');
    if (!colon) {
        setLastError(MACTAB_ERR_PARSE, "No ':' in macro line");
        return -1;
    }
    std::string key(item, colon - item);
    return addItem(key.c_str(), colon + 1, charset);
}

namespace {

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

int CMacroTable::writeToFile(const char *fname)
{
    FILE *f = openMacroFileWrite(fname);
    if (!f) {
        setLastError(MACTAB_ERR_IO, "Failed to open macro file for write");
        return 0;
    }

    setLastError(MACTAB_ERR_OK, "");
    writeHeader(f);

    const int n = (int)m_entries.size();
    std::vector<int> order((size_t)n);
    for (int i = 0; i < n; i++)
        order[(size_t)i] = i;

    std::sort(order.begin(), order.end(), [this](int a, int b) {
        const StdVnChar *ka = m_entries[(size_t)a].key.data();
        const StdVnChar *kb = m_entries[(size_t)b].key.data();
        return compareStdVnKeys(ka, kb) < 0;
    });

    for (int i = 0; i < n; i++) {
        const bool last = (i == n - 1);
        if (writeOneRowUtf8(f, m_entries[(size_t)order[(size_t)i]], !last) != 0) {
            fclose(f);
            setLastError(MACTAB_ERR_IO, "Write macro row failed");
            return 0;
        }
    }

    fclose(f);
    MacroBinaryCache::persistForTextFile(fname, *this);
    return 1;
}

int CMacroTable::loadFromFile(const char *fname)
{
    resetContent();

    if (MacroBinaryCache::tryLoadForTextFile(fname, *this)) {
        setLastError(MACTAB_ERR_OK, "");
        return 1;
    }

    resetContent();

    FILE *f = openMacroFileRead(fname);
    if (!f) {
        setLastError(MACTAB_ERR_IO, "Failed to open macro file for read");
        return 0;
    }

    int version = 0;
    if (!readHeader(f, version)) {
        fclose(f);
        setLastError(MACTAB_ERR_IO, "Failed to read macro file header");
        return 0;
    }

    std::vector<std::string> allLines;
    readRemainingLines(f, allLines);
    fclose(f);

    const bool isUtf8Version = (version == UKMACRO_VERSION_UTF8);
    const int charset = isUtf8Version ? CONV_CHARSET_UNIUTF8 : CONV_CHARSET_VIQR;

    std::unordered_map<std::string, size_t> lastLineForFoldedKey;
    buildLastWinsLineIndex(allLines, lastLineForFoldedKey);

    const bool anyLineFailed =
        applyLoadedLinesToTable(this, allLines, charset, lastLineForFoldedKey);

    if (anyLineFailed) {
        if (m_lastError != MACTAB_ERR_OOM)
            setLastError(MACTAB_ERR_INCOMPLETE, "One or more macro lines could not be added");
        return 0;
    }

    setLastError(MACTAB_ERR_OK, "");

    if (version != UKMACRO_VERSION_UTF8)
        writeToFile(fname);
    else
        MacroBinaryCache::persistForTextFile(fname, *this);

    return 1;
}
