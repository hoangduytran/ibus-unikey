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

class CacheManagement;
class TextMacroFormat;

/**
 * @class CMacroTable
 * @brief Holds macro items and hash lookup by folded key.
 */
class DllInterface CMacroTable
{
    friend class CacheManagement;
    friend class TextMacroFormat;

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
    void setLastError(int code, const char *msg = "");

    /**
     * @brief Convert external key/trigger strings into VNSTANDARD vectors for inserts.
     * @param key Source trigger buffer in caller encoding (`charset`).
     * @param text Source expansion buffer in caller encoding (`charset`).
     * @param charset `VnConvert` input charset.
     * @param[out] outKey Receives decoded key sequence.
     * @param[out] outText Receives decoded expansion sequence.
     * @return True on success; sets `MACTAB_ERR_CONVERT` when either side fails.
     */
    bool decodeMacroKeyAndText(const void *key, const void *text, int charset,
                               std::vector<StdVnChar> &outKey,
                               std::vector<StdVnChar> &outText);

    /**
     * @brief Append macro row or replace existing mapping for the same folded key (`last wins`).
     * @param foldLookupBytes Map key identical to lookup folding from `foldedLookupKeyBytes`.
     * @param keyVec Key vector consumed into storage.
     * @param textVec Expansion vector consumed into storage.
     * @return Stored row index; leaves `MACTAB_ERR_OK` on success paths.
     */
    int upsertMacroByFoldKey(std::string foldLookupBytes,
                             std::vector<StdVnChar> keyVec,
                             std::vector<StdVnChar> textVec);

    void rebuildLookupMap();

    std::vector<MacroEntry> m_entries;
    /** Folded-key byte string (StdVnChar little-endian concatenation) -> index in m_entries (last wins). */
    std::unordered_map<std::string, size_t> m_lookup;

    int m_lastError;
    std::string m_lastErrorMessage;
};

#endif
