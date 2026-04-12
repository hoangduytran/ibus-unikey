// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/*------------------------------------------------------------------------------
UniKey - Open-source Vietnamese Keyboard
Copyright (C) 1998-2004 Pham Kim Long
Contact:
  longcz@yahoo.com
  http://unikey.sf.net

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

#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <iostream>
#include "unikey.h"
#include "ukengine.h"
#include "usrkeymap.h"

using namespace std;

//---- exported variables for use in UkEngine class ----
/**
 * @brief Pointer to shared engine state used across the UniKey API.
 *
 * This is allocated in UnikeySetup() and provides access to engine-wide
 * configuration, input mapping and macro store. External code can read
 * or modify fields inside `pShMem` to change runtime behaviour. Do not
 * delete this pointer directly; call UnikeyCleanup() instead.
 *
 * Example:
 *   UnikeySetup();
 *   pShMem->vietKey = 1; // enable Vietnamese mode
 */
UkSharedMem *pShMem = 0;

/**
 * @brief Single global instance of the keyboard engine.
 *
 * Provides the main processing API used by the C wrapper functions below.
 * Methods like `process`, `reset`, `setCtrlInfo` are invoked through
 * this object.
 *
 * Example:
 *   MyKbEngine.process(ch, backspaces, buf, bufChars, output);
 */
UkEngine MyKbEngine;

/**
 * @brief State of CapsLock reported to UniKey.
 *
 * Updated via `UnikeySetCapsState()` by the embedding application.
 */
int UnikeyCapsLockOn = 0;

/**
 * @brief State of Shift (pressed) as reported to UniKey.
 *
 * Updated via `UnikeySetCapsState()` by the embedding application.
 */
int UnikeyShiftPressed = 0;
//----------------------------------------------------

unsigned char UnikeyBuf[1024];
int UnikeyBackspaces;
int UnikeyBufChars;
UkOutputType UnikeyOutput;

//--------------------------------------------
void UnikeySetInputMethod(UkInputMethod im)
{
  if (im == UkTelex || im == UkVni || im == UkSimpleTelex || im == UkSimpleTelex2) {
    pShMem->input.setIM(im);
    MyKbEngine.reset();
  }
  else if (im == UkUsrIM && pShMem->usrKeyMapLoaded) {
    //cout << "Switched to user mode\n"; //DEBUG
    pShMem->input.setIM(pShMem->usrKeyMap);
    MyKbEngine.reset();
  }

  //cout << "IM changed to: " << im << endl; //DEBUG
}

/**
 * @brief Set the active input method used by UniKey.
 *
 * @param im Input method enum (e.g., `UkTelex`, `UkVni`, `UkSimpleTelex`, `UkUsrIM`).
 *
 * Updates the shared input mapping stored in `pShMem->input` and resets
 * the engine so the new mapping takes effect. When switching to the
 * user-defined mapping (`UkUsrIM`) this function only applies it if the
 * user key map has been loaded.
 *
 * Example:
 *   UnikeySetInputMethod(UkVni);
 */


//--------------------------------------------
void UnikeySetCapsState(int shiftPressed, int CapsLockOn)
{
  //UnikeyCapsAll = (shiftPressed && !CapsLockOn) || (!shiftPressed && CapsLockOn);
  UnikeyCapsLockOn = CapsLockOn;
  UnikeyShiftPressed = shiftPressed;
}

/**
 * @brief Update the keyboard modifier state reported to UniKey.
 *
 * @param shiftPressed Non-zero when Shift is currently pressed.
 * @param CapsLockOn Non-zero when CapsLock is active.
 *
 * The engine uses these values for case-sensitive decisions when processing
 * input. Call this from the host application whenever modifier state changes.
 *
 * Example:
 *   UnikeySetCapsState(1, 0); // Shift pressed, CapsLock off
 */

//--------------------------------------------
int UnikeySetOutputCharset(int charset)
{
    pShMem->charsetId = charset;
    MyKbEngine.reset();
    return 1;
}

/**
 * @brief Set the output character encoding used by UniKey.
 *
 * @param charset Charset identifier (e.g., `CONV_CHARSET_XUTF8`).
 * @return 1 on success.
 *
 * The engine will be reset so subsequent output is encoded using the
 * requested charset. Call this before sending processed characters to
 * external consumers if you need a specific encoding.
 *
 * Example:
 *   UnikeySetOutputCharset(CONV_CHARSET_XUTF8);
 */

//--------------------------------------------
void UnikeySetOptions(UnikeyOptions *pOpt)
{
  pShMem->options.freeMarking = pOpt->freeMarking;
  pShMem->options.modernStyle = pOpt->modernStyle;
  pShMem->options.macroEnabled = pOpt->macroEnabled;
  pShMem->options.useUnicodeClipboard = pOpt->useUnicodeClipboard;
  pShMem->options.alwaysMacro = pOpt->alwaysMacro;
  pShMem->options.spellCheckEnabled = pOpt->spellCheckEnabled;
  pShMem->options.autoNonVnRestore = pOpt->autoNonVnRestore;
}

/**
 * @brief Apply runtime options to the UniKey engine.
 *
 * @param pOpt Pointer to a `UnikeyOptions` struct with desired settings.
 *
 * Copies user-visible options into the shared memory options structure.
 * Example: toggle free-marking or macro behaviour.
 *
 * Example:
 *   UnikeyOptions opt = {1,0,1,0,0,1,1};
 *   UnikeySetOptions(&opt);
 */

//--------------------------------------------
void UnikeyGetOptions(UnikeyOptions *pOpt)
{
  *pOpt = pShMem->options;
}

/**
 * @brief Retrieve current UniKey runtime options.
 *
 * @param pOpt Pointer to a `UnikeyOptions` structure that will be filled.
 *
 * Copies the engine's current options into the caller-provided struct.
 */

//--------------------------------------------
void CreateDefaultUnikeyOptions(UnikeyOptions *pOpt)
{
  pOpt->freeMarking = 1;
  pOpt->modernStyle = 0;
  pOpt->macroEnabled = 1;
  pOpt->useUnicodeClipboard = 0;
  pOpt->alwaysMacro = 0;
  pOpt->spellCheckEnabled = 1;
  pOpt->autoNonVnRestore = 1;
}

/**
 * @brief Initialize a `UnikeyOptions` struct with sensible defaults.
 *
 * @param pOpt Pointer to the struct to initialize.
 *
 * Useful when creating new shared-memory option blocks or resetting
 * configuration to defaults.
 */

//--------------------------------------------
void UnikeyCheckKbCase(int *pShiftPressed, int *pCapsLockOn)
{
  *pShiftPressed = UnikeyShiftPressed;
  *pCapsLockOn = UnikeyCapsLockOn;
}

/**
 * @brief Query the current keyboard modifier state known to UniKey.
 *
 * @param pShiftPressed Out parameter set to non-zero when Shift is pressed.
 * @param pCapsLockOn Out parameter set to non-zero when CapsLock is active.
 *
 * This is the function the engine exposes to internal consumers to obtain
 * modifier state; it is set on `MyKbEngine` via `setCheckKbCaseFunc`.
 *
 * Example:
 *   int shift, caps;
 *   UnikeyCheckKbCase(&shift, &caps);
 */

//--------------------------------------------
void UnikeySetup()
{
    SetupUnikeyEngine();
    pShMem = new UkSharedMem;
    pShMem->input.init();
    pShMem->macStore.init();
    pShMem->vietKey = 1;
    pShMem->usrKeyMapLoaded = 0;
    MyKbEngine.setCtrlInfo(pShMem);
    MyKbEngine.setCheckKbCaseFunc(&UnikeyCheckKbCase);
    UnikeySetInputMethod(UkTelex);
    UnikeySetOutputCharset(CONV_CHARSET_XUTF8);
    pShMem->initialized = 1;
    CreateDefaultUnikeyOptions(&pShMem->options);
}

/**
 * @brief Initialize UniKey runtime and allocate shared structures.
 *
 * This sets up the engine internals, allocates `pShMem`, initializes
 * input mapping and macro store, and wires the engine callbacks. Call
 * this before invoking other UniKey APIs.
 *
 * Example:
 *   UnikeySetup();
 */

//--------------------------------------------
void UnikeyCleanup()
{
  delete pShMem;
}

/**
 * @brief Clean up UniKey runtime and free shared structures.
 *
 * Frees `pShMem`. After calling this, UniKey APIs should not be used
 * until `UnikeySetup()` is called again.
 *
 * Example:
 *   UnikeyCleanup();
 */

//--------------------------------------------
void UnikeyFilter(unsigned int ch)
{
  UnikeyBufChars = sizeof(UnikeyBuf);
  MyKbEngine.process(ch, UnikeyBackspaces, UnikeyBuf, UnikeyBufChars, UnikeyOutput);
}

/**
 * @brief Process an input character through the UniKey engine.
 *
 * @param ch Unicode codepoint or input key code to filter/process.
 *
 * This is the primary API for filtering keyed characters. The engine may
 * modify `UnikeyBuf` and `UnikeyBackspaces`, and will set `UnikeyOutput`
 * to describe the produced output.
 *
 * Example:
 *   UnikeyFilter('a');
 */

//--------------------------------------------
void UnikeyPutChar(unsigned int ch)
{
  MyKbEngine.pass(ch);
  UnikeyBufChars = 0;
  UnikeyBackspaces = 0;
}

/**
 * @brief Pass a character directly to the engine without filtering.
 *
 * @param ch Unicode codepoint to forward.
 *
 * Use when you want to inject characters directly into the engine output
 * pipeline (for example when the host already decided the character is final).
 */

//--------------------------------------------
void UnikeyResetBuf()
{
  MyKbEngine.reset();
}

/**
 * @brief Reset the engine's input buffer and internal state.
 *
 * Typically used when the host needs to abandon the current composition
 * (for example on focus loss or when switching contexts).
 */

//--------------------------------------------
void UnikeySetSingleMode()
{
  MyKbEngine.setSingleMode();
}

/**
 * @brief Switch engine to single-character mode.
 *
 * In single mode the engine treats input as isolated characters rather
 * than composing multi-character sequences. Useful for special host
 * behaviour where composition is undesired.
 */

//--------------------------------------------
void UnikeyBackspacePress()
{
  UnikeyBufChars = sizeof(UnikeyBuf);
  MyKbEngine.processBackspace(UnikeyBackspaces, UnikeyBuf, UnikeyBufChars, UnikeyOutput);
  //  printf("Backspaces: %d\n",UnikeyBackspaces);
}

/**
 * @brief Notify the engine that the host processed a backspace key.
 *
 * This function allows the engine to update composition state to reflect
 * backspace operations made by the host application. `UnikeyBackspaces`
 * and `UnikeyBuf` are used to coordinate edits.
 */

//--------------------------------------------
int UnikeyLoadMacroTable(const char *fileName)
{
  return pShMem->macStore.loadFromFile(fileName);
}

/**
 * @brief Load a macro table from a file into the engine's macro store.
 *
 * @param fileName Path to macro table file.
 * @return Non-zero on success, zero on failure.
 *
 * Example:
 *   if (!UnikeyLoadMacroTable("macros.dat")) {
 *       // handle error
 *   }
 */

//--------------------------------------------
int UnikeyLoadUserKeyMap(const char *fileName)
{
  if (UkLoadKeyMap(fileName, pShMem->usrKeyMap)) {
    //cout << "User key map loaded!\n"; //DEBUG
    pShMem->usrKeyMapLoaded = 1;
    return 1;
  }
  return 0;
}

/**
 * @brief Load a user-defined key mapping into the engine.
 *
 * @param fileName Path to the user key map file.
 * @return 1 when loaded successfully, 0 otherwise.
 *
 * When successful, the mapping becomes available via `UkUsrIM` input
 * method in `UnikeySetInputMethod`.
 */

//--------------------------------------------
void UnikeyRestoreKeyStrokes()
{
    UnikeyBufChars = sizeof(UnikeyBuf);
    MyKbEngine.restoreKeyStrokes(UnikeyBackspaces, UnikeyBuf, UnikeyBufChars, UnikeyOutput);
}

/**
 * @brief Ask the engine to re-emit or restore buffered keystrokes.
 *
 * Useful when the host needs UniKey to re-send composed characters after
 * a temporary interruption or when recovering from a lost event.
 */

bool UnikeyAtWordBeginning()
{
    return MyKbEngine.atWordBeginning();
}

/**
 * @brief Query whether the current caret position is at the beginning of a word.
 *
 * @return true when the engine believes the cursor is at a word boundary.
 *
 * Example:
 *   if (UnikeyAtWordBeginning()) { /* adjust composition behaviour */ }
 */

