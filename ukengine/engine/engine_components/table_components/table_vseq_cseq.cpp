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
