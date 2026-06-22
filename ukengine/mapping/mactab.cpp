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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "mactab.h"
#include "vnconv.h"

using namespace std;

/**
 * @brief Macro file version identifier for UTF-8 encoded macro tables.
 * Used to distinguish between legacy VIQR and modern UTF-8 macro files.
 */
#define UKMACRO_VERSION_UTF8 1

//---------------------------------------------------------------
void CMacroTable::init()
{
    // Set the total macro memory size
    m_memSize = MACRO_MEM_SIZE;
    // Reset macro entry count
    m_count = 0;
    // Reset used memory counter
    m_occupied = 0;
}

//---------------------------------------------------------------

/**
 * @brief Global pointer to the start of macro memory for comparison routines.
 *
 * Used by macCompare and macKeyCompare to resolve key/text offsets in macro memory.
 * WARNING: This is not thread-safe and assumes single-threaded access during sort/search.
 */
char *MacCompareStartMem;

/**
 * @brief Convert a standard Vietnamese character code to its lowercase equivalent.
 *
 * This macro checks if x is a standard Vietnamese character (in the defined range)
 * and if it is uppercase (even index), returns the corresponding lowercase code (odd index).
 * Otherwise, returns x unchanged.
 *
 * @param x Standard Vietnamese character code (StdVnChar)
 * @return Lowercase equivalent if applicable, else x
 */
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

    // Compare each character in the macro keys (case-insensitive)
    for (i = 0; s1[i] != 0 && s2[i] != 0; i++)
    {
        ls1 = STD_TO_LOWER(s1[i]);
        ls2 = STD_TO_LOWER(s2[i]);
        if (ls1 > ls2)
            return 1;
        if (ls1 < ls2)
            return -1;
        // (Legacy: direct code comparison, now commented)
    }
    // If both strings ended, they are equal; otherwise, shorter is less
    if (s1[i] == 0)
        return (s2[i] == 0) ? 0 : -1;
    return 1;
}

//---------------------------------------------------------------
/**
 * @brief Compare a macro key buffer to a macro table entry for binary search.
 *
 * Performs a case-insensitive comparison between a provided key buffer and a macro entry's key.
 * Used as the comparison function for bsearch in macro lookup.
 *
 * @param key Pointer to a null-terminated StdVnChar buffer representing the search key.
 * @param ele Pointer to a MacroDef entry in the macro table (used to resolve the stored key).
 * @return 0 if equal, <0 if key < entry, >0 if key > entry (strcmp-style).
 */
int macKeyCompare(const void *key, const void *ele)
{
    // Cast input pointers to appropriate types
    StdVnChar *s1 = (StdVnChar *)key;                                                         // Search key buffer
    StdVnChar *s2 = (StdVnChar *)((char *)MacCompareStartMem + ((MacroDef *)ele)->keyOffset); // Macro entry key

    StdVnChar ls1, ls2; // Lowercase comparison variables
    int i;              // Loop index

    // Compare each character in the key and entry (case-insensitive)
    for (i = 0; s1[i] != 0 && s2[i] != 0; i++)
    {
        // Convert both characters to lowercase for comparison
        ls1 = STD_TO_LOWER(s1[i]);
        ls2 = STD_TO_LOWER(s2[i]);
        // If search key is greater, return 1
        if (ls1 > ls2)
            return 1;
        // If search key is less, return -1
        if (ls1 < ls2)
            return -1;
        // (Legacy: direct code comparison, now commented)
    }
    // If both strings ended, they are equal; otherwise, shorter is less
    if (s1[i] == 0)
        return (s2[i] == 0) ? 0 : -1;
    // If search key is longer, it is greater
    return 1;
}

//---------------------------------------------------------------
/**
 * @brief Look up a macro replacement for a given key sequence.
 *
 * Performs a binary search in the sorted macro table for the provided key.
 * If found, returns a pointer to the replacement text; otherwise, returns nullptr.
 *
 * @param key Pointer to a null-terminated StdVnChar buffer representing the macro trigger sequence.
 * @return Pointer to replacement text (StdVnChar*) if found, or nullptr if not found.
 */
const StdVnChar *CMacroTable::lookup(StdVnChar *key)
{
    // Set up global pointer for comparison routines (used by macKeyCompare)
    MacCompareStartMem = m_macroMem;
    // Perform binary search for the macro key in the sorted table
    MacroDef *p = (MacroDef *)bsearch(key, m_table, m_count, sizeof(MacroDef), macKeyCompare);
    if (p)
        // If found, return pointer to replacement text in macro memory
        return (StdVnChar *)(m_macroMem + p->textOffset);
    // If not found, return nullptr
    return 0;
}

//----------------------------------------------------------------------------
// Read header, if it's present in the file. Get the version of the file
// If header is absent, go back to the beginning of file and set version to 0
// Return false if reading failed.
//
// Header format: ;[DO NOT DELETE THIS LINE]***version=n
//----------------------------------------------------------------------------
/**
 * @brief Read the macro file header and extract the version number if present.
 *
 * Attempts to read the first line of the macro file and parse a version marker.
 * If the header is missing, resets the file pointer and sets version to 0.
 *
 * @param f Pointer to an open FILE for reading (must be at the start of the file).
 * @param version Output parameter set to the detected version (0 if not found).
 * @return true if header was read or file is empty, false on read error.
 */
bool CMacroTable::readHeader(FILE *f, int &version)
{
    char line[MAX_MACRO_LINE]; // Buffer for reading the first line
    // Try to read the first line (header)
    if (!fgets(line, sizeof(line), f))
    {
        // If file is empty, reset to start and set version 0
        if (feof(f))
        {
            fseek(f, 0, SEEK_SET);
            version = 0;
            return true;
        }
        // Read error (not EOF)
        return false;
    }

    // If BOM (Byte Order Mark) is available, skip it
    char *p = line;
    size_t len = strlen(line);
    if (len >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF)
    {
        p += 3;
    }

    // Look for version marker in header ("*** version=n")
    p = strstr(p, "***");
    if (p)
    {
        p += 3;
        // Skip possible spaces after marker
        while (*p == ' ')
            p++;
        // Parse version number if present
        if (sscanf(p, "version=%d", &version) == 1)
            return true;
    }

    // No header found, reset to start and set version 0
    fseek(f, 0, SEEK_SET);
    version = 0;
    return true;
}

//----------------------------------------------------------------
/**
 * @brief Write the macro file header with version information.
 *
 * Outputs a header line at the start of the macro file, including the version marker.
 * On Windows, also writes a UTF-8 BOM (Byte Order Mark) for compatibility.
 *
 * @param f Pointer to an open FILE for writing (must be at the start of the file).
 *           The file should be opened in text mode for correct BOM and line ending handling.
 *
 * @details
 * - On Windows, writes BOM (\xEF\xBB\xBF) followed by the header line.
 * - On other platforms, writes only the header line.
 * - The header format is: ";DO NOT DELETE THIS LINE*** version=n ***"
 * - The version number is defined by UKMACRO_VERSION_UTF8.
 */
void CMacroTable::writeHeader(FILE *f)
{
#if defined(WIN32)
    // Write BOM (Byte Order Mark) for UTF-8 files on Windows
    // This ensures editors recognize the file encoding correctly.
    fprintf(f, "\xEF\xBB\xBF;DO NOT DELETE THIS LINE*** version=%d ***\n", UKMACRO_VERSION_UTF8);
#else
    // Write header line without BOM for non-Windows systems
    fprintf(f, "DO NOT DELETE THIS LINE*** version=%d ***\n", UKMACRO_VERSION_UTF8);
#endif
    // No return value; assumes file is writable and errors are handled by caller.
}

/**
 * @brief Load macro definitions from a file into the macro table.
 *
 * Opens the specified macro file, reads its header to determine the version and encoding,
 * parses each macro entry, and populates the macro table in memory. If the file is in a legacy
 * encoding, it is automatically rewritten in UTF-8 format after loading.
 *
 * @param fname Path to the macro file to load (null-terminated C string).
 *              The file should be readable and contain macro entries in the expected format.
 * @return 1 on success, 0 on failure (e.g., file cannot be opened).
 *
 * @details
 * - Opens the file for reading (text mode, platform-specific).
 * - Resets the macro table content before loading new entries.
 * - Reads the header to detect the file version and encoding.
 * - For each line, removes trailing newline/carriage return, then adds the macro entry using the detected charset.
 * - After loading, sorts the macro table for fast lookup.
 * - If the file was in legacy encoding, rewrites it in UTF-8 format.
 * - Returns 1 on success, 0 if the file could not be opened.
 *
 * @code
 * int result = macroTable.loadFromFile("macros.txt");
 * if (result) { loaded successfully }
 * else { failed to load }
 * @endcode
 *
 */
int CMacroTable::loadFromFile(const char *fname)
{
    FILE *f; // File pointer for macro file
#if defined(WIN32)
    // Open macro file for reading (Windows, Unicode-aware)
    f = _tfopen(fname, _TEXT("rt"));
#else
    // Open macro file for reading (non-Windows)
    f = fopen(fname, "r");
#endif

    if (f == NULL)
        return 0; // File open failed

    char line[MAX_MACRO_LINE]; // Buffer for reading each line
    size_t len;                // Length of the current line

    // Reset macro table content before loading new entries
    resetContent();

    // Read possible header and version
    int version; // Detected macro file version
    if (!readHeader(f, version))
    {
        // If header read fails, assume legacy version
        version = 0;
    }

    // Read each macro entry line from the file
    while (fgets(line, sizeof(line), f))
    {
        len = strlen(line); // Get line length
        // Remove trailing newline character (if present)
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = 0;
        // Remove trailing carriage return (if present)
        if (len > 1 && line[len - 2] == '\r')
            line[len - 2] = 0;
        // Add macro entry using detected charset (UTF-8 or VIQR)
        if (version == UKMACRO_VERSION_UTF8)
            addItem(line, CONV_CHARSET_UNIUTF8);
        else
            addItem(line, CONV_CHARSET_VIQR);
    }
    fclose(f); // Close the file after reading

    // Sort macro table for fast binary search lookup
    MacCompareStartMem = m_macroMem;
    qsort(m_table, m_count, sizeof(MacroDef), macCompare);

    // If legacy version, rewrite file in UTF-8 for consistency
    if (version != UKMACRO_VERSION_UTF8)
    {
        writeToFile(fname);
    }
    return 1; // Success
}

//---------------------------------------------------------------
/**
 * @brief Write all macro table entries to a file in UTF-8 format.
 *
 * Opens the specified file for writing, outputs a header with version information,
 * and writes each macro entry (key:text) in UTF-8 encoding. Handles both Windows and non-Windows platforms.
 *
 * @param fname Path to the macro file to write (null-terminated C string).
 *              The file will be overwritten if it exists.
 * @return 1 on success, 0 on failure (e.g., file cannot be opened).
 *
 * @details
 * - Opens the file for writing (text mode, platform-specific).
 * - Writes the macro file header (with BOM on Windows).
 * - Converts each macro key and text from VN standard encoding to UTF-8.
 * - Writes each macro entry as "key:text" (one per line, except last entry has no trailing newline).
 * - Returns 1 on success, 0 if the file could not be opened.
 *
 * @code
 * int result = macroTable.writeToFile("macros.txt");
 * // result == 1 on success, 0 on failure
 * @endcode
 */
int CMacroTable::writeToFile(const char *fname)
{
    int ret;              // Return value for VnConvert
    int inLen, maxOutLen; // Input length and max output length for conversion
    FILE *f;              // File pointer for output file
#if defined(WIN32)
    f = _tfopen(fname, _TEXT("wt")); // Open file for writing (Windows)
#else
    f = fopen(fname, "w"); // Open file for writing (non-Windows)
#endif

    if (f == NULL)
        return 0; // File open failed

    // Prepare buffers for UTF-8 output
    char line[MAX_MACRO_LINE * 3 + 1]; // Output buffer for a line (UTF-8)
    char key[MAX_MACRO_KEY_LEN * 3];   // Buffer for UTF-8 macro key
    char text[MAX_MACRO_TEXT_LEN * 3]; // Buffer for UTF-8 macro text

    // Write file header (includes BOM if needed)
    writeHeader(f);

    UKBYTE *p; // Pointer for macro memory traversal
    for (int i = 0; i < m_count; i++)
    {
        // Convert macro key to UTF-8 for output
        p = (UKBYTE *)m_macroMem + m_table[i].keyOffset;
        inLen = -1; // Input is null-terminated
        maxOutLen = sizeof(key);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8,
                        (UKBYTE *)p, (UKBYTE *)key,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue; // Skip entry if conversion fails

        // Convert macro text to UTF-8 for output
        p = (UKBYTE *)m_macroMem + m_table[i].textOffset;
        inLen = -1; // Input is null-terminated
        maxOutLen = sizeof(text);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8,
                        p, (UKBYTE *)text,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue; // Skip entry if conversion fails

        // Write macro entry to file as "key:text"
        if (i < m_count - 1)
            sprintf(line, "%s:%s\n", key, text); // Add newline for all but last entry
        else
            sprintf(line, "%s:%s", key, text); // Last entry: no trailing newline
        fputs(line, f);
    }

    fclose(f); // Close the file after writing
    return 1;  // Success
}

//---------------------------------------------------------------

/**
 * @brief Add a macro entry with separate key and text buffers.
 *
 * Converts the input key and text from the specified charset to the internal Vietnamese standard encoding,
 * and stores them consecutively in the macro memory buffer. Offsets for key and text are recorded in the macro table.
 *
 * @param key     Pointer to null-terminated key string (input charset).
 * @param text    Pointer to null-terminated text string (input charset).
 * @param charset Charset identifier for input strings.
 * @return Index of the newly added entry on success, -1 on failure (e.g., memory full or conversion error).
 */
int CMacroTable::addItem(const void *key, const void *text, int charset)
{
    int ret;                       // Return value from VnConvert
    int inLen, maxOutLen;          // Input length and max output length for conversion
    int offset = m_occupied;       // Current offset in macro memory
    char *p = m_macroMem + offset; // Pointer to write location in macro memory

    // Check if macro entry limit is reached
    if (m_count >= MAX_MACRO_ITEMS)
        return -1;

    // Store key offset in macro memory for this entry
    m_table[m_count].keyOffset = offset;

    // Convert macro key to internal VN standard encoding
    inLen = -1; // Input is null-terminated
    maxOutLen = MAX_MACRO_KEY_LEN * sizeof(StdVnChar);
    if (maxOutLen + offset > m_memSize)
        maxOutLen = m_memSize - offset; // Prevent buffer overflow
    ret = VnConvert(charset, CONV_CHARSET_VNSTANDARD,
                    (UKBYTE *)key, (UKBYTE *)p,
                    &inLen, &maxOutLen);
    if (ret != 0)
        return -1; // Conversion failed

    // Advance offset and pointer for text
    offset += maxOutLen;
    p += maxOutLen;

    // Store text offset in macro memory for this entry
    m_table[m_count].textOffset = offset;
    inLen = -1; // Input is null-terminated
    maxOutLen = MAX_MACRO_TEXT_LEN * sizeof(StdVnChar);
    if (maxOutLen + offset > m_memSize)
        maxOutLen = m_memSize - offset; // Prevent buffer overflow
    ret = VnConvert(charset, CONV_CHARSET_VNSTANDARD,
                    (UKBYTE *)text, (UKBYTE *)p,
                    &inLen, &maxOutLen);
    if (ret != 0)
        return -1; // Conversion failed

    // Update used memory and macro entry count
    m_occupied = offset + maxOutLen;
    m_count++;
    return (m_count - 1); // Return index of new entry
}

//---------------------------------------------------------------
// add a new macro into the sorted macro table
// item format: key:text (key and text are separated by a colon)
//---------------------------------------------------------------

/**
 * @brief Add a macro entry from a single colon-separated string.
 *
 * Parses the input string in the format "key:text", splits it into key and text,
 * and adds the macro entry using the specified charset.
 *
 * @param item    Null-terminated string in the format "key:text".
 * @param charset Charset identifier for input string.
 * @return Index of the newly added entry on success, -1 on failure (e.g., parse error).
 */
int CMacroTable::addItem(const char *item, int charset)
{
    char key[MAX_MACRO_KEY_LEN]; // Buffer for parsed key

    // Find the colon separator between key and text
    char *pos = (char *)strchr(item, ':');
    if (pos == NULL)
        return -1; // No colon found, invalid format
    int keyLen = (int)(pos - item);
    if (keyLen > MAX_MACRO_KEY_LEN - 1)
        keyLen = MAX_MACRO_KEY_LEN - 1; // Truncate if key too long
    strncpy(key, item, keyLen);         // Copy key part
    key[keyLen] = '\0';                 // Null-terminate key
    // Add macro entry using parsed key and text (text starts after colon)
    return addItem(key, ++pos, charset);
}

//---------------------------------------------------------------

/**
 * @brief Reset the macro table content.
 *
 * Clears all macro entries by resetting the used memory and entry count.
 * Does not deallocate memory, just marks the table as empty.
 */
void CMacroTable::resetContent()
{
    m_occupied = 0; // Reset used memory
    m_count = 0;    // Reset entry count
}

//---------------------------------------------------------------

/**
 * @brief Get the macro key for a given entry index.
 *
 * Returns a pointer to the macro key in internal VN standard encoding.
 *
 * @param idx Index of the macro entry (0-based).
 * @return Pointer to the macro key, or 0 if index is out of bounds.
 */
const StdVnChar *CMacroTable::getKey(int idx)
{
    if (idx < 0 || idx >= m_count)
        return 0;                                              // Out of bounds
    return (StdVnChar *)(m_macroMem + m_table[idx].keyOffset); // Pointer to key
}

//---------------------------------------------------------------

/**
 * @brief Get the macro replacement text for a given entry index.
 *
 * Returns a pointer to the macro replacement text in internal VN standard encoding.
 *
 * @param idx Index of the macro entry (0-based).
 * @return Pointer to the macro text, or 0 if index is out of bounds.
 */
const StdVnChar *CMacroTable::getText(int idx)
{
    if (idx < 0 || idx >= m_count)
        return 0;                                               // Out of bounds
    return (StdVnChar *)(m_macroMem + m_table[idx].textOffset); // Pointer to text
}
