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
 * @file diacritic_processing.cpp
 * @brief Vietnamese diacritic composition for `UkEngine`: roof (ˆ), hook/bowl, tone marks,
 *        Telex `dd`→đ and `w`, and map-char repeat-undo.
 *
 * Mutates `m_buffer` / `WordInfo` using syllable tables (`VSeqList`, `VowelSeqInfo`) from
 * `engine_tables_shared.h`. Public entry points are declared on `UkEngine` in `ukengine.h`.
 *
 * **Progression — typical mark application (roof or hook):**
 * 1. Validate Vietnamese mode and vowel span at `m_current`.
 * 2. Either strip an existing mark (undo) or place the table-driven successor sequence.
 * 3. `vowelApplySubsequences` — refresh per-letter `vseq` fields after the nucleus changes.
 * 4. `relocateToneIfMoved` — if the tone-bearing slot index changed, move the stored tone.
 * 5. On strip-only paths, `finishRemovedDiacriticAppendRevert` may re-enter `processAppend`.
 *
 * @note Most helpers assume callers have already checked `vietKey`, cursor bounds, and valid
 *       `vOffset` where required. Indexing like `m_current - 2` follows original UniKey
 *       syllable layout assumptions for `th`-onset shortcuts.
 */
#include <ctype.h>
#if defined(_WIN32)
#include "keyhook.h"
#endif

#include "engine_components/engine_internal.h"
#include "engine_components/engine_tables_shared.h"

namespace {

/**
 * @brief Map a roof key event to the lexeme used for roof add/remove matching.
 *
 * @param evType `UkKeyEvent::evType` (`vneRoof_a` / `vneRoof_e` / `vneRoof_o`, etc.).
 * @return Target roof letter (`vnl_ar`, `vnl_er`, `vnl_or`) or `vnl_nonVnChar` if unspecified.
 */
VnLexiName roofTargetFromEvType(int evType)
{
    switch (evType)
    {
    case vneRoof_a:
        return vnl_ar;
    case vneRoof_e:
        return vnl_er;
    case vneRoof_o:
        return vnl_or;
    default:
        return vnl_nonVnChar;
    }
}

} // namespace

/**
 * @brief After a vowel transform, copy sub-sequence enums from `info.sub[]` into `m_buffer`.
 *
 * Keeps each letter’s `WordInfo::vseq` consistent with `VSeqList` for the new nucleus shape.
 *
 * @param vowelSpanStart First buffer index of the vowel run.
 * @param info Row from `VSeqList` for the vowel sequence now stored at `vowelSpanStart`.
 */
void UkEngine::vowelApplySubsequences(int vowelSpanStart, const VowelSeqInfo &info)
{
    for (int letterIdx = 0; letterIdx < info.len; letterIdx++)
        m_buffer[vowelSpanStart + letterIdx].vseq = info.sub[letterIdx];
}

/**
 * @brief If the tone mark sits on a different letter after a nucleus edit, move the tone index.
 *
 * Does nothing when the slot is unchanged or the nucleus had no tone (`tone == 0`).
 *
 * @param curTonePos Buffer index that held the tone before the transform.
 * @param newTonePos Buffer index that should hold the tone after the transform.
 * @param tone Tone value to preserve (0 means none).
 */
void UkEngine::relocateToneIfMoved(int curTonePos, int newTonePos, int tone)
{
    const bool toneSlotChanged = (curTonePos != newTonePos);
    const bool hadTone = (tone != 0);
    if (toneSlotChanged && hadTone)
    {
        markChange(newTonePos);
        m_buffer[newTonePos].tone = tone;
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    }
}

/**
 * @brief Common tail when removing a diacritic: leave single mode, consume the key via append,
 *        mark `m_reverted` so upstream can treat the stroke as an undo path.
 *
 * @param ev Original key event passed through to `processAppend`.
 */
void UkEngine::finishRemovedDiacriticAppendRevert(UkKeyEvent &ev)
{
    m_singleMode = false;
    processAppend(ev);
    m_reverted = true;
}

/**
 * @brief Whether the current syllable frame allows `vowelSeq` under CVC table rules.
 *
 * Reads consonant classes at `m_current` using `c1Offset` / `c2Offset` and calls `isValidCVC`.
 *
 * @param vowelSeq Candidate vowel sequence after a transform.
 * @return True when `isValidCVC` accepts the triple (onset, vowelSeq, coda).
 */
bool UkEngine::syllableValidForVowelSeq(VowelSeq vowelSeq) const
{
    ConSeq leadingConsonant = cs_nil;
    ConSeq codaConsonant = cs_nil;
    const bool hasLeadingConsonant = (m_buffer[m_current].c1Offset != -1);
    if (hasLeadingConsonant)
        leadingConsonant = m_buffer[m_current - m_buffer[m_current].c1Offset].cseq;

    const bool hasCodaConsonant = (m_buffer[m_current].c2Offset != -1);
    if (hasCodaConsonant)
        codaConsonant = m_buffer[m_current - m_buffer[m_current].c2Offset].cseq;

    return isValidCVC(leadingConsonant, vowelSeq, codaConsonant);
}

/**
 * @brief For hook removal, check whether `currentHookLetter` matches the active key event kind.
 *
 * Used before stripping ư / ơ / ă so the wrong hook key does not mutate the buffer.
 *
 * @param evType Hook-related `UkKeyEvent::evType`.
 * @param currentHookLetter Letter at `hookPos` in the buffer (hooked form).
 * @return True when stripping is allowed for this event and letter.
 */
bool UkEngine::hookUndoKeyMatches(int evType, VnLexiName currentHookLetter) const
{
    switch (evType)
    {
    case vneHook_u:
        return currentHookLetter == vnl_uh;
    case vneHook_o:
        return currentHookLetter == vnl_oh;
    case vneBowl:
        return currentHookLetter == vnl_ab;
    default:
    {
        const bool rejectHookUoOnBowl = (evType == vneHook_uo && currentHookLetter == vnl_ab);
        if (rejectHookUoOnBowl)
            return false;
        return true;
    }
    }
}

/**
 * @brief For hook application, check whether the table’s hook letter matches the key event.
 *
 * @param evType Hook-related `UkKeyEvent::evType`.
 * @param vowelSeqInfo Target row (`withHook`) the engine is about to apply.
 * @return True when the event targets the hook letter at `hookPos`.
 */
bool UkEngine::hookApplyKeyMatches(int evType, const VowelSeqInfo &vowelSeqInfo) const
{
    switch (evType)
    {
    case vneHook_u:
        return vowelSeqInfo.v[vowelSeqInfo.hookPos] == vnl_uh;
    case vneHook_o:
        return vowelSeqInfo.v[vowelSeqInfo.hookPos] == vnl_oh;
    case vneBowl:
        return vowelSeqInfo.v[vowelSeqInfo.hookPos] == vnl_ab;
    default:
    {
        const VnLexiName hookLetter = vowelSeqInfo.v[vowelSeqInfo.hookPos];
        const bool rejectHookUoOnBowl = (evType == vneHook_uo && hookLetter == vnl_ab);
        if (rejectHookUoOnBowl)
            return false;
        return true;
    }
    }
}

/**
 * @brief Fill outputs with the vowel span enclosing `m_current` and the current tone slot.
 *
 * Computes vowel end from `m_buffer[m_current].vOffset`, start from `VSeqList[].len`,
 * and tone position via `getTonePosition`.
 *
 * @param[out] outVEnd Index of the vowel-end `WordInfo` in `m_buffer`.
 * @param[out] outVStart First index of the vowel run.
 * @param[out] outVs `VowelSeq` at `outVEnd`.
 * @param[out] outCurTonePos Buffer index carrying the tone for this nucleus.
 * @param[out] outTone Numeric tone value at `outCurTonePos`.
 */
void UkEngine::readVowelSpanFromCursor(int &outVEnd, int &outVStart, VowelSeq &outVs,
                                         int &outCurTonePos, int &outTone)
{
    outVEnd = m_current - m_buffer[m_current].vOffset;
    outVs = m_buffer[outVEnd].vseq;
    outVStart = outVEnd - (VSeqList[outVs].len - 1);
    const bool vowelSpanEndAtCursor = (outVEnd == m_current);
    outCurTonePos = outVStart + getTonePosition(outVs, vowelSpanEndAtCursor);
    outTone = m_buffer[outCurTonePos].tone;
}

/**
 * @brief Remove a circumflex (roof) from the vowel span when the event targets that mark.
 *
 * Respects `freeMarking`: if marking is locked to the cursor, returns false without changing
 * the buffer. When `targetRoofLexi` is specific, the stripped letter must match it.
 *
 * @param vowelSeq Current `VowelSeq` for letters at `vowelSpanStart`.
 * @param targetRoofLexi Expected roof glyph (`vnl_ar` / `vnl_er` / `vnl_or`), or `vnl_nonVnChar`
 *        to accept any roof at `roofPos`.
 * @param vowelSpanStart First buffer index of the vowel run.
 * @param[out] nextVowelSeq Vowel sequence after `lookupVSeq` on the edited letters.
 * @param[out] vowelSeqInfoOut Points at `VSeqList[nextVowelSeq]` on success.
 * @param[out] roofRemoved Set true when a roof letter was cleared.
 * @param[out] changePos Buffer index that was modified.
 * @return False when there is no roof, target mismatch, or marking is cursor-locked.
 */
bool UkEngine::roofStripRoofMark(VowelSeq vowelSeq, VnLexiName targetRoofLexi, int vowelSpanStart,
                                 VowelSeq &nextVowelSeq, VowelSeqInfo *&vowelSeqInfoOut, bool &roofRemoved,
                                 int &changePos)
{
    const VowelSeqInfo &seqInfo = VSeqList[vowelSeq];
    if (seqInfo.roofPos == -1)
        return false;

    VnLexiName currentRoofLetter = m_buffer[vowelSpanStart + seqInfo.roofPos].vnSym;
    const bool targetIsSpecificRoof = (targetRoofLexi != vnl_nonVnChar);
    const bool targetMatchesBuffer = (currentRoofLetter == targetRoofLexi);
    if (targetIsSpecificRoof && !targetMatchesBuffer)
        return false;

    VnLexiName plainVowelLetter =
        (currentRoofLetter == vnl_ar) ? vnl_a : ((currentRoofLetter == vnl_er) ? vnl_e : vnl_o);
    changePos = vowelSpanStart + seqInfo.roofPos;

    const bool markingLockedToCursor = !m_pCtrl->options.freeMarking && changePos != m_current;
    if (markingLockedToCursor)
        return false;

    markChange(changePos);
    m_buffer[changePos].vnSym = plainVowelLetter;

    if (seqInfo.len == 3)
        nextVowelSeq = lookupVSeq(m_buffer[vowelSpanStart].vnSym, m_buffer[vowelSpanStart + 1].vnSym,
                                  m_buffer[vowelSpanStart + 2].vnSym);
    else if (seqInfo.len == 2)
        nextVowelSeq =
            lookupVSeq(m_buffer[vowelSpanStart].vnSym, m_buffer[vowelSpanStart + 1].vnSym);
    else
        nextVowelSeq = lookupVSeq(m_buffer[vowelSpanStart].vnSym);

    vowelSeqInfoOut = &VSeqList[nextVowelSeq];
    roofRemoved = true;
    return true;
}

/**
 * @brief Apply `withRoof` (or u+ơ pair for uho-family) at the table’s roof position.
 *
 * Validates syllable shape with `syllableValidForVowelSeq` and `freeMarking` cursor rules.
 * `doubleChangeUO` rewrites both `u` and the following vowel for `uho*` → `u+ơ+…` style paths.
 *
 * @param targetRoofLexi Roof letter the key targets, or `vnl_nonVnChar` for generic roof keys.
 * @param vowelSpanStart First buffer index of the vowel run.
 * @param nextVowelSeq Row chosen by caller (typically `VSeqList[v].withRoof` or `lookupVSeq` result).
 * @param doubleChangeUO When true, set `u`/`ơ` pair at span start instead of a single slot.
 * @param[out] vowelSeqInfoOut `VSeqList` row for `nextVowelSeq`.
 * @param[out] changePos Index written by `markChange` (first vowel slot when `doubleChangeUO`).
 * @return False on target mismatch, invalid CVC, or cursor-locked marking.
 */
bool UkEngine::roofPlaceRoofMark(VnLexiName targetRoofLexi, int vowelSpanStart, VowelSeq nextVowelSeq,
                                 bool doubleChangeUO, VowelSeqInfo *&vowelSeqInfoOut, int &changePos)
{
    vowelSeqInfoOut = &VSeqList[nextVowelSeq];
    const VnLexiName expectedRoofAtPos = vowelSeqInfoOut->v[vowelSeqInfoOut->roofPos];
    const bool targetIsSpecificRoof = (targetRoofLexi != vnl_nonVnChar);
    const bool targetMatchesTable = (expectedRoofAtPos == targetRoofLexi);
    if (targetIsSpecificRoof && !targetMatchesTable)
        return false;

    if (!syllableValidForVowelSeq(nextVowelSeq))
        return false;

    if (doubleChangeUO)
        changePos = vowelSpanStart;
    else
        changePos = vowelSpanStart + vowelSeqInfoOut->roofPos;

    const bool markingLockedToCursor = !m_pCtrl->options.freeMarking && changePos != m_current;
    if (markingLockedToCursor)
        return false;

    markChange(changePos);
    if (doubleChangeUO)
    {
        m_buffer[vowelSpanStart].vnSym = vnl_u;
        m_buffer[vowelSpanStart + 1].vnSym = vnl_or;
    }
    else
    {
        m_buffer[changePos].vnSym = vowelSeqInfoOut->v[vowelSeqInfoOut->roofPos];
    }
    return true;
}

/**
 * @brief `processHookWithUO` branch for `vneHook_u`: toggle hook on `u` in u+o nuclei.
 *
 * Plain `u` becomes `ư`; already-hooked / composite paths collapse toward `u`+`o` and may
 * clear a tone on the first slot (`toneRemoved`).
 *
 * @param vowelSeq Nucleus before the edit.
 * @param seqLetters `VSeqList[vowelSeq].v` (three slots; unused slots may be ignored by logic).
 * @param vowelSpanStart First index of the vowel run in `m_buffer`.
 * @param[out] nextVowelSeq Resulting vowel sequence enum after edits.
 * @param[out] hookRemoved True when the hook path removed a hook (widened to plain letters).
 * @param[out] toneRemoved True when tone on the first vowel slot should be treated as cleared.
 */
void UkEngine::hookWithUO_onHookU(VowelSeq vowelSeq, const VnLexiName *seqLetters, int vowelSpanStart,
                                  VowelSeq &nextVowelSeq, bool &hookRemoved, bool &toneRemoved)
{
    const bool firstLetterIsPlainU = (seqLetters[0] == vnl_u);
    if (firstLetterIsPlainU)
    {
        nextVowelSeq = VSeqList[vowelSeq].withHook;
        markChange(vowelSpanStart);
        m_buffer[vowelSpanStart].vnSym = vnl_uh;
    }
    else
    {
        nextVowelSeq = lookupVSeq(vnl_u, vnl_o, seqLetters[2]);
        markChange(vowelSpanStart);
        m_buffer[vowelSpanStart].vnSym = vnl_u;
        m_buffer[vowelSpanStart + 1].vnSym = vnl_o;
        hookRemoved = true;
        const bool firstSlotHasTone = (m_buffer[vowelSpanStart].tone != 0);
        toneRemoved = firstSlotHasTone;
    }
}

/**
 * @brief `processHookWithUO` branch for `vneHook_o`: hook or unhook the `o` side of u+o.
 *
 * Special-cases `th` + short vowel at cursor so hook targets the second nucleus letter only
 * when that matches historical UniKey “thuo…” behavior.
 *
 * @param vowelSeq Nucleus before the edit.
 * @param seqLetters `VSeqList[vowelSeq].v`.
 * @param vowelSpanStart First index of the vowel run.
 * @param vowelSpanEnd Vowel-end index (same convention as `m_current - vOffset`).
 * @param[out] nextVowelSeq Resulting `VowelSeq` after edits.
 * @param[out] hookRemoved True when hook was stripped from this key.
 * @param[out] toneRemoved True when a tone on the second vowel slot was cleared by the strip.
 */
void UkEngine::hookWithUO_onHookO(VowelSeq vowelSeq, const VnLexiName *seqLetters, int vowelSpanStart,
                                  int vowelSpanEnd, VowelSeq &nextVowelSeq, bool &hookRemoved,
                                  bool &toneRemoved)
{
    const bool secondLetterIsOOrORoof =
        (seqLetters[1] == vnl_o || seqLetters[1] == vnl_or);
    if (secondLetterIsOOrORoof)
    {
        const VowelSeqInfo &seqInfo = VSeqList[vowelSeq];
        const bool vowelEndAtCursor = (vowelSpanEnd == m_current);
        const bool twoLetterNucleus = (seqInfo.len == 2);
        const bool syllableShapeIsCv = (m_buffer[m_current].form == vnw_cv);
        const bool onsetIsTh = (m_buffer[m_current - 2].cseq == cs_th);
        const bool isThShortVowelHookCase =
            vowelEndAtCursor && twoLetterNucleus && syllableShapeIsCv && onsetIsTh;
        if (isThShortVowelHookCase)
        {
            nextVowelSeq = VSeqList[vowelSeq].withHook;
            markChange(vowelSpanStart + 1);
            m_buffer[vowelSpanStart + 1].vnSym = vnl_oh;
        }
        else
        {
            nextVowelSeq = lookupVSeq(vnl_uh, vnl_oh, seqLetters[2]);
            const bool firstLetterIsPlainU = (seqLetters[0] == vnl_u);
            if (firstLetterIsPlainU)
            {
                markChange(vowelSpanStart);
                m_buffer[vowelSpanStart].vnSym = vnl_uh;
                m_buffer[vowelSpanStart + 1].vnSym = vnl_oh;
            }
            else
            {
                markChange(vowelSpanStart + 1);
                m_buffer[vowelSpanStart + 1].vnSym = vnl_oh;
            }
        }
    }
    else
    {
        nextVowelSeq = lookupVSeq(vnl_u, vnl_o, seqLetters[2]);
        const bool firstLetterIsHookU = (seqLetters[0] == vnl_uh);
        if (firstLetterIsHookU)
        {
            markChange(vowelSpanStart);
            m_buffer[vowelSpanStart].vnSym = vnl_u;
            m_buffer[vowelSpanStart + 1].vnSym = vnl_o;
        }
        else
        {
            markChange(vowelSpanStart + 1);
            m_buffer[vowelSpanStart + 1].vnSym = vnl_o;
        }
        hookRemoved = true;
        const bool secondSlotHasTone = (m_buffer[vowelSpanStart + 1].tone != 0);
        toneRemoved = secondSlotHasTone;
    }
}

/**
 * @brief `processHookWithUO` branch for combined hook keys (`vneHookAll`, `vneHook_uo`, etc.).
 *
 * May hook both `u` and `o`, only one side, or strip hooks back to `u`+`o` depending on
 * current letters and the `th` short-vowel shortcut (`m_current - 2` onset check).
 *
 * @param vowelSeq Nucleus before the edit.
 * @param seqLetters `VSeqList[vowelSeq].v`.
 * @param vowelSpanStart First index of the vowel run.
 * @param vowelSpanEnd Vowel-end index.
 * @param[out] nextVowelSeq Resulting `VowelSeq` after edits.
 * @param[out] hookRemoved True when hooks were removed.
 * @param[out] toneRemoved True when tone on the affected slots should be treated as cleared.
 */
void UkEngine::hookWithUO_onHookAll(VowelSeq vowelSeq, const VnLexiName *seqLetters, int vowelSpanStart,
                                    int vowelSpanEnd, VowelSeq &nextVowelSeq, bool &hookRemoved,
                                    bool &toneRemoved)
{
    const bool firstLetterIsPlainU = (seqLetters[0] == vnl_u);
    if (firstLetterIsPlainU)
    {
        const bool secondLetterIsOOrORoof =
            (seqLetters[1] == vnl_o || seqLetters[1] == vnl_or);
        if (secondLetterIsOOrORoof)
        {
            const bool isUoOrUorSequence = (vowelSeq == vs_uo || vowelSeq == vs_uor);
            const bool vowelEndAtCursor = (vowelSpanEnd == m_current);
            const bool syllableShapeIsCv = (m_buffer[m_current].form == vnw_cv);
            const bool onsetIsTh = (m_buffer[m_current - 2].cseq == cs_th);
            const bool isThuoStyleHook =
                isUoOrUorSequence && vowelEndAtCursor && syllableShapeIsCv && onsetIsTh;
            if (isThuoStyleHook)
            {
                nextVowelSeq = vs_uoh;
                markChange(vowelSpanStart + 1);
                m_buffer[vowelSpanStart + 1].vnSym = vnl_oh;
            }
            else
            {
                nextVowelSeq = VSeqList[vowelSeq].withHook;
                markChange(vowelSpanStart);
                m_buffer[vowelSpanStart].vnSym = vnl_uh;
                nextVowelSeq = VSeqList[nextVowelSeq].withHook;
                m_buffer[vowelSpanStart + 1].vnSym = vnl_oh;
            }
        }
        else
        {
            nextVowelSeq = VSeqList[vowelSeq].withHook;
            markChange(vowelSpanStart);
            m_buffer[vowelSpanStart].vnSym = vnl_uh;
        }
    }
    else
    {
        const bool secondLetterIsPlainO = (seqLetters[1] == vnl_o);
        if (secondLetterIsPlainO)
        {
            nextVowelSeq = VSeqList[vowelSeq].withHook;
            markChange(vowelSpanStart + 1);
            m_buffer[vowelSpanStart + 1].vnSym = vnl_oh;
        }
        else
        {
            nextVowelSeq = lookupVSeq(vnl_u, vnl_o, seqLetters[2]);
            markChange(vowelSpanStart);
            m_buffer[vowelSpanStart].vnSym = vnl_u;
            m_buffer[vowelSpanStart + 1].vnSym = vnl_o;
            hookRemoved = true;
            const bool toneOnFirstOrSecond = (m_buffer[vowelSpanStart].tone != 0 ||
                                                m_buffer[vowelSpanStart + 1].tone != 0);
            toneRemoved = toneOnFirstOrSecond;
        }
    }
}

/**
 * @brief Remove horn/bowl hooks (ư/ơ/ă) when the key matches `hookUndoKeyMatches`.
 *
 * @param evType Hook-related `UkKeyEvent::evType`.
 * @param vowelSeq Current vowel sequence.
 * @param vowelSpanStart First index of the vowel run.
 * @param[out] nextVowelSeq Sequence after `lookupVSeq` on stripped letters.
 * @param[out] vowelSeqInfoOut `VSeqList[nextVowelSeq]`.
 * @param[out] hookRemoved Set true on successful strip.
 * @param[out] changePos Buffer index of the hooked letter that was cleared.
 * @return False if no `hookPos`, key mismatch, or marking is cursor-locked.
 */
bool UkEngine::hookStripHookMark(int evType, VowelSeq vowelSeq, int vowelSpanStart,
                                 VowelSeq &nextVowelSeq, VowelSeqInfo *&vowelSeqInfoOut,
                                 bool &hookRemoved, int &changePos)
{
    const VowelSeqInfo &seqInfo = VSeqList[vowelSeq];
    if (seqInfo.hookPos == -1)
        return false;

    VnLexiName hookedLetter = m_buffer[vowelSpanStart + seqInfo.hookPos].vnSym;
    VnLexiName plainLetterAfterStrip =
        (hookedLetter == vnl_ab) ? vnl_a : ((hookedLetter == vnl_uh) ? vnl_u : vnl_o);
    changePos = vowelSpanStart + seqInfo.hookPos;

    const bool markingLockedToCursor = !m_pCtrl->options.freeMarking && changePos != m_current;
    if (markingLockedToCursor)
        return false;

    if (!hookUndoKeyMatches(evType, hookedLetter))
        return false;

    markChange(changePos);
    m_buffer[changePos].vnSym = plainLetterAfterStrip;

    if (seqInfo.len == 3)
        nextVowelSeq =
            lookupVSeq(m_buffer[vowelSpanStart].vnSym, m_buffer[vowelSpanStart + 1].vnSym,
                      m_buffer[vowelSpanStart + 2].vnSym);
    else if (seqInfo.len == 2)
        nextVowelSeq =
            lookupVSeq(m_buffer[vowelSpanStart].vnSym, m_buffer[vowelSpanStart + 1].vnSym);
    else
        nextVowelSeq = lookupVSeq(m_buffer[vowelSpanStart].vnSym);

    vowelSeqInfoOut = &VSeqList[nextVowelSeq];
    hookRemoved = true;
    return true;
}

/**
 * @brief Apply `withHook` row `nextVowelSeq` at `hookPos` when the key matches the target hook.
 *
 * @param evType Hook-related `UkKeyEvent::evType`.
 * @param vowelSpanStart First index of the vowel run.
 * @param nextVowelSeq Hooked sequence from `VSeqList[prev].withHook`.
 * @param[out] vowelSeqInfoOut Row for `nextVowelSeq`.
 * @param[out] changePos Buffer index receiving the hooked letter.
 * @return False on `hookApplyKeyMatches` failure, invalid CVC, or cursor-locked marking.
 */
bool UkEngine::hookPlaceHookMark(int evType, int vowelSpanStart, VowelSeq nextVowelSeq,
                                 VowelSeqInfo *&vowelSeqInfoOut, int &changePos)
{
    vowelSeqInfoOut = &VSeqList[nextVowelSeq];
    if (!hookApplyKeyMatches(evType, *vowelSeqInfoOut))
        return false;

    if (!syllableValidForVowelSeq(nextVowelSeq))
        return false;

    changePos = vowelSpanStart + vowelSeqInfoOut->hookPos;

    const bool markingLockedToCursor = !m_pCtrl->options.freeMarking && changePos != m_current;
    if (markingLockedToCursor)
        return false;

    markChange(changePos);
    m_buffer[changePos].vnSym = vowelSeqInfoOut->v[vowelSeqInfoOut->hookPos];
    return true;
}

/**
 * @brief Apply or remove circumflex marks (â, ê, ô) from the vowel under the cursor.
 *
 * Chooses strip vs place from `VSeqList[v].withRoof` vs `vs_nil`, handles uho-family u+ơ
 * promotion, then refreshes sub-sequences and relocates tone. Stripping may append the raw
 * key via `finishRemovedDiacriticAppendRevert`.
 *
 * @param ev Roof key event (`vneRoof_*`).
 * @return 1 if consumed; otherwise forwards to `processAppend` when no edit applies.
 */
int UkEngine::processRoof(UkKeyEvent &ev)
{
    const bool vietnameseOff = !m_pCtrl->vietKey;
    const bool cursorInvalid = (m_current < 0);
    const bool noActiveVowelSpan = (m_current >= 0 && m_buffer[m_current].vOffset < 0);
    if (vietnameseOff || cursorInvalid || noActiveVowelSpan)
        return processAppend(ev);

    const VnLexiName targetRoofLexi = roofTargetFromEvType(ev.evType);

    int vowelSpanStart = 0;
    int vowelSpanEnd = 0;
    int curTonePos = 0;
    int newTonePos = 0;
    int tone = 0;
    int changePos = 0;
    bool roofRemoved = false;
    VowelSeq vowelSeq = vs_nil;
    VowelSeq nextVowelSeq = vs_nil;
    VowelSeqInfo *vowelSeqInfo = nullptr;

    readVowelSpanFromCursor(vowelSpanEnd, vowelSpanStart, vowelSeq, curTonePos, tone);

    const bool isUhoFamilyNeedingUorPair =
        (vowelSeq == vs_uho || vowelSeq == vs_uhoh || vowelSeq == vs_uhoi || vowelSeq == vs_uhohi);
    bool doubleChangeUO = false;
    if (isUhoFamilyNeedingUorPair)
    {
        nextVowelSeq = lookupVSeq(vnl_u, vnl_or, VSeqList[vowelSeq].v[2]);
        doubleChangeUO = true;
    }
    else
    {
        nextVowelSeq = VSeqList[vowelSeq].withRoof;
    }

    if (nextVowelSeq == vs_nil)
    {
        if (!roofStripRoofMark(vowelSeq, targetRoofLexi, vowelSpanStart, nextVowelSeq, vowelSeqInfo,
                               roofRemoved, changePos))
            return processAppend(ev);
    }
    else
    {
        if (!roofPlaceRoofMark(targetRoofLexi, vowelSpanStart, nextVowelSeq, doubleChangeUO, vowelSeqInfo,
                               changePos))
            return processAppend(ev);
    }

    vowelApplySubsequences(vowelSpanStart, *vowelSeqInfo);
    const bool vowelEndAtCursor = (vowelSpanEnd == m_current);
    newTonePos = vowelSpanStart + getTonePosition(nextVowelSeq, vowelEndAtCursor);
    /* //For now, users don't seem to like the following processing, thus commented out
    if (roofRemoved && tone != 0 &&
        (!vowelSeqInfo->complete || changePos == curTonePos)) {
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    } else
    */
    relocateToneIfMoved(curTonePos, newTonePos, tone);

    if (roofRemoved)
        finishRemovedDiacriticAppendRevert(ev);

    return 1;
}

/**
 * @brief Hook marks for u+o (and related) nuclei: delegate from `processHook` only.
 *
 * Encodes Telex/VNI horn placement and paired u/o behavior, including strip paths that
 * re-append via `finishRemovedDiacriticAppendRevert`. Not valid for generic single-vowel hooks;
 * `processHook` gates entry.
 *
 * @param ev Hook key event.
 * @return 1 when consumed; `processAppend` when `freeMarking` blocks off-cursor edits.
 *
 * @warning Must only be called from `processHook` after the u+o delegate checks; callers
 *          elsewhere can violate vowel-span and cursor assumptions.
 */
int UkEngine::processHookWithUO(UkKeyEvent &ev)
{
    VowelSeq vowelSeq = vs_nil;
    VowelSeq nextVowelSeq = vs_nil;
    int vowelSpanStart = 0;
    int vowelSpanEnd = 0;
    int curTonePos = 0;
    int newTonePos = 0;
    int tone = 0;
    bool hookRemoved = false;
    const bool removeWithUndo = true;
    bool toneRemoved = false;

    (void)toneRemoved;

    const bool freeMarkingOff = !m_pCtrl->options.freeMarking;
    const bool cursorNotAtVowelStart = (m_buffer[m_current].vOffset != 0);
    if (freeMarkingOff && cursorNotAtVowelStart)
        return processAppend(ev);

    vowelSpanEnd = m_current - m_buffer[m_current].vOffset;
    vowelSeq = m_buffer[vowelSpanEnd].vseq;
    vowelSpanStart = vowelSpanEnd - (VSeqList[vowelSeq].len - 1);
    const VnLexiName *seqLetters = VSeqList[vowelSeq].v;
    const bool vowelEndAtCursor = (vowelSpanEnd == m_current);
    curTonePos = vowelSpanStart + getTonePosition(vowelSeq, vowelEndAtCursor);
    tone = m_buffer[curTonePos].tone;

    switch (ev.evType)
    {
    case vneHook_u:
        hookWithUO_onHookU(vowelSeq, seqLetters, vowelSpanStart, nextVowelSeq, hookRemoved, toneRemoved);
        break;
    case vneHook_o:
        hookWithUO_onHookO(vowelSeq, seqLetters, vowelSpanStart, vowelSpanEnd, nextVowelSeq, hookRemoved,
                           toneRemoved);
        break;
    default:
        hookWithUO_onHookAll(vowelSeq, seqLetters, vowelSpanStart, vowelSpanEnd, nextVowelSeq, hookRemoved,
                             toneRemoved);
        break;
    }

    VowelSeqInfo *updatedSeqInfo = &VSeqList[nextVowelSeq];
    vowelApplySubsequences(vowelSpanStart, *updatedSeqInfo);

    newTonePos = vowelSpanStart + getTonePosition(nextVowelSeq, vowelEndAtCursor);
    /* //For now, users don't seem to like the following processing, thus commented out
    if (hookRemoved && tone != 0 && (!updatedSeqInfo->complete || toneRemoved)) {
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    }
    else
    */
    relocateToneIfMoved(curTonePos, newTonePos, tone);

    if (hookRemoved && removeWithUndo)
        finishRemovedDiacriticAppendRevert(ev);

    return 1;
}

/**
 * @brief Apply or remove hook / bowl marks (ư, ơ, ă) on the active vowel span.
 *
 * Delegates u+o family nuclei to `processHookWithUO`; otherwise uses `withHook` table transitions
 * and shared strip/place helpers. Tone may relocate after the nucleus edits.
 *
 * @param ev Hook-related key event.
 * @return 1 when consumed, else `processAppend` when Vietnamese mode or span is invalid or
 *         the edit is disallowed.
 */
int UkEngine::processHook(UkKeyEvent &ev)
{
    const bool vietnameseOff = !m_pCtrl->vietKey;
    const bool cursorInvalid = (m_current < 0);
    const bool noActiveVowelSpan = (m_current >= 0 && m_buffer[m_current].vOffset < 0);
    if (vietnameseOff || cursorInvalid || noActiveVowelSpan)
        return processAppend(ev);

    VowelSeq vowelSeq = vs_nil;
    VowelSeq nextVowelSeq = vs_nil;
    int vowelSpanStart = 0;
    int vowelSpanEnd = 0;
    int curTonePos = 0;
    int newTonePos = 0;
    int tone = 0;
    int changePos = 0;
    bool hookRemoved = false;
    VowelSeqInfo *vowelSeqInfo = nullptr;

    vowelSpanEnd = m_current - m_buffer[m_current].vOffset;
    vowelSeq = m_buffer[vowelSpanEnd].vseq;
    const VowelSeqInfo &seqInfoForDelegate = VSeqList[vowelSeq];
    const VnLexiName *seqLetters = seqInfoForDelegate.v;

    const bool multiLetterNucleus = (seqInfoForDelegate.len > 1);
    const bool eventIsNotBowl = (ev.evType != vneBowl);
    const bool firstLetterIsPlainUOrHookU =
        (seqLetters[0] == vnl_u || seqLetters[0] == vnl_uh);
    const bool secondLetterIsOFamily =
        (seqLetters[1] == vnl_o || seqLetters[1] == vnl_oh || seqLetters[1] == vnl_or);
    const bool shouldUseUoHookDelegate =
        multiLetterNucleus && eventIsNotBowl && firstLetterIsPlainUOrHookU && secondLetterIsOFamily;
    if (shouldUseUoHookDelegate)
        return processHookWithUO(ev);

    vowelSpanStart = vowelSpanEnd - (seqInfoForDelegate.len - 1);
    const bool vowelEndAtCursor = (vowelSpanEnd == m_current);
    curTonePos = vowelSpanStart + getTonePosition(vowelSeq, vowelEndAtCursor);
    tone = m_buffer[curTonePos].tone;

    nextVowelSeq = seqInfoForDelegate.withHook;
    if (nextVowelSeq == vs_nil)
    {
        if (!hookStripHookMark(ev.evType, vowelSeq, vowelSpanStart, nextVowelSeq, vowelSeqInfo, hookRemoved,
                               changePos))
            return processAppend(ev);
    }
    else
    {
        if (!hookPlaceHookMark(ev.evType, vowelSpanStart, nextVowelSeq, vowelSeqInfo, changePos))
            return processAppend(ev);
    }

    vowelApplySubsequences(vowelSpanStart, *vowelSeqInfo);
    newTonePos = vowelSpanStart + getTonePosition(nextVowelSeq, vowelEndAtCursor);
    /* //For now, users don't seem to like the following processing, thus commented out
    if (hookRemoved && tone != 0 &&
        (!vowelSeqInfo->complete || (hookRemoved && curTonePos == changePos))) {
        markChange(curTonePos);
        m_buffer[curTonePos].tone = 0;
    }
    else */
    relocateToneIfMoved(curTonePos, newTonePos, tone);

    if (hookRemoved)
        finishRemovedDiacriticAppendRevert(ev);

    return 1;
}

/**
 * @brief Which vowel slot (offset within the nucleus) receives the tone mark.
 *
 * Prefers explicit `roofPos` / `hookPos` from `VSeqList`, then triple-nucleus and `modernStyle`
 * rules for pairs like oa/oe/uy.
 *
 * @param vowelSeq Vowel sequence enum for the nucleus.
 * @param vowelSpanEndsAtCursor True when the vowel end is at `m_current` (affects some pairs).
 * @return Offset from the first nucleus letter: 0-based index into the vowel run.
 *
 * @note `vowelSpanEndsAtCursor` drives the default first-vs-second vowel choice when the table
 *       does not pin tone to roof/hook.
 */
int UkEngine::getTonePosition(VowelSeq vowelSeq, bool vowelSpanEndsAtCursor)
{
    VowelSeqInfo &info = VSeqList[vowelSeq];
    const bool singleVowelLetter = (info.len == 1);
    if (singleVowelLetter)
        return 0;

    if (info.roofPos != -1)
        return info.roofPos;
    if (info.hookPos != -1)
    {
        const bool vowelIsUHOHShape = (vowelSeq == vs_uhoh || vowelSeq == vs_uhohi || vowelSeq == vs_uhohu);
        if (vowelIsUHOHShape) // u+o+, u+o+u, u+o+i
            return 1;
        return info.hookPos;
    }

    const bool tripleLetterNucleus = (info.len == 3);
    if (tripleLetterNucleus)
        return 1;

    const bool modernStyleSecondTone =
        m_pCtrl->options.modernStyle && (vowelSeq == vs_oa || vowelSeq == vs_oe || vowelSeq == vs_uy);
    if (modernStyleSecondTone)
        return 1;

    return vowelSpanEndsAtCursor ? 0 : 1;
}

int UkEngine::processToneGiOrGin(UkKeyEvent &ev)
{
    const bool syllableIsGi = (m_buffer[m_current].cseq == cs_gi);
    const int giToneBufferIndex = syllableIsGi ? m_current : (m_current - 1);

    const bool bufferSaysNoTone = (m_buffer[giToneBufferIndex].tone == 0);
    const bool keySaysNoTone = (ev.tone == 0);
    if (bufferSaysNoTone && keySaysNoTone)
        return processAppend(ev);

    markChange(giToneBufferIndex);

    const bool keyDuplicatesBufferTone = (m_buffer[giToneBufferIndex].tone == ev.tone);
    if (keyDuplicatesBufferTone)
    {
        m_buffer[giToneBufferIndex].tone = 0;
        m_singleMode = false;
        processAppend(ev);
        m_reverted = true;
        return 1;
    }
    m_buffer[giToneBufferIndex].tone = ev.tone;
    return 1;
}

/**
 * @brief Set or clear a tone on the computed slot for `vowelSeq` at `vowelSpanEnd`.
 *
 * Blocks incomplete nuclei when spell-check and non-free marking demand a complete word;
 * rejects falling/question/tilde on plosive codas `c`, `ch`, `p`, `t`. Repeat-tone clears
 * the mark and appends the raw stroke (revert path).
 *
 * @param ev Key event with tone id in `ev.tone`.
 * @param vowelSpanEnd Index of the vowel-end `WordInfo` for this nucleus.
 * @param vowelSeq Nucleus enum at `vowelSpanEnd`.
 * @return 1 when tone applied/cleared/reverted; `processAppend` when disallowed or no tone op.
 */
int UkEngine::processToneOnVowelSpan(UkKeyEvent &ev, int vowelSpanEnd, VowelSeq vowelSeq)
{
    VowelSeqInfo &vowelInfo = VSeqList[vowelSeq];
    const bool spellCheckWantsComplete = m_pCtrl->options.spellCheckEnabled;
    const bool markingNotFree = !m_pCtrl->options.freeMarking;
    const bool nucleusIncomplete = !vowelInfo.complete;
    if (spellCheckWantsComplete && markingNotFree && nucleusIncomplete)
        return processAppend(ev);

    const bool formIsVc = (m_buffer[m_current].form == vnw_vc);
    const bool formIsCvc = (m_buffer[m_current].form == vnw_cvc);
    if (formIsVc || formIsCvc)
    {
        const ConSeq codaSeq = m_buffer[m_current].cseq;
        const bool codaIsUntonablePlosive =
            (codaSeq == cs_c || codaSeq == cs_ch || codaSeq == cs_p || codaSeq == cs_t);
        const bool toneIsFallingOrQuestionOrTilde =
            (ev.tone == 2 || ev.tone == 3 || ev.tone == 4);
        if (codaIsUntonablePlosive && toneIsFallingOrQuestionOrTilde)
            return processAppend(ev); // c, ch, p, t suffixes don't allow ` ? ~
    }

    const bool vowelSpanEndAtCursor = (vowelSpanEnd == m_current);
    const int toneOffset = getTonePosition(vowelSeq, vowelSpanEndAtCursor);
    const int toneBufferIndex = vowelSpanEnd - (vowelInfo.len - 1) + toneOffset;

    const bool bufferSlotHasNoTone = (m_buffer[toneBufferIndex].tone == 0);
    const bool keyCarriesNoTone = (ev.tone == 0);
    if (bufferSlotHasNoTone && keyCarriesNoTone)
        return processAppend(ev);

    const bool keyDuplicatesSlotTone = (m_buffer[toneBufferIndex].tone == ev.tone);
    if (keyDuplicatesSlotTone)
    {
        markChange(toneBufferIndex);
        m_buffer[toneBufferIndex].tone = 0;
        m_singleMode = false;
        processAppend(ev);
        m_reverted = true;
        return 1;
    }

    markChange(toneBufferIndex);
    m_buffer[toneBufferIndex].tone = ev.tone;
    return 1;
}

/**
 * @brief Route tone keys to the correct syllable piece: `gi`/`gin`, vowel span, or append.
 *
 * @param ev Key event with tone in `ev.tone`.
 * @return 1 when a tone was applied, cleared, or the stroke was consumed; `processAppend` when
 *         Vietnamese is off, the cursor is invalid, there is no vowel context, or rules block tone.
 */
int UkEngine::processTone(UkKeyEvent &ev)
{
    const bool cursorInvalid = (m_current < 0);
    const bool vietnameseOff = !m_pCtrl->vietKey;
    if (cursorInvalid || vietnameseOff)
        return processAppend(ev);

    const bool wordPieceIsConsonantOnly = (m_buffer[m_current].form == vnw_c);
    const bool clusterIsGi = (m_buffer[m_current].cseq == cs_gi);
    const bool clusterIsGin = (m_buffer[m_current].cseq == cs_gin);
    if (wordPieceIsConsonantOnly && (clusterIsGi || clusterIsGin))
        return processToneGiOrGin(ev);

    if (m_buffer[m_current].vOffset < 0)
        return processAppend(ev);

    const int vowelSpanEnd = m_current - m_buffer[m_current].vOffset;
    const VowelSeq vowelSeqAtSpan = m_buffer[vowelSpanEnd].vseq;
    return processToneOnVowelSpan(ev, vowelSpanEnd, vowelSeqAtSpan);
}

/**
 * @brief Telex `dd` when the current `d` is “raw” non-Vietnamese and follows a non-vowel.
 *
 * Promotes surface `d` into consonant `đ` (`cs_dd`) for abbreviation-style typing without
 * a full syllable frame.
 *
 * @param ev Unused; kept for uniform `processDd` try-slot signature.
 * @return 1 when the abbrev path ran; 0 when preconditions failed (caller tries consonant path).
 */
int UkEngine::processDdTryAbbrevNonVnD(UkKeyEvent &ev)
{
    (void)ev;
    const WordInfo &prev = m_buffer[m_current - 1];
    const bool previousCharIsNotLexicalVowel =
        (prev.vnSym == vnl_nonVnChar) || !IsVnVowel[prev.vnSym];

    const WordInfo &cur = m_buffer[m_current];
    const bool curIsNonVnSurfaceD =
        (cur.form == vnw_nonVn) && (cur.vnSym == vnl_d) && previousCharIsNotLexicalVowel;
    if (!curIsNonVnSurfaceD)
        return 0;

    const int cursorPos = m_current;
    m_singleMode = true;
    markChange(cursorPos);
    m_buffer[cursorPos].cseq = cs_dd;
    m_buffer[cursorPos].vnSym = vnl_dd;
    m_buffer[cursorPos].form = vnw_c;
    m_buffer[cursorPos].c1Offset = 0;
    m_buffer[cursorPos].c2Offset = -1;
    m_buffer[cursorPos].vOffset = -1;
    return 1;
}

/**
 * @brief Toggle `d` ↔ `đ` on the onset cluster at `c1Offset`, or fall through to append.
 *
 * Requires a consonant frame. `freeMarking` forces the cluster to be at the cursor. Doubling
 * `d→đ` sets `m_singleMode` to skip spell-check on abbreviated `dd-` words; reverting `đ→d`
 * appends the raw key and sets `m_reverted`.
 *
 * @param ev Unused; uniform signature with other `processDd*` helpers.
 * @return 1 when `d`/`đ` was toggled; `processAppend` when no consonant slot or wrong cluster.
 */
int UkEngine::processDdTryConsonantDOrDd(UkKeyEvent &ev)
{
    (void)ev;
    const bool missingConsonantFrame = (m_buffer[m_current].c1Offset < 0);
    if (missingConsonantFrame)
        return processAppend(ev);

    const int consonantClusterStart = m_current - m_buffer[m_current].c1Offset;
    const bool freeMarkingOff = !m_pCtrl->options.freeMarking;
    const bool clusterNotAtCursor = (consonantClusterStart != m_current);
    if (freeMarkingOff && clusterNotAtCursor)
        return processAppend(ev);

    if (m_buffer[consonantClusterStart].cseq == cs_d)
    {
        markChange(consonantClusterStart);
        m_buffer[consonantClusterStart].cseq = cs_dd;
        m_buffer[consonantClusterStart].vnSym = vnl_dd;
        // never spellcheck a word which starts with dd, because it's used alot in abbreviation
        m_singleMode = true;
        return 1;
    }

    if (m_buffer[consonantClusterStart].cseq == cs_dd)
    {
        markChange(consonantClusterStart);
        m_buffer[consonantClusterStart].cseq = cs_d;
        m_buffer[consonantClusterStart].vnSym = vnl_d;
        m_singleMode = false;
        processAppend(ev);
        m_reverted = true;
        return 1;
    }

    return processAppend(ev);
}

/**
 * @brief Telex/VNI `dd`: try abbreviation `d`→`đ`, else toggle onset `d`/`đ`.
 *
 * @param ev Second `d` key in the sequence (or equivalent).
 * @return 1 when `đ` apply/undo or abbrev path succeeded; otherwise `processAppend` result.
 */
int UkEngine::processDd(UkKeyEvent &ev)
{
    const bool vietnamese_mode_ready = m_pCtrl->vietKey && m_current >= 0;
    if (!vietnamese_mode_ready)
        return processAppend(ev);

    const int abbrevHandled = processDdTryAbbrevNonVnD(ev);
    if (abbrevHandled != 0)
        return abbrevHandled;

    return processDdTryConsonantDOrDd(ev);
}

/**
 * @brief Rewind one character when the user repeats the key that produced a map-char (e.g. `w`→`ư`, `w` again).
 *
 * Decrements `m_current` again after the caller’s first decrement and may relocate tone on the
 * shortened nucleus.
 *
 * @param ev Unused; reserved so undo call sites can pass the same arguments as other handlers.
 * @param entry `WordInfo` at `m_current` when entered — the composed cell matched for repeat-undo.
 * @param[out] undoRepeatStroke Always set true so `processMapChar` can force revert state.
 */
void UkEngine::processMapCharApplyUndoRepeatKey(UkKeyEvent &ev, WordInfo &entry, bool &undoRepeatStroke)
{
    (void)ev;
    const bool rewindTargetsConsonantOnly = (entry.form == vnw_c);
    if (!rewindTargetsConsonantOnly)
    {
        int vowelSpanStart = 0;
        int vowelSpanEnd = 0;
        int curTonePos = 0;
        int newTonePos = 0;
        int toneValue = 0;
        VowelSeq vowelSeq = vs_nil;
        VowelSeq vowelSeqAfterRewind = vs_nil;

        vowelSpanEnd = m_current - entry.vOffset;
        vowelSeq = m_buffer[vowelSpanEnd].vseq;
        vowelSpanStart = vowelSpanEnd - VSeqList[vowelSeq].len + 1;
        const bool vowelSpanEndAtCursor = (vowelSpanEnd == m_current);
        curTonePos = vowelSpanStart + getTonePosition(vowelSeq, vowelSpanEndAtCursor);
        toneValue = m_buffer[curTonePos].tone;
        markChange(m_current);
        m_current--;

        const bool toneMovedElsewhere = (toneValue != 0);
        const bool stillInsideBuffer = (m_current >= 0);
        const bool priorIsVVowel = (m_buffer[m_current].form == vnw_v);
        const bool priorIsCv = (m_buffer[m_current].form == vnw_cv);
        if (toneMovedElsewhere && stillInsideBuffer && (priorIsVVowel || priorIsCv))
        {
            vowelSeqAfterRewind = m_buffer[m_current].vseq;
            newTonePos = vowelSpanStart + getTonePosition(vowelSeqAfterRewind, true);
            const bool toneMustMoveAcrossSlots = (newTonePos != curTonePos);
            if (toneMustMoveAcrossSlots)
            {
                markChange(newTonePos);
                m_buffer[newTonePos].tone = toneValue;
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
    undoRepeatStroke = true;
}

/**
 * @brief `vneMapChar` path: apply composed-letter mapping, with repeat-to-undo when a stroke just finished composition.
 *
 * Applies caps to `ev.vnSym`, runs `processAppend`, then if composition ended rewinds one slot and
 * may undo the prior cell when its base letter matches the key. Re-dispatches as a normal key for
 * the final append.
 *
 * @param ev Event in map-char mode (`evType`, `vnSym`, `keyCode`).
 * @return 1 when composition continues, undo runs, or append consumes the key; 0 when mapChar
 *         cannot apply (buffer edge case).
 */
int UkEngine::processMapChar(UkKeyEvent &ev)
{
    int capsLockIsOn = 0;
    int shiftIsPressed = 0;
    if (m_keyCheckFunc)
        m_keyCheckFunc(&shiftIsPressed, &capsLockIsOn);

    if (capsLockIsOn)
        ev.vnSym = changeCase(ev.vnSym);

    int appendResult = processAppend(ev);
    if (!m_pCtrl->vietKey)
        return appendResult;

    const bool cursorPointsIntoBuffer = (m_current >= 0);
    const bool currentIsNotEmptyMarker = cursorPointsIntoBuffer && (m_buffer[m_current].form != vnw_empty);
    const bool currentIsNotPlainNonVn =
        cursorPointsIntoBuffer && (m_buffer[m_current].form != vnw_nonVn);
    const bool compositionStillActive = currentIsNotEmptyMarker && currentIsNotPlainNonVn;
    if (compositionStillActive)
        return 1;

    if (!cursorPointsIntoBuffer)
        return 0;

    // mapChar doesn't apply
    m_current--;
    WordInfo &entry = m_buffer[m_current];

    bool undoRepeatStroke = false;
    const bool entryIsNotEmpty = (entry.form != vnw_empty);
    const bool entryIsNotPlainNonVn = (entry.form != vnw_nonVn);
    if (entryIsNotEmpty && entryIsNotPlainNonVn)
    {
        VnLexiName entryLowerSymbol = entry.vnSym;
        const bool entryHadShiftedGlyph = (entry.caps != 0);
        if (entryHadShiftedGlyph)
            entryLowerSymbol = (VnLexiName)(entryLowerSymbol - 1);

        const bool repeatKeyMatchesComposedLetter = (entryLowerSymbol == ev.vnSym);
        if (repeatKeyMatchesComposedLetter)
            processMapCharApplyUndoRepeatKey(ev, entry, undoRepeatStroke);
    }

    ev.evType = vneNormal;
    ev.chType = m_pCtrl->input.getCharType(ev.keyCode);
    ev.vnSym = IsoToVnLexi(ev.keyCode);
    appendResult = processAppend(ev);
    if (undoRepeatStroke)
    {
        m_singleMode = false;
        m_reverted = true;
        return 1;
    }
    return appendResult;
}

/**
 * @brief Telex `w` branch when the previous stroke used mapChar: try `ư` first, else hook-all.
 *
 * On `processMapChar` returning 0, rewinds the cursor once and retries as `vneHookAll`.
 *
 * @param ev Key event for `w` (case from `keyCode` / shift).
 * @param capsLockIsOn From `m_keyCheckFunc`; folded into `ev.vnSym` via `changeCase`.
 * @param[out] usedAsMapChar Set false when falling back to hook-all; unchanged on mapChar success.
 * @return Result of `processMapChar` or `processHook`.
 */
int UkEngine::processTelexWBranchMapChar(UkKeyEvent &ev, int capsLockIsOn, bool &usedAsMapChar)
{
    ev.evType = vneMapChar;
    ev.vnSym = isupper(ev.keyCode) ? vnl_Uh : vnl_uh;
    if (capsLockIsOn)
        ev.vnSym = changeCase(ev.vnSym);
    ev.chType = ukcVn;
    const int mapCharResult = processMapChar(ev);
    const bool mapCharIgnoredKey = (mapCharResult == 0);
    if (mapCharIgnoredKey)
    {
        const bool canRewindCursor = (m_current >= 0);
        if (canRewindCursor)
            m_current--;
        usedAsMapChar = false;
        ev.evType = vneHookAll;
        return processHook(ev);
    }
    return mapCharResult;
}

/**
 * @brief Telex `w` branch (hook-first): run hook-all, then `ư` via mapChar if hook ignores the key.
 *
 * @param ev Key event for `w`.
 * @param capsLockIsOn From `m_keyCheckFunc`; applied when synthesizing `vnl_Uh` / `vnl_uh`.
 * @param[out] usedAsMapChar Set true when the fallback mapChar path runs.
 * @return Result of `processHook` or `processMapChar`.
 */
int UkEngine::processTelexWBranchHook(UkKeyEvent &ev, int capsLockIsOn, bool &usedAsMapChar)
{
    ev.evType = vneHookAll;
    usedAsMapChar = false;
    const int hookResult = processHook(ev);
    const bool hookIgnoredKey = (hookResult == 0);
    if (hookIgnoredKey)
    {
        const bool canRewindCursor = (m_current >= 0);
        if (canRewindCursor)
            m_current--;
        ev.evType = vneMapChar;
        ev.vnSym = isupper(ev.keyCode) ? vnl_Uh : vnl_uh;
        if (capsLockIsOn)
            ev.vnSym = changeCase(ev.vnSym);
        ev.chType = ukcVn;
        usedAsMapChar = true;
        return processMapChar(ev);
    }
    return hookResult;
}

/**
 * @brief Telex `w`: alternate between hook-first and mapChar-first using process-local state.
 *
 * Selects hook vs `ư` behavior depending on which path succeeded on the prior `w` (function-static
 * `usedAsMapChar`).
 *
 * @param ev Key event for `w`.
 * @return Hook/mapChar branch result, or `processAppend` when Vietnamese is off.
 *
 * @note `usedAsMapChar` is static storage; intended for single-threaded input method context.
 */
int UkEngine::processTelexW(UkKeyEvent &ev)
{
    if (!m_pCtrl->vietKey)
        return processAppend(ev);

    static bool usedAsMapChar = false;
    int capsLockIsOn = 0;
    int shiftIsPressed = 0;
    if (m_keyCheckFunc)
        m_keyCheckFunc(&shiftIsPressed, &capsLockIsOn);

    const bool secondStrokeUsesMapCharPath = usedAsMapChar;
    if (secondStrokeUsesMapCharPath)
        return processTelexWBranchMapChar(ev, capsLockIsOn, usedAsMapChar);
    return processTelexWBranchHook(ev, capsLockIsOn, usedAsMapChar);
}
