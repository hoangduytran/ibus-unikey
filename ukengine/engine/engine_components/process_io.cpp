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

#include "engine_components/engine_tables_shared.h"
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

int UkEngine::processThroughEscapeGate(UkKeyEvent &ev)
{
    if (!m_toEscape)
        return (this->*UkKeyProcList[ev.evType])(ev);

    m_toEscape = false;
    const bool missingComposition = m_current < 0;
    const bool escapeOrNormalTyping =
        ev.evType == vneNormal || ev.evType == vneEscChar;
    if (missingComposition || escapeOrNormalTyping)
        return processAppend(ev);

    m_current--;
    processAppend(ev);
    markChange(m_current);
    return 1;
}

void UkEngine::processApplyNoSpellCheckFallback(UkKeyEvent &ev, int &ret)
{
    const bool vietModeEnabled = m_pCtrl->vietKey;
    const bool hasActiveSlot = m_current >= 0;
    const bool compositionTailIsNonVn =
        hasActiveSlot && m_buffer[m_current].form == vnw_nonVn;
    const bool incomingIsVietnameseLetter = ev.chType == ukcVn;
    const bool spellCheckInactiveOrSingleWordMode =
        !m_pCtrl->options.spellCheckEnabled || m_singleMode;

    if (!vietModeEnabled || !compositionTailIsNonVn || !incomingIsVietnameseLetter ||
        !spellCheckInactiveOrSingleWordMode)
        return;

    ret = processNoSpellCheck(ev);
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

    int ret = processThroughEscapeGate(ev);

    processApplyNoSpellCheckFallback(ev, ret);

    // we add key to key buffer only if that key has not caused a reset
    const bool hasActiveSlot = m_current >= 0;
    if (hasActiveSlot)
    {
        ev.chType = m_pCtrl->input.getCharType(ev.keyCode);
        m_keyCurrent++;
        m_keyStrokes[m_keyCurrent].ev = ev;
        m_keyStrokes[m_keyCurrent].converted = (ret && !m_keyRestored);
    }

    const bool noOutput = ret == 0;
    if (noOutput)
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
        const bool isNonVnChar = m_buffer[i].vnSym != vnl_nonVnChar;
        if (isNonVnChar)
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

        const bool stdCharIsValid = stdChar != INVALID_STD_CHAR;
        if (stdCharIsValid)
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

    const bool lastIsBeforeFirst = last < first;
    if (lastIsBeforeFirst)
        return 0;

    const bool charsetSupportsTone = m_pCtrl->charsetId == CONV_CHARSET_XUTF8 || m_pCtrl->charsetId == CONV_CHARSET_UNICODE;
    if (charsetSupportsTone)
        return (last - first + 1);

    StringBOStream os(0, 0);
    int i, bytesWritten;

    VnCharset *pCharset = VnCharsetLibObj.getVnCharset(m_pCtrl->charsetId);
    pCharset->startOutput();

    for (i = first; i <= last; i++)
    {
        const bool isNonVnChar = m_buffer[i].vnSym != vnl_nonVnChar;
        if (isNonVnChar)
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

        const bool stdCharIsValid = stdChar != INVALID_STD_CHAR;
        if (stdCharIsValid)
            pCharset->putChar(os, stdChar, bytesWritten);
    }

    int len = os.getOutBytes();
    const bool charsetUnidecomposed = m_pCtrl->charsetId == CONV_CHARSET_UNIDECOMPOSED;
    if (charsetUnidecomposed)
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
    const bool posIsBeforeChangePos = pos < m_changePos;
    if (posIsBeforeChangePos)
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
    const bool hasActiveKeyStroke = m_keyCurrent >= 0;
    if (hasActiveKeyStroke)
        m_keyCurrent--;
    const bool hasActiveSlot = m_current >= 0;
    const bool slotIsEmpty = m_buffer[m_current].form == vnw_empty;
    const bool slotIsEmptyAndHasActiveKeyStroke = hasActiveSlot && slotIsEmpty;
    if (slotIsEmptyAndHasActiveKeyStroke)
    {
        // Character buffer sits on an empty slot after a word break; align the
        // keystroke cursor with the nearest preceding ukcWordBreak (or -1).
        while (m_keyCurrent >= 0)
        {
            const UkKeyEvent &stroke = m_keyStrokes[m_keyCurrent].ev;
            const bool strokeEndsWord = stroke.chType == ukcWordBreak;
            if (strokeEndsWord)
                break;
            m_keyCurrent--;
        }
    }
}

//---------------------------------------------
/**
 * @brief Process a backspace event and update output/backspace counts.
 *
 * **Progression:**
 * 1. Early-out when Vietnamese processing is off or buffer empty.
 * 2. `backspaceAtSimpleBoundary` — drop cursor without relocating tone marks.
 * 3. Otherwise compute vowel span / tone indices; relocate tone if `backspaceShouldRelocateTone`.
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

    const bool vietDisabledOrNoComposition = !m_pCtrl->vietKey || m_current < 0;
    if (vietDisabledOrNoComposition)
    {
        backs = 0;
        outSize = 0;
        return 0;
    }

    m_backs = 0;
    m_changePos = m_current + 1;
    markChange(m_current);

    if (backspaceAtSimpleBoundary())
        return backspaceCommitDecrementAndSync(backs, outSize);

    const int vowelClusterEndIndex = m_current - m_buffer[m_current].vOffset;
    const VowelSeq vowelSeqAtCluster = m_buffer[vowelClusterEndIndex].vseq;
    const int vowelSpanStartIndex =
        vowelClusterEndIndex - VSeqList[vowelSeqAtCluster].len + 1;
    const VowelSeq vowelSeqAfterCursorStep = m_buffer[m_current - 1].vseq;
    const bool vowelClusterEndsAtCurrent = vowelClusterEndIndex == m_current;
    const int toneIndexBeforeStep =
        vowelSpanStartIndex +
        getTonePosition(vowelSeqAtCluster, vowelClusterEndsAtCurrent);
    const int toneIndexAfterCursorStep =
        vowelSpanStartIndex + getTonePosition(vowelSeqAfterCursorStep, true);
    const int toneMark = m_buffer[toneIndexBeforeStep].tone;

    if (!backspaceShouldRelocateTone(toneMark, toneIndexBeforeStep, toneIndexAfterCursorStep))
        return backspaceCommitDecrementAndSync(backs, outSize);

    markChange(toneIndexAfterCursorStep);
    m_buffer[toneIndexAfterCursorStep].tone = toneMark;
    markChange(toneIndexBeforeStep);
    m_buffer[toneIndexBeforeStep].tone = 0;
    m_current--;
    synchKeyStrokeBuffer();
    backs = m_backs;
    writeOutput(outBuf, outSize);
    return 1;
}

bool UkEngine::backspaceAtSimpleBoundary() const
{
    if (m_current == 0)
        return true;

    const WordInfo &current = m_buffer[m_current];
    const WordInfo &previous = m_buffer[m_current - 1];

    const bool currentIsBareOrBreak =
        current.form == vnw_empty || current.form == vnw_nonVn || current.form == vnw_c;
    const bool previousEndsWithConsonantTail =
        previous.form == vnw_c || previous.form == vnw_cvc || previous.form == vnw_vc;

    return currentIsBareOrBreak || previousEndsWithConsonantTail;
}

bool UkEngine::backspaceShouldRelocateTone(int tone, int curTonePos, int newTonePos) const
{
    const bool hasTone = tone != 0;
    if (!hasTone)
        return false;

    const bool toneSlotWouldChange = curTonePos != newTonePos;
    if (!toneSlotWouldChange)
        return false;

    const bool toneLivesOnTrailingKeyBeingRemoved =
        curTonePos == m_current && m_buffer[m_current].tone != 0;
    if (toneLivesOnTrailingKeyBeingRemoved)
        return false;

    return true;
}

int UkEngine::backspaceCommitDecrementAndSync(int &backs, int &outSize)
{
    m_current--;
    backs = m_backs;
    outSize = 0;
    synchKeyStrokeBuffer();
    const bool restoredMultiCodeUnitSpan = backs > 1;
    return restoredMultiCodeUnitSpan ? 1 : 0;
}
