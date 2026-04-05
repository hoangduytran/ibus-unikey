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
 * @brief Entry header for macro key/text pairs stored inside `m_macroMem`.
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
     * @return 0 on success, nonzero on failure.
     */
    int loadFromFile(const char *fname);

    /**
     * @brief Serialize macro table to file.
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
    int getCount() { return m_count; }

    /**
     * @brief Remove all macro items and reset memory.
     */
    void resetContent();

    /**
     * @brief Add a macro item from text string with charset hint.
     */
    int addItem(const char *item, int charset);

    /**
     * @brief Add a macro item from binary key/text blocks.
     */
    int addItem(const void *key, const void *text, int charset);

protected:
    /**
     * @brief Read persisted macro table header for loading versioned format.
     */
    bool readHeader(FILE *f, int & version);

    /**
     * @brief Write persisted macro table header.
     */
    void writeHeader(FILE *f);

    MacroDef m_table[MAX_MACRO_ITEMS]; /**< in-memory macro entry metadata */
    char m_macroMem[MACRO_MEM_SIZE];   /**< raw storage for macro key/text data */

    int m_count;      /**< number of macro entries loaded */
    int m_memSize;    /**< total allocated macro memory size */
    int m_occupied;   /**< bytes currently used in macro memory */
};

#endif
