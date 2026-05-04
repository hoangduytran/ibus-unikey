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
