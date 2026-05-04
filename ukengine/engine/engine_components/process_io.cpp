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
