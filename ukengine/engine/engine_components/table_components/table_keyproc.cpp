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
#include "engine_components/engine_tables_shared.h"

/**
 * @brief Table mapping input method actions to UkEngine key event handler functions.
 *
 * UkKeyProcList[] is an array of member function pointers (UkKeyProc) indexed by
 * input method action enums (e.g., vneRoofAll, vneTone0, etc.). Each entry points to
 * the corresponding handler in UkEngine for processing a specific type of key event.
 *
 * Usage:
 *   - Used to dispatch the correct handler for a given input action:
 *       UkKeyProc proc = UkKeyProcList[action];
 *       (engine->*proc)(event);
 *   - Enables modular and efficient mapping from input method logic to implementation.
 *
 * Example mapping:
 *   - vneRoofAll: &UkEngine::processRoof (e.g., "a" → "â")
 *   - vneTone0: &UkEngine::processTone (e.g., "a" → "à")
 *   - vneDd: &UkEngine::processDd (e.g., "d" → "đ")
 *   - vne_telex_w: &UkEngine::processTelexW (e.g., "aw" → "ă")
 *   - vneMapChar: &UkEngine::processMapChar (custom mappings)
 */
UkKeyProc UkKeyProcList[vneCount] = {
    &UkEngine::processRoof,    // vneRoofAll      (e.g., "a" → "â", "e" → "ê")
    &UkEngine::processRoof,    // vneRoof_a       (e.g., "a" → "â")
    &UkEngine::processRoof,    // vneRoof_e       (e.g., "e" → "ê")
    &UkEngine::processRoof,    // vneRoof_o       (e.g., "o" → "ô")
    &UkEngine::processHook,    // vneHookAll      (e.g., "o" → "ơ", "u" → "ư")
    &UkEngine::processHook,    // vneHook_uo      (e.g., "uo" → "ươ")
    &UkEngine::processHook,    // vneHook_u       (e.g., "u" → "ư")
    &UkEngine::processHook,    // vneHook_o       (e.g., "o" → "ơ")
    &UkEngine::processHook,    // vneBowl         (e.g., "a" → "ă")
    &UkEngine::processDd,      // vneDd           (e.g., "d" → "đ")
    &UkEngine::processTone,    // vneTone0        (e.g., "a" → "à")
    &UkEngine::processTone,    // vneTone1        (e.g., "a" → "á")
    &UkEngine::processTone,    // vneTone2        (e.g., "a" → "ả")
    &UkEngine::processTone,    // vneTone3        (e.g., "a" → "ã")
    &UkEngine::processTone,    // vneTone4        (e.g., "a" → "ạ")
    &UkEngine::processTone,    // vneTone5        (e.g., "a" → "ạ")
    &UkEngine::processTelexW,  // vne_telex_w     (e.g., "aw" → "ă", "uw" → "ư")
    &UkEngine::processMapChar, // vneMapChar      (custom mappings, e.g., "z" → "d")
    &UkEngine::processEscChar, // vneEscChar      (escape sequences)
    &UkEngine::processAppend   // vneNormal       (default: append character)
};
bool UkEngine::m_classInit = false;
