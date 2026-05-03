// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/* Unikey Vietnamese Input Method
 * Copyright (C) 2000-2005 Pham Kim Long
 */

#ifndef __MACRO_TABLE_H
#define __MACRO_TABLE_H

/**
 * @file mactab.h
 * @brief Macro table storage and lookup for UniKey macros.
 */

#include "charset.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
    #if defined(UNIKEYHOOK)
        #define DllInterface   __declspec( dllexport )
    #else
        #define DllInterface   __declspec( dllimport )
    #endif
#else
    #define DllInterface
    #define DllExport
    #define DllImport
#endif

#define MACTAB_ERR_OK 0
#define MACTAB_ERR_OOM 1
#define MACTAB_ERR_CONVERT 2
#define MACTAB_ERR_PARSE 3
#define MACTAB_ERR_IO 4
#define MACTAB_ERR_INCOMPLETE 5

/**
 * @brief One macro row: NUL-terminated StdVnChar sequences (trailing 0 present).
 */
struct MacroEntry {
    std::vector<StdVnChar> key;
    std::vector<StdVnChar> text;
};

#if !defined(WIN32)
typedef char TCHAR;
#endif

class MacroBinaryCache;

/**
 * @class CMacroTable
 * @brief Holds macro items and hash lookup by folded key.
 */
class DllInterface CMacroTable
{
    friend class MacroBinaryCache;

public:
    void init();

    /**
     * Try precomputed cache first; fall back to parsing UTF-8/VIQR text lines.
     */
    int loadFromFile(const char *fname);

    /** Write UTF-8 text macro file and refresh sidecar cache. */
    int writeToFile(const char *fname);

    const StdVnChar *lookup(StdVnChar *key);

    const StdVnChar *getKey(int idx) const;

    const StdVnChar *getText(int idx) const;

    int getCount() const { return (int)m_entries.size(); }

    int getLastError() const { return m_lastError; }

    const char *getLastErrorMessage() const { return m_lastErrorMessage.c_str(); }

    /** Approximate bytes stored (VNSTANDARD payloads). */
    size_t getOccupiedBytes() const;

    void resetContent();

    int addItem(const char *item, int charset);

    int addItem(const void *key, const void *text, int charset);

protected:
    bool readHeader(FILE *f, int &version);
    void writeHeader(FILE *f);
    void setLastError(int code, const char *msg = "");

    void rebuildLookupMap();

    std::vector<MacroEntry> m_entries;
    /** Folded-key byte string (StdVnChar little-endian concatenation) -> index in m_entries (last wins). */
    std::unordered_map<std::string, size_t> m_lookup;

    int m_lastError;
    std::string m_lastErrorMessage;
};

#endif
