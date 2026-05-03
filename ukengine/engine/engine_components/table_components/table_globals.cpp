// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
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
#include "engine_components/engine_tables_shared.h"
/**
 * @brief Lookup table: IsVnVowel[i] is true if VnLexiName i is a Vietnamese vowel letter.
 *
 * Used for fast checks in input processing, e.g. to distinguish vowels from consonants.
 * Example usage:
 *   if (IsVnVowel[vnl_a]) { ... } // true, since 'a' is a vowel
 *   if (IsVnVowel[vnl_b]) { ... } // false, since 'b' is a consonant
 *
 * Populated in engineClassInit().
 */
bool IsVnVowel[vnl_lastChar];

/**
 * @brief Mapping tables for Vietnamese alphabetic characters.
 *
 * AZLexiUpper[]: Maps 'A'-'Z' to their corresponding VnLexiName uppercase symbols.
 * AZLexiLower[]: Maps 'a'-'z' to their corresponding VnLexiName lowercase symbols.
 *
 * Example usage:
 *   VnLexiName upperA = AZLexiUpper['A' - 'A']; // gets VnLexiName for 'A'
 *   VnLexiName lowera = AZLexiLower['a' - 'a']; // gets VnLexiName for 'a'
 *
 * Used for case conversion and character classification.
 * Defined in inputproc.cpp.
 */
extern VnLexiName AZLexiUpper[]; // defined in inputproc.cpp
extern VnLexiName AZLexiLower[];

// see vnconv/data.cpp for explanation of these characters
unsigned char SpecialWesternChars[] = {
    0x80, /* € Euro sign */
    0x82, /* ‚ single low-9 quotation mark */
    0x83, /* ƒ Latin small letter f with hook */
    0x84, /* „ double low-9 quotation mark */
    0x85, /* … horizontal ellipsis */
    0x86, /* † dagger */
    0x87, /* ‡ double dagger */
    0x88, /* ˆ modifier letter circumflex accent */
    0x89, /* ‰ per mille sign */
    0x8A, /* Š Latin capital letter S with caron */
    0x8B, /* ‹ single left-pointing angle quotation */
    0x8C, /* Œ Latin capital ligature OE */
    0x8E, /* Ž Latin capital letter Z with caron */
    0x91, /* ‘ left single quotation mark */
    0x92, /* ’ right single quotation mark */
    0x93, /* “ left double quotation mark */
    0x94, /* ” right double quotation mark */
    0x95, /* • bullet */
    0x96, /* – en dash */
    0x97, /* — em dash */
    0x98, /* ˜ small tilde */
    0x99, /* ™ trade mark sign */
    0x9A, /* š Latin small letter s with caron */
    0x9B, /* › single right-pointing angle quotation */
    0x9C, /* œ Latin small ligature oe */
    0x9E, /* ž Latin small letter z with caron */
    0x9F, /* Ÿ Latin capital letter Y with diaeresis */
    0x00};

/**
 * @brief Lookup table for mapping ISO/extended key codes to internal Vietnamese character codes.
 *
 * IsoStdVnCharMap[code] gives the corresponding StdVnChar for a given 8-bit input code (0..255).
 * Used to convert input bytes from various encodings (e.g. ISO-8859-1, Windows-1252) to the engine's
 * internal Vietnamese character representation.
 *
 * Example usage:
 *   StdVnChar ch = IsoStdVnCharMap['a']; // maps ASCII 'a' to Vietnamese 'a'
 *   StdVnChar euro = IsoStdVnCharMap[0x80]; // maps 0x80 to Euro sign if defined
 *
 * Populated at engine initialization.
 */
StdVnChar IsoStdVnCharMap[256];
