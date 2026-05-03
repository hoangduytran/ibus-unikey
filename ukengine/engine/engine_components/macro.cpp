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

#include "keycons.h"
#include "engine_components/engine_internal.h"
#include "ukengine.h"
#include "vnconv.h"

enum VnCaseType
{
    VnCaseNoChange,
    VnCaseAllCapital,
    VnCaseAllSmall
};

//----------------------------------------------------
/**
 * @brief Attempt macro expansion for the current input sequence.
 *
 * @param ev key event triggering macro lookup
 * @return 1 when a macro matched and expanded, 0 otherwise
 */
int UkEngine::macroMatch(UkKeyEvent &ev)
{
    int capsLockOn = 0;
    int shiftPressed = 0;
    if (m_keyCheckFunc)
        m_keyCheckFunc(&shiftPressed, &capsLockOn);

    if (shiftPressed && (ev.keyCode == ' ' || ev.keyCode == ENTER_CHAR))
        return 0;

    const StdVnChar *pMacText = NULL;
    m_macroKeyScratch.resize((size_t)MACRO_MATCH_MAX_KEY_UNITS + 2u);
    StdVnChar *const key = m_macroKeyScratch.data();
    StdVnChar *pKeyStart;

    int i, j;

    i = m_current;
    while (i >= 0 && (m_current - i + 1) < MACRO_MATCH_MAX_KEY_UNITS)
    {
        while (i >= 0 && m_buffer[i].form != vnw_empty && (m_current - i + 1) < MACRO_MATCH_MAX_KEY_UNITS)
            i--;
        if (i >= 0 && m_buffer[i].form != vnw_empty)
            return 0;

        if (i >= 0)
        {
            if (m_buffer[i].vnSym != vnl_nonVnChar)
            {
                key[0] = m_buffer[i].vnSym + VnStdCharOffset;
                if (m_buffer[i].caps)
                    key[0]--;
                key[0] += m_buffer[i].tone * 2;
            }
            else
                key[0] = m_buffer[i].keyCode;
        }

        for (j = i + 1; j <= m_current; j++)
        {
            if (m_buffer[j].vnSym != vnl_nonVnChar)
            {
                key[j - i] = m_buffer[j].vnSym + VnStdCharOffset;
                if (m_buffer[j].caps)
                    key[j - i]--;
                key[j - i] += m_buffer[j].tone * 2;
            }
            else
                key[j - i] = m_buffer[j].keyCode;
        }
        key[m_current - i + 1] = 0;
        // search macro table
        pMacText = m_pCtrl->macStore.lookup(key + 1);
        if (pMacText)
        {
            i++; // mark the position where change is needed
            pKeyStart = key + 1;
            break;
        }
        if (i >= 0)
        {
            pMacText = m_pCtrl->macStore.lookup(key);
            if (pMacText)
            {
                pKeyStart = key;
                break;
            }
        }
        i--;
    }

    if (!pMacText)
    {
        return 0;
    }

    markChange(i);

    // determine the form of macro replacements: ALL CAPITALS, First Character Capital, or no change
    VnCaseType macroCase;
    if (IS_STD_VN_LOWER(*pKeyStart))
    {
        macroCase = VnCaseAllSmall;
    }
    else if (IS_STD_VN_UPPER(*pKeyStart))
    {
        macroCase = VnCaseAllCapital;
        for (i = 1; pKeyStart[i]; i++)
        {
            if (IS_STD_VN_LOWER(pKeyStart[i]))
            {
                macroCase = VnCaseNoChange;
            }
        }
    }
    else
        macroCase = VnCaseNoChange;

    // Convert case of macro text according to macroCase
    int charCount = 0;
    while (pMacText[charCount] != 0)
        charCount++;

    m_macroTextScratch.resize((size_t)charCount + 1u);
    StdVnChar *const macroText = m_macroTextScratch.data();

    for (i = 0; i < charCount; i++)
    {
        if (macroCase == VnCaseAllCapital)
            macroText[i] = StdVnToUpper(pMacText[i]);
        else if (macroCase == VnCaseAllSmall)
            macroText[i] = StdVnToLower(pMacText[i]);
        else
            macroText[i] = pMacText[i];
    }
    macroText[charCount] = 0;

    // Convert to target output charset
    int outSize;
    int maxOutSize = *m_pOutSize;
    int inLen = charCount * sizeof(StdVnChar);
    VnConvert(CONV_CHARSET_VNSTANDARD, m_pCtrl->charsetId,
              (UKBYTE *)macroText, (UKBYTE *)m_pOutBuf,
              &inLen, &maxOutSize);
    outSize = maxOutSize;

    // write the last input character
    StdVnChar vnChar;
    if (outSize < *m_pOutSize)
    {
        maxOutSize = *m_pOutSize - outSize;
        if (ev.vnSym != vnl_nonVnChar)
            vnChar = ev.vnSym + VnStdCharOffset;
        else
            vnChar = ev.keyCode;
        inLen = sizeof(StdVnChar);
        VnConvert(CONV_CHARSET_VNSTANDARD, m_pCtrl->charsetId,
                  (UKBYTE *)&vnChar, ((UKBYTE *)m_pOutBuf) + outSize,
                  &inLen, &maxOutSize);
        outSize += maxOutSize;
    }
    int backs = m_backs; // store m_backs before calling reset
    reset();
    m_outputWritten = true;
    m_backs = backs;
    *m_pOutSize = outSize;
    return 1;
}
