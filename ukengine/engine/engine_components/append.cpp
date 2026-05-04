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

namespace {
/** Sentinel for `appendVowel*` helpers that should fall through to the shared footer. */
constexpr int kAppendVowelContinueToFooter = -1;
} // namespace

/**
 * @brief Check if the key event is an escape character for VIQR input mode.
 *
 * @param ev Key event to check.
 * @return 1 if the key event is an escape character, 0 otherwise.
 *
 * @note This function is only used in VIQR input mode.
 */
int UkEngine::checkEscapeVIQR(UkKeyEvent &ev)
{
    if (m_current < 0)
        return 0;
    WordInfo &entry = m_buffer[m_current];
    int escape = 0;

    const bool is_vowel_or_cv = entry.form == vnw_v || entry.form == vnw_cv;
    const bool is_non_vn = entry.form == vnw_nonVn;
    if (is_vowel_or_cv)
    {
        // Check if the key event is an escape character for VIQR input mode.
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
    else if (is_non_vn)
    {
        // Check if the key event is an escape character for VIQR input mode.
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
        // Append the escape character to the buffer.
        m_current++;
        WordInfo *p = &m_buffer[m_current];
        p->form = (ev.chType == ukcWordBreak) ? vnw_empty : vnw_nonVn;
        p->c1Offset = p->c2Offset = p->vOffset = -1;
        p->keyCode = '?';
        p->vnSym = vnl_nonVnChar;

        // Append the key event to the buffer.
        m_current++;
        p++;
        p->form = (ev.chType == ukcWordBreak) ? vnw_empty : vnw_nonVn;
        p->c1Offset = p->c2Offset = p->vOffset = -1;
        p->keyCode = ev.keyCode;
        p->vnSym = vnl_nonVnChar;

        // Write output.
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
    switch (ev.chType)
    {
    case ukcReset:
        return processAppendReset(ev);
    case ukcWordBreak:
        m_singleMode = false;
        return processWordEnd(ev);
    case ukcNonVn:
        return processAppendNonVn(ev);
    case ukcVn:
        return processAppendVietnameseLetter(ev);
    default:
        return 0;
    }
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

    const bool atWordStart = (m_current == 0);
    const bool vietProcessingDisabled = !m_pCtrl->vietKey;
    if (atWordStart || vietProcessingDisabled)
    {
        entry.form = vnw_v;
        entry.c1Offset = entry.c2Offset = -1;
        entry.vOffset = 0;
        entry.vseq = lookupVSeq(canSym);

        const bool skipMarkUnlessUniCString =
            !m_pCtrl->vietKey ||
            ((m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING) && isalpha(entry.keyCode));
        if (skipMarkUnlessUniCString)
            return 0;
        markChange(m_current);
        return 1;
    }

    WordInfo &prev = m_buffer[m_current - 1];

    switch (prev.form)
    {

    case vnw_empty:
        entry.form = vnw_v;
        entry.c1Offset = entry.c2Offset = -1;
        entry.vOffset = 0;
        entry.vseq = lookupVSeq(canSym);
        break;

    case vnw_nonVn:
    case vnw_cvc:
    case vnw_vc:
        entry.form = vnw_nonVn;
        entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
        break;

    case vnw_v:
    case vnw_cv:
    {
        const int vowelSeqOutcome =
            appendVowelAfterVOrCv(ev, entry, prev, lowerSym, canSym);
        
        const bool vowelSeqOutcomeIsNotFooter = vowelSeqOutcome != kAppendVowelContinueToFooter;
        if (vowelSeqOutcomeIsNotFooter)
            return vowelSeqOutcome;
        break;
    }
    case vnw_c:
    {
        const int afterConsonantOutcome =
            appendVowelAfterConsonantForm(ev, entry, prev, lowerSym, canSym);
        
        const bool afterConsonantOutcomeIsNotFooter = afterConsonantOutcome != kAppendVowelContinueToFooter;
        if (afterConsonantOutcomeIsNotFooter)
            return afterConsonantOutcome;
        break;
    }
    }

    const bool charsetSkipsPlainAscii =
        (m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING);
    const bool keyIsAsciiLetter = isalpha(entry.keyCode);
    const bool should_auto_complete = !autoCompleted && charsetSkipsPlainAscii && keyIsAsciiLetter;
    if (should_auto_complete)
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
    m_current++;
    WordInfo &entry = m_buffer[m_current];

    VnLexiName lowerSym = vnToLower(ev.vnSym);

    entry.vnSym = lowerSym;
    entry.caps = (lowerSym != ev.vnSym);
    entry.keyCode = ev.keyCode;
    entry.tone = 0;

    const bool atWordStart = (m_current == 0);
    const bool vietProcessingDisabled = !m_pCtrl->vietKey;
    const bool should_skip_mark = atWordStart || vietProcessingDisabled;
    if (should_skip_mark)
    {
        entry.form = vnw_c;
        entry.c1Offset = 0;
        entry.c2Offset = -1;
        entry.vOffset = -1;
        entry.cseq = lookupCSeq(lowerSym);
        const bool skipMarkUnlessUniCString =
            !m_pCtrl->vietKey || m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING;
        if (skipMarkUnlessUniCString)
            return 0;
        markChange(m_current);
        return 1;
    }

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
        return appendConsonantAfterVowelForm(ev, entry, prev, lowerSym);
    case vnw_c:
    case vnw_vc:
    case vnw_cvc:
        return appendConsonantAfterClusterForm(ev, entry, prev, lowerSym);
    }

    if (m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING)
        return 0;
    markChange(m_current);
    return 1;
}

int UkEngine::processAppendReset(UkKeyEvent &ev)
{
#if defined(_WIN32)
    if (ev.keyCode == ENTER_CHAR)
    {
        if (m_pCtrl->options.macroEnabled && macroMatch(ev))
            return 1;
    }
#else
    (void)ev;
#endif
    reset();
    return 0;
}

int UkEngine::processAppendNonVn(UkKeyEvent &ev)
{
    if (m_pCtrl->vietKey && m_pCtrl->charsetId == CONV_CHARSET_VIQR && checkEscapeVIQR(ev))
        return 1;

    m_current++;
    WordInfo &entry = m_buffer[m_current];
    entry.form = vnw_nonVn;
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

int UkEngine::processAppendVietnameseLetter(UkKeyEvent &ev)
{
    if (IsVnVowel[ev.vnSym])
    {
        VnLexiName v = (VnLexiName)StdVnNoTone[vnToLower(ev.vnSym)];
        const bool tailIsConsonant = m_current >= 0 && m_buffer[m_current].form == vnw_c;
        const bool followsQplusU = tailIsConsonant && m_buffer[m_current].cseq == cs_q && v == vnl_u;
        const bool followsGplusI = tailIsConsonant && m_buffer[m_current].cseq == cs_g && v == vnl_i;
        const bool treatVowelAsConsonantAfterQg = followsQplusU || followsGplusI;
        if (treatVowelAsConsonantAfterQg)
            return appendConsonnant(ev);
        return appendVowel(ev);
    }
    return appendConsonnant(ev);
}

/**
 * @brief Append a vowel symbol to the current Vietnamese composition buffer after a vowel or cv.
 *
 * @param ev key event describing the vowel input
 * @param entry current word info
 * @param prev previous word info
 * @param lowerSym lowercase Vietnamese symbol
 * @return 1 if the buffer was modified and output should be updated, 0 otherwise
 */
int UkEngine::appendVowelAfterVOrCv(UkKeyEvent &ev, WordInfo &entry, WordInfo &prev,
                                    VnLexiName lowerSym, VnLexiName canSym)
{
    (void)ev;
    VowelSeq vs = prev.vseq;
    const int prevTonePos =
        (m_current - 1) - (VSeqList[vs].len - 1) + getTonePosition(vs, true);
    int tone = m_buffer[prevTonePos].tone;

    VowelSeq newVs;
    const bool newKeyCarriesTone = lowerSym != canSym;
    const bool vowelChainAlreadyHasTone = tone != 0;
    const bool newKeyCarriesToneAndVowelChainAlreadyHasTone = newKeyCarriesTone && vowelChainAlreadyHasTone;
    if (newKeyCarriesToneAndVowelChainAlreadyHasTone)
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

    const bool haveCandidateSeq = newVs != vs_nil && prev.form == vnw_cv;
    if (haveCandidateSeq)
    {
        ConSeq cs = m_buffer[m_current - 1 - prev.c1Offset].cseq;
        const bool cvPatternLegal = isValidCV(cs, newVs);
        if (!cvPatternLegal)
            newVs = vs_nil;
    }

    const bool no_valid_sequence = newVs == vs_nil;
    if (no_valid_sequence)
    {
        entry.form = vnw_nonVn;
        entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
        return kAppendVowelContinueToFooter;
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

    const int newTone = (lowerSym - canSym) / 2;
    const bool tone_anchor_has_tone = tone != 0;
    if (tone_anchor_has_tone)
    {
        const bool placementAddsTone = newTone != 0;
        if (placementAddsTone)
        {
            tone = newTone;
            const int tonePos =
                getTonePosition(newVs, true) + ((m_current - 1) - VSeqList[vs].len + 1);
            markChange(tonePos);
            m_buffer[tonePos].tone = tone;
            return 1;
        }
    }
    else
    {
        const int newTonePos =
            getTonePosition(newVs, true) + ((m_current - 1) - VSeqList[vs].len + 1);
        const bool toneAnchorMoved = newTonePos != prevTonePos;
        if (toneAnchorMoved)
        {
            markChange(prevTonePos);
            m_buffer[prevTonePos].tone = 0;
            markChange(newTonePos);

            const bool tone_anchor_has_tone = newTone != 0;
            if (tone_anchor_has_tone)
                tone = newTone;
            m_buffer[newTonePos].tone = tone;
            return 1;
        }
        const bool replacesWithDifferentTone = newTone != 0 && newTone != tone;
        if (replacesWithDifferentTone)
        {
            tone = newTone;
            markChange(prevTonePos);
            m_buffer[prevTonePos].tone = tone;
            return 1;
        }
    }

    return kAppendVowelContinueToFooter;
}

/**
 * @brief Append a vowel symbol to the current Vietnamese composition buffer after a consonant.
 *
 * @param ev key event describing the vowel input
 * @param entry current word info
 * @param prev previous word info
 * @param lowerSym lowercase Vietnamese symbol
 * @return 1 if the buffer was modified and output should be updated, 0 otherwise
 */
int UkEngine::appendVowelAfterConsonantForm(UkKeyEvent &ev, WordInfo &entry, WordInfo &prev,
                                            VnLexiName lowerSym, VnLexiName canSym)
{
    (void)ev;
    (void)lowerSym;
    VowelSeq newVs = lookupVSeq(canSym);
    ConSeq cs = prev.cseq;
    if (!isValidCV(cs, newVs))
    {
        entry.form = vnw_nonVn;
        entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
        return kAppendVowelContinueToFooter;
    }

    entry.form = vnw_cv;
    entry.c1Offset = 1;
    entry.c2Offset = -1;
    entry.vOffset = 0;
    entry.vseq = newVs;

    const bool giPatternAcceptsTail = cs == cs_gi && prev.tone != 0;
    if (giPatternAcceptsTail)
    {
        if (entry.tone == 0)
            entry.tone = prev.tone;
        markChange(m_current - 1);
        prev.tone = 0;
        return 1;
    }

    return kAppendVowelContinueToFooter;
}

/**
 * @brief Append a consonant symbol to the current Vietnamese composition buffer after a vowel.
 *
 * @param ev key event describing the consonant input
 * @param entry current word info
 * @param prev previous word info
 * @param lowerSym lowercase Vietnamese symbol
 * @return 1 if the buffer was modified and output should be updated, 0 otherwise
 */
int UkEngine::appendConsonantAfterVowelForm(UkKeyEvent &ev, WordInfo &entry, WordInfo &prev,
                                            VnLexiName lowerSym)
{
    (void)ev;
    bool complexEvent = false;
    VowelSeq vs = prev.vseq;
    VowelSeq newVs = vs;

    bool is_uoh = vs == vs_uoh || vs == vs_uho;
    if (is_uoh)
        newVs = vs_uhoh;

    ConSeq c1 = cs_nil;
    if (prev.c1Offset != -1)
        c1 = m_buffer[m_current - 1 - prev.c1Offset].cseq;

    ConSeq newCs = lookupCSeq(lowerSym);
    const bool cvcPatternAcceptsTail = isValidCVC(c1, newVs, newCs);

    if (cvcPatternAcceptsTail)
    {
        const bool is_uho = vs == vs_uho;
        const bool is_uoh = vs == vs_uoh;   
        if (is_uho)
        {
            markChange(m_current - 1);
            prev.vnSym = vnl_oh;
            prev.vseq = vs_uhoh;
            complexEvent = true;
        }
        else if (is_uoh)
        {
            markChange(m_current - 2);
            m_buffer[m_current - 2].vnSym = vnl_uh;
            m_buffer[m_current - 2].vseq = vs_uh;
            prev.vseq = vs_uhoh;
            complexEvent = true;
        }

        const bool is_v = prev.form == vnw_v;
        if (is_v)
        {
            entry.form = vnw_vc;
            entry.c1Offset = -1;
            entry.c2Offset = 0;
            entry.vOffset = 1;
        }
        else
        {
            entry.form = vnw_cvc;
            entry.c1Offset = prev.c1Offset + 1;
            entry.c2Offset = 0;
            entry.vOffset = 1;
        }
        entry.cseq = newCs;

        const int oldIdx =
            (m_current - 1) - (VSeqList[vs].len - 1) + getTonePosition(vs, true);
        const bool tone_anchor_has_tone = m_buffer[oldIdx].tone != 0;
        if (tone_anchor_has_tone)
        {
            const int newIdx =
                (m_current - 1) - (VSeqList[newVs].len - 1) + getTonePosition(newVs, false);
            const bool tone_anchor_moved = newIdx != oldIdx;
            if (tone_anchor_moved)
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
        return 1;

    if (m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING)
        return 0;
    markChange(m_current);
    return 1;
}

/**
 * @brief Append a consonant symbol to the current Vietnamese composition buffer after a cluster.
 *
 * @param ev key event describing the consonant input
 * @param entry current word info
 * @param prev previous word info
 * @param lowerSym lowercase Vietnamese symbol
 * @return 1 if the buffer was modified and output should be updated, 0 otherwise
 */
int UkEngine::appendConsonantAfterClusterForm(UkKeyEvent &ev, WordInfo &entry, WordInfo &prev,
                                              VnLexiName lowerSym)
{
    (void)ev;
    ConSeq cs = prev.cseq;
    ConSeq newCs;

    const bool csHas3Letters = CSeqList[cs].len == 3;
    const bool csHas2Letters = CSeqList[cs].len == 2;
    if (csHas3Letters)
        newCs = cs_nil;
    else if (csHas2Letters)
        newCs = lookupCSeq(CSeqList[cs].c[0], CSeqList[cs].c[1], lowerSym);
    else
        newCs = lookupCSeq(CSeqList[cs].c[0], lowerSym);

    const bool haveCandidateSeq = newCs != cs_nil && (prev.form == vnw_vc || prev.form == vnw_cvc);
    if (haveCandidateSeq)
    {
        ConSeq c1 = cs_nil;
        const bool c1OffsetIsNotNegative = prev.c1Offset != -1;
        if (c1OffsetIsNotNegative)
            c1 = m_buffer[m_current - 1 - prev.c1Offset].cseq;

        const int vIdx = (m_current - 1) - prev.vOffset;
        VowelSeq vs = m_buffer[vIdx].vseq;
        if (!isValidCVC(c1, vs, newCs))
            newCs = cs_nil;
    }

    const bool no_valid_sequence = newCs == cs_nil;
    if (no_valid_sequence)
    {
        entry.form = vnw_nonVn;
        entry.c1Offset = entry.c2Offset = entry.vOffset = -1;
    }
    else
    {
        const bool is_c = prev.form == vnw_c;
        const bool is_vc = prev.form == vnw_vc;
        
        if (is_c)
        {
            entry.form = vnw_c;
            entry.c1Offset = 0;
            entry.c2Offset = -1;
            entry.vOffset = -1;
        }
        else if (is_vc)
        {
            entry.form = vnw_vc;
            entry.c1Offset = -1;
            entry.c2Offset = 0;
            entry.vOffset = prev.vOffset + 1;
        }
        else
        {
            entry.form = vnw_cvc;
            entry.c1Offset = prev.c1Offset + 1;
            entry.c2Offset = 0;
            entry.vOffset = prev.vOffset + 1;
        }
        entry.cseq = newCs;
    }

    const bool charsetSkipsPlainAscii = m_pCtrl->charsetId != CONV_CHARSET_UNI_CSTRING;
    if (charsetSkipsPlainAscii)
        return 0;
    markChange(m_current);
    return 1;
}
