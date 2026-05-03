// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * @file mactab.cpp
 * @brief CMacroTable: vector storage, hash lookup; load/save via TextMacroFormat + CacheManagement.
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

int CMacroTable::writeToFile(const char *fname)
{
    TextMacroFormat textFmt;
    if (textFmt.exportToPath(fname, *this) != 1)
        return 0;
    CacheManagement::persist(fname, *this);
    return 1;
}

int CMacroTable::loadFromFile(const char *fname)
{
    resetContent();

    if (CacheManagement::tryLoad(fname, *this)) {
        setLastError(MACTAB_ERR_OK, "");
        return 1;
    }

    resetContent();

    int version = 0;
    TextMacroFormat textFmt;
    if (textFmt.importFromPath(fname, *this, &version) != 1)
        return 0;

    setLastError(MACTAB_ERR_OK, "");

    if (version != TextMacroFormat::kUtf8Version)
        return writeToFile(fname);

    CacheManagement::persist(fname, *this);
    return 1;
}
