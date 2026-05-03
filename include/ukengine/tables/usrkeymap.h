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

#ifndef __UNIKEY_USER_KEY_MAP_H
#define __UNIKEY_USER_KEY_MAP_H

#include "inputproc.h"

// A single user-defined key mapping entry.
// This pairs an input key code with a custom action value.
struct UkKeyMapPair
{
    unsigned char key; // raw key code from the keyboard mapping file
    int action;        // action or mapped value to apply for this key
};

// Load a full 256-entry keyboard mapping table from a file.
// @param fileName path to the key map file to load
// @param keyMap output array of 256 mapping values to fill
// @return status code indicating success or failure
DllInterface int UkLoadKeyMap(const char *fileName, int keyMap[256]);

// Load an ordered list of key mappings used for custom key order rules.
// @param fileName path to the key order map file to load
// @param pMap output array of UkKeyMapPair entries to populate
// @param pMapCount input/output pointer to the size of pMap; receives loaded entry count
// @return status code indicating success or failure
DllInterface int UkLoadKeyOrderMap(const char *fileName, UkKeyMapPair *pMap, int *pMapCount);

// Store the current ordered key mapping list back to a file.
// @param fileName path to write the key order map to
// @param pMap array of UkKeyMapPair entries to write
// @param mapCount number of entries in pMap to store
// @return status code indicating success or failure
DllInterface int UkStoreKeyOrderMap(const char *fileName, UkKeyMapPair *pMap, int mapCount);

#endif
