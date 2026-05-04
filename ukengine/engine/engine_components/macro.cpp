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

/**
 * @file macro.cpp
 * @brief Macro expansion: build fold-lookup keys from `m_buffer`, query `CMacroTable`,
 *        mirror trigger casing onto replacement text, convert via `VnConvert`.
 *
 * **Progression — `UkEngine::macroMatch`:**
 * 1. `macroMatchPreSkip` — ignore shift+space / shift+enter edge case.
 * 2. `macroMatchScanForHit` — walk candidate spans, rewind to word boundary, lookup.
 * 3. `markChange` — record where the buffer must be rewritten (caller in `macroMatch`).
 * 4. `macroMatchBuildExpansionStdVn` — case-transform macro text in `VNSTANDARD`.
 * 5. `macroMatchFlushOutput` — encode expansion + trailing key into `m_pOutBuf`, `reset`.
 */

#if defined(_WIN32)
#include "keyhook.h"
#endif

#include "engine_components/engine_internal.h"
#include "ukengine.h"
#include "vnconv.h"

namespace {

enum class MacroExpansionCase
{
    Verbatim,
    AllCaps,
    AllLower,
    TitleWords,
};

/**
 * @brief Count `StdVnChar` units before the first NUL in `run`.
 * @param run pointer to NUL-terminated sequence (may be empty).
 * @return number of units not including the terminator.
 *
 * Progression:
 * 1. Advance `unitCount` while `run[unitCount] != 0`.
 * 2. Return the count.
 */
int stdVnCharCountBeforeNul(const StdVnChar *run)
{
    int unitCount = 0;
    while (run[unitCount] != 0)
        unitCount++;
    return unitCount;
}

/**
 * @brief True when `unit` is an upper/lower Vietnamese letter in `VNSTANDARD` range.
 * @param unit standard Vietnamese code unit.
 * @return non-zero if `IS_STD_VN_LOWER` or `IS_STD_VN_UPPER`.
 *
 * Progression:
 * 1. Evaluate lower/upper predicates used for title-casing word initials.
 */
bool isStdVnLetterForCasing(StdVnChar unit)
{
    const bool lower = IS_STD_VN_LOWER(unit);
    const bool upper = IS_STD_VN_UPPER(unit);
    return lower || upper;
}

/**
 * @brief Detect mixed-case trigger pattern after the first character (e.g. `Hn…`).
 * @param nulTerminatedTrigger NUL-terminated VNSTANDARD key (must be non-empty).
 * @return true if any position `>= 1` holds a lowercase Vietnamese letter.
 *
 * Progression:
 * 1. Walk indices after the first unit.
 * 2. Return on first `IS_STD_VN_LOWER` hit; else false.
 */
bool triggerHasLowercaseAfterFirst(const StdVnChar *nulTerminatedTrigger)
{
    for (int idx = 1; nulTerminatedTrigger[idx]; idx++)
    {
        if (IS_STD_VN_LOWER(nulTerminatedTrigger[idx]))
            return true;
    }
    return false;
}

/**
 * @brief Map the typed trigger’s first segment to a macro replacement casing rule.
 * @param triggerKeyStart NUL-terminated trigger (VNSTANDARD) as typed.
 * @return `AllLower`, `AllCaps`, `TitleWords`, or `Verbatim`.
 *
 * Progression:
 * 1. All-low first unit ⇒ `AllLower`.
 * 2. Non-upper first unit ⇒ `Verbatim`.
 * 3. Upper first with later lowercase ⇒ `TitleWords`.
 * 4. Else uniform upper ⇒ `AllCaps`.
 */
MacroExpansionCase classifyMacroKeyCase(const StdVnChar *triggerKeyStart)
{
    if (IS_STD_VN_LOWER(*triggerKeyStart))
        return MacroExpansionCase::AllLower;
    if (!IS_STD_VN_UPPER(*triggerKeyStart))
        return MacroExpansionCase::Verbatim;
    if (triggerHasLowercaseAfterFirst(triggerKeyStart))
        return MacroExpansionCase::TitleWords;
    return MacroExpansionCase::AllCaps;
}

/**
 * @brief Overwrite `dst` with lowercased copies of the first `unitCount` units of `src`.
 * @param src source units (length `unitCount`, not NUL-terminated).
 * @param unitCount number of units.
 * @param dst destination buffer with room for at least `unitCount` units.
 *
 * Progression:
 * 1. For each index, assign `StdVnToLower(src[i])` to `dst[i]`.
 */
void copyUnitsLowercased(const StdVnChar *src, int unitCount, StdVnChar *dst)
{
    for (int i = 0; i < unitCount; i++)
        dst[i] = StdVnToLower(src[i]);
}

/**
 * @brief Title-case already-lowered text: capitalize first VN letter after start / ASCII space.
 * @param dst read/write buffer of length `unitCount` (no NUL yet).
 * @param unitCount number of units in `dst`.
 *
 * Progression:
 * 1. Track `atWordStart`; ASCII space sets next letter as word-initial.
 * 2. At word start, if unit is a VN letter, `StdVnToUpper`.
 */
void capitalizeWordInitialsAfterSpace(StdVnChar *dst, int unitCount)
{
    bool atWordStart = true;
    for (int i = 0; i < unitCount; i++)
    {
        const bool isAsciiSpace = dst[i] == (StdVnChar)' ';
        if (isAsciiSpace)
        {
            atWordStart = true;
            continue;
        }
        const bool shouldCapitalize = atWordStart && isStdVnLetterForCasing(dst[i]);
        if (shouldCapitalize)
        {
            dst[i] = StdVnToUpper(dst[i]);
            atWordStart = false;
        }
        else
            atWordStart = false;
    }
}

/**
 * @brief Apply casing rule to macro expansion text and NUL-terminate.
 * @param kase casing mode derived from the trigger.
 * @param src macro table text (VNSTANDARD), `unitCount` units (no embedded NUL).
 * @param unitCount length of `src` in units.
 * @param dst output buffer with room for `unitCount + 1` units (receives trailing NUL).
 *
 * Progression:
 * 1. If `TitleWords`, lower all then capitalize word initials after ASCII space.
 * 2. Else per-unit upper, lower, or copy for `AllCaps` / `AllLower` / `Verbatim`.
 * 3. Write NUL at `dst[unitCount]`.
 */
void applyMacroCaseToBuffer(MacroExpansionCase kase, const StdVnChar *src, int unitCount,
                            StdVnChar *dst)
{
    if (kase == MacroExpansionCase::TitleWords)
    {
        copyUnitsLowercased(src, unitCount, dst);
        capitalizeWordInitialsAfterSpace(dst, unitCount);
    }
    else
    {
        for (int i = 0; i < unitCount; i++)
        {
            if (kase == MacroExpansionCase::AllCaps)
                dst[i] = StdVnToUpper(src[i]);
            else if (kase == MacroExpansionCase::AllLower)
                dst[i] = StdVnToLower(src[i]);
            else
                dst[i] = src[i];
        }
    }
    dst[unitCount] = 0;
}

} // namespace

/**
 * @brief Encode one `WordInfo` slot as a VNSTANDARD code unit for macro key assembly.
 * @param w composition buffer entry (vowel/consonant/non-VN path).
 * @return lookup key unit (tone and caps applied when `vnSym` is set; else raw `keyCode`).
 *
 * Progression:
 * 1. If `vnSym` set, map lexical id to `VnStdCharOffset` grid, adjust caps/tone.
 * 2. Else return ASCII/other `keyCode` as `StdVnChar`.
 */
StdVnChar UkEngine::macroMatchWordInfoToStdKey(const WordInfo &w)
{
    if (w.vnSym != vnl_nonVnChar)
    {
        StdVnChar unit = w.vnSym + VnStdCharOffset;
        if (w.caps)
            unit--;
        return unit + w.tone * 2;
    }
    return (StdVnChar)w.keyCode;
}

/**
 * @brief True if a macro key covering `firstBufferIndex`…`m_current` fits engine limits.
 * @param firstBufferIndex start index in `m_buffer` (-1 means empty prefix).
 * @return false if index invalid or span length exceeds `MAX_UK_ENGINE`.
 *
 * Progression:
 * 1. Reject negative `firstBufferIndex`.
 * 2. Compare `(m_current - firstBufferIndex + 1)` to `MAX_UK_ENGINE`.
 */
bool UkEngine::macroMatchMacroKeyFitsEngine(int firstBufferIndex) const
{
    if (firstBufferIndex < 0)
        return false;
    const int macroKeySlotCount = m_current - firstBufferIndex + 1;
    return macroKeySlotCount <= MAX_UK_ENGINE;
}

/**
 * @brief Walk backward across non–word-break slots until boundary or length limit.
 * @param[in,out] wordSpanStartIndex starting scan index; updated to boundary index on success.
 * @return false if the scan stops inside non-`vnw_empty` without a valid break (impossible key).
 *
 * Progression:
 * 1. While index valid, not at `vnw_empty`, and span still ≤ `MAX_UK_ENGINE`, decrement index.
 * 2. If still on non-empty form, report failure; else success.
 */
bool UkEngine::macroMatchRewindToWordBoundary(int &wordSpanStartIndex) const
{
    while (wordSpanStartIndex >= 0)
    {
        const VnWordForm form = m_buffer[wordSpanStartIndex].form;
        const bool slotIsWordBreak = form == vnw_empty;
        const int macroKeySlotCount = m_current - wordSpanStartIndex + 1;
        const bool spanStillFitsEngine = macroKeySlotCount <= MAX_UK_ENGINE;

        const bool shouldKeepRewinding =
            !slotIsWordBreak && spanStillFitsEngine;
        if (!shouldKeepRewinding)
            break;
        wordSpanStartIndex--;
    }

    const bool stuckInsideNonBreak =
        wordSpanStartIndex >= 0 && m_buffer[wordSpanStartIndex].form != vnw_empty;
    return !stuckInsideNonBreak;
}

/**
 * @brief Pack `m_buffer[wordSpanStartIndex…m_current]` into a NUL-terminated VN key for lookup.
 * @param wordSpanStartIndex first `m_buffer` slot encoded at `key[0]`, or -1 when the
 *        composed key occupies `key[1…]` only (suffix lookup uses `lookup(key+1)`).
 * @param nulTerminatedKeyOut scratch with room for span+1 units plus NUL (see `m_macroKeyScratch`).
 *
 * Progression:
 * 1. If `wordSpanStartIndex >= 0`, fill `key[0]` from that slot.
 * 2. Copy remaining slots through `m_current` into subsequent key indices.
 * 3. Write terminating NUL.
 */
void UkEngine::macroMatchBuildFoldLookupKey(int wordSpanStartIndex,
                                            StdVnChar *nulTerminatedKeyOut)
{
    if (wordSpanStartIndex >= 0)
        nulTerminatedKeyOut[0] = macroMatchWordInfoToStdKey(m_buffer[wordSpanStartIndex]);

    for (int bufIdx = wordSpanStartIndex + 1; bufIdx <= m_current; bufIdx++)
    {
        const int keySlot = bufIdx - wordSpanStartIndex;
        nulTerminatedKeyOut[keySlot] = macroMatchWordInfoToStdKey(m_buffer[bufIdx]);
    }
    nulTerminatedKeyOut[m_current - wordSpanStartIndex + 1] = 0;
}

/**
 * @brief Try macro table lookup for key suffix-only then full key (word-boundary semantics).
 * @param wordSpanStartIndex buffer index of first key slot (-1 if suffix-only attempt).
 * @param nulTerminatedKey scratch key built by `macroMatchBuildFoldLookupKey`.
 * @param[out] outMacText table expansion pointer on hit.
 * @param[out] markIndex `markChange` index matching original UniKey behavior.
 * @param[out] pKeyStart pointer into scratch for casing rules (suffix vs full key).
 * @return true when a row matches.
 *
 * Progression:
 * 1. `lookup(key + 1)` for trigger after boundary; set marks for suffix path.
 * 2. Else if `wordSpanStartIndex >= 0`, `lookup(key)` for full span.
 */
bool UkEngine::macroMatchLookupAtSpan(int wordSpanStartIndex, StdVnChar *nulTerminatedKey,
                                      const StdVnChar *&outMacText, int &markIndex,
                                      StdVnChar *&pKeyStart)
{
    CMacroTable &macroTable = m_pCtrl->macStore;

    outMacText = macroTable.lookup(nulTerminatedKey + 1);
    if (outMacText)
    {
        markIndex = wordSpanStartIndex + 1;
        pKeyStart = nulTerminatedKey + 1;
        return true;
    }

    const bool spanIncludesPrefixSlot = wordSpanStartIndex >= 0;
    if (spanIncludesPrefixSlot)
    {
        outMacText = macroTable.lookup(nulTerminatedKey);
        if (outMacText)
        {
            markIndex = wordSpanStartIndex;
            pKeyStart = nulTerminatedKey;
            return true;
        }
    }
    return false;
}

/**
 * @brief Decide whether macro matching should be skipped for this event (shift + space/CR).
 * @param ev current key event.
 * @return true to skip `macroMatch` for this press.
 *
 * Progression:
 * 1. Query `m_keyCheckFunc` for shift (caps lock value ignored here).
 * 2. Return true when shift is down and key is space or enter (`ENTER_CHAR`).
 */
bool UkEngine::macroMatchPreSkip(const UkKeyEvent &ev) const
{
    int shiftPressed = 0;
    int capsLockOn = 0;
    if (m_keyCheckFunc)
        m_keyCheckFunc(&shiftPressed, &capsLockOn);
    (void)capsLockOn;

    const bool shiftIsDown = shiftPressed != 0;
    const bool keyIsSpaceOrEnter =
        ev.keyCode == ' ' || ev.keyCode == ENTER_CHAR;
    return shiftIsDown && keyIsSpaceOrEnter;
}

/**
 * @brief Scan backward from `m_current` for a macro hit matching composed text.
 * @param[out] pMacText expansion text from `CMacroTable` on success.
 * @param[out] markIndex index passed to `markChange`.
 * @param[out] pKeyStart start of typed trigger inside scratch (for casing rules).
 * @return false if no row matches any candidate span.
 *
 * Progression:
 * 1. Size `m_macroKeyScratch`; loop `nextCandidateStartIndex` from `m_current` downward.
 * 2. While span fits engine, `macroMatchRewindToWordBoundary`, build key, `macroMatchLookupAtSpan`.
 * 3. On miss, advance `nextCandidateStartIndex = wordSpanStartIndex - 1` (UniKey outer `i--`).
 */
bool UkEngine::macroMatchScanForHit(const StdVnChar *&pMacText, int &markIndex,
                                    StdVnChar *&pKeyStart)
{
    m_macroKeyScratch.resize((size_t)MAX_UK_ENGINE + 2u);
    StdVnChar *const foldLookupKey = m_macroKeyScratch.data();

    int nextCandidateStartIndex = m_current;
    while (nextCandidateStartIndex >= 0 &&
           macroMatchMacroKeyFitsEngine(nextCandidateStartIndex))
    {
        int wordSpanStartIndex = nextCandidateStartIndex;
        if (!macroMatchRewindToWordBoundary(wordSpanStartIndex))
            return false;

        macroMatchBuildFoldLookupKey(wordSpanStartIndex, foldLookupKey);
        if (macroMatchLookupAtSpan(wordSpanStartIndex, foldLookupKey, pMacText, markIndex,
                                   pKeyStart))
            return true;

        nextCandidateStartIndex = wordSpanStartIndex - 1;
    }
    return false;
}

/**
 * @brief Prepare cased macro replacement in `m_macroTextScratch` (VNSTANDARD, NUL-terminated).
 * @param pKeyStart trigger substring used for casing (`pKeyStart` from scan).
 * @param pMacText table expansion text (VNSTANDARD, NUL-terminated).
 * @return number of `StdVnChar` units in expansion excluding the final NUL.
 *
 * Progression:
 * 1. Measure `pMacText` length.
 * 2. Resize scratch; `classifyMacroKeyCase` then `applyMacroCaseToBuffer`.
 */
int UkEngine::macroMatchBuildExpansionStdVn(const StdVnChar *pKeyStart,
                                            const StdVnChar *pMacText)
{
    const int expansionUnitCount = stdVnCharCountBeforeNul(pMacText);

    m_macroTextScratch.resize((size_t)expansionUnitCount + 1u);
    StdVnChar *const macroText = m_macroTextScratch.data();
    const MacroExpansionCase expansionCase = classifyMacroKeyCase(pKeyStart);
    applyMacroCaseToBuffer(expansionCase, pMacText, expansionUnitCount, macroText);
    return expansionUnitCount;
}

/**
 * @brief Encode expansion to output charset, append triggering key unit, reset engine state.
 * @param ev key event whose symbol is appended after expansion bytes.
 * @param expansionUnitCount units in `m_macroTextScratch` before its NUL (from build step).
 *
 * Progression:
 * 1. `VnConvert` scratch expansion into `m_pOutBuf` within `*m_pOutSize`.
 * 2. If room remains, append converted `ev` VN or raw key code.
 * 3. Preserve `m_backs` across `reset`; set `m_outputWritten` and `*m_pOutSize`.
 */
void UkEngine::macroMatchFlushOutput(const UkKeyEvent &ev, int expansionUnitCount)
{
    int maxOutBytes = *m_pOutSize;
    int inLenBytes = expansionUnitCount * (int)sizeof(StdVnChar);
    VnConvert(CONV_CHARSET_VNSTANDARD, m_pCtrl->charsetId,
              (UKBYTE *)m_macroTextScratch.data(), (UKBYTE *)m_pOutBuf, &inLenBytes,
              &maxOutBytes);
    int totalOutBytes = maxOutBytes;

    const bool roomLeftInOutBuffer = totalOutBytes < *m_pOutSize;
    if (roomLeftInOutBuffer)
    {
        maxOutBytes = *m_pOutSize - totalOutBytes;
        const bool eventHasVnSymbol = ev.vnSym != vnl_nonVnChar;
        const StdVnChar tailKeyStdVn =
            eventHasVnSymbol ? ev.vnSym + VnStdCharOffset : (StdVnChar)ev.keyCode;
        inLenBytes = (int)sizeof(StdVnChar);
        VnConvert(CONV_CHARSET_VNSTANDARD, m_pCtrl->charsetId, (UKBYTE *)&tailKeyStdVn,
                  ((UKBYTE *)m_pOutBuf) + totalOutBytes, &inLenBytes, &maxOutBytes);
        totalOutBytes += maxOutBytes;
    }

    const int pendingBackspacesBeforeReset = m_backs;
    reset();
    m_outputWritten = true;
    m_backs = pendingBackspacesBeforeReset;
    *m_pOutSize = totalOutBytes;
}

/**
 * @brief Attempt macro expansion for the current input sequence.
 * @param ev key event triggering macro lookup.
 * @return 1 when a macro matched and output was written, 0 otherwise.
 *
 * Progression:
 * 1. `macroMatchPreSkip` short-circuit.
 * 2. `macroMatchScanForHit`; on failure return 0.
 * 3. `markChange`, `macroMatchBuildExpansionStdVn`, `macroMatchFlushOutput`; return 1.
 */
int UkEngine::macroMatch(UkKeyEvent &ev)
{
    if (macroMatchPreSkip(ev))
        return 0;

    const StdVnChar *expansionStdVn = nullptr;
    int markChangeBufferIndex = 0;
    StdVnChar *triggerKeyRunStart = nullptr;
    if (!macroMatchScanForHit(expansionStdVn, markChangeBufferIndex, triggerKeyRunStart))
        return 0;

    markChange(markChangeBufferIndex);
    const int expansionUnitCount =
        macroMatchBuildExpansionStdVn(triggerKeyRunStart, expansionStdVn);
    macroMatchFlushOutput(ev, expansionUnitCount);
    return 1;
}
