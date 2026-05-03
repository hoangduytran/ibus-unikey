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

#include "ukengine.h"
//----------------------------------------------------
/**
 * @brief Restore original keystrokes after a failed Vietnamese conversion.
 *
 * @param backs[out] number of backspaces to emit
 * @param outBuf buffer to receive restored key text
 * @param outSize[in,out] buffer capacity on entry and bytes written on exit
 * @param outType[out] output type describing produced content
 * @return 1 when restoration was performed, 0 otherwise
 */
int UkEngine::restoreKeyStrokes(int &backs, unsigned char *outBuf, int &outSize, UkOutputType &outType)
{
    outType = UkKeyOutput;
    if (!lastWordHasVnMark())
    {
        backs = 0;
        outSize = 0;
        return 0;
    }

    m_backs = 0;
    m_changePos = m_current + 1;

    int keyStart;
    bool converted = false;
    for (keyStart = m_keyCurrent; keyStart >= 0 && m_keyStrokes[keyStart].ev.chType != ukcWordBreak; keyStart--)
    {
        if (m_keyStrokes[keyStart].converted)
        {
            converted = true;
        }
    }
    keyStart++;

    if (!converted)
    {
        // no key stroke has been converted, so it doesn't make sense to restore key strokes
        backs = 0;
        outSize = 0;
        return 0;
    }

    // int i = m_current;
    while (m_current >= 0 && m_buffer[m_current].form != vnw_empty)
        m_current--;
    markChange(m_current + 1);
    backs = m_backs;

    int count;
    int i;
    UkKeyEvent ev;
    m_keyRestoring = true;
    for (i = keyStart, count = 0; i <= m_keyCurrent; i++)
    {
        if (count < outSize)
        {
            outBuf[count++] = (unsigned char)m_keyStrokes[i].ev.keyCode;
        }
        m_pCtrl->input.keyCodeToSymbol(m_keyStrokes[i].ev.keyCode, ev);
        m_keyStrokes[i].converted = false;
        processAppend(ev);
    }
    outSize = count;
    m_keyRestoring = false;

    return 1;
}

//--------------------------------------------------
/**
 * @brief Enable single-mode input where Vietnamese conversion can work inside
 *        non-VN text segments.
 */
void UkEngine::setSingleMode()
{
    m_singleMode = true;
}
