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
#include <ctype.h>
#if defined(_WIN32)
#include "keyhook.h"
#endif

#include "engine_components/engine_internal.h"
#include "engine_components/engine_tables_shared.h"
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
