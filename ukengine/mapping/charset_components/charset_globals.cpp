// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4;
// indent-tabs-mode:nil -*-
/*------------------------------------------------------------------------------
VnConv: Vietnamese Encoding Converter Library
UniKey Project: http://unikey.sourceforge.net
Copyleft (C) 1998-2002 Pham Kim Long
Contact: longp@cslab.felk.cvut.cz

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
--------------------------------------------------------------------------------*/
#include "charset.h"

/**
 * @brief Lowercase vowel lookup table.
 *
 * Each index corresponds to a lowercase ASCII letter 'a'..'z'.
 * Nonzero means the letter is a Vietnamese vowel.
 *
 * Usage:
 *   if (LoVowel['a'-'a']) // 'a' is a vowel
 */
int LoVowel['z' - 'a' + 1];

/**
 * @brief Uppercase vowel lookup table.
 *
 * Mirrors LoVowel for 'A'..'Z'.
 * Usage is identical to LoVowel, but for uppercase.
 */
int HiVowel['Z' - 'A' + 1];

/**
 * @brief Macro to check if a character is a Vietnamese vowel (upper or lower).
 *
 * Usage:
 *   if (IS_VOWEL(c)) { ... }
 */
#define IS_VOWEL(x)                                \
	((x >= 'a' && x <= 'z' && LoVowel[x - 'a']) || \
	 (x >= 'A' && x <= 'Z' && HiVowel[x - 'A']))

/**
 * @brief Array of pointers to single-byte charset converters.
 *
 * Indexed by CONV_* IDs. Used by the engine to convert byte streams to
 * StdVnChar. Example: SingleByteCharset *latin1 = SgCharsets[CONV_LATIN1]; if
 * (latin1) latin1->nextInput(is, stdChar, bytesRead);
 */
SingleByteCharset *SgCharsets[CONV_TOTAL_SINGLE_CHARSETS];

/**
 * @brief Array of pointers to double-byte charset converters.
 *
 * Used for multi-byte legacy encodings.
 */
DoubleByteCharset *DbCharsets[CONV_TOTAL_DOUBLE_CHARSETS];

/**
 * @brief Central charset library object exported from the mapping module.
 *
 * Provides registration, lookup, and lifetime management for available charset
 * converters. Example: CVnCharsetLib &lib = VnCharsetLibObj;
 */
DllExport CVnCharsetLib VnCharsetLibObj;
