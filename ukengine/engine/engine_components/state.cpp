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
#include <string.h>
#if defined(_WIN32)
#include "keyhook.h"
#endif

#include "keycons.h"
#include "engine_components/engine_tables_shared.h"
//------------------------------------------------
/**
 * @brief Reset the engine composition state to an empty starting point.
 */
void UkEngine::reset()
{
    m_current = -1;
    m_keyCurrent = -1;
    m_singleMode = false;
    m_toEscape = false;
}

//------------------------------------------------
/**
 * @brief Reset internal keystroke history without clearing current composition.
 */
void UkEngine::resetKeyBuf()
{
    m_keyCurrent = -1;
}

//------------------------------------------------
/**
 * @brief Construct a new UkEngine instance and initialize static tables.
 */
UkEngine::UkEngine()
{
    if (!m_classInit)
    {
        engineClassInit();
        m_classInit = true;
    }
    m_pCtrl = 0;
    m_bufSize = MAX_UK_ENGINE;
    m_keyBufSize = MAX_UK_ENGINE;
    m_current = -1;
    m_keyCurrent = -1;
    m_singleMode = false;
    m_keyCheckFunc = 0;
    m_reverted = false;
    m_toEscape = false;
    m_keyRestored = false;
    m_macroKeyScratch.reserve((size_t)MAX_UK_ENGINE + 4u);
}

//----------------------------------------------------
// make sure there are at least 10 entries available
//----------------------------------------------------
/**
 * @brief Ensure there is enough remaining space in the internal buffers.
 *
 * Trims older entries when necessary to keep at least ten slots available.
 */
void UkEngine::prepareBuffer()
{
    int rid;
    // prepare symbol buffer
    if (m_current >= 0 && m_current + 10 >= m_bufSize)
    {
        // Get rid of at least half of the current entries
        // don't get rid from the middle of a word.
        for (rid = m_current / 2; m_buffer[rid].form != vnw_empty && rid < m_current; rid++)
            ;
        if (rid == m_current)
        {
            m_current = -1;
        }
        else
        {
            rid++;
            memmove(m_buffer, m_buffer + rid, (m_current - rid + 1) * sizeof(WordInfo));
            m_current -= rid;
        }
    }

    // prepare key stroke buffer
    if (m_keyCurrent > 0 && m_keyCurrent + 1 >= m_keyBufSize)
    {
        // Get rid of at least half of the current entries
        rid = m_keyCurrent / 2;
        memmove(m_keyStrokes, m_keyStrokes + rid, (m_keyCurrent - rid + 1) * sizeof(m_keyStrokes[0]));
        m_keyCurrent -= rid;
    }
}
