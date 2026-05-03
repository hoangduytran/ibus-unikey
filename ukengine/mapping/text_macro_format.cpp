// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file text_macro_format.cpp
 * @brief UniKey TEXT macro import/export (MacroFormat / TextMacroFormat).
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

int TextMacroFormat::importFromPath(const char *path, CMacroTable &table, int *outSourceVersion)
{
    FILE *f = openMacroFileRead(path);
    if (!f) {
        table.setLastError(MACTAB_ERR_IO, "Failed to open macro file for read");
        return 0;
    }

    int version = 0;
    if (!readVersionHeader(f, version)) {
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

int TextMacroFormat::exportToPath(const char *path, CMacroTable &table)
{
    FILE *f = openMacroFileWrite(path);
    if (!f) {
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
