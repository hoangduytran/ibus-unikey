// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/**
 * @file engine_tables_shared.h
 * @brief Syllable table types and cross-TU symbols for UniKey engine data in `table_components/`.
 */
#ifndef UKENGINE_ENGINE_TABLES_SHARED_H
#define UKENGINE_ENGINE_TABLES_SHARED_H

#include "ukengine.h"

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

extern bool IsVnVowel[vnl_lastChar];
extern unsigned char SpecialWesternChars[];
extern StdVnChar IsoStdVnCharMap[256];

inline StdVnChar IsoToStdVnChar(int keyCode)
{
    return (keyCode < 256) ? IsoStdVnCharMap[keyCode] : keyCode;
}

extern VowelSeqInfo VSeqList[];
extern const int VSeqCount;
extern ConSeqInfo CSeqList[];
extern const int CSeqCount;
extern VSeqPair SortedVSeqList[];
extern CSeqPair SortedCSeqList[];
extern VCPair VCPairList[];
extern const int VCPairCount;

typedef int (UkEngine::*UkKeyProc)(UkKeyEvent &ev);
extern UkKeyProc UkKeyProcList[];

VowelSeq lookupVSeq(VnLexiName v1, VnLexiName v2 = vnl_nonVnChar, VnLexiName v3 = vnl_nonVnChar);
ConSeq lookupCSeq(VnLexiName c1, VnLexiName c2 = vnl_nonVnChar, VnLexiName c3 = vnl_nonVnChar);
bool isValidCV(ConSeq c, VowelSeq v);
bool isValidVC(VowelSeq v, ConSeq c);
bool isValidCVC(ConSeq c1, VowelSeq v, ConSeq c2);
void engineClassInit();

#endif
