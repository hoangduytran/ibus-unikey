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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "keycons.h"

/*
#if defined(_WIN32)
#include "keyhook.h"
#endif
*/

#include "vnlexi.h"
#include "ukengine.h"

#include "charset.h"

#define ENTER_CHAR 13
#define IS_ODD(x) (x & 1)
#define IS_EVEN(x) (!(x & 1))

#define IS_STD_VN_LOWER(x) ((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && IS_ODD(x))
#define IS_STD_VN_UPPER(x) ((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && IS_EVEN(x))

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

/**
 * @brief Convert a raw key code to the corresponding internal Vietnamese character code.
 *
 * If keyCode < 256, returns IsoStdVnCharMap[keyCode]. Otherwise, returns keyCode unchanged.
 *
 * Example usage:
 *   StdVnChar ch = IsoToStdVnChar('e'); // returns Vietnamese 'e' code
 *   StdVnChar special = IsoToStdVnChar(0x80); // returns mapped special character
 */
inline StdVnChar IsoToStdVnChar(int keyCode)
{
    return (keyCode < 256) ? IsoStdVnCharMap[keyCode] : keyCode;
}

/**
 * @brief Describes a Vietnamese vowel sequence and its transformation properties.
 *
 * This struct encodes all information about a Vietnamese vowel sequence (e.g., "a", "ai", "ươu")
 * and how it can be transformed (e.g., adding a roof to get â, or a hook to get ă).
 *
 * Used in the VSeqList[] table to define all valid vowel sequences for Vietnamese syllable processing.
 *
 * Example usage:
 *   // Access the vowel sequence info for "a"
 *   const VowelSeqInfo& info = VSeqList[0];
 *   // info.v[0] == vnl_a ("a"), info.withRoof == vs_ar ("â"), info.withHook == vs_ab ("ă")
 *
 *   // For "ai":
 *   const VowelSeqInfo& aiInfo = VSeqList[12];
 *   // aiInfo.v[0] == vnl_a, aiInfo.v[1] == vnl_i, aiInfo.len == 2
 */
struct VowelSeqInfo
{
    /**
     * @brief Number of letters in the vowel sequence (1, 2, or 3).
     * Example: "a" -> 1, "ai" -> 2, "iêu" -> 3
     */
    int len;

    /**
     * @brief True if this sequence is a complete vowel nucleus (can stand alone in a syllable).
     * Example: "ai" (complete=1), "eu" (complete=0, only appears in certain contexts)
     */
    int complete;

    /**
     * @brief True if this vowel sequence can be followed by a consonant suffix.
     * Example: "ai" (conSuffix=0, cannot be followed by a consonant), "an" (conSuffix=1, can be followed)
     */
    int conSuffix;

    /**
     * @brief Array of up to 3 VnLexiName symbols representing the letters in the sequence.
     * Example: "ai" -> {vnl_a, vnl_i, vnl_nonVnChar}
     *          "ươu" -> {vnl_uh, vnl_o, vnl_u}
     */
    VnLexiName v[3];

    /**
     * @brief Array of up to 3 VowelSeq enums representing the sub-sequences.
     * Example: "ai" -> {vs_a, vs_ai, vs_nil}
     *          "iêu" -> {vs_i, vs_ie, vs_ieu}
     */
    VowelSeq sub[3];

    /**
     * @brief Index in v[] where a roof diacritic can be applied (-1 if not applicable).
     * Example: For "a" (roofPos=-1), for "â" (roofPos=0)
     */
    int roofPos;

    /**
     * @brief VowelSeq representing the sequence with a roof diacritic applied (vs_nil if not applicable).
     * Example: For "a", withRoof=vs_ar ("â"); for "e", withRoof=vs_er ("ê")
     */
    VowelSeq withRoof;

    /**
     * @brief Index in v[] where a hook/bowl diacritic can be applied (-1 if not applicable).
     * Example: For "a" (hookPos=-1), for "ă" (hookPos=0)
     */
    int hookPos;

    /**
     * @brief VowelSeq representing the sequence with a hook/bowl diacritic applied (vs_nil if not applicable).
     * Example: For "a", withHook=vs_ab ("ă"); for "o", withHook=vs_oh ("ơ")
     */
    VowelSeq withHook;
};

/**
 * @brief Lookup table of all valid Vietnamese vowel sequences and their transformation properties.
 *
 * VSeqList[] is a central table that defines every possible vowel sequence (nucleus) in Vietnamese syllables,
 * along with how each sequence can be transformed (e.g., with roof or hook diacritics), and whether it can
 * be followed by a consonant suffix or stand alone. Each entry is a VowelSeqInfo struct describing:
 *   - The sequence's length (1, 2, or 3 letters)
 *   - The actual letters (as VnLexiName values)
 *   - The sequence's completeness (can it be a syllable nucleus?)
 *   - Whether it can take a consonant suffix
 *   - How to transform it with roof/hook diacritics (and where)
 *
 * This table is used throughout the Vietnamese input engine for:
 *   - Parsing and validating user input
 *   - Determining how to apply diacritics (e.g., turning "a" into "â" or "ă")
 *   - Syllable segmentation and synthesis
 *   - Fast lookup of sequence properties during input processing
 *
 * Example usage:
 *   // Find the VowelSeqInfo for the sequence "ai"
 *   const VowelSeqInfo& aiInfo = VSeqList[12];
 *   // aiInfo.v[0] == vnl_a, aiInfo.v[1] == vnl_i, aiInfo.len == 2
 *   // aiInfo.complete == 1 (can be a syllable nucleus)
 *   // aiInfo.withRoof == vs_nil (no roof diacritic for "ai")
 *
 *   // For single vowel "a":
 *   const VowelSeqInfo& aInfo = VSeqList[0];
 *   // aInfo.v[0] == vnl_a, aInfo.len == 1
 *   // aInfo.withRoof == vs_ar ("â"), aInfo.withHook == vs_ab ("ă")
 *
 *   // To check if a sequence can take a consonant suffix:
 *   if (aiInfo.conSuffix) { / ... / }

 *   // To transform a sequence with a roof:
 *   if (aInfo.withRoof != vs_nil) { / ... / }

 * See also: lookupVSeq(), VowelSeq, VnLexiName, and VowelSeqInfo struct definition above.
 */
VowelSeqInfo VSeqList[] = {
    {1, 1, 1, {vnl_a, vnl_nonVnChar, vnl_nonVnChar}, {vs_a, vs_nil, vs_nil}, -1, vs_ar, -1, vs_ab},    // "a" (roof=>â, hook=>ă)
    {1, 1, 1, {vnl_ar, vnl_nonVnChar, vnl_nonVnChar}, {vs_ar, vs_nil, vs_nil}, 0, vs_nil, -1, vs_ab},  // "â" (has roof, hook=>ă)
    {1, 1, 1, {vnl_ab, vnl_nonVnChar, vnl_nonVnChar}, {vs_ab, vs_nil, vs_nil}, -1, vs_ar, 0, vs_nil},  // "ă" (roof=>â, has hook)
    {1, 1, 1, {vnl_e, vnl_nonVnChar, vnl_nonVnChar}, {vs_e, vs_nil, vs_nil}, -1, vs_er, -1, vs_nil},   // "e" (roof=>ê)
    {1, 1, 1, {vnl_er, vnl_nonVnChar, vnl_nonVnChar}, {vs_er, vs_nil, vs_nil}, 0, vs_nil, -1, vs_nil}, // "ê" (has roof)
    {1, 1, 1, {vnl_i, vnl_nonVnChar, vnl_nonVnChar}, {vs_i, vs_nil, vs_nil}, -1, vs_nil, -1, vs_nil},  // "i"
    {1, 1, 1, {vnl_o, vnl_nonVnChar, vnl_nonVnChar}, {vs_o, vs_nil, vs_nil}, -1, vs_or, -1, vs_oh},    // "o" (roof=>ô, hook=>ơ)
    {1, 1, 1, {vnl_or, vnl_nonVnChar, vnl_nonVnChar}, {vs_or, vs_nil, vs_nil}, 0, vs_nil, -1, vs_oh},  // "ô" (has roof, hook=>ơ)
    {1, 1, 1, {vnl_oh, vnl_nonVnChar, vnl_nonVnChar}, {vs_oh, vs_nil, vs_nil}, -1, vs_or, 0, vs_nil},  // "ơ" (roof=>ô, has hook)
    {1, 1, 1, {vnl_u, vnl_nonVnChar, vnl_nonVnChar}, {vs_u, vs_nil, vs_nil}, -1, vs_nil, -1, vs_uh},   // "u" (hook=>ư)
    {1, 1, 1, {vnl_uh, vnl_nonVnChar, vnl_nonVnChar}, {vs_uh, vs_nil, vs_nil}, -1, vs_nil, 0, vs_nil}, // "ư" (has hook)
    {1, 1, 1, {vnl_y, vnl_nonVnChar, vnl_nonVnChar}, {vs_y, vs_nil, vs_nil}, -1, vs_nil, -1, vs_nil},  // "y"
    {2, 1, 0, {vnl_a, vnl_i, vnl_nonVnChar}, {vs_a, vs_ai, vs_nil}, -1, vs_nil, -1, vs_nil},           // "ai"
    {2, 1, 0, {vnl_a, vnl_o, vnl_nonVnChar}, {vs_a, vs_ao, vs_nil}, -1, vs_nil, -1, vs_nil},           // "ao"
    {2, 1, 0, {vnl_a, vnl_u, vnl_nonVnChar}, {vs_a, vs_au, vs_nil}, -1, vs_aru, -1, vs_nil},           // "au"
    {2, 1, 0, {vnl_a, vnl_y, vnl_nonVnChar}, {vs_a, vs_ay, vs_nil}, -1, vs_ary, -1, vs_nil},           // "ay"
    {2, 1, 0, {vnl_ar, vnl_u, vnl_nonVnChar}, {vs_ar, vs_aru, vs_nil}, 0, vs_nil, -1, vs_nil},         // "âu"
    {2, 1, 0, {vnl_ar, vnl_y, vnl_nonVnChar}, {vs_ar, vs_ary, vs_nil}, 0, vs_nil, -1, vs_nil},         // "ây"
    {2, 1, 0, {vnl_e, vnl_o, vnl_nonVnChar}, {vs_e, vs_eo, vs_nil}, -1, vs_nil, -1, vs_nil},           // "eo"
    {2, 0, 0, {vnl_e, vnl_u, vnl_nonVnChar}, {vs_e, vs_eu, vs_nil}, -1, vs_eru, -1, vs_nil},           // "eu"
    {2, 1, 0, {vnl_er, vnl_u, vnl_nonVnChar}, {vs_er, vs_eru, vs_nil}, 0, vs_nil, -1, vs_nil},         // "êu"
    {2, 1, 0, {vnl_i, vnl_a, vnl_nonVnChar}, {vs_i, vs_ia, vs_nil}, -1, vs_nil, -1, vs_nil},           // "ia"
    {2, 0, 1, {vnl_i, vnl_e, vnl_nonVnChar}, {vs_i, vs_ie, vs_nil}, -1, vs_ier, -1, vs_nil},           // "ie"
    {2, 1, 1, {vnl_i, vnl_er, vnl_nonVnChar}, {vs_i, vs_ier, vs_nil}, 1, vs_nil, -1, vs_nil},          // "iê"
    {2, 1, 0, {vnl_i, vnl_u, vnl_nonVnChar}, {vs_i, vs_iu, vs_nil}, -1, vs_nil, -1, vs_nil},           // "iu"
    {2, 1, 1, {vnl_o, vnl_a, vnl_nonVnChar}, {vs_o, vs_oa, vs_nil}, -1, vs_nil, -1, vs_oab},           // "oa"
    {2, 1, 1, {vnl_o, vnl_ab, vnl_nonVnChar}, {vs_o, vs_oab, vs_nil}, -1, vs_nil, 1, vs_nil},          // "oă"
    {2, 1, 1, {vnl_o, vnl_e, vnl_nonVnChar}, {vs_o, vs_oe, vs_nil}, -1, vs_nil, -1, vs_nil},           // "oe"
    {2, 1, 0, {vnl_o, vnl_i, vnl_nonVnChar}, {vs_o, vs_oi, vs_nil}, -1, vs_ori, -1, vs_ohi},           // "oi"
    {2, 1, 0, {vnl_or, vnl_i, vnl_nonVnChar}, {vs_or, vs_ori, vs_nil}, 0, vs_nil, -1, vs_ohi},         // "ôi"
    {2, 1, 0, {vnl_oh, vnl_i, vnl_nonVnChar}, {vs_oh, vs_ohi, vs_nil}, -1, vs_ori, 0, vs_nil},         // "ơi"
    {2, 1, 1, {vnl_u, vnl_a, vnl_nonVnChar}, {vs_u, vs_ua, vs_nil}, -1, vs_uar, -1, vs_uha},           // "ua"
    {2, 1, 1, {vnl_u, vnl_ar, vnl_nonVnChar}, {vs_u, vs_uar, vs_nil}, 1, vs_nil, -1, vs_nil},          // "uâ"
    {2, 0, 1, {vnl_u, vnl_e, vnl_nonVnChar}, {vs_u, vs_ue, vs_nil}, -1, vs_uer, -1, vs_nil},           // "ue"
    {2, 1, 1, {vnl_u, vnl_er, vnl_nonVnChar}, {vs_u, vs_uer, vs_nil}, 1, vs_nil, -1, vs_nil},          // "uê"
    {2, 1, 0, {vnl_u, vnl_i, vnl_nonVnChar}, {vs_u, vs_ui, vs_nil}, -1, vs_nil, -1, vs_uhi},           // "ui"
    {2, 0, 1, {vnl_u, vnl_o, vnl_nonVnChar}, {vs_u, vs_uo, vs_nil}, -1, vs_uor, -1, vs_uho},           // "uo"
    {2, 1, 1, {vnl_u, vnl_or, vnl_nonVnChar}, {vs_u, vs_uor, vs_nil}, 1, vs_nil, -1, vs_uoh},          // "uô"
    {2, 1, 1, {vnl_u, vnl_oh, vnl_nonVnChar}, {vs_u, vs_uoh, vs_nil}, -1, vs_uor, 1, vs_uhoh},         // "uơ"
    {2, 0, 0, {vnl_u, vnl_u, vnl_nonVnChar}, {vs_u, vs_uu, vs_nil}, -1, vs_nil, -1, vs_uhu},           // "uu"
    {2, 1, 1, {vnl_u, vnl_y, vnl_nonVnChar}, {vs_u, vs_uy, vs_nil}, -1, vs_nil, -1, vs_nil},           // "uy"
    {2, 1, 0, {vnl_uh, vnl_a, vnl_nonVnChar}, {vs_uh, vs_uha, vs_nil}, -1, vs_nil, 0, vs_nil},         // "ưa"
    {2, 1, 0, {vnl_uh, vnl_i, vnl_nonVnChar}, {vs_uh, vs_uhi, vs_nil}, -1, vs_nil, 0, vs_nil},         // "ưi"
    {2, 0, 1, {vnl_uh, vnl_o, vnl_nonVnChar}, {vs_uh, vs_uho, vs_nil}, -1, vs_nil, 0, vs_uhoh},        // "ưo"
    {2, 1, 1, {vnl_uh, vnl_oh, vnl_nonVnChar}, {vs_uh, vs_uhoh, vs_nil}, -1, vs_nil, 0, vs_nil},       // "ươ"
    {2, 1, 0, {vnl_uh, vnl_u, vnl_nonVnChar}, {vs_uh, vs_uhu, vs_nil}, -1, vs_nil, 0, vs_nil},         // "ưu"
    {2, 0, 1, {vnl_y, vnl_e, vnl_nonVnChar}, {vs_y, vs_ye, vs_nil}, -1, vs_yer, -1, vs_nil},           // "ye"
    {2, 1, 1, {vnl_y, vnl_er, vnl_nonVnChar}, {vs_y, vs_yer, vs_nil}, 1, vs_nil, -1, vs_nil},          // "yê"
    {3, 0, 0, {vnl_i, vnl_e, vnl_u}, {vs_i, vs_ie, vs_ieu}, -1, vs_ieru, -1, vs_nil},                  // "ieu"
    {3, 1, 0, {vnl_i, vnl_er, vnl_u}, {vs_i, vs_ier, vs_ieru}, 1, vs_nil, -1, vs_nil},                 // "iêu"
    {3, 1, 0, {vnl_o, vnl_a, vnl_i}, {vs_o, vs_oa, vs_oai}, -1, vs_nil, -1, vs_nil},                   // "oai"
    {3, 1, 0, {vnl_o, vnl_a, vnl_y}, {vs_o, vs_oa, vs_oay}, -1, vs_nil, -1, vs_nil},                   // "oay"
    {3, 1, 0, {vnl_o, vnl_e, vnl_o}, {vs_o, vs_oe, vs_oeo}, -1, vs_nil, -1, vs_nil},                   // "oeo"
    {3, 0, 0, {vnl_u, vnl_a, vnl_y}, {vs_u, vs_ua, vs_uay}, -1, vs_uary, -1, vs_nil},                  // "uay"
    {3, 1, 0, {vnl_u, vnl_ar, vnl_y}, {vs_u, vs_uar, vs_uary}, 1, vs_nil, -1, vs_nil},                 // "uây"
    {3, 0, 0, {vnl_u, vnl_o, vnl_i}, {vs_u, vs_uo, vs_uoi}, -1, vs_uori, -1, vs_uhoi},                 // "uoi"
    {3, 0, 0, {vnl_u, vnl_o, vnl_u}, {vs_u, vs_uo, vs_uou}, -1, vs_nil, -1, vs_uhou},                  // "uou"
    {3, 1, 0, {vnl_u, vnl_or, vnl_i}, {vs_u, vs_uor, vs_uori}, 1, vs_nil, -1, vs_uohi},                // "uôi"
    {3, 0, 0, {vnl_u, vnl_oh, vnl_i}, {vs_u, vs_uoh, vs_uohi}, -1, vs_uori, 1, vs_uhohi},              // "uơi"
    {3, 0, 0, {vnl_u, vnl_oh, vnl_u}, {vs_u, vs_uoh, vs_uohu}, -1, vs_nil, 1, vs_uhohu},               // "uơu"
    {3, 1, 0, {vnl_u, vnl_y, vnl_a}, {vs_u, vs_uy, vs_uya}, -1, vs_nil, -1, vs_nil},                   // "uya"
    {3, 0, 1, {vnl_u, vnl_y, vnl_e}, {vs_u, vs_uy, vs_uye}, -1, vs_uyer, -1, vs_nil},                  // "uye"
    {3, 1, 1, {vnl_u, vnl_y, vnl_er}, {vs_u, vs_uy, vs_uyer}, 2, vs_nil, -1, vs_nil},                  // "uyê"
    {3, 1, 0, {vnl_u, vnl_y, vnl_u}, {vs_u, vs_uy, vs_uyu}, -1, vs_nil, -1, vs_nil},                   // "uyu"
    {3, 0, 0, {vnl_uh, vnl_o, vnl_i}, {vs_uh, vs_uho, vs_uhoi}, -1, vs_nil, 0, vs_uhohi},              // "ưoi"
    {3, 0, 0, {vnl_uh, vnl_o, vnl_u}, {vs_uh, vs_uho, vs_uhou}, -1, vs_nil, 0, vs_uhohu},              // "ưou"
    {3, 1, 0, {vnl_uh, vnl_oh, vnl_i}, {vs_uh, vs_uhoh, vs_uhohi}, -1, vs_nil, 0, vs_nil},             // "ươi"
    {3, 1, 0, {vnl_uh, vnl_oh, vnl_u}, {vs_uh, vs_uhoh, vs_uhohu}, -1, vs_nil, 0, vs_nil},             // "ươu"
    {3, 0, 0, {vnl_y, vnl_e, vnl_u}, {vs_y, vs_ye, vs_yeu}, -1, vs_yeru, -1, vs_nil},                  // "yeu"
    {3, 1, 0, {vnl_y, vnl_er, vnl_u}, {vs_y, vs_yer, vs_yeru}, 1, vs_nil, -1, vs_nil}};

/**
 * @brief Describes a Vietnamese consonant sequence and its properties.
 *
 * The ConSeqInfo struct encodes all information about a Vietnamese consonant sequence (e.g., "ng", "ch", "nh")
 * used in syllable construction. It is used in the CSeqList[] table to define all valid consonant sequences for
 * Vietnamese syllable processing.
 *
 * Fields:
 *   - len: Number of letters in the consonant sequence (1, 2, or 3).
 *   - c[3]: Array of up to 3 VnLexiName symbols representing the consonant letters in the sequence.
 *   - suffix: True if this consonant sequence can appear as a syllable ending (coda), false if only as an onset.
 *
 * Usage:
 *   - Used for parsing and validating user input syllables.
 *   - Used to determine if a consonant sequence can be a valid onset or coda in a Vietnamese syllable.
 *   - Enables fast lookup of consonant sequence properties during input processing.
 *
 * Example usage:
 *   // Access the consonant sequence info for "ng"
 *   const ConSeqInfo& ngInfo = CSeqList[16];
 *   // ngInfo.len == 2, ngInfo.c[0] == vnl_n, ngInfo.c[1] == vnl_g
 *   // ngInfo.suffix == true ("ng" can be a syllable ending)
 *
 *   // For single consonant "b":
 *   const ConSeqInfo& bInfo = CSeqList[0];
 *   // bInfo.len == 1, bInfo.c[0] == vnl_b, bInfo.suffix == false
 *
 *   // To check if a sequence can be a coda:
 *   // To check if a sequence can be a coda:

 *
 * See also: CSeqList[], lookupCSeq(), VnLexiName, and ConSeqInfo struct definition.
 */
struct ConSeqInfo
{
    /**
     * @brief Number of letters in the consonant sequence (1, 2, or 3).
     * Example: "ng" -> 2, "b" -> 1, "ngh" -> 3
     * Usage:
     *   const ConSeqInfo& ngInfo = CSeqList[16];
     *   int n = ngInfo.len; // n == 2 for "ng"
     */
    int len;

    /**
     * @brief Array of up to 3 VnLexiName symbols representing the consonant letters in the sequence.
     * Example: "ng" -> {vnl_n, vnl_g, vnl_nonVnChar}
     *          "ch" -> {vnl_c, vnl_h, vnl_nonVnChar}
     *          "b"  -> {vnl_b, vnl_nonVnChar, vnl_nonVnChar}
     * Usage:
     *   const ConSeqInfo& chInfo = CSeqList[2];
     *   VnLexiName first = chInfo.c[0]; // first == vnl_c
     *   VnLexiName second = chInfo.c[1]; // second == vnl_h
     */
    VnLexiName c[3];

    /**
     * @brief True if this consonant sequence can appear as a syllable ending (coda), false if only as an onset.
     * Example: "ng" (suffix == true), "gh" (suffix == false)
     */
    bool suffix; // Usage: const ConSeqInfo& ngInfo = CSeqList[16]; if (ngInfo.suffix) { /* can be used as a coda */ }
};

ConSeqInfo CSeqList[] = {
    {1, {vnl_b, vnl_nonVnChar, vnl_nonVnChar}, false},  // "b"
    {1, {vnl_c, vnl_nonVnChar, vnl_nonVnChar}, true},   // "c"
    {2, {vnl_c, vnl_h, vnl_nonVnChar}, true},           // "ch"
    {1, {vnl_d, vnl_nonVnChar, vnl_nonVnChar}, false},  // "d"
    {1, {vnl_dd, vnl_nonVnChar, vnl_nonVnChar}, false}, // "đ"
    {2, {vnl_d, vnl_z, vnl_nonVnChar}, false},          // "dz"
    {1, {vnl_g, vnl_nonVnChar, vnl_nonVnChar}, false},  // "g"
    {2, {vnl_g, vnl_h, vnl_nonVnChar}, false},          // "gh"
    {2, {vnl_g, vnl_i, vnl_nonVnChar}, false},          // "gi"
    {3, {vnl_g, vnl_i, vnl_n}, false},                  // "gin"
    {1, {vnl_h, vnl_nonVnChar, vnl_nonVnChar}, false},  // "h"
    {1, {vnl_k, vnl_nonVnChar, vnl_nonVnChar}, false},  // "k"
    {2, {vnl_k, vnl_h, vnl_nonVnChar}, false},          // "kh"
    {1, {vnl_l, vnl_nonVnChar, vnl_nonVnChar}, false},  // "l"
    {1, {vnl_m, vnl_nonVnChar, vnl_nonVnChar}, true},   // "m"
    {1, {vnl_n, vnl_nonVnChar, vnl_nonVnChar}, true},   // "n"
    {2, {vnl_n, vnl_g, vnl_nonVnChar}, true},           // "ng"
    {3, {vnl_n, vnl_g, vnl_h}, false},                  // "ngh"
    {2, {vnl_n, vnl_h, vnl_nonVnChar}, true},           // "nh"
    {1, {vnl_p, vnl_nonVnChar, vnl_nonVnChar}, true},   // "p"
    {2, {vnl_p, vnl_h, vnl_nonVnChar}, false},          // "ph"
    {1, {vnl_q, vnl_nonVnChar, vnl_nonVnChar}, false},  // "q"
    {2, {vnl_q, vnl_u, vnl_nonVnChar}, false},          // "qu"
    {1, {vnl_r, vnl_nonVnChar, vnl_nonVnChar}, false},  // "r"
    {1, {vnl_s, vnl_nonVnChar, vnl_nonVnChar}, false},  // "s"
    {1, {vnl_t, vnl_nonVnChar, vnl_nonVnChar}, true},   // "t"
    {2, {vnl_t, vnl_h, vnl_nonVnChar}, false},          // "th"
    {2, {vnl_t, vnl_r, vnl_nonVnChar}, false},          // "tr"
    {1, {vnl_v, vnl_nonVnChar, vnl_nonVnChar}, false},  // "v"
    {1, {vnl_x, vnl_nonVnChar, vnl_nonVnChar}, false}   // "x"
};

/**
 * @brief Number of entries in the `VSeqList` table.
 *
 * Computed as `sizeof(VSeqList) / sizeof(VowelSeqInfo)`. Use this constant
 * when sizing auxiliary arrays or iterating over all known vowel sequences.
 *
 * Example:
 *   for (int i = 0; i < VSeqCount; ++i) { // inspect VSeqList[i]
 *   }
 */
const int VSeqCount = sizeof(VSeqList) / sizeof(VowelSeqInfo);

/**
 * @brief Mapping pair used for sorting and fast lookup of vowel sequences.
 *
 * - `v[3]` : Up to three `VnLexiName` values composing the vowel sequence
 *            (unused entries are `vnl_nonVnChar`).
 * - `vs`   : The associated `VowelSeq` enum value (e.g., `vs_ai`, `vs_o`).
 *
 * Typical usage: populate an array of `VSeqPair`, sort it, then binary-search
 * for a sequence of `VnLexiName` letters to obtain the corresponding `VowelSeq`.
 *
 * Example:
 *   // check first two letters match "ai"
 *   if (pair.v[0] == vnl_a && pair.v[1] == vnl_i) { VowelSeq seq = pair.vs; }
 */
struct VSeqPair
{
    VnLexiName v[3]; ///< Up to three VnLexiName elements composing the vowel sequence.
                     ///< v[0] is the first letter, v[1]/v[2] are vnl_nonVnChar if absent.
                     ///< Example: "ai" -> {vnl_a, vnl_i, vnl_nonVnChar}.
    VowelSeq vs;     ///< Canonical VowelSeq enum for the `v` triple (e.g., vs_ai for "ai").
};

/**
 * @brief Sorted lookup table of `VSeqPair` entries, sized to `VSeqCount`.
 *
 * Populated and sorted during engine initialization (see `tripleVowelCompare`).
 * Use `SortedVSeqList` for efficient mapping from a 1..3-letter sequence
 * (`VnLexiName` triple) to the canonical `VowelSeq` enum.
 *
 * Example:
 *   // binary-search `SortedVSeqList` to convert letters -> VowelSeq
 *   VowelSeq seq = lookupVSeq(v1, v2, v3); // may use SortedVSeqList internally
 */
VSeqPair SortedVSeqList[VSeqCount];

/**
 * @brief Number of entries in the `CSeqList` table.
 *
 * Computed as `sizeof(CSeqList) / sizeof(ConSeqInfo)`. Use this constant when
 * sizing auxiliary arrays or iterating over all known consonant sequences.
 *
 * Example:
 *   for (int i = 0; i < CSeqCount; ++i) { // inspect CSeqList[i]
 *   }
 */
const int CSeqCount = sizeof(CSeqList) / sizeof(ConSeqInfo);

/**
 * @brief Mapping pair used for sorting and fast lookup of consonant sequences.
 *
 * Members:
 *  - `c[3]` : Up to three `VnLexiName` values composing the consonant sequence.
 *            Unused positions are filled with `vnl_nonVnChar`.
 *  - `cs`   : The associated `ConSeq` enum value (e.g., `cs_ng`, `cs_ch`).
 *
 * Typical usage: populate an array of `CSeqPair`, sort it, then binary-search
 * for a sequence of `VnLexiName` letters to obtain the corresponding `ConSeq`.
 *
 * Examples:
 *   // "ng" represented as {vnl_n, vnl_g, vnl_nonVnChar}
 *   if (pair.c[0] == vnl_n && pair.c[1] == vnl_g) { ConSeq cs = pair.cs; }
 */
struct CSeqPair
{
    VnLexiName c[3]; ///< c[0]..c[2] letters composing the consonant sequence.
    ConSeq cs;       ///< Canonical ConSeq enum for the `c` triple (e.g., cs_ng).
};

/**
 * @brief Sorted lookup table of `CSeqPair` entries, sized to `CSeqCount`.
 *
 * Populated and sorted during engine initialization. Use `SortedCSeqList` for
 * efficient mapping from a 1..3-letter consonant triple to the canonical `ConSeq`.
 *
 * Example:
 *   // lookupCSeq may use this table internally to convert letters -> ConSeq
 *   ConSeq cs = lookupCSeq(c1, c2, c3);
 */
CSeqPair SortedCSeqList[CSeqCount];

/**
 * @brief Struct representing a vowel-consonant pair for Vietnamese syllable endings.
 *
 * Members:
 *  - `v` : The canonical `VowelSeq` enum value representing the vowel nucleus.
 *          Example values: `vs_a` ("a"), `vs_or` ("ô"), `vs_uh` ("ư").
 *  - `c` : The canonical `ConSeq` enum value representing the consonant coda.
 *          Example values: `cs_c` ("c"), `cs_ng` ("ng"), `cs_ch` ("ch").
 *
 * A `VCPair` entry models a valid syllable ending formed by concatenating the
 * vowel sequence and the consonant sequence (v + c). For example, {vs_a, cs_c}
 * corresponds to the ending "ac" as found in words such as "bác" and "lạc".
 */
struct VCPair
{
    VowelSeq v; ///< Vowel sequence (canonical `VowelSeq`). Example: `vs_a` -> "a".
    ConSeq c;   ///< Consonant sequence (canonical `ConSeq`). Example: `cs_ng` -> "ng".
};

/**
 * @brief List of valid Vietnamese vowel-consonant (VC) pairs for syllable endings.
 *
 * Each entry is a `VCPair {vowel sequence, consonant sequence}` that forms a valid
 * Vietnamese syllable ending. The inline comments next to each initializer give
 * example Vietnamese words containing that ending.
 *
 * Usage examples:
 *   - Check whether a given vowel and consonant can form a valid coda:
 *       VCPair key = {vs_a, cs_c}; // "ac"
 *       // iterate VCPairList to confirm validity or use a lookup helper.
 *   - Use the enums directly when generating or validating syllables.
 */
VCPair VCPairList[] = {
    {vs_a, cs_c},   // "ac" (bác, lạc)
    {vs_a, cs_ch},  // "ach" (sách, bạch)
    {vs_a, cs_m},   // "am" (nam, làm)
    {vs_a, cs_n},   // "an" (bàn, lan)
    {vs_a, cs_ng},  // "ang" (vang, sang)
    {vs_a, cs_nh},  // "anh" (xanh, nhanh)
    {vs_a, cs_p},   // "ap" (áp, nạp)
    {vs_a, cs_t},   // "at" (mát, phát)
    {vs_ar, cs_c},  // "âc" (bậc, cấc)
    {vs_ar, cs_m},  // "âm" (âm, câm)
    {vs_ar, cs_n},  // "ân" (ân, tân)
    {vs_ar, cs_ng}, // "âng" (nâng, vâng)
    {vs_ar, cs_p},  // "âp" (hấp, ấp)
    {vs_ar, cs_t},  // "ât" (mật, thật)
    {vs_ab, cs_c},  // "ăc" (mắc, lắc)
    {vs_ab, cs_m},  // "ăm" (năm, cắm)
    {vs_ab, cs_n},  // "ăn" (ăn, căn)
    {vs_ab, cs_ng}, // "ăng" (năng, trắng)
    {vs_ab, cs_p},  // "ăp" (sắp, tắp)
    {vs_ab, cs_t},  // "ăt" (mắt, vặt)

    {vs_e, cs_c},   // "ec" (méc, léc)
    {vs_e, cs_ch},  // "ech" (invalid)
    {vs_e, cs_m},   // "em" (kem, xem)
    {vs_e, cs_n},   // "en" (đen, khen)
    {vs_e, cs_ng},  // "eng" (reng, leng, keng)
    {vs_e, cs_nh},  // "enh" (invalid)
    {vs_e, cs_p},   // "ep" (dép, khép, nép)
    {vs_e, cs_t},   // "et" (tét, bét)
    {vs_er, cs_c},  // "êc" (invalid)
    {vs_er, cs_ch}, // "êch" (lệch, mếch)
    {vs_er, cs_m},  // "êm" (êm, thêm)
    {vs_er, cs_n},  // "ên" (bên, nên)
    {vs_er, cs_nh}, // "ênh" (bênh, kênh)
    {vs_er, cs_p},  // "êp" (nếp, xếp)
    {vs_er, cs_t},  // "êt" (mệt, tiết)

    {vs_i, cs_c},  // "ic" (tích, bích)
    {vs_i, cs_ch}, // "ich" (bích, địch)
    {vs_i, cs_m},  // "im" (kim, tìm)
    {vs_i, cs_n},  // "in" (tin, mịn)
    {vs_i, cs_nh}, // "inh" (xinh, binh)
    {vs_i, cs_p},  // "ip" (bíp, híp)
    {vs_i, cs_t},  // "it" (mít, thịt)

    {vs_o, cs_c},   // "oc" (học, bọc)
    {vs_o, cs_m},   // "om" (xóm, bom)
    {vs_o, cs_n},   // "on" (con, son)
    {vs_o, cs_ng},  // "ong" (bong, xong)
    {vs_o, cs_p},   // "op" (họp, góp)
    {vs_o, cs_t},   // "ot" (lọt, tót)
    {vs_or, cs_c},  // "ôc" (ốc, lộc)
    {vs_or, cs_m},  // "ôm" (ôm, chôm)
    {vs_or, cs_n},  // "ôn" (ôn, tôn)
    {vs_or, cs_ng}, // "ông" (ông, sông)
    {vs_or, cs_p},  // "ôp" (hộp, tốp)
    {vs_or, cs_t},  // "ôt" (mốt, tốt)
    {vs_oh, cs_m},  // "ơm" (cơm, thơm)
    {vs_oh, cs_n},  // "ơn" (ơn, hơn)
    {vs_oh, cs_p},  // "ơp" (lớp, hợp)
    {vs_oh, cs_t},  // "ơt" (vớt, hớt)

    {vs_u, cs_c},   // "uc" (lục, dục)
    {vs_u, cs_m},   // "um" (chum, sum)
    {vs_u, cs_n},   // "un" (vun, hun)
    {vs_u, cs_ng},  // "ung" (sung, cùng)
    {vs_u, cs_p},   // "up" (húp, súp)
    {vs_u, cs_t},   // "ut" (mút, hút)
    {vs_uh, cs_c},  // "ưc" (ức, bực)
    {vs_uh, cs_m},  // "ưm" (hừm, ừm, hứm)
    {vs_uh, cs_n},  // "ưn" (invalid)
    {vs_uh, cs_ng}, // "ưng" (bưng, từng)
    {vs_uh, cs_t},  // "ưt" (dứt, nứt)

    {vs_y, cs_t},    // "yt" (invalid)
    {vs_ie, cs_c},   // "iec" (invalid)
    {vs_ie, cs_m},   // "iem" (invalid)
    {vs_ie, cs_n},   // "ien" (invalid)
    {vs_ie, cs_ng},  // "ieng" (invalid)
    {vs_ie, cs_p},   // "iep" (invalid)
    {vs_ie, cs_t},   // "iet" (invalid)
    {vs_ier, cs_c},  // "iêc" (việc, tiếc)
    {vs_ier, cs_m},  // "iêm" (tiêm, viêm)
    {vs_ier, cs_n},  // "iên" (liên, miền)
    {vs_ier, cs_ng}, // "iêng" (tiếng, kiềng)
    {vs_ier, cs_p},  // "iêp" (tiệp, tiếp)
    {vs_ier, cs_t},  // "iêt" (tiệt, liệt)

    {vs_oa, cs_c},   // "oac" (xoạc, choạc)
    {vs_oa, cs_ch},  // "oach" (hoạch, xoạch)
    {vs_oa, cs_m},   // "oam" (ngoạm)
    {vs_oa, cs_n},   // "oan" (loan, xoan)
    {vs_oa, cs_ng},  // "oang" (hoàng, xoàng)
    {vs_oa, cs_nh},  // "oanh" (hoành, đoành)
    {vs_oa, cs_p},   // "oap" (oạp, ngoáp)
    {vs_oa, cs_t},   // "oat" (hoạt, xoạt)
    {vs_oab, cs_c},  // "oăc" (hoặc, ngoặc)
    {vs_oab, cs_m},  // "oăm" (oăm, hoắm)
    {vs_oab, cs_n},  // "oăn" (hoăn, khoắn)
    {vs_oab, cs_ng}, // "oăng" (hoẵng, thoắng)
    {vs_oab, cs_t},  // "oăt" (loắt, choắt, khoắt)

    {vs_oe, cs_n}, // "oen" (hoen)
    {vs_oe, cs_t}, // "oet" (xoẹt, khoét)

    {vs_ua, cs_n},   // "uan" (quan)
    {vs_ua, cs_ng},  // "uang" (quang)
    {vs_ua, cs_t},   // "uat" (invalid)
    {vs_uar, cs_n},  // "uân" (luân, quân)
    {vs_uar, cs_ng}, // "uâng" (khuâng, quầng)
    {vs_uar, cs_t},  // "uât" (luật, xuất)

    {vs_ue, cs_c},   // "uec" (invalid)
    {vs_ue, cs_ch},  // "uech" (invalid)
    {vs_ue, cs_n},   // "uen" (quen)
    {vs_ue, cs_nh},  // "uenh" (invalid)
    {vs_uer, cs_c},  // "uêc" (tuếch, khuếch)
    {vs_uer, cs_ch}, // "uêch" (khuếch, tuếch)
    {vs_uer, cs_n},  // "uên" (quên)
    {vs_uer, cs_nh}, // "uênh" (huênh)

    {vs_uo, cs_c},    // "uoc" (invalid)
    {vs_uo, cs_m},    // "uom" (invalid)
    {vs_uo, cs_n},    // "uon" (invalid)
    {vs_uo, cs_ng},   // "uong" (invalid)
    {vs_uo, cs_p},    // "uop" (invalid)
    {vs_uo, cs_t},    // "uot" (invalid)
    {vs_uor, cs_c},   // "uôc" (cuốc, thuốc)
    {vs_uor, cs_m},   // "uôm" (luộm, thuộm)
    {vs_uor, cs_n},   // "uôn" (uốn, luôn)
    {vs_uor, cs_ng},  // "uông" (buông, chuông)
    {vs_uor, cs_t},   // "uôt" (chuột, ruột)
    {vs_uho, cs_c},   // "ưoc" (invalid)
    {vs_uho, cs_m},   // "ươm" (ươm, sương)
    {vs_uho, cs_n},   // "ươn" (ươn, trường)
    {vs_uho, cs_ng},  // "ương" (hương, trường)
    {vs_uho, cs_p},   // "ươp" (ướp, cướp)
    {vs_uho, cs_t},   // "ươt" (ướt, trượt)
    {vs_uhoh, cs_c},  // "ươc" (nước, được)
    {vs_uhoh, cs_m},  // "ươm" (ươm, tươm)
    {vs_uhoh, cs_n},  // "ươn" (lươn, vươn)
    {vs_uhoh, cs_ng}, // "ương" (hương, vương)
    {vs_uhoh, cs_p},  // "ươp" (ướp, cướp)
    {vs_uhoh, cs_t},  // "ươt" (ướt, trượt)

    {vs_uy, cs_c},  // "uyc" (invalid)
    {vs_uy, cs_ch}, // "uych" (huỵch, uỵch)
    {vs_uy, cs_n},  // "uyn" (invalid)
    {vs_uy, cs_nh}, // "uynh" (quỳnh, huỳnh)
    {vs_uy, cs_p},  // "uyp" (invalid)
    {vs_uy, cs_t},  // "uyt" (xuýt, buýt)

    {vs_ye, cs_m},   // "yem" (invalid)
    {vs_ye, cs_n},   // "yen" (yen)
    {vs_ye, cs_ng},  // "yeng" (invalid)
    {vs_ye, cs_p},   // "yep" (invalid)
    {vs_ye, cs_t},   // "yet" (invalid)
    {vs_yer, cs_m},  // "yêm" (yếm, yểm)
    {vs_yer, cs_n},  // "yên" (yên, viên)
    {vs_yer, cs_ng}, // "yêng" (chim yểng, yêng hùng)
    {vs_yer, cs_t},  // "yêt" (yết)

    {vs_uye, cs_n},  // "uyen" (khuyên, quyên)
    {vs_uye, cs_t},  // "uyet" (khuyết, quyết)
    {vs_uyer, cs_n}, // "uyên" (duyên, truyền)
    {vs_uyer, cs_t}  // "uyêt" (tuyệt, duyệt)
};

/**
 * @brief Number of entries in the VCPairList table.
 *
 * This constant holds the total number of valid Vietnamese vowel-consonant (VC) pairs
 * defined in VCPairList[]. It is computed as sizeof(VCPairList) / sizeof(VCPair).
 *
 * Usage:
 *   - Use VCPairCount to iterate over all VC pairs in VCPairList[] for validation,
 *     lookup, or generation of Vietnamese syllable endings.
 *   - Ensures that loops and algorithms referencing VCPairList[] remain correct
 *     even if the table is extended or modified.
 *
 * Example:
 *   for (int i = 0; i < VCPairCount; ++i) {
 *       // process VCPairList[i]
 *   }
 */
const int VCPairCount = sizeof(VCPairList) / sizeof(VCPair);

/**
 * @brief Member function pointer type for UkEngine key event handlers.
 *
 * UkKeyProc is a typedef for a pointer to a member function of UkEngine that takes a
 * reference to a UkKeyEvent and returns an int. This allows the engine to store and
 * dispatch different key processing routines (such as processRoof, processTone, etc.)
 * dynamically, for flexible input method handling.
 *
 * Usage:
 *   - Used in arrays or tables (e.g., UkKeyProcList[]) to map input method actions
 *     or key event types to their corresponding handler functions in UkEngine.
 *   - Enables efficient and modular dispatch of key event processing logic.
 *
 * Example:
 *   UkKeyProc proc = UkKeyProcList[action];
 *   (engine->*proc)(event); // Calls the appropriate handler
 */
typedef int (UkEngine::*UkKeyProc)(UkKeyEvent &ev);

/**
 * @brief Table mapping input method actions to UkEngine key event handler functions.
 *
 * UkKeyProcList[] is an array of member function pointers (UkKeyProc) indexed by
 * input method action enums (e.g., vneRoofAll, vneTone0, etc.). Each entry points to
 * the corresponding handler in UkEngine for processing a specific type of key event.
 *
 * Usage:
 *   - Used to dispatch the correct handler for a given input action:
 *       UkKeyProc proc = UkKeyProcList[action];
 *       (engine->*proc)(event);
 *   - Enables modular and efficient mapping from input method logic to implementation.
 *
 * Example mapping:
 *   - vneRoofAll: &UkEngine::processRoof (e.g., "a" → "â")
 *   - vneTone0: &UkEngine::processTone (e.g., "a" → "à")
 *   - vneDd: &UkEngine::processDd (e.g., "d" → "đ")
 *   - vne_telex_w: &UkEngine::processTelexW (e.g., "aw" → "ă")
 *   - vneMapChar: &UkEngine::processMapChar (custom mappings)
 */
UkKeyProc UkKeyProcList[vneCount] = {
    &UkEngine::processRoof,    // vneRoofAll      (e.g., "a" → "â", "e" → "ê")
    &UkEngine::processRoof,    // vneRoof_a       (e.g., "a" → "â")
    &UkEngine::processRoof,    // vneRoof_e       (e.g., "e" → "ê")
    &UkEngine::processRoof,    // vneRoof_o       (e.g., "o" → "ô")
    &UkEngine::processHook,    // vneHookAll      (e.g., "o" → "ơ", "u" → "ư")
    &UkEngine::processHook,    // vneHook_uo      (e.g., "uo" → "ươ")
    &UkEngine::processHook,    // vneHook_u       (e.g., "u" → "ư")
    &UkEngine::processHook,    // vneHook_o       (e.g., "o" → "ơ")
    &UkEngine::processHook,    // vneBowl         (e.g., "a" → "ă")
    &UkEngine::processDd,      // vneDd           (e.g., "d" → "đ")
    &UkEngine::processTone,    // vneTone0        (e.g., "a" → "à")
    &UkEngine::processTone,    // vneTone1        (e.g., "a" → "á")
    &UkEngine::processTone,    // vneTone2        (e.g., "a" → "ả")
    &UkEngine::processTone,    // vneTone3        (e.g., "a" → "ã")
    &UkEngine::processTone,    // vneTone4        (e.g., "a" → "ạ")
    &UkEngine::processTone,    // vneTone5        (e.g., "a" → "ạ")
    &UkEngine::processTelexW,  // vne_telex_w     (e.g., "aw" → "ă", "uw" → "ư")
    &UkEngine::processMapChar, // vneMapChar      (custom mappings, e.g., "z" → "d")
    &UkEngine::processEscChar, // vneEscChar      (escape sequences)
    &UkEngine::processAppend   // vneNormal       (default: append character)
};

/**
 * @brief Lookup the canonical VowelSeq enum for a sequence of up to 3 VnLexiName letters.
 *
 * This function maps a sequence of 1-3 Vietnamese lexical letter enums (VnLexiName)
 * to the corresponding VowelSeq enum value, using the sorted lookup table.
 *
 * Usage:
 *   - Used during syllable parsing to convert input letters to a canonical vowel sequence.
 *   - Enables fast mapping from user input to internal vowel sequence representation.
 *
 * Example:
 *   // Map the sequence 'a', 'i' to the corresponding VowelSeq ("ai")
 *   VowelSeq seq = lookupVSeq(vnl_a, vnl_i);
 *   // seq now holds the enum value for "ai" (e.g., vs_ai)
 *
 *   // For a single vowel letter:
 *   VowelSeq seq2 = lookupVSeq(vnl_o);
 *   // seq2 == vs_o
 *
 * See also: SortedVSeqList, VSeqPair, VowelSeq
 */
VowelSeq lookupVSeq(VnLexiName v1, VnLexiName v2 = vnl_nonVnChar, VnLexiName v3 = vnl_nonVnChar);

/**
 * @brief Lookup the canonical ConSeq enum for a sequence of up to 3 VnLexiName letters.
 *
 * This function maps a sequence of 1-3 Vietnamese lexical letter enums (VnLexiName)
 * to the corresponding ConSeq enum value, using the sorted lookup table.
 *
 * Usage:
 *   - Used during syllable parsing to convert input letters to a canonical consonant sequence.
 *   - Enables fast mapping from user input to internal consonant sequence representation.
 *
 * Example:
 *   // Map the sequence 'n', 'g' to the corresponding ConSeq ("ng")
 *   ConSeq cs = lookupCSeq(vnl_n, vnl_g);
 *   // cs now holds the enum value for "ng" (e.g., cs_ng)
 *
 *   // For a single consonant letter:
 *   ConSeq cs2 = lookupCSeq(vnl_b);
 *   // cs2 == cs_b
 *
 * See also: SortedCSeqList, CSeqPair, ConSeq
 */
ConSeq lookupCSeq(VnLexiName c1, VnLexiName c2 = vnl_nonVnChar, VnLexiName c3 = vnl_nonVnChar);

/**
 * @brief Static initialization flag for UkEngine class-wide tables.
 *
 * This boolean tracks whether the static lookup tables and shared vowel/consonant
 * data have been initialized for the UkEngine class. It is set to true after
 * engineClassInit() runs, ensuring that initialization occurs only once per process.
 *
 * Usage:
 *   - Checked at the start of UkEngine construction or input processing routines.
 *   - If false, triggers engineClassInit() to populate tables (e.g., SortedVSeqList).
 *
 * Example:
 *   if (!UkEngine::m_classInit) {
 *       engineClassInit();
 *   }
 *
 *   // After initialization, m_classInit is true and tables are ready for use.
 */
bool UkEngine::m_classInit = false;

//------------------------------------------------
/**
 * @brief Compare two vowel sequence triples for sorting.
 *
 * @param p1 pointer to first VSeqPair object
 * @param p2 pointer to second VSeqPair object
 * @return -1 if first < second, 1 if first > second, 0 if equal
 */
int tripleVowelCompare(const void *p1, const void *p2)
{
    VSeqPair *t1 = (VSeqPair *)p1;
    VSeqPair *t2 = (VSeqPair *)p2;

    for (int i = 0; i < 3; i++)
    {
        if (t1->v[i] < t2->v[i])
            return -1;
        if (t1->v[i] > t2->v[i])
            return 1;
    }
    return 0;
}

//------------------------------------------------
/**
 * @brief Compare two consonant sequence triples for sorting.
 *
 * @param p1 pointer to first CSeqPair object
 * @param p2 pointer to second CSeqPair object
 * @return -1 if first < second, 1 if first > second, 0 if equal
 */
int tripleConCompare(const void *p1, const void *p2)
{
    CSeqPair *t1 = (CSeqPair *)p1;
    CSeqPair *t2 = (CSeqPair *)p2;

    for (int i = 0; i < 3; i++)
    {
        if (t1->c[i] < t2->c[i])
            return -1;
        if (t1->c[i] > t2->c[i])
            return 1;
    }
    return 0;
}

//------------------------------------------------
/**
 * @brief Compare vowel-consonant pairs for binary search ordering.
 *
 * @param p1 pointer to first VCPair object
 * @param p2 pointer to second VCPair object
 * @return -1 if first < second, 1 if first > second, 0 if equal
 */
int VCPairCompare(const void *p1, const void *p2)
{
    VCPair *t1 = (VCPair *)p1;
    VCPair *t2 = (VCPair *)p2;

    if (t1->v < t2->v)
        return -1;
    if (t1->v > t2->v)
        return 1;

    if (t1->c < t2->c)
        return -1;
    if (t1->c > t2->c)
        return 1;
    return 0;
}

//----------------------------------------------------------
/**
 * @brief Validate a consonant-vowel pairing.
 *
 * @param c consonant sequence to validate
 * @param v vowel sequence to validate
 * @return true when the consonant and vowel form a valid Vietnamese pair
 */
bool isValidCV(ConSeq c, VowelSeq v)
{
    if (c == cs_nil || v == vs_nil)
        return true;

    VowelSeqInfo &vInfo = VSeqList[v];

    if ((c == cs_gi && vInfo.v[0] == vnl_i) ||
        (c == cs_qu && vInfo.v[0] == vnl_u))
        return false; // gi doesn't go with i, qu doesn't go with u

    if (c == cs_k)
    {
        // k can only go with the following vowel sequences
        static VowelSeq kVseq[] = {vs_e, vs_i, vs_y, vs_er, vs_eo, vs_eu,
                                   vs_eru, vs_ia, vs_ie, vs_ier, vs_ieu, vs_ieru, vs_nil};
        int i;
        for (i = 0; kVseq[i] != vs_nil && kVseq[i] != v; i++)
            ;
        return (kVseq[i] != vs_nil);
    }

    // More checks
    return true;
}

//----------------------------------------------------------
/**
 * @brief Validate a vowel-consonant pairing.
 *
 * @param v vowel sequence to validate
 * @param c consonant sequence to validate
 * @return true when the vowel and consonant form a valid ending sequence
 */
bool isValidVC(VowelSeq v, ConSeq c)
{
    if (v == vs_nil || c == cs_nil)
        return true;

    VowelSeqInfo &vInfo = VSeqList[v];
    if (!vInfo.conSuffix)
        return false;

    ConSeqInfo &cInfo = CSeqList[c];
    if (!cInfo.suffix)
        return false;

    VCPair p;
    p.v = v;
    p.c = c;
    if (bsearch(&p, VCPairList, VCPairCount, sizeof(VCPair), VCPairCompare))
        return true;

    return false;
}

//----------------------------------------------------------
/**
 * @brief Validate a consonant-vowel-consonant sequence.
 *
 * @param c1 leading consonant sequence
 * @param v vowel sequence
 * @param c2 trailing consonant sequence
 * @return true when the full CVC sequence is valid in Vietnamese spelling
 */
bool isValidCVC(ConSeq c1, VowelSeq v, ConSeq c2)
{
    if (v == vs_nil)
        return (c1 == cs_nil || c2 != cs_nil);

    if (c1 == cs_nil)
        return isValidVC(v, c2);

    if (c2 == cs_nil)
        return isValidCV(c1, v);

    bool okCV = isValidCV(c1, v);
    bool okVC = isValidVC(v, c2);

    if (okCV && okVC)
        return true;

    if (!okVC)
    {
        // check some exceptions: vc fails but cvc passes

        // quyn, quynh
        if (c1 == cs_qu && v == vs_y && (c2 == cs_n || c2 == cs_nh))
            return true;

        // gieng, gie^ng
        if (c1 == cs_gi && (v == vs_e || v == vs_er) && (c2 == cs_n || c2 == cs_ng))
            return true;
    }
    return false;
}

//------------------------------------------------
/**
 * @brief Initialize static engine lookup tables and shared vowel data.
 *
 * Populates the sorted sequence tables and the vowel lookup map used by
 * the UniKey engine across instances.
 */
void engineClassInit()
{
    int i, j;

    for (i = 0; i < VSeqCount; i++)
    {
        for (j = 0; j < 3; j++)
            SortedVSeqList[i].v[j] = VSeqList[i].v[j];
        SortedVSeqList[i].vs = (VowelSeq)i;
    }

    for (i = 0; i < CSeqCount; i++)
    {
        for (j = 0; j < 3; j++)
            SortedCSeqList[i].c[j] = CSeqList[i].c[j];
        SortedCSeqList[i].cs = (ConSeq)i;
    }

    qsort(SortedVSeqList, VSeqCount, sizeof(VSeqPair), tripleVowelCompare);
    qsort(SortedCSeqList, CSeqCount, sizeof(CSeqPair), tripleConCompare);
    qsort(VCPairList, VCPairCount, sizeof(VCPair), VCPairCompare);

    for (i = 0; i < vnl_lastChar; i++)
        IsVnVowel[i] = true;

    unsigned char ch;
    for (ch = 'a'; ch <= 'z'; ch++)
    {
        if (ch != 'a' && ch != 'e' && ch != 'i' &&
            ch != 'o' && ch != 'u' && ch != 'y')
        {
            IsVnVowel[AZLexiLower[ch - 'a']] = false;
            IsVnVowel[AZLexiUpper[ch - 'a']] = false;
        }
    }
    IsVnVowel[vnl_dd] = false;
    IsVnVowel[vnl_DD] = false;
}

//------------------------------------------------
/**
 * @brief Implementation of lookupVSeq.
 *
 * Maps up to three VnLexiName letters to a VowelSeq enum using binary search.
 *
 * @param v1 First vowel letter (required)
 * @param v2 Second vowel letter (optional, default vnl_nonVnChar)
 * @param v3 Third vowel letter (optional, default vnl_nonVnChar)
 * @return The canonical VowelSeq enum value, or vs_nil if not found.
 *
 * Example usage:
 *   VowelSeq seq = lookupVSeq(vnl_a, vnl_i); // "ai" → vs_ai
 *   VowelSeq seq2 = lookupVSeq(vnl_o);       // "o"  → vs_o
 */
VowelSeq lookupVSeq(VnLexiName v1, VnLexiName v2, VnLexiName v3)
{
    VSeqPair key;
    key.v[0] = v1;
    key.v[1] = v2;
    key.v[2] = v3;

    VSeqPair *pInfo = (VSeqPair *)bsearch(&key, SortedVSeqList, VSeqCount, sizeof(VSeqPair), tripleVowelCompare);
    if (pInfo == 0)
        return vs_nil;
    return pInfo->vs;
}

//------------------------------------------------
/**
 * @brief Implementation of lookupCSeq.
 *
 * Maps up to three VnLexiName letters to a ConSeq enum using binary search.
 *
 * @param c1 First consonant letter (required)
 * @param c2 Second consonant letter (optional, default vnl_nonVnChar)
 * @param c3 Third consonant letter (optional, default vnl_nonVnChar)
 * @return The canonical ConSeq enum value, or cs_nil if not found.
 *
 * Example usage:
 *   ConSeq cs = lookupCSeq(vnl_n, vnl_g); // "ng" → cs_ng
 *   ConSeq cs2 = lookupCSeq(vnl_b);       // "b"  → cs_b
 */
ConSeq lookupCSeq(VnLexiName c1, VnLexiName c2, VnLexiName c3)
{
    CSeqPair key;
    key.c[0] = c1;
    key.c[1] = c2;
    key.c[2] = c3;

    CSeqPair *pInfo = (CSeqPair *)bsearch(&key, SortedCSeqList, CSeqCount, sizeof(CSeqPair), tripleConCompare);
    if (pInfo == 0)
        return cs_nil;
    return pInfo->cs;
}

//------------------------------------------------------------------
/**
 * @brief Handle roof-mark input events for Vietnamese vowel composition.
 *
 * @param ev key event information describing the roof operation
 * @return 1 when event is consumed, or result of processAppend() when roof
 *         cannot be applied
 */
int UkEngine::processRoof(UkKeyEvent &ev)
{
    // 1. Early exit if Vietnamese mode is off or buffer is invalid
    if (!m_pCtrl->vietKey || m_current < 0 || m_buffer[m_current].vOffset < 0)
        return processAppend(ev);

    // 2. Determine which roof mark is being processed (â, ê, ô)
    VnLexiName target;
    switch (ev.evType)
    {
    case vneRoof_a:
        target = vnl_ar;
        break;
    case vneRoof_e:
        target = vnl_er;
        break;
    case vneRoof_o:
        target = vnl_or;
        break;
    default:
        target = vnl_nonVnChar;
    }

    // 3. Gather current vowel sequence and tone information
    VowelSeq vs, newVs;
    int i, vStart, vEnd;
    int curTonePos, newTonePos, tone;
    int changePos;
    bool roofRemoved = false;

    vEnd = m_current - m_buffer[m_current].vOffset;
    vs = m_buffer[vEnd].vseq;
    vStart = vEnd - (VSeqList[vs].len - 1);
    curTonePos = vStart + getTonePosition(vs, vEnd == m_current);
    tone = m_buffer[curTonePos].tone;

    // 4. Handle special cases for u+o vowel sequences (e.g., "uo" → "uô")
    bool doubleChangeUO = false;
    if (vs == vs_uho || vs == vs_uhoh || vs == vs_uhoi || vs == vs_uhohi)
    {
        // Special case: convert "uo"-like sequences to "uô"
        newVs = lookupVSeq(vnl_u, vnl_or, VSeqList[vs].v[2]);
        doubleChangeUO = true;
    }
    else
    {
        // Normal case: get the vowel sequence with roof mark
        newVs = VSeqList[vs].withRoof;
    }

    VowelSeqInfo *pInfo;

    // 5. If no valid new vowel sequence, try to remove an existing roof mark (undo)
    if (newVs == vs_nil)
    {
        if (VSeqList[vs].roofPos == -1)
            return processAppend(ev); // No roof position, can't apply

        // Undo roof: check if the current roof matches the target
        VnLexiName curCh = m_buffer[vStart + VSeqList[vs].roofPos].vnSym;
        if (target != vnl_nonVnChar && curCh != target)
            return processAppend(ev); // Target roof doesn't match current

        // Remove the roof mark (convert â→a, ê→e, ô→o)
        VnLexiName newCh = (curCh == vnl_ar) ? vnl_a : ((curCh == vnl_er) ? vnl_e : vnl_o);
        changePos = vStart + VSeqList[vs].roofPos;

        // Only allow marking at the current position unless freeMarking is enabled
        if (!m_pCtrl->options.freeMarking && changePos != m_current)
            return processAppend(ev);

        markChange(changePos);
        m_buffer[changePos].vnSym = newCh;

        // Recompute the vowel sequence after removing the roof
        if (VSeqList[vs].len == 3)
            newVs = lookupVSeq(m_buffer[vStart].vnSym, m_buffer[vStart + 1].vnSym, m_buffer[vStart + 2].vnSym);
        else if (VSeqList[vs].len == 2)
            newVs = lookupVSeq(m_buffer[vStart].vnSym, m_buffer[vStart + 1].vnSym);
        else
            newVs = lookupVSeq(m_buffer[vStart].vnSym);

        pInfo = &VSeqList[newVs];
        roofRemoved = true;
    }
    else
    {
        // 6. Otherwise, apply the roof mark to the vowel sequence
        pInfo = &VSeqList[newVs];
        if (target != vnl_nonVnChar && pInfo->v[pInfo->roofPos] != target)
            return processAppend(ev);

        // Validate the new CVC (consonant-vowel-consonant) sequence
        bool valid = true;
        ConSeq c1 = cs_nil;
        ConSeq c2 = cs_nil;
        if (m_buffer[m_current].c1Offset != -1)
            c1 = m_buffer[m_current - m_buffer[m_current].c1Offset].cseq;

        if (m_buffer[m_current].c2Offset != -1)
            c2 = m_buffer[m_current - m_buffer[m_current].c2Offset].cseq;

        valid = isValidCVC(c1, newVs, c2);
        if (!valid)
            return processAppend(ev);

        // Determine which position to change (special for "uo" cases)
        if (doubleChangeUO)
        {
            changePos = vStart;
        }
        else
        {
            changePos = vStart + pInfo->roofPos;
        }
        if (!m_pCtrl->options.freeMarking && changePos != m_current)
            return processAppend(ev);
        markChange(changePos);
        if (doubleChangeUO)
        {
            // For "uo" → "uô", update both letters
            m_buffer[vStart].vnSym = vnl_u;
            m_buffer[vStart + 1].vnSym = vnl_or;
        }
        else
        {
            m_buffer[changePos].vnSym = pInfo->v[pInfo->roofPos];
        }
    }

    // 7. Update all sub-vowel sequences in the buffer
    for (i = 0; i < pInfo->len; i++)
    { // update sub-sequences
        m_buffer[vStart + i].vseq = pInfo->sub[i];
    }

    // 8. Reposition the tone mark if needed
    newTonePos = vStart + getTonePosition(newVs, vEnd == m_current);
    /* //For now, users don't seem to like the following processing, thus commented out
    if (roofRemoved && tone != 0 &&
        (!pInfo->complete || changePos == curTonePos)) {
        //remove tone if the vowel sequence becomes incomplete as a result of roof removal OR
        //if removed roof is at the same position as the current tone
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    } else
    */
    if (curTonePos != newTonePos && tone != 0)
    {
        markChange(newTonePos);
        m_buffer[newTonePos].tone = tone;
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    }

    // 9. If roof was removed, update state and revert to append mode
    if (roofRemoved)
    {
        m_singleMode = false;
        processAppend(ev);
        m_reverted = true;
    }

    // 10. Success: event was handled
    return 1;
}

//------------------------------------------------------------------
// can only be called from processHook
//------------------------------------------------------------------
/**
 * @brief Process hook marks when the current vowel sequence contains u+o forms.
 *
 * @param ev key event information describing the hook operation
 * @return 1 when the event is consumed, or result of processAppend() on fallback
 */
int UkEngine::processHookWithUO(UkKeyEvent &ev)
{
    // 1. Setup and early exit if marking is not allowed
    VowelSeq vs, newVs;
    int i, vStart, vEnd;
    int curTonePos, newTonePos, tone;
    bool hookRemoved = false;
    bool removeWithUndo = true;
    bool toneRemoved = false;

    (void)toneRemoved; // fix warning

    VnLexiName *v;

    if (!m_pCtrl->options.freeMarking && m_buffer[m_current].vOffset != 0)
        return processAppend(ev);

    // 2. Gather current vowel sequence and tone information
    // Calculate the end index of the current vowel sequence in the buffer.
    vEnd = m_current - m_buffer[m_current].vOffset;
    // Retrieve the vowel sequence enum at the computed end index.
    vs = m_buffer[vEnd].vseq;
    // Calculate the start index of the vowel sequence in the buffer.
    vStart = vEnd - (VSeqList[vs].len - 1);
    // Get a pointer to the array of letters that make up this vowel sequence.
    v = VSeqList[vs].v;
    // Compute the position in the buffer where the tone mark should be applied.
    curTonePos = vStart + getTonePosition(vs, vEnd == m_current);
    // Retrieve the current tone value at the computed tone position.
    tone = m_buffer[curTonePos].tone;

    // 3. Handle hook event by type (u, o, or all)
    switch (ev.evType)
    {
    case vneHook_u:
        // a) Add or remove hook on 'u'
        if (v[0] == vnl_u)
        {
            // Add hook: u → ư
            newVs = VSeqList[vs].withHook;
            markChange(vStart);
            m_buffer[vStart].vnSym = vnl_uh;
        }
        else
        { // Remove hook: ư → uo
            newVs = lookupVSeq(vnl_u, vnl_o, v[2]);
            markChange(vStart);
            m_buffer[vStart].vnSym = vnl_u;
            m_buffer[vStart + 1].vnSym = vnl_o;
            hookRemoved = true;
            toneRemoved = (m_buffer[vStart].tone != 0);
        }
        break;
    case vneHook_o:
        // b) Add or remove hook on 'o'
        if (v[1] == vnl_o || v[1] == vnl_or)
        {
            // Add hook: o → ơ (with special handling for "th" prefix)
            if (vEnd == m_current && VSeqList[vs].len == 2 &&
                m_buffer[m_current].form == vnw_cv && m_buffer[m_current - 2].cseq == cs_th)
            {
                // o|ô → ơ
                newVs = VSeqList[vs].withHook;
                markChange(vStart + 1);
                m_buffer[vStart + 1].vnSym = vnl_oh;
            }
            else
            {
                // General case: uo → ươ
                newVs = lookupVSeq(vnl_uh, vnl_oh, v[2]);
                if (v[0] == vnl_u)
                {
                    markChange(vStart);
                    m_buffer[vStart].vnSym = vnl_uh;
                    m_buffer[vStart + 1].vnSym = vnl_oh;
                }
                else
                {
                    markChange(vStart + 1);
                    m_buffer[vStart + 1].vnSym = vnl_oh;
                }
            }
        }
        else
        { // Remove hook: ơ → o
            newVs = lookupVSeq(vnl_u, vnl_o, v[2]);
            if (v[0] == vnl_uh)
            {
                markChange(vStart);
                m_buffer[vStart].vnSym = vnl_u;
                m_buffer[vStart + 1].vnSym = vnl_o;
            }
            else
            {
                markChange(vStart + 1);
                m_buffer[vStart + 1].vnSym = vnl_o;
            }
            hookRemoved = true;
            toneRemoved = (m_buffer[vStart + 1].tone != 0);
        }
        break;
    default: // vneHookAll, vneHookUO:
        // c) Handle all hook cases (u/o or both)
        if (v[0] == vnl_u)
        {
            if (v[1] == vnl_o || v[1] == vnl_or)
            {
                // uo → uo+ (with "th" prefix) or u+o+
                if ((vs == vs_uo || vs == vs_uor) && vEnd == m_current &&
                    m_buffer[m_current].form == vnw_cv && m_buffer[m_current - 2].cseq == cs_th)
                {
                    // Special: "thuo" → "thuơ"
                    newVs = vs_uoh;
                    markChange(vStart + 1);
                    m_buffer[vStart + 1].vnSym = vnl_oh;
                }
                else
                {
                    // General: uo → ư, then ư → ươ
                    newVs = VSeqList[vs].withHook;
                    markChange(vStart);
                    m_buffer[vStart].vnSym = vnl_uh;
                    newVs = VSeqList[newVs].withHook;
                    m_buffer[vStart + 1].vnSym = vnl_oh;
                }
            }
            else
            { // uo+ → ư+o+
                newVs = VSeqList[vs].withHook;
                markChange(vStart);
                m_buffer[vStart].vnSym = vnl_uh;
            }
        }
        else
        { // v[0] == vnl_uh
            if (v[1] == vnl_o)
            { // ưo → ư+o+
                newVs = VSeqList[vs].withHook;
                markChange(vStart + 1);
                m_buffer[vStart + 1].vnSym = vnl_oh;
            }
            else
            { // ư+ơ → uo
                newVs = lookupVSeq(vnl_u, vnl_o, v[2]);
                markChange(vStart);
                m_buffer[vStart].vnSym = vnl_u;
                m_buffer[vStart + 1].vnSym = vnl_o;
                hookRemoved = true;
                toneRemoved = (m_buffer[vStart].tone != 0 || m_buffer[vStart + 1].tone != 0);
            }
        }
        break;
    }

    // 4. Update all sub-vowel sequences in the buffer
    VowelSeqInfo *p = &VSeqList[newVs];
    for (i = 0; i < p->len; i++)
    { // update sub-sequences
        m_buffer[vStart + i].vseq = p->sub[i];
    }

    // 5. Reposition the tone mark if needed
    newTonePos = vStart + getTonePosition(newVs, vEnd == m_current);
    /* //For now, users don't seem to like the following processing, thus commented out
    if (hookRemoved && tone != 0 && (!p->complete || toneRemoved)) {
        //remove tone if the vowel sequence becomes incomplete as a result of hook removal
        //OR if a removed hook is at the same position as the current tone
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    }
    else
    */
    if (curTonePos != newTonePos && tone != 0)
    {
        markChange(newTonePos);
        m_buffer[newTonePos].tone = tone;
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    }

    // 6. If hook was removed, update state and revert to append mode
    if (hookRemoved && removeWithUndo)
    {
        m_singleMode = false;
        processAppend(ev);
        m_reverted = true;
    }

    // 7. Success: event was handled
    return 1;
}

//------------------------------------------------------------------
/**
 * @brief Handle hook-mark events for Vietnamese vowel composition.
 *
 * @param ev key event information describing the hook operation
 * @return 1 when the event is consumed, or result of processAppend() when
 *         conversion is not possible
 */
int UkEngine::processHook(UkKeyEvent &ev)
{
    // 1. Early exit if Vietnamese mode is off or buffer is invalid
    if (!m_pCtrl->vietKey || m_current < 0 || m_buffer[m_current].vOffset < 0)
        return processAppend(ev);

    // 2. Gather current vowel sequence and tone information
    VowelSeq vs, newVs;
    int i, vStart, vEnd;
    int curTonePos, newTonePos, tone;
    int changePos;
    bool hookRemoved = false;
    VowelSeqInfo *pInfo;
    VnLexiName *v;

    // Calculate the end index of the current vowel sequence in the buffer.
    vEnd = m_current - m_buffer[m_current].vOffset;
    // Retrieve the vowel sequence enum at the computed end index.
    vs = m_buffer[vEnd].vseq;
    // Get a pointer to the array of letters that make up this vowel sequence.
    v = VSeqList[vs].v;

    // 3. Special case: handle "uo"/"ươ"/"uô"/"uơ" sequences with processHookWithUO
    if (VSeqList[vs].len > 1 &&
        ev.evType != vneBowl &&
        (v[0] == vnl_u || v[0] == vnl_uh) &&
        (v[1] == vnl_o || v[1] == vnl_oh || v[1] == vnl_or))
        return processHookWithUO(ev);

    // 4. Compute start of vowel sequence and tone position
    vStart = vEnd - (VSeqList[vs].len - 1);
    curTonePos = vStart + getTonePosition(vs, vEnd == m_current);
    tone = m_buffer[curTonePos].tone;

    // 5. Attempt to apply the hook mark
    newVs = VSeqList[vs].withHook;
    if (newVs == vs_nil)
    {
        // 5a. If not possible, try to remove an existing hook (undo)
        if (VSeqList[vs].hookPos == -1)
            return processAppend(ev); // hook is not applicable

        // Undo hook: check if the current hook matches the event type
        VnLexiName curCh = m_buffer[vStart + VSeqList[vs].hookPos].vnSym;
        VnLexiName newCh = (curCh == vnl_ab) ? vnl_a : ((curCh == vnl_uh) ? vnl_u : vnl_o);
        changePos = vStart + VSeqList[vs].hookPos;
        if (!m_pCtrl->options.freeMarking && changePos != m_current)
            return processAppend(ev);

        switch (ev.evType)
        {
        case vneHook_u:
            if (curCh != vnl_uh)
                return processAppend(ev);
            break;
        case vneHook_o:
            if (curCh != vnl_oh)
                return processAppend(ev);
            break;
        case vneBowl:
            if (curCh != vnl_ab)
                return processAppend(ev);
            break;
        default:
            if (ev.evType == vneHook_uo && curCh == vnl_ab)
                return processAppend(ev);
        }

        // Actually remove the hook
        markChange(changePos);
        m_buffer[changePos].vnSym = newCh;

        // Recompute the vowel sequence after removing the hook
        if (VSeqList[vs].len == 3)
            newVs = lookupVSeq(m_buffer[vStart].vnSym, m_buffer[vStart + 1].vnSym, m_buffer[vStart + 2].vnSym);
        else if (VSeqList[vs].len == 2)
            newVs = lookupVSeq(m_buffer[vStart].vnSym, m_buffer[vStart + 1].vnSym);
        else
            newVs = lookupVSeq(m_buffer[vStart].vnSym);

        pInfo = &VSeqList[newVs];
        hookRemoved = true;
    }
    else
    {
        // 5b. Otherwise, apply the hook mark to the vowel sequence
        pInfo = &VSeqList[newVs];

        // Ensure the hook matches the event type
        switch (ev.evType)
        {
        case vneHook_u:
            if (pInfo->v[pInfo->hookPos] != vnl_uh)
                return processAppend(ev);
            break;
        case vneHook_o:
            if (pInfo->v[pInfo->hookPos] != vnl_oh)
                return processAppend(ev);
            break;
        case vneBowl:
            if (pInfo->v[pInfo->hookPos] != vnl_ab)
                return processAppend(ev);
            break;
        default: // vneHook_uo, vneHookAll
            if (ev.evType == vneHook_uo && pInfo->v[pInfo->hookPos] == vnl_ab)
                return processAppend(ev);
        }

        // Validate the new CVC (consonant-vowel-consonant) sequence
        bool valid = true;
        ConSeq c1 = cs_nil;
        ConSeq c2 = cs_nil;
        if (m_buffer[m_current].c1Offset != -1)
            c1 = m_buffer[m_current - m_buffer[m_current].c1Offset].cseq;

        if (m_buffer[m_current].c2Offset != -1)
            c2 = m_buffer[m_current - m_buffer[m_current].c2Offset].cseq;

        valid = isValidCVC(c1, newVs, c2);

        if (!valid)
            return processAppend(ev);

        changePos = vStart + pInfo->hookPos;
        if (!m_pCtrl->options.freeMarking && changePos != m_current)
            return processAppend(ev);

        // Actually apply the hook
        markChange(changePos);
        m_buffer[changePos].vnSym = pInfo->v[pInfo->hookPos];
    }

    // 6. Update all sub-vowel sequences in the buffer
    for (i = 0; i < pInfo->len; i++)
    { // update sub-sequences
        m_buffer[vStart + i].vseq = pInfo->sub[i];
    }

    // 7. Reposition the tone mark if needed
    newTonePos = vStart + getTonePosition(newVs, vEnd == m_current);
    /* //For now, users don't seem to like the following processing, thus commented out
    if (hookRemoved && tone != 0 &&
        (!pInfo->complete || (hookRemoved && curTonePos == changePos))) {
        //remove tone if the vowel sequence becomes incomplete as a result of hook removal
        //OR if a removed hook was at the same position as the current tone
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    }
    else */
    if (curTonePos != newTonePos && tone != 0)
    {
        markChange(newTonePos);
        m_buffer[newTonePos].tone = tone;
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    }

    // 8. If hook was removed, update state and revert to append mode
    if (hookRemoved)
    {
        m_singleMode = false;
        processAppend(ev);
        m_reverted = true;
    }

    // 9. Success: event was handled
    return 1;
}

//----------------------------------------------------------
/**
 * @brief Determine the tone marker position for a vowel sequence.
 *
 * @param vs vowel sequence being evaluated
 * @param terminated true if the sequence is at the current cursor position
 * @return index within the vowel sequence where the tone should be applied
 */
int UkEngine::getTonePosition(VowelSeq vs, bool terminated)
{
    VowelSeqInfo &info = VSeqList[vs];
    if (info.len == 1)
        return 0;

    if (info.roofPos != -1)
        return info.roofPos;
    if (info.hookPos != -1)
    {
        if (vs == vs_uhoh || vs == vs_uhohi || vs == vs_uhohu) // u+o+, u+o+u, u+o+i
            return 1;
        return info.hookPos;
    }

    if (info.len == 3)
        return 1;

    if (m_pCtrl->options.modernStyle &&
        (vs == vs_oa || vs == vs_oe || vs == vs_uy))
        return 1;

    return terminated ? 0 : 1;
}

//----------------------------------------------------------
/**
 * @brief Handle tone mark key events for Vietnamese input.
 *
 * @param ev key event containing the tone input
 * @return 1 when the event is consumed, or result of processAppend() if the
 *         tone cannot be applied
 */
int UkEngine::processTone(UkKeyEvent &ev)
{
    if (m_current < 0 || !m_pCtrl->vietKey)
        return processAppend(ev);

    if (m_buffer[m_current].form == vnw_c &&
        (m_buffer[m_current].cseq == cs_gi || m_buffer[m_current].cseq == cs_gin))
    {
        int p = (m_buffer[m_current].cseq == cs_gi) ? m_current : m_current - 1;
        if (m_buffer[p].tone == 0 && ev.tone == 0)
            return processAppend(ev);
        markChange(p);
        if (m_buffer[p].tone == ev.tone)
        {
            m_buffer[p].tone = 0;
            m_singleMode = false;
            processAppend(ev);
            m_reverted = true;
            return 1;
        }
        m_buffer[p].tone = ev.tone;
        return 1;
    }

    if (m_buffer[m_current].vOffset < 0)
        return processAppend(ev);

    int vEnd;
    VowelSeq vs;

    vEnd = m_current - m_buffer[m_current].vOffset;
    vs = m_buffer[vEnd].vseq;
    VowelSeqInfo &info = VSeqList[vs];
    if (m_pCtrl->options.spellCheckEnabled && !m_pCtrl->options.freeMarking && !info.complete)
        return processAppend(ev);

    if (m_buffer[m_current].form == vnw_vc || m_buffer[m_current].form == vnw_cvc)
    {
        ConSeq cs = m_buffer[m_current].cseq;
        if ((cs == cs_c || cs == cs_ch || cs == cs_p || cs == cs_t) &&
            (ev.tone == 2 || ev.tone == 3 || ev.tone == 4))
            return processAppend(ev); // c, ch, p, t suffixes don't allow ` ? ~
    }

    int toneOffset = getTonePosition(vs, vEnd == m_current);
    int tonePos = vEnd - (info.len - 1) + toneOffset;
    if (m_buffer[tonePos].tone == 0 && ev.tone == 0)
        return processAppend(ev);

    if (m_buffer[tonePos].tone == ev.tone)
    {
        markChange(tonePos);
        m_buffer[tonePos].tone = 0;
        m_singleMode = false;
        processAppend(ev);
        m_reverted = true;
        return 1;
    }

    markChange(tonePos);
    m_buffer[tonePos].tone = ev.tone;
    return 1;
}

//----------------------------------------------------------
/**
 * @brief Process the Telex "dd" sequence into the Vietnamese đ character.
 *
 * @param ev key event describing the double-d input
 * @return 1 when the dd mapping is applied or undone, or result of
 *         processAppend() otherwise
 */
int UkEngine::processDd(UkKeyEvent &ev)
{
    if (!m_pCtrl->vietKey || m_current < 0)
        return processAppend(ev);

    int pos;

    // we want to allow dd even in non-vn sequence, because dd is used a lot in abbreviation
    // we allow dd only if preceding character is not a vowel
    if (m_buffer[m_current].form == vnw_nonVn &&
        m_buffer[m_current].vnSym == vnl_d &&
        (m_buffer[m_current - 1].vnSym == vnl_nonVnChar || !IsVnVowel[m_buffer[m_current - 1].vnSym]))
    {
        m_singleMode = true;
        pos = m_current;
        markChange(pos);
        m_buffer[pos].cseq = cs_dd;
        m_buffer[pos].vnSym = vnl_dd;
        m_buffer[pos].form = vnw_c;
        m_buffer[pos].c1Offset = 0;
        m_buffer[pos].c2Offset = -1;
        m_buffer[pos].vOffset = -1;
        return 1;
    }

    if (m_buffer[m_current].c1Offset < 0)
    {
        return processAppend(ev);
    }

    pos = m_current - m_buffer[m_current].c1Offset;
    if (!m_pCtrl->options.freeMarking && pos != m_current)
    {
        return processAppend(ev);
    }

    if (m_buffer[pos].cseq == cs_d)
    {
        markChange(pos);
        m_buffer[pos].cseq = cs_dd;
        m_buffer[pos].vnSym = vnl_dd;
        // never spellcheck a word which starts with dd, because it's used alot in abbreviation
        m_singleMode = true;
        return 1;
    }

    if (m_buffer[pos].cseq == cs_dd)
    {
        // undo dd
        markChange(pos);
        m_buffer[pos].cseq = cs_d;
        m_buffer[pos].vnSym = vnl_d;
        m_singleMode = false;
        processAppend(ev);
        m_reverted = true;
        return 1;
    }

    return processAppend(ev);
}

//----------------------------------------------------------
/**
 * @brief Toggle a lexicon symbol between lowercase and uppercase Vietnamese forms.
 *
 * @param x input lexicon symbol
 * @return corresponding symbol with inverted case, or x unchanged for non-VN symbols
 */
VnLexiName changeCase(VnLexiName x)
{
    if (x == vnl_nonVnChar)
        return x;
    if (!(x & 0x01))
        return (VnLexiName)(x + 1);
    return (VnLexiName)(x - 1);
}

//----------------------------------------------------------
/**
 * @brief Convert a Vietnamese lexicon symbol to its lowercase form.
 *
 * @param x input lexicon symbol
 * @return lowercase form of x, or x unchanged when it is not a VN symbol
 */
inline VnLexiName vnToLower(VnLexiName x)
{
    if (x == vnl_nonVnChar)
        return x;
    if (!(x & 0x01)) // even
        return (VnLexiName)(x + 1);
    return x;
}

//----------------------------------------------------------
/**
 * @brief Map a raw character key to Vietnamese input processing rules.
 *
 * @param ev key event containing the mapped character
 * @return 1 when the event is consumed, or output of processAppend() otherwise
 */
int UkEngine::processMapChar(UkKeyEvent &ev)
{
    int capsLockOn = 0;
    int shiftPressed = 0;
    if (m_keyCheckFunc)
        m_keyCheckFunc(&shiftPressed, &capsLockOn);

    if (capsLockOn)
        ev.vnSym = changeCase(ev.vnSym);

    int ret = processAppend(ev);
    if (!m_pCtrl->vietKey)
        return ret;

    if (m_current >= 0 && m_buffer[m_current].form != vnw_empty &&
        m_buffer[m_current].form != vnw_nonVn)
    {
        return 1;
    }

    if (m_current < 0)
        return 0;

    // mapChar doesn't apply
    m_current--;
    WordInfo &entry = m_buffer[m_current];

    bool undo = false;
    // test if undo is needed
    if (entry.form != vnw_empty && entry.form != vnw_nonVn)
    {
        VnLexiName prevSym = entry.vnSym;
        if (entry.caps)
        {
            prevSym = (VnLexiName)(prevSym - 1);
        }
        if (prevSym == ev.vnSym)
        {
            if (entry.form != vnw_c)
            {
                int vStart, vEnd, curTonePos, newTonePos, tone;
                VowelSeq vs, newVs;

                vEnd = m_current - entry.vOffset;
                vs = m_buffer[vEnd].vseq;
                vStart = vEnd - VSeqList[vs].len + 1;
                curTonePos = vStart + getTonePosition(vs, vEnd == m_current);
                tone = m_buffer[curTonePos].tone;
                markChange(m_current);
                m_current--;

                // check if tone position is needed
                if (tone != 0 && m_current >= 0 &&
                    (m_buffer[m_current].form == vnw_v || m_buffer[m_current].form == vnw_cv))
                {
                    newVs = m_buffer[m_current].vseq;
                    newTonePos = vStart + getTonePosition(newVs, true);
                    if (newTonePos != curTonePos)
                    {
                        markChange(newTonePos);
                        m_buffer[newTonePos].tone = tone;
                        markChange(curTonePos);
                        m_buffer[curTonePos].tone = 0;
                    }
                }
            }
            else
            {
                markChange(m_current);
                m_current--;
            }
            undo = true;
        }
    }

    ev.evType = vneNormal;
    ev.chType = m_pCtrl->input.getCharType(ev.keyCode);
    ev.vnSym = IsoToVnLexi(ev.keyCode);
    ret = processAppend(ev);
    if (undo)
    {
        m_singleMode = false;
        m_reverted = true;
        return 1;
    }
    return ret;
}

//----------------------------------------------------------
/**
 * @brief Handle the Telex 'w' key special case in Vietnamese input.
 *
 * @param ev key event for the 'w' mapping
 * @return 1 when the event is consumed, or result of fallback processing
 */
int UkEngine::processTelexW(UkKeyEvent &ev)
{
    if (!m_pCtrl->vietKey)
        return processAppend(ev);

    int ret;
    static bool usedAsMapChar = false;
    int capsLockOn = 0;
    int shiftPressed = 0;
    if (m_keyCheckFunc)
        m_keyCheckFunc(&shiftPressed, &capsLockOn);

    if (usedAsMapChar)
    {
        ev.evType = vneMapChar;
        ev.vnSym = isupper(ev.keyCode) ? vnl_Uh : vnl_uh;
        if (capsLockOn)
            ev.vnSym = changeCase(ev.vnSym);
        ev.chType = ukcVn;
        ret = processMapChar(ev);
        if (ret == 0)
        {
            if (m_current >= 0)
                m_current--;
            usedAsMapChar = false;
            ev.evType = vneHookAll;
            return processHook(ev);
        }
        return ret;
    }

    ev.evType = vneHookAll;
    usedAsMapChar = false;
    ret = processHook(ev);
    if (ret == 0)
    {
        if (m_current >= 0)
            m_current--;
        ev.evType = vneMapChar;
        ev.vnSym = isupper(ev.keyCode) ? vnl_Uh : vnl_uh;
        if (capsLockOn)
            ev.vnSym = changeCase(ev.vnSym);
        ev.chType = ukcVn;
        usedAsMapChar = true;
        return processMapChar(ev);
    }
    return ret;
}

//----------------------------------------------------------
/**
 * @brief Determine whether a VIQR escape should be emitted for the current key.
 *
 * @param ev key event with the candidate escape character
 * @return nonzero if VIQR escape is produced, zero otherwise
 */
int UkEngine::checkEscapeVIQR(UkKeyEvent &ev)
{
    if (m_current < 0)
        return 0;
    WordInfo &entry = m_buffer[m_current];
    int escape = 0;
    if (entry.form == vnw_v || entry.form == vnw_cv)
    {
        switch (ev.keyCode)
        {
        case '^':
            escape = (entry.vnSym == vnl_a || entry.vnSym == vnl_o || entry.vnSym == vnl_e);
            break;
        case '(':
            escape = (entry.vnSym == vnl_a);
            break;
        case '+':
            escape = (entry.vnSym == vnl_o || entry.vnSym == vnl_u);
            break;
        case '\'':
        case '`':
        case '?':
        case '~':
        case '.':
            escape = (entry.tone == 0);
            break;
        }
    }
    else if (entry.form == vnw_nonVn)
    {
        unsigned char ch = toupper(entry.keyCode);
        switch (ev.keyCode)
        {
        case '^':
            escape = (ch == 'A' || ch == 'O' || ch == 'E');
            break;
        case '(':
            escape = (ch == 'A');
            break;
        case '+':
            escape = (ch == 'O' || ch == 'U');
            break;
        case '\'':
        case '`':
        case '?':
        case '~':
        case '.':
            escape = (ch == 'A' || ch == 'E' || ch == 'I' ||
                      ch == 'O' || ch == 'U' || ch == 'Y');
            break;
        }
    }

    if (escape)
    {
        m_current++;
        WordInfo *p = &m_buffer[m_current];
        p->form = (ev.chType == ukcWordBreak) ? vnw_empty : vnw_nonVn;
        p->c1Offset = p->c2Offset = p->vOffset = -1;
        p->keyCode = '?';
        p->vnSym = vnl_nonVnChar;

        m_current++;
        p++;
        p->form = (ev.chType == ukcWordBreak) ? vnw_empty : vnw_nonVn;
        p->c1Offset = p->c2Offset = p->vOffset = -1;
        p->keyCode = ev.keyCode;
        p->vnSym = vnl_nonVnChar;

        // write output
        m_pOutBuf[0] = '\\';
        m_pOutBuf[1] = ev.keyCode;
        *m_pOutSize = 2;
        m_outputWritten = true;
    }
    return escape;
}

//----------------------------------------------------------
/**
 * @brief Append the key event into the engine buffer according to its character type.
 *
 * @param ev key event to append
 * @return 1 when a substitution or output is produced, 0 otherwise
 */
int UkEngine::processAppend(UkKeyEvent &ev)
{
    int ret = 0;
    switch (ev.chType)
    {
    case ukcReset:
#if defined(_WIN32)
        if (ev.keyCode == ENTER_CHAR)
        {
            if (m_pCtrl->options.macroEnabled && macroMatch(ev))
                return 1;
        }
#endif
        reset();
        return 0;
    case ukcWordBreak:
        m_singleMode = false;
        return processWordEnd(ev);
    case ukcNonVn:
    {
        if (m_pCtrl->vietKey && m_pCtrl->charsetId == CONV_CHARSET_VIQR && checkEscapeVIQR(ev))
            return 1;

        m_current++;
        WordInfo &entry = m_buffer[m_current];
        entry.form = (ev.chType == ukcWordBreak) ? vnw_empty : vnw_nonVn;
        entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
        entry.keyCode = ev.keyCode;
        entry.vnSym = vnToLower(ev.vnSym);
        entry.tone = 0;
        entry.caps = (entry.vnSym != ev.vnSym);
        if (!m_pCtrl->vietKey || m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING)
            return 0;
        markChange(m_current);
        return 1;
    }
    case ukcVn:
    {
        if (IsVnVowel[ev.vnSym])
        {
            VnLexiName v = (VnLexiName)StdVnNoTone[vnToLower(ev.vnSym)];
            if (m_current >= 0 && m_buffer[m_current].form == vnw_c &&
                ((m_buffer[m_current].cseq == cs_q && v == vnl_u) ||
                 (m_buffer[m_current].cseq == cs_g && v == vnl_i)))
            {
                return appendConsonnant(ev); // process u after q, i after g as consonnants
            }
            return appendVowel(ev);
        }
        return appendConsonnant(ev);
    }
    break;
    }

    return ret;
}

//----------------------------------------------------------
/**
 * @brief Append a vowel symbol to the current Vietnamese composition buffer.
 *
 * @param ev key event describing the vowel input
 * @return 1 if the buffer was modified and output should be updated, 0 otherwise
 */
int UkEngine::appendVowel(UkKeyEvent &ev)
{
    bool autoCompleted = false;

    m_current++;
    WordInfo &entry = m_buffer[m_current];

    VnLexiName lowerSym = vnToLower(ev.vnSym);
    VnLexiName canSym = (VnLexiName)StdVnNoTone[lowerSym];

    entry.vnSym = canSym;
    entry.caps = (lowerSym != ev.vnSym);
    entry.tone = (lowerSym - canSym) / 2;
    entry.keyCode = ev.keyCode;

    if (m_current == 0 || !m_pCtrl->vietKey)
    {
        entry.form = vnw_v;
        entry.c1Offset = entry.c2Offset = -1;
        entry.vOffset = 0;
        entry.vseq = lookupVSeq(canSym);

        if (!m_pCtrl->vietKey ||
            ((m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING) && isalpha(entry.keyCode)))
        {
            return 0;
        }
        markChange(m_current);
        return 1;
    }

    WordInfo &prev = m_buffer[m_current - 1];
    VowelSeq vs, newVs;
    ConSeq cs;
    int prevTonePos;
    int tone, newTone, tonePos, newTonePos;

    switch (prev.form)
    {

    case vnw_empty:
        entry.form = vnw_v;
        entry.c1Offset = entry.c2Offset = -1;
        entry.vOffset = 0;
        entry.vseq = newVs = lookupVSeq(canSym);
        break;

    case vnw_nonVn:
    case vnw_cvc:
    case vnw_vc:
        entry.form = vnw_nonVn;
        entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
        break;

    case vnw_v:
    case vnw_cv:
        vs = prev.vseq;
        prevTonePos = (m_current - 1) - (VSeqList[vs].len - 1) + getTonePosition(vs, true);
        tone = m_buffer[prevTonePos].tone;

        if (lowerSym != canSym && tone != 0) // new sym has a tone, but there's is already a preceeding tone
            newVs = vs_nil;
        else
        {
            if (VSeqList[vs].len == 3)
                newVs = vs_nil;
            else if (VSeqList[vs].len == 2)
                newVs = lookupVSeq(VSeqList[vs].v[0], VSeqList[vs].v[1], canSym);
            else
                newVs = lookupVSeq(VSeqList[vs].v[0], canSym);
        }

        if (newVs != vs_nil && prev.form == vnw_cv)
        {
            cs = m_buffer[m_current - 1 - prev.c1Offset].cseq;
            if (!isValidCV(cs, newVs))
                newVs = vs_nil;
        }

        if (newVs == vs_nil)
        {
            entry.form = vnw_nonVn;
            entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
            break;
        }

        entry.form = prev.form;
        if (prev.form == vnw_cv)
            entry.c1Offset = prev.c1Offset + 1;
        else
            entry.c1Offset = -1;
        entry.c2Offset = -1;
        entry.vOffset = 0;
        entry.vseq = newVs;
        entry.tone = 0;

        newTone = (lowerSym - canSym) / 2;
        if (tone == 0)
        {
            if (newTone != 0)
            {
                tone = newTone;
                tonePos = getTonePosition(newVs, true) + ((m_current - 1) - VSeqList[vs].len + 1);
                markChange(tonePos);
                m_buffer[tonePos].tone = tone;
                return 1;
            }
        }
        else
        {
            newTonePos = getTonePosition(newVs, true) + ((m_current - 1) - VSeqList[vs].len + 1);
            if (newTonePos != prevTonePos)
            {
                markChange(prevTonePos);
                m_buffer[prevTonePos].tone = 0;
                markChange(newTonePos);
                if (newTone != 0)
                    tone = newTone;
                m_buffer[newTonePos].tone = tone;
                return 1;
            }
            if (newTone != 0 && newTone != tone)
            {
                tone = newTone;
                markChange(prevTonePos);
                m_buffer[prevTonePos].tone = tone;
                return 1;
            }
        }

        break;
    case vnw_c:
        newVs = lookupVSeq(canSym);
        cs = prev.cseq;
        if (!isValidCV(cs, newVs))
        {
            entry.form = vnw_nonVn;
            entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
            break;
        }

        entry.form = vnw_cv;
        entry.c1Offset = 1;
        entry.c2Offset = -1;
        entry.vOffset = 0;
        entry.vseq = newVs;

        if (cs == cs_gi && prev.tone != 0)
        {
            if (entry.tone == 0)
                entry.tone = prev.tone;
            markChange(m_current - 1);
            prev.tone = 0;
            return 1;
        }

        break;
    }

    if (!autoCompleted &&
        (m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING) &&
        isalpha(entry.keyCode))
    {
        return 0;
    }

    markChange(m_current);
    return 1;
}

//----------------------------------------------------------
/**
 * @brief Append a consonant symbol to the current Vietnamese composition buffer.
 *
 * @param ev key event describing the consonant input
 * @return 1 if the buffer was modified and output should be updated, 0 otherwise
 */
int UkEngine::appendConsonnant(UkKeyEvent &ev)
{
    bool complexEvent = false;
    m_current++;
    WordInfo &entry = m_buffer[m_current];

    VnLexiName lowerSym = vnToLower(ev.vnSym);

    entry.vnSym = lowerSym;
    entry.caps = (lowerSym != ev.vnSym);
    entry.keyCode = ev.keyCode;
    entry.tone = 0;

    if (m_current == 0 || !m_pCtrl->vietKey)
    {
        entry.form = vnw_c;
        entry.c1Offset = 0;
        entry.c2Offset = -1;
        entry.vOffset = -1;
        entry.cseq = lookupCSeq(lowerSym);
        if (!m_pCtrl->vietKey || m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING)
            return 0;
        markChange(m_current);
        return 1;
    }

    ConSeq cs, newCs, c1;
    VowelSeq vs, newVs;
    bool isValid;

    WordInfo &prev = m_buffer[m_current - 1];

    switch (prev.form)
    {
    case vnw_nonVn:
        entry.form = vnw_nonVn;
        entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
        if (m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING)
            return 0;
        markChange(m_current);
        return 1;
    case vnw_empty:
        entry.form = vnw_c;
        entry.c1Offset = 0;
        entry.c2Offset = -1;
        entry.vOffset = -1;
        entry.cseq = lookupCSeq(lowerSym);
        if (m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING)
            return 0;
        markChange(m_current);
        return 1;
    case vnw_v:
    case vnw_cv:
        vs = prev.vseq;
        newVs = vs;
        if (vs == vs_uoh || vs == vs_uho)
        {
            newVs = vs_uhoh;
        }

        c1 = cs_nil;
        if (prev.c1Offset != -1)
            c1 = m_buffer[m_current - 1 - prev.c1Offset].cseq;

        newCs = lookupCSeq(lowerSym);
        isValid = isValidCVC(c1, newVs, newCs);

        if (isValid)
        {
            // check u+o -> u+o+
            if (vs == vs_uho)
            {
                markChange(m_current - 1);
                prev.vnSym = vnl_oh;
                prev.vseq = vs_uhoh;
                complexEvent = true;
            }
            else if (vs == vs_uoh)
            {
                markChange(m_current - 2);
                m_buffer[m_current - 2].vnSym = vnl_uh;
                m_buffer[m_current - 2].vseq = vs_uh;
                prev.vseq = vs_uhoh;
                complexEvent = true;
            }

            if (prev.form == vnw_v)
            {
                entry.form = vnw_vc;
                entry.c1Offset = -1;
                entry.c2Offset = 0;
                entry.vOffset = 1;
            }
            else
            { // prev == vnw_cv
                entry.form = vnw_cvc;
                entry.c1Offset = prev.c1Offset + 1;
                entry.c2Offset = 0;
                entry.vOffset = 1;
            }
            entry.cseq = newCs;

            // reposition tone if needed
            int oldIdx = (m_current - 1) - (VSeqList[vs].len - 1) + getTonePosition(vs, true);
            if (m_buffer[oldIdx].tone != 0)
            {
                int newIdx = (m_current - 1) - (VSeqList[newVs].len - 1) + getTonePosition(newVs, false);
                if (newIdx != oldIdx)
                {
                    markChange(newIdx);
                    m_buffer[newIdx].tone = m_buffer[oldIdx].tone;
                    markChange(oldIdx);
                    m_buffer[oldIdx].tone = 0;
                    return 1;
                }
            }
        }
        else
        {
            entry.form = vnw_nonVn;
            entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
        }

        if (complexEvent)
        {
            return 1;
        }

        if (m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING)
            return 0;
        markChange(m_current);
        return 1;
    case vnw_c:
    case vnw_vc:
    case vnw_cvc:
        cs = prev.cseq;
        if (CSeqList[cs].len == 3)
            newCs = cs_nil;
        else if (CSeqList[cs].len == 2)
            newCs = lookupCSeq(CSeqList[cs].c[0], CSeqList[cs].c[1], lowerSym);
        else
            newCs = lookupCSeq(CSeqList[cs].c[0], lowerSym);

        if (newCs != cs_nil && (prev.form == vnw_vc || prev.form == vnw_cvc))
        {
            // Check CVC combination
            c1 = cs_nil;
            if (prev.c1Offset != -1)
                c1 = m_buffer[m_current - 1 - prev.c1Offset].cseq;

            int vIdx = (m_current - 1) - prev.vOffset;
            vs = m_buffer[vIdx].vseq;
            isValid = isValidCVC(c1, vs, newCs);

            if (!isValid)
                newCs = cs_nil;
        }

        if (newCs == cs_nil)
        {
            entry.form = vnw_nonVn;
            entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
        }
        else
        {
            if (prev.form == vnw_c)
            {
                entry.form = vnw_c;
                entry.c1Offset = 0;
                entry.c2Offset = -1;
                entry.vOffset = -1;
            }
            else if (prev.form == vnw_vc)
            {
                entry.form = vnw_vc;
                entry.c1Offset = -1;
                entry.c2Offset = 0;
                entry.vOffset = prev.vOffset + 1;
            }
            else
            { // vnw_cvc
                entry.form = vnw_cvc;
                entry.c1Offset = prev.c1Offset + 1;
                entry.c2Offset = 0;
                entry.vOffset = prev.vOffset + 1;
            }
            entry.cseq = newCs;
        }
        if (m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING)
            return 0;
        markChange(m_current);
        return 1;
    }

    if (m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING)
        return 0;
    markChange(m_current);
    return 1;
}

//----------------------------------------------------------
/**
 * @brief Handle explicit escape-character entry in Vietnamese input mode.
 *
 * @param ev key event for the escape character
 * @return result of processAppend() after marking escape mode if needed
 */
int UkEngine::processEscChar(UkKeyEvent &ev)
{
    if (m_pCtrl->vietKey &&
        m_current >= 0 && m_buffer[m_current].form != vnw_empty && m_buffer[m_current].form != vnw_nonVn)
    {
        m_toEscape = true;
    }
    return processAppend(ev);
}

//----------------------------------------------------------
/**
 * @brief Pass a key code through the engine without transformation.
 *
 * @param keyCode raw key code to forward into processAppend()
 */
void UkEngine::pass(int keyCode)
{
    UkKeyEvent ev;
    m_pCtrl->input.keyCodeToEvent(keyCode, ev);
    processAppend(ev);
}

//---------------------------------------------
// This can be called only after other processing have been done.
// The new event is supposed to be put into m_buffer already
//---------------------------------------------
/**
 * @brief Process current input when spell checking is disabled.
 *
 * @param ev key event driving the current state update
 * @return 1 if the buffer state changed, 0 otherwise
 */
int UkEngine::processNoSpellCheck(UkKeyEvent &ev)
{
    WordInfo &entry = m_buffer[m_current];
    if (IsVnVowel[entry.vnSym])
    {
        entry.form = vnw_v;
        entry.vOffset = 0;
        entry.vseq = lookupVSeq(entry.vnSym);
        entry.c1Offset = entry.c2Offset = -1;
    }
    else
    {
        entry.form = vnw_c;
        entry.c1Offset = 0;
        entry.c2Offset = -1;
        entry.vOffset = -1;
        entry.cseq = lookupCSeq(entry.vnSym);
    }

    if (ev.evType == vneNormal &&
        ((entry.keyCode >= 'a' && entry.keyCode <= 'z') ||
         (entry.keyCode >= 'A' && entry.keyCode <= 'Z')))
        return 0;
    markChange(m_current);
    return 1;
}
//----------------------------------------------------------
/**
 * @brief Main entrypoint for processing a raw key code through UniKey.
 *
 * @param keyCode input key code to process
 * @param backs[out] number of backspace operations required by the host
 * @param outBuf output buffer for generated characters
 * @param outSize[in,out] buffer size on entry and bytes written on exit
 * @param outType[out] output type describing produced content
 * @return 1 if the key event was consumed, 0 if ignored, or negative on error
 */
int UkEngine::process(unsigned int keyCode, int &backs, unsigned char *outBuf, int &outSize, UkOutputType &outType)
{
    UkKeyEvent ev;
    prepareBuffer();
    m_backs = 0;
    m_changePos = m_current + 1;
    m_pOutBuf = outBuf;
    m_pOutSize = &outSize;
    m_outputWritten = false;
    m_reverted = false;
    m_keyRestored = false;
    m_keyRestoring = false;
    m_outType = UkCharOutput;

    m_pCtrl->input.keyCodeToEvent(keyCode, ev);

    int ret;
    if (!m_toEscape)
    {
        ret = (this->*UkKeyProcList[ev.evType])(ev);
    }
    else
    {
        m_toEscape = false;
        if (m_current < 0 || ev.evType == vneNormal || ev.evType == vneEscChar)
        {
            ret = processAppend(ev);
        }
        else
        {
            m_current--;
            processAppend(ev);
            markChange(m_current); // this will assign m_backs to 1 and mark the character for output
            ret = 1;
        }
    }

    if (m_pCtrl->vietKey &&
        m_current >= 0 && m_buffer[m_current].form == vnw_nonVn &&
        ev.chType == ukcVn &&
        (!m_pCtrl->options.spellCheckEnabled || m_singleMode))
    {

        // The spell check has failed, but because we are in non-spellcheck mode,
        // we consider the new character as the beginning of a new word
        ret = processNoSpellCheck(ev);
        /*
        if ((!m_pCtrl->options.spellCheckEnabled || m_singleMode) ||
            ( !m_reverted &&
              (m_current < 1 || m_buffer[m_current-1].form != vnw_nonVn)) ) {

            ret = processNoSpellCheck(ev);
        }
        */
    }

    // we add key to key buffer only if that key has not caused a reset
    if (m_current >= 0)
    {
        ev.chType = m_pCtrl->input.getCharType(ev.keyCode);
        m_keyCurrent++;
        m_keyStrokes[m_keyCurrent].ev = ev;
        m_keyStrokes[m_keyCurrent].converted = (ret && !m_keyRestored);
    }

    if (ret == 0)
    {
        backs = 0;
        outSize = 0;
        outType = m_outType;
        return 0;
    }

    backs = m_backs;
    if (!m_outputWritten)
    {
        writeOutput(outBuf, outSize);
    }
    outType = m_outType;

    return ret;
}

//----------------------------------------------------------
// Returns 0 on success
//         error code otherwise
//  outBuf: buffer to write
//  outSize: [in] size of buffer in bytes
//           [out] bytes written to buffer
//----------------------------------------------------------
/**
 * @brief Write the current composition output into the caller-provided buffer.
 *
 * @param outBuf buffer to receive output bytes
 * @param outSize[in,out] on entry byte capacity, on exit bytes written
 * @return 0 on success, VNCONV_OUT_OF_MEMORY on failure
 */
int UkEngine::writeOutput(unsigned char *outBuf, int &outSize)
{
    StdVnChar stdChar;
    int i, bytesWritten;
    int ret = 1;
    StringBOStream os(outBuf, outSize);
    VnCharset *pCharset = VnCharsetLibObj.getVnCharset(m_pCtrl->charsetId);
    pCharset->startOutput();

    for (i = m_changePos; i <= m_current; i++)
    {
        if (m_buffer[i].vnSym != vnl_nonVnChar)
        {
            // process vn symbol
            stdChar = m_buffer[i].vnSym + VnStdCharOffset;
            if (m_buffer[i].caps)
                stdChar--;
            if (m_buffer[i].tone != 0)
                stdChar += m_buffer[i].tone * 2;
        }
        else
        {
            stdChar = IsoToStdVnChar(m_buffer[i].keyCode);
        }

        if (stdChar != INVALID_STD_CHAR)
            ret = pCharset->putChar(os, stdChar, bytesWritten);
    }

    outSize = os.getOutBytes();
    return (ret ? 0 : VNCONV_OUT_OF_MEMORY);
}

//---------------------------------------------
// Returns the number of backspaces needed to
// go back from last to first
//---------------------------------------------
/**
 * @brief Compute the number of backspace steps needed between buffer indices.
 *
 * @param first starting buffer index
 * @param last ending buffer index
 * @return number of backspace operations required
 */
int UkEngine::getSeqSteps(int first, int last)
{
    StdVnChar stdChar;

    if (last < first)
        return 0;

    if (m_pCtrl->charsetId == CONV_CHARSET_XUTF8 ||
        m_pCtrl->charsetId == CONV_CHARSET_UNICODE)
        return (last - first + 1);

    StringBOStream os(0, 0);
    int i, bytesWritten;

    VnCharset *pCharset = VnCharsetLibObj.getVnCharset(m_pCtrl->charsetId);
    pCharset->startOutput();

    for (i = first; i <= last; i++)
    {
        if (m_buffer[i].vnSym != vnl_nonVnChar)
        {
            // process vn symbol
            stdChar = m_buffer[i].vnSym + VnStdCharOffset;
            if (m_buffer[i].caps)
                stdChar--;
            if (m_buffer[i].tone != 0)
                stdChar += m_buffer[i].tone * 2;
        }
        else
        {
            stdChar = m_buffer[i].keyCode;
        }

        if (stdChar != INVALID_STD_CHAR)
            pCharset->putChar(os, stdChar, bytesWritten);
    }

    int len = os.getOutBytes();
    if (m_pCtrl->charsetId == CONV_CHARSET_UNIDECOMPOSED)
        len = len / 2;
    return len;
}

//---------------------------------------------
/**
 * @brief Mark a position in the buffer where a compositional change begins.
 *
 * @param pos buffer index where the next output change starts
 */
void UkEngine::markChange(int pos)
{
    if (pos < m_changePos)
    {
        m_backs += getSeqSteps(pos, m_changePos - 1);
        m_changePos = pos;
    }
}

//----------------------------------------------------------------
// Called from processBackspace to keep
// character buffer (m_buffer) and key stroke buffer in synch
//----------------------------------------------------------------
/**
 * @brief Synchronize the internal keystroke history with the current buffer state.
 */
void UkEngine::synchKeyStrokeBuffer()
{
    // synchronize with key-stroke buffer
    if (m_keyCurrent >= 0)
        m_keyCurrent--;
    if (m_current >= 0 && m_buffer[m_current].form == vnw_empty)
    {
        // in character buffer, we have reached a word break,
        // so we also need to move key stroke pointer backward to corresponding word break
        while (m_keyCurrent >= 0 && m_keyStrokes[m_keyCurrent].ev.chType != ukcWordBreak)
        {
            m_keyCurrent--;
        }
    }
}

//---------------------------------------------
/**
 * @brief Process a backspace event and update output/backspace counts.
 *
 * @param backs[out] number of backspaces to emit
 * @param outBuf buffer to receive any replacement output
 * @param outSize[in,out] buffer capacity on entry and bytes written on exit
 * @param outType[out] output type describing produced content
 * @return 1 when character data was restored, 0 when only backspace happens
 */
int UkEngine::processBackspace(int &backs, unsigned char *outBuf, int &outSize, UkOutputType &outType)
{
    outType = UkCharOutput;
    if (!m_pCtrl->vietKey || m_current < 0)
    {
        backs = 0;
        outSize = 0;
        return 0;
    }

    m_backs = 0;
    m_changePos = m_current + 1;
    markChange(m_current);

    if (m_current == 0 ||
        m_buffer[m_current].form == vnw_empty ||
        m_buffer[m_current].form == vnw_nonVn ||
        m_buffer[m_current].form == vnw_c ||
        m_buffer[m_current - 1].form == vnw_c ||
        m_buffer[m_current - 1].form == vnw_cvc ||
        m_buffer[m_current - 1].form == vnw_vc)
    {

        m_current--;
        backs = m_backs;
        outSize = 0;
        synchKeyStrokeBuffer();
        return (backs > 1);
    }

    VowelSeq vs, newVs;
    int curTonePos, newTonePos, tone, vStart, vEnd;

    vEnd = m_current - m_buffer[m_current].vOffset;
    vs = m_buffer[vEnd].vseq;
    vStart = vEnd - VSeqList[vs].len + 1;
    newVs = m_buffer[m_current - 1].vseq;
    curTonePos = vStart + getTonePosition(vs, vEnd == m_current);
    newTonePos = vStart + getTonePosition(newVs, true);
    tone = m_buffer[curTonePos].tone;

    if (tone == 0 || curTonePos == newTonePos ||
        (curTonePos == m_current && m_buffer[m_current].tone != 0))
    {
        m_current--;
        backs = m_backs;
        outSize = 0;
        synchKeyStrokeBuffer();
        return (backs > 1);
    }

    markChange(newTonePos);
    m_buffer[newTonePos].tone = tone;
    markChange(curTonePos);
    m_buffer[curTonePos].tone = 0;
    m_current--;
    synchKeyStrokeBuffer();
    backs = m_backs;
    writeOutput(outBuf, outSize);
    return 1;
}

//------------------------------------------------
/**
 * @brief Reset the engine composition state to an empty starting point.
 */
void UkEngine::reset()
{
    m_current = -1;
    m_keyCurrent = -1;
    m_singleMode = false;
    m_toEscape = false;
}

//------------------------------------------------
/**
 * @brief Reset internal keystroke history without clearing current composition.
 */
void UkEngine::resetKeyBuf()
{
    m_keyCurrent = -1;
}

//------------------------------------------------
/**
 * @brief Construct a new UkEngine instance and initialize static tables.
 */
UkEngine::UkEngine()
{
    if (!m_classInit)
    {
        engineClassInit();
        m_classInit = true;
    }
    m_pCtrl = 0;
    m_bufSize = MAX_UK_ENGINE;
    m_keyBufSize = MAX_UK_ENGINE;
    m_current = -1;
    m_keyCurrent = -1;
    m_singleMode = false;
    m_keyCheckFunc = 0;
    m_reverted = false;
    m_toEscape = false;
    m_keyRestored = false;
}

//----------------------------------------------------
// make sure there are at least 10 entries available
//----------------------------------------------------
/**
 * @brief Ensure there is enough remaining space in the internal buffers.
 *
 * Trims older entries when necessary to keep at least ten slots available.
 */
void UkEngine::prepareBuffer()
{
    int rid;
    // prepare symbol buffer
    if (m_current >= 0 && m_current + 10 >= m_bufSize)
    {
        // Get rid of at least half of the current entries
        // don't get rid from the middle of a word.
        for (rid = m_current / 2; m_buffer[rid].form != vnw_empty && rid < m_current; rid++)
            ;
        if (rid == m_current)
        {
            m_current = -1;
        }
        else
        {
            rid++;
            memmove(m_buffer, m_buffer + rid, (m_current - rid + 1) * sizeof(WordInfo));
            m_current -= rid;
        }
    }

    // prepare key stroke buffer
    if (m_keyCurrent > 0 && m_keyCurrent + 1 >= m_keyBufSize)
    {
        // Get rid of at least half of the current entries
        rid = m_keyCurrent / 2;
        memmove(m_keyStrokes, m_keyStrokes + rid, (m_keyCurrent - rid + 1) * sizeof(m_keyStrokes[0]));
        m_keyCurrent -= rid;
    }
}

#define ENTER_CHAR 13
enum VnCaseType
{
    VnCaseNoChange,
    VnCaseAllCapital,
    VnCaseAllSmall
};

//----------------------------------------------------
/**
 * @brief Attempt macro expansion for the current input sequence.
 *
 * @param ev key event triggering macro lookup
 * @return 1 when a macro matched and expanded, 0 otherwise
 */
int UkEngine::macroMatch(UkKeyEvent &ev)
{
    int capsLockOn = 0;
    int shiftPressed = 0;
    if (m_keyCheckFunc)
        m_keyCheckFunc(&shiftPressed, &capsLockOn);

    if (shiftPressed && (ev.keyCode == ' ' || ev.keyCode == ENTER_CHAR))
        return 0;

    const StdVnChar *pMacText = NULL;
    StdVnChar key[MAX_MACRO_KEY_LEN + 1];
    StdVnChar *pKeyStart;

    // Use static macro text so we can gain a bit of performance
    // by avoiding memory allocation each time this function is called
    static StdVnChar macroText[MAX_MACRO_TEXT_LEN + 1];

    int i, j;

    i = m_current;
    while (i >= 0 && (m_current - i + 1) < MAX_MACRO_KEY_LEN)
    {
        while (i >= 0 && m_buffer[i].form != vnw_empty && (m_current - i + 1) < MAX_MACRO_KEY_LEN)
            i--;
        if (i >= 0 && m_buffer[i].form != vnw_empty)
            return 0;

        if (i >= 0)
        {
            if (m_buffer[i].vnSym != vnl_nonVnChar)
            {
                key[0] = m_buffer[i].vnSym + VnStdCharOffset;
                if (m_buffer[i].caps)
                    key[0]--;
                key[0] += m_buffer[i].tone * 2;
            }
            else
                key[0] = m_buffer[i].keyCode;
        }

        for (j = i + 1; j <= m_current; j++)
        {
            if (m_buffer[j].vnSym != vnl_nonVnChar)
            {
                key[j - i] = m_buffer[j].vnSym + VnStdCharOffset;
                if (m_buffer[j].caps)
                    key[j - i]--;
                key[j - i] += m_buffer[j].tone * 2;
            }
            else
                key[j - i] = m_buffer[j].keyCode;
        }
        key[m_current - i + 1] = 0;
        // search macro table
        pMacText = m_pCtrl->macStore.lookup(key + 1);
        if (pMacText)
        {
            i++; // mark the position where change is needed
            pKeyStart = key + 1;
            break;
        }
        if (i >= 0)
        {
            pMacText = m_pCtrl->macStore.lookup(key);
            if (pMacText)
            {
                pKeyStart = key;
                break;
            }
        }
        i--;
    }

    if (!pMacText)
    {
        return 0;
    }

    markChange(i);

    // determine the form of macro replacements: ALL CAPITALS, First Character Capital, or no change
    VnCaseType macroCase;
    if (IS_STD_VN_LOWER(*pKeyStart))
    {
        macroCase = VnCaseAllSmall;
    }
    else if (IS_STD_VN_UPPER(*pKeyStart))
    {
        macroCase = VnCaseAllCapital;
        for (i = 1; pKeyStart[i]; i++)
        {
            if (IS_STD_VN_LOWER(pKeyStart[i]))
            {
                macroCase = VnCaseNoChange;
            }
        }
    }
    else
        macroCase = VnCaseNoChange;

    // Convert case of macro text according to macroCase
    int charCount = 0;
    while (pMacText[charCount] != 0)
        charCount++;

    for (i = 0; i < charCount; i++)
    {
        if (macroCase == VnCaseAllCapital)
            macroText[i] = StdVnToUpper(pMacText[i]);
        else if (macroCase == VnCaseAllSmall)
            macroText[i] = StdVnToLower(pMacText[i]);
        else
            macroText[i] = pMacText[i];
    }

    // Convert to target output charset
    int outSize;
    int maxOutSize = *m_pOutSize;
    int inLen = charCount * sizeof(StdVnChar);
    VnConvert(CONV_CHARSET_VNSTANDARD, m_pCtrl->charsetId,
              (UKBYTE *)macroText, (UKBYTE *)m_pOutBuf,
              &inLen, &maxOutSize);
    outSize = maxOutSize;

    // write the last input character
    StdVnChar vnChar;
    if (outSize < *m_pOutSize)
    {
        maxOutSize = *m_pOutSize - outSize;
        if (ev.vnSym != vnl_nonVnChar)
            vnChar = ev.vnSym + VnStdCharOffset;
        else
            vnChar = ev.keyCode;
        inLen = sizeof(StdVnChar);
        VnConvert(CONV_CHARSET_VNSTANDARD, m_pCtrl->charsetId,
                  (UKBYTE *)&vnChar, ((UKBYTE *)m_pOutBuf) + outSize,
                  &inLen, &maxOutSize);
        outSize += maxOutSize;
    }
    int backs = m_backs; // store m_backs before calling reset
    reset();
    m_outputWritten = true;
    m_backs = backs;
    *m_pOutSize = outSize;
    return 1;
}

//----------------------------------------------------
/**
 * @brief Restore original keystrokes after a failed Vietnamese conversion.
 *
 * @param backs[out] number of backspaces to emit
 * @param outBuf buffer to receive restored key text
 * @param outSize[in,out] buffer capacity on entry and bytes written on exit
 * @param outType[out] output type describing produced content
 * @return 1 when restoration was performed, 0 otherwise
 */
int UkEngine::restoreKeyStrokes(int &backs, unsigned char *outBuf, int &outSize, UkOutputType &outType)
{
    outType = UkKeyOutput;
    if (!lastWordHasVnMark())
    {
        backs = 0;
        outSize = 0;
        return 0;
    }

    m_backs = 0;
    m_changePos = m_current + 1;

    int keyStart;
    bool converted = false;
    for (keyStart = m_keyCurrent; keyStart >= 0 && m_keyStrokes[keyStart].ev.chType != ukcWordBreak; keyStart--)
    {
        if (m_keyStrokes[keyStart].converted)
        {
            converted = true;
        }
    }
    keyStart++;

    if (!converted)
    {
        // no key stroke has been converted, so it doesn't make sense to restore key strokes
        backs = 0;
        outSize = 0;
        return 0;
    }

    // int i = m_current;
    while (m_current >= 0 && m_buffer[m_current].form != vnw_empty)
        m_current--;
    markChange(m_current + 1);
    backs = m_backs;

    int count;
    int i;
    UkKeyEvent ev;
    m_keyRestoring = true;
    for (i = keyStart, count = 0; i <= m_keyCurrent; i++)
    {
        if (count < outSize)
        {
            outBuf[count++] = (unsigned char)m_keyStrokes[i].ev.keyCode;
        }
        m_pCtrl->input.keyCodeToSymbol(m_keyStrokes[i].ev.keyCode, ev);
        m_keyStrokes[i].converted = false;
        processAppend(ev);
    }
    outSize = count;
    m_keyRestoring = false;

    return 1;
}

//--------------------------------------------------
/**
 * @brief Enable single-mode input where Vietnamese conversion can work inside
 *        non-VN text segments.
 */
void UkEngine::setSingleMode()
{
    m_singleMode = true;
}

//--------------------------------------------------
/**
 * @brief Initialize global UniKey engine tables and character maps.
 *
 * This must be called once prior to using the engine conversion routines.
 */
void SetupUnikeyEngine()
{
    SetupInputClassifierTable();
    int i;
    VnLexiName lexi;

    // Calculate IsoStdVnCharMap
    for (i = 0; i < 256; i++)
    {
        IsoStdVnCharMap[i] = i;
    }

    for (i = 0; SpecialWesternChars[i]; i++)
    {
        IsoStdVnCharMap[SpecialWesternChars[i]] = (vnl_lastChar + i) + VnStdCharOffset;
    }

    for (i = 0; i < 256; i++)
    {
        if ((lexi = IsoToVnLexi(i)) != vnl_nonVnChar)
        {
            IsoStdVnCharMap[i] = lexi + VnStdCharOffset;
        }
    }
}

//--------------------------------------------------
/**
 * @brief Check if the engine is currently positioned at the start of a word.
 *
 * @return true when no current word is in progress, false otherwise
 */
bool UkEngine::atWordBeginning()
{
    return (m_current < 0 || m_buffer[m_current].form == vnw_empty);
}

//--------------------------------------------------
// Check for macro first, if there's a match, expand macro. If not:
// Spell-check, if is valid Vietnamese, return normally, if not:
// restore key strokes if auto-restore is enabled
//--------------------------------------------------
/**
 * @brief Finalize the current word when a word boundary key is received.
 *
 * @param ev key event representing the word break
 * @return 1 when the boundary event produces output, 0 otherwise
 */
int UkEngine::processWordEnd(UkKeyEvent &ev)
{
    if (m_pCtrl->options.macroEnabled && macroMatch(ev))
        return 1;

    if (!m_pCtrl->options.spellCheckEnabled || m_singleMode || m_current < 0 || m_keyRestoring)
    {
        m_current++;
        WordInfo &entry = m_buffer[m_current];
        entry.form = vnw_empty;
        entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
        entry.keyCode = ev.keyCode;
        entry.vnSym = vnToLower(ev.vnSym);
        entry.caps = (entry.vnSym != ev.vnSym);
        return 0;
    }

    int outSize = 0;
    if (m_pCtrl->options.autoNonVnRestore && lastWordIsNonVn())
    {
        outSize = *m_pOutSize;
        if (restoreKeyStrokes(m_backs, m_pOutBuf, outSize, m_outType))
        {
            m_keyRestored = true;
            m_outputWritten = true;
        }
    }

    m_current++;
    WordInfo &entry = m_buffer[m_current];
    entry.form = vnw_empty;
    entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
    entry.keyCode = ev.keyCode;
    entry.vnSym = vnToLower(ev.vnSym);
    entry.caps = (entry.vnSym != ev.vnSym);

    if (m_keyRestored && outSize < *m_pOutSize)
    {
        m_pOutBuf[outSize] = ev.keyCode;
        outSize++;
        *m_pOutSize = outSize;
        return 1;
    }

    return 0;
}

//---------------------------------------------------------------------------
// Test if last word is a non-Vietnamese word, so that
// the engine can restore key strokes if it is indeed not a Vietnamese word
//---------------------------------------------------------------------------
/**
 * @brief Determine whether the last composed word is non-Vietnamese.
 *
 * @return true if the last word should be treated as non-VN text
 */
bool UkEngine::lastWordIsNonVn()
{
    if (m_current < 0)
        return false;

    switch (m_buffer[m_current].form)
    {
    case vnw_nonVn:
        return true;
    case vnw_empty:
    case vnw_c:
        return false;
    case vnw_v:
    case vnw_cv:
        return !VSeqList[m_buffer[m_current].vseq].complete;
    case vnw_vc:
    case vnw_cvc:
    {
        int vIndex = m_current - m_buffer[m_current].vOffset;
        VowelSeq vs = m_buffer[vIndex].vseq;
        if (!VSeqList[vs].complete)
            return true;
        ConSeq cs = m_buffer[m_current].cseq;
        ConSeq c1 = cs_nil;
        if (m_buffer[m_current].c1Offset != -1)
            c1 = m_buffer[m_current - m_buffer[m_current].c1Offset].cseq;

        if (!isValidCVC(c1, vs, cs))
        {
            return true;
        }

        int tonePos = (vIndex - VSeqList[vs].len + 1) + getTonePosition(vs, false);
        int tone = m_buffer[tonePos].tone;
        if ((cs == cs_c || cs == cs_ch || cs == cs_p || cs == cs_t) &&
            (tone == 2 || tone == 3 || tone == 4))
        {
            return true;
        }
    }
    }
    return false;
}

//---------------------------------------------------------------------------
// Test if last word has a Vietnamese mark, that is tones, decorators
//---------------------------------------------------------------------------
/**
 * @brief Check whether the last composed word contains Vietnamese markings.
 *
 * @return true if the word contains tones or Vietnamese character modifiers
 */
bool UkEngine::lastWordHasVnMark()
{
    for (int i = m_current; i >= 0 && m_buffer[i].form != vnw_empty; i--)
    {
        VnLexiName sym = m_buffer[i].vnSym;
        if (sym != vnl_nonVnChar)
        {
            if (IsVnVowel[sym])
            {
                if (m_buffer[i].tone)
                    return true;
            }
            if (sym != StdVnRootChar[sym])
                return true;
        }
    }
    return false;
}
