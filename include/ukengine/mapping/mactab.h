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

#ifndef __MACRO_TABLE_H
#define __MACRO_TABLE_H

/**
 * @file mactab.h
 * @brief Macro table storage and lookup for UniKey macros.
 *
 * Contains in-memory macro table and persistence methods to load/save from files.
 */

#include "keycons.h"
#include "charset.h"

#include <cstddef>
#include <string>
#include <vector>

#if defined(_WIN32)
    #if defined(UNIKEYHOOK)
        #define DllInterface   __declspec( dllexport )
    #else
        #define DllInterface   __declspec( dllimport )
    #endif
#else
    #define DllInterface //not used
    #define DllExport
    #define DllImport
#endif

/**
 * @brief Return values for getLastError() after a failed add/load/write.
 */
#define MACTAB_ERR_OK 0
#define MACTAB_ERR_OOM 1
#define MACTAB_ERR_CONVERT 2
#define MACTAB_ERR_PARSE 3
#define MACTAB_ERR_IO 4
#define MACTAB_ERR_INCOMPLETE 5 /**< e.g. load or sync lost one or more rows */

/**
 * @brief Entry header for macro key/text pairs stored inside m_macroMem.
 */
struct MacroDef
{
  int keyOffset;  /**< byte offset to stored key sequence */
  int textOffset; /**< byte offset to stored replacement text */
};

#if !defined(WIN32)
typedef char TCHAR;
#endif

/**
 * @class CMacroTable
 * @brief Holds macro items and provides lookup by key sequences.
 */
class DllInterface CMacroTable
{
public:
    /**
     * @brief Initialize macro table internal counters.
     */
    void init();

    /**
     * @brief Load macro table from file.
     * @return 0 on failure, 1 on success.
     */
    int loadFromFile(const char *fname);

    /**
     * @brief Serialize macro table to file.
     * @return 0 on failure, 1 on success.
     */
    int writeToFile(const char *fname);

    /**
     * @brief Look up replacement text for a given key sequence.
     *
     * @param key null-terminated StdVnChar sequence
     * @return heap pointer to replacement or nullptr when not found.
     */
    const StdVnChar *lookup(StdVnChar *key);

    /**
     * @brief Get the key sequence for item index.
     */
    const StdVnChar *getKey(int idx);

    /**
     * @brief Get replacement text for item index.
     */
    const StdVnChar *getText(int idx);

    /**
     * @brief Number of macro entries currently loaded.
     */
    int getCount() { return (int)m_table.size(); }

    /**
     * @brief Last error after a failed operation (mactab.h MACTAB_ERR_*).
     */
    int getLastError() const { return m_lastError; }

    /**
     * @brief English/locale string for the last error, or "" if none.
     */
    const char *getLastErrorMessage() const { return m_lastErrorMessage.c_str(); }

    /**
     * @brief Bytes used in the macro key/text payload area.
     */
    size_t getOccupiedBytes() const
    {
        return (size_t)m_occupied;
    }

    /**
     * @brief Remove all macro items and reset memory.
     */
    void resetContent();

    /**
     * @brief Add a macro item from text string with charset hint.
     * @return new entry index, or -1 on failure.
     */
    int addItem(const char *item, int charset);

    /**
     * @brief Add a macro item from binary key/text blocks.
     * @return new entry index, or -1 on failure.
     */
    int addItem(const void *key, const void *text, int charset);

protected:
    bool readHeader(FILE *f, int & version);
    void writeHeader(FILE *f);
    void setLastError(int code, const char *msg = "");

    std::vector<MacroDef> m_table; /**< in-memory macro entry metadata */
    std::vector<char> m_macroMem;   /**< raw storage for macro key/text data */
    int m_occupied;   /**< bytes currently used in m_macroMem */
    int m_lastError;
    std::string m_lastErrorMessage;
};

#endif
