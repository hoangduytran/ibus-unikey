// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/*------------------------------------------------------------------------------
UniKey - Open-source Vietnamese Keyboard
Copyright (C) 2000-2005 Pham Kim Long
Contact:
  unikey@gmail.com
  http://unikey.org

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
--------------------------------------------------------------------------------*/

#ifndef __UNIKEY_H
#define __UNIKEY_H

/**
 * @file unikey.h
 * @brief C interface for UniKey Vietnamese input engine API.
 *
 * Provides flat C bindings for setup, keyboard event filtering and configuration.
 * Designed for use by platform integration layers.
 */

#include "keycons.h"

/*----------------------------------------------------
Initialization steps:
   1. UnikeySetup: This will initialized Unikey module,
      with default options, input method = TELEX, output format = UTF-8
   2. If you want a different settings:
     + Call UnikeySetInputMethod to change input method
     + Call UnikeySetOutputVIQR/UTF8 to chang output format
     + Call UnikeySetOptions to change extra options

Key event handling:

- Call UnikeyFilter when a key event occurs, examine results in
    + UnikeyBackspaces: number of backspaces that need to be sent
    + UnikeyBufChars: number of chars in buffer that need to be sent
    + UnikeyAnsiBuf: buffer containing output characters.
    + UnikeyUniBuf: not used

  You should also call UnikeySetCapsState() before calling UnikeyFilter.

  To make this module portable across platforms, UnikeyFilter should not
  be called on special keys: Enter, Tab, movement keys, delete, backspace...

- Special events:
    + Call UnikeyResetBuf to reset the engine's state in situations such as:
      focus lost, movement keys: arrow keys, pgup, pgdown....
    + If a backspace is received, call UnikeyBackspacePress,
      then examine the result:
      UnikeyBackspaces is the number of backspaces actually required to
      remove one character.

Clean up:
- When the Engine is no longer needed, call UnikeyCleanup
------------------------------------------------------*/

#if defined(__cplusplus)
extern "C" {
#endif
    /**
     * @brief Transient output buffer for filtered characters.
     *
     * Populated by UnikeyFilter and consumed by host app.
     */
    extern unsigned char UnikeyBuf[];

    /**
     * @brief Number of backspaces to emit after filtering.
     */
    extern int UnikeyBackspaces;

    /**
     * @brief Number of characters in UnikeyBuf currently.
     */
    extern int UnikeyBufChars;

    /**
     * @brief Current output type (charset/flags) after filtering.
     */
    extern UkOutputType UnikeyOutput;

    /**
     * @brief Initialize the UniKey engine subsystem.
     *
     * Must be called prior to any other API function.
     */
    void UnikeySetup();

    /**
     * @brief Clean up engine resources prior to module unload.
     */
    void UnikeyCleanup();

    /**
     * @brief Reset input context state (e.g., on focus change).
     */
    void UnikeyResetBuf();

    /**
     * @brief Main per-key handler; call on every character input.
     *
     * @param ch Unicode codepoint or virtual key to process.
     */
    void UnikeyFilter(unsigned int ch);

    /**
     * @brief Feed a raw character into engine without transformation.
     */
    void UnikeyPutChar(unsigned int ch);

    /**
     * @brief Update keyboard modifier state before filtering.
     *
     * @param shiftPressed nonzero for shift held
     * @param CapsLockOn nonzero if caps lock active
     */
    void UnikeySetCapsState(int shiftPressed, int CapsLockOn);

    /**
     * @brief Process backspace key event to modify engine buffer.
     */
    void UnikeyBackspacePress();

    /**
     * @brief Restore input to original keystrokes after cancellation.
     */
    void UnikeyRestoreKeyStrokes();

    /**
     * @brief Apply new engine options.
     */
    void UnikeySetOptions(UnikeyOptions *pOpt);

    /**
     * @brief Initialize a options struct with library defaults.
     */
    void CreateDefaultUnikeyOptions(UnikeyOptions *pOpt);

    /**
     * @brief Read current engine options into caller struct.
     */
    void UnikeyGetOptions(UnikeyOptions *pOpt);

    /**
     * @brief Select active input method (Telex/VNI/ViQR/etc.).
     */
    void UnikeySetInputMethod(UkInputMethod im);

    /**
     * @brief Select output charset encoding.
     */
    int UnikeySetOutputCharset(int charset);

    /**
     * @brief Load macro definitions from file.
     * @return 0 on success or nonzero on failure.
     */
    int UnikeyLoadMacroTable(const char *fileName);

    /**
     * @brief Load user key mapping table from file.
     * @return 0 on success or nonzero on failure.
     */
    int UnikeyLoadUserKeyMap(const char *fileName);

    /**
     * @brief Enable non-VN sequence typing mode.
     *
     * Maintains Vietnamese composition across non-VN characters until word-break.
     */
    void UnikeySetSingleMode();

    /**
     * @brief Check whether engine is at start of a word.
     */
    bool UnikeyAtWordBeginning();
#if defined(__cplusplus)
}
#endif

#endif
