// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/* Unikey Vietnamese Input Method
 * Copyright (C) 2000-2005 Pham Kim Long
 * Contact:
 *   unikey@gmail.com
 *   UniKey project: http://unikey.org
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

#include <cctype>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "mactab.h"
#include "vnconv.h"

/**
 * @brief Macro file version identifier for UTF-8 encoded macro tables.
 * Used to distinguish between legacy VIQR and modern UTF-8 macro files.
 */
#define UKMACRO_VERSION_UTF8 1

// ASCII/byte-wise fold for macro file key (same family as strcasecmp for Latin keys)
static std::string mactabFoldKeyPrefix(const char *line, size_t keyLen)
{
    std::string s(line, keyLen);
    for (char &c : s)
    {
        unsigned char u = (unsigned char)c;
        if (u < 128U)
            c = (char)std::tolower((int)u);
    }
    return s;
}

//---------------------------------------------------------------
void CMacroTable::setLastError(int code, const char *msg)
{
    m_lastError = code;
    m_lastErrorMessage = (msg && msg[0]) ? msg : "";
}

//---------------------------------------------------------------
void CMacroTable::init()
{
    m_table.clear();
    m_macroMem.clear();
    m_occupied = 0;
    setLastError(MACTAB_ERR_OK, "");
}

//---------------------------------------------------------------

/**
 * @brief Global pointer to the start of macro memory for comparison routines.
 *
 * Used by macCompare and macKeyCompare to resolve key/text offsets in macro memory.
 * WARNING: This is not thread-safe and assumes single-threaded access during sort/search.
 */
static char *MacCompareStartMem;

#define STD_TO_LOWER(x) (((x) >= VnStdCharOffset &&                        \
                          (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && \
                          !((x) & 1))                                      \
                             ? (x + 1)                                     \
                             : (x))

int macCompare(const void *p1, const void *p2)
{
    // Resolve macro key pointers from memory offsets
    StdVnChar *s1 = (StdVnChar *)((char *)MacCompareStartMem + ((MacroDef *)p1)->keyOffset);
    StdVnChar *s2 = (StdVnChar *)((char *)MacCompareStartMem + ((MacroDef *)p2)->keyOffset);

    int i;
    StdVnChar ls1, ls2;

    for (i = 0; s1[i] != 0 && s2[i] != 0; i++)
    {
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

//---------------------------------------------------------------
int macKeyCompare(const void *key, const void *ele)
{
    StdVnChar *s1 = (StdVnChar *)key;
    StdVnChar *s2 = (StdVnChar *)((char *)MacCompareStartMem + ((MacroDef *)ele)->keyOffset);

    StdVnChar ls1, ls2;
    int i;

    for (i = 0; s1[i] != 0 && s2[i] != 0; i++)
    {
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

//---------------------------------------------------------------
const StdVnChar *CMacroTable::lookup(StdVnChar *key)
{
    if (m_table.empty() || m_macroMem.empty())
        return 0;

    MacCompareStartMem = m_macroMem.data();
    MacroDef *p = (MacroDef *)bsearch(key, m_table.data(), m_table.size(), sizeof(MacroDef), macKeyCompare);
    if (p)
        return (StdVnChar *)(m_macroMem.data() + p->textOffset);
    return 0;
}

//----------------------------------------------------------------------------
// Read header, if it's present in the file. Get the version of the file
// If header is absent, go back to the beginning of file and set version to 0
// Return false if reading failed.
//
// Header format: ;[DO NOT DELETE THIS LINE]***version=n
//----------------------------------------------------------------------------
bool CMacroTable::readHeader(FILE *f, int &version)
{
    char line[MAX_MACRO_LINE];
    if (!fgets(line, sizeof(line), f))
    {
        if (feof(f))
        {
            fseek(f, 0, SEEK_SET);
            version = 0;
            return true;
        }
        return false;
    }

    char *p = line;
    size_t len = strlen(line);
    if (len >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF)
    {
        p += 3;
    }

    p = strstr(p, "***");
    if (p)
    {
        p += 3;
        while (*p == ' ')
            p++;
        if (sscanf(p, "version=%d", &version) == 1)
            return true;
    }

    fseek(f, 0, SEEK_SET);
    version = 0;
    return true;
}

//----------------------------------------------------------------
void CMacroTable::writeHeader(FILE *f)
{
#if defined(WIN32)
    fprintf(f, "\xEF\xBB\xBF;DO NOT DELETE THIS LINE*** version=%d ***\n", UKMACRO_VERSION_UTF8);
#else
    fprintf(f, "DO NOT DELETE THIS LINE*** version=%d ***\n", UKMACRO_VERSION_UTF8);
#endif
}

int CMacroTable::loadFromFile(const char *fname)
{
    FILE *f;
#if defined(WIN32)
    f = _tfopen(fname, _TEXT("rt"));
#else
    f = fopen(fname, "r");
#endif

    if (f == NULL)
    {
        setLastError(MACTAB_ERR_IO, "Failed to open macro file for read");
        return 0;
    }

    char line[MAX_MACRO_LINE];
    size_t len;

    resetContent();
    int version;
    if (!readHeader(f, version))
    {
        setLastError(MACTAB_ERR_IO, "Failed to read macro file header");
        fclose(f);
        return 0;
    }
    std::vector<std::string> all_lines;
    while (fgets(line, sizeof(line), f))
    {
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = 0;
        if (len > 1 && line[len - 2] == '\r')
            line[len - 2] = 0;
        all_lines.push_back(std::string(line));
    }
    fclose(f);

    const int charset = (version == UKMACRO_VERSION_UTF8) ? CONV_CHARSET_UNIUTF8 : CONV_CHARSET_VIQR;

    // Last wins for duplicate keys (case-insensitive ASCII fold on the key part before ':')
    std::unordered_map<std::string, size_t> last_line_for_key;
    for (size_t i = 0; i < all_lines.size(); i++)
    {
        const char *L = all_lines[i].c_str();
        const char *colon = strchr(L, ':');
        if (colon == NULL)
            continue;
        size_t keyLen = (size_t)(colon - L);
        if (keyLen > (size_t)(MAX_MACRO_KEY_LEN - 1))
            keyLen = (size_t)(MAX_MACRO_KEY_LEN - 1);
        last_line_for_key[mactabFoldKeyPrefix(L, keyLen)] = i;
    }

    bool anyLineFailed = false;
    for (size_t i = 0; i < all_lines.size(); i++)
    {
        const char *L = all_lines[i].c_str();
        const char *colon = strchr(L, ':');
        if (colon == NULL)
        {
            if (addItem(L, charset) < 0)
                anyLineFailed = true;
            continue;
        }
        size_t keyLen = (size_t)(colon - L);
        if (keyLen > (size_t)(MAX_MACRO_KEY_LEN - 1))
            keyLen = (size_t)(MAX_MACRO_KEY_LEN - 1);
        if (last_line_for_key[mactabFoldKeyPrefix(L, keyLen)] != i)
            continue;
        if (addItem(L, charset) < 0)
            anyLineFailed = true;
    }

    MacCompareStartMem = m_macroMem.data();
    if (!m_table.empty())
        qsort(m_table.data(), m_table.size(), sizeof(MacroDef), macCompare);

    if (anyLineFailed)
    {
        if (m_lastError != MACTAB_ERR_OOM)
            setLastError(MACTAB_ERR_INCOMPLETE, "One or more macro lines could not be added");
        return 0;
    }
    if (m_lastError != MACTAB_ERR_OOM)
        setLastError(MACTAB_ERR_OK, "");

    if (version != UKMACRO_VERSION_UTF8)
    {
        writeToFile(fname);
    }
    return 1;
}

//---------------------------------------------------------------
int CMacroTable::writeToFile(const char *fname)
{
    int ret;
    int inLen, maxOutLen;
    FILE *f;
#if defined(WIN32)
    f = _tfopen(fname, _TEXT("wt"));
#else
    f = fopen(fname, "w");
#endif

    if (f == NULL)
    {
        setLastError(MACTAB_ERR_IO, "Failed to open macro file for write");
        return 0;
    }

    setLastError(MACTAB_ERR_OK, "");
    char line[MAX_MACRO_LINE * 3 + 1];
    char key[MAX_MACRO_KEY_LEN * 3];
    char text[MAX_MACRO_TEXT_LEN * 3];

    writeHeader(f);

    UKBYTE *p;
    for (int i = 0; i < (int)m_table.size(); i++)
    {
        p = (UKBYTE *)m_macroMem.data() + m_table[i].keyOffset;
        inLen = -1;
        maxOutLen = sizeof(key);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8,
                        (UKBYTE *)p, (UKBYTE *)key,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue;

        p = (UKBYTE *)m_macroMem.data() + m_table[i].textOffset;
        inLen = -1;
        maxOutLen = sizeof(text);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8,
                        p, (UKBYTE *)text,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue;

        if (i < (int)m_table.size() - 1)
            sprintf(line, "%s:%s\n", key, text);
        else
            sprintf(line, "%s:%s", key, text);
        fputs(line, f);
    }

    fclose(f);
    return 1;
}

/**
 * @brief Grow macroMem to at least `need` bytes; on failure, leaves table unchanged
 * and throws std::bad_alloc.
 */
static void mactabEnsureBytes(std::vector<char> *mem, size_t need)
{
    if (mem->size() < need)
        mem->resize(need);
}

int CMacroTable::addItem(const void *key, const void *text, int charset)
{
    int ret;
    int inLen, maxOutLen;
    const int old_occupied = m_occupied;
    const size_t old_n = m_table.size();

    if (!key || !text)
    {
        setLastError(MACTAB_ERR_PARSE, "Key or text is null");
        return -1;
    }

    try
    {
        const int keyOff = m_occupied;
        mactabEnsureBytes(&m_macroMem,
            (size_t)keyOff + (size_t)MAX_MACRO_KEY_LEN * sizeof(StdVnChar) + 1u);
        {
            int maxKeyOut = (int)((size_t)MAX_MACRO_KEY_LEN * sizeof(StdVnChar));
            char *p = m_macroMem.data() + (size_t)keyOff;
            inLen = -1;
            maxOutLen = maxKeyOut;
            ret = VnConvert(charset, CONV_CHARSET_VNSTANDARD,
                            (UKBYTE *)key, (UKBYTE *)p,
                            &inLen, &maxOutLen);
            if (ret != 0)
            {
                m_macroMem.resize((size_t)old_occupied);
                setLastError(MACTAB_ERR_CONVERT, "Key conversion failed");
                return -1;
            }
        }
        int offAfterKey = keyOff + maxOutLen;
        const int textOff = offAfterKey;
        mactabEnsureBytes(&m_macroMem,
            (size_t)offAfterKey + (size_t)MAX_MACRO_TEXT_LEN * sizeof(StdVnChar) + 1u);
        {
            int maxTextOut = (int)((size_t)MAX_MACRO_TEXT_LEN * sizeof(StdVnChar));
            char *p2 = m_macroMem.data() + (size_t)textOff;
            inLen = -1;
            maxOutLen = maxTextOut;
            ret = VnConvert(charset, CONV_CHARSET_VNSTANDARD,
                            (UKBYTE *)text, (UKBYTE *)p2,
                            &inLen, &maxOutLen);
            if (ret != 0)
            {
                m_occupied = old_occupied;
                m_macroMem.resize((size_t)old_occupied);
                setLastError(MACTAB_ERR_CONVERT, "Text conversion failed");
                return -1;
            }
            m_occupied = offAfterKey + maxOutLen;
        }
        MacroDef def;
        def.keyOffset = keyOff;
        def.textOffset = textOff;
        m_table.push_back(def);
        setLastError(MACTAB_ERR_OK, "");
        return (int)m_table.size() - 1;
    }
    catch (const std::bad_alloc &)
    {
        m_occupied = old_occupied;
        m_table.resize(old_n);
        try
        {
            m_macroMem.resize((size_t)old_occupied);
        }
        catch (const std::bad_alloc &)
        {
        }
        setLastError(MACTAB_ERR_OOM, "Out of memory for macro table");
        return -1;
    }
}

int CMacroTable::addItem(const char *item, int charset)
{
    char key[MAX_MACRO_KEY_LEN];

    char *pos = (char *)strchr(item, ':');
    if (pos == NULL)
    {
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

void CMacroTable::resetContent()
{
    m_occupied = 0;
    m_table.clear();
    m_macroMem.clear();
    setLastError(MACTAB_ERR_OK, "");
}

const StdVnChar *CMacroTable::getKey(int idx)
{
    if (idx < 0 || (size_t)idx >= m_table.size())
        return 0;
    if (m_macroMem.empty())
        return 0;
    return (const StdVnChar *)(m_macroMem.data() + m_table[(size_t)idx].keyOffset);
}

const StdVnChar *CMacroTable::getText(int idx)
{
    if (idx < 0 || (size_t)idx >= m_table.size())
        return 0;
    if (m_macroMem.empty())
        return 0;
    return (const StdVnChar *)(m_macroMem.data() + m_table[(size_t)idx].textOffset);
}
