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
#if defined(_WIN32)
#include "keyhook.h"
#endif

#include "engine_components/engine_internal.h"
#include "engine_components/engine_tables_shared.h"
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
