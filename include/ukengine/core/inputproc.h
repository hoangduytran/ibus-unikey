/**
 * @file inputproc.h
 * @brief Vietnamese input method key event classification and mapping.
 *
 * Contains core state and control logic for UkInputProcessor, including
 * keyboard layout mapping and classification of Vietnamese character events.
 * This module is a central part of the input method engine and is designed
 * for use in both single-threaded and shared-memory contexts.
 */
// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
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
#ifndef __UK_INPUT_PROCESSOR_H
#define __UK_INPUT_PROCESSOR_H

#include "keycons.h"
#include "vnlexi.h"

#if defined(_WIN32)
#define DllExport __declspec(dllexport)
#define DllImport __declspec(dllimport)
#if defined(UNIKEYHOOK)
#define DllInterface __declspec(dllexport)
#else
#define DllInterface __declspec(dllimport)
#endif
#else
#define DllInterface // not used
#define DllExport
#define DllImport
#endif

/**
 * @brief Types of input key events for Vietnamese diacritic/character handling.
 *
 * The event classification allows the input processor to choose the right
 * modification strategy for tones, bowls, hooks, and other Vietnamese marks.
 */
enum UkKeyEvName
{
  /**
   * Roof mark event on any roofable vowel.
   * Example: `oo` or `OO` → `ô`, `ee` → `ê`, `aa` → `â`
   */
  vneRoofAll,

  /**
   * Roof mark on 'a' vowel.
   * Example: `aa` → `â`
   */
  vneRoof_a,
  /**
   * Roof mark on 'e' vowel.
   * Example: `ee` → `ê`
   */
  vneRoof_e,
  /**
   * Roof mark on 'o' vowel.
   * Example: `oo` → `ô`
   */
  vneRoof_o,

  /**
   * Hook mark event for all hookable vowels.
   * Example: `uw` → `ư`, `ow` → `ơ`
   */
  vneHookAll,
  /**
   * Hook mark on the `uơ` vowel variant.
   * Example: `u7` or `uw` in some IMs → `ư`
   */
  vneHook_uo,
  /**
   * Hook mark on 'u'.
   * Example: `uw` → `ư`
   */
  vneHook_u,
  /**
   * Hook mark on 'o'.
   * Example: `ow` → `ơ`
   */
  vneHook_o,

  /**
   * Bowl shape modifier used for `ă` and related variants.
   * Example: `a8` or `a(` → `ă`
   */
  vneBowl,

  /**
   * Special doubled consonant event for `đ`.
   * Example: `dd` → `đ`
   */
  vneDd,

  /** No tone / tone removed. */
  vneTone0,
  /** Acute tone (sắc). Example: `as` → `á`. */
  vneTone1,
  /** Grave tone (huyền). Example: `af` → `à`. */
  vneTone2,
  /** Hook tone (hỏi). Example: `ar` → `ả`. */
  vneTone3,
  /** Tilde tone (ngã). Example: `ax` → `ã`. */
  vneTone4,
  /** Dot tone (nặng). Example: `aj` → `ạ`. */
  vneTone5,

  /**
   * Telex special `w` handling technique.
   * Example: `aw` → `ă`, `ow` → `ơ`, `uw` → `ư`
   */
  vne_telex_w,

  /**
   * Explicit mapping to a Vietnamese symbol instead of a diacritic action.
   * Example: direct `[` → `ô` or `]` → `ư`
   */
  vneMapChar,

  /**
   * Escape or cancel input rule, used to treat a marker as a raw character.
   */
  vneEscChar,

  /** Normal key: no Vietnamese event mapping. */
  vneNormal,

  /** Number of UkKeyEvName values; useful for table sizing. */
  vneCount
};

enum UkCharType
{
  /**
   * Vietnamese character that can receive diacritics / tones.
   * Example: `a`, `á`, `â`, `ơ`, `đ`
   */
  ukcVn,
  /**
   * Word boundary character such as space or punctuation.
   * Example: ` `, `,`, `.`
   */
  ukcWordBreak,
  /**
   * Non-Vietnamese character that is treated as ordinary input.
   * Example: `1`, `@`, `z` outside Vietnamese composition.
   */
  ukcNonVn,
  /**
   * Reset/fallback state used during classifier initialization.
   */
  ukcReset
};

/**
 * @brief Key classification event used by the input processor.
 */
struct UkKeyEvent
{
  int evType;           ///< UkKeyEvName event type
  UkCharType chType;    ///< Character classification
  VnLexiName vnSym;     ///< Vietnamese shape symbol (only for ukcVn)
  unsigned int keyCode; ///< Physical key code
  int tone;             ///< Tone value for vowels
};

/**
 * @brief Mapping from a raw key to an input action used by input method engines.
 *
 * Each entry pairs a physical byte value with a Vk-like action code used by
 * the processor to produce Vietnamese composition behavior.
 *
 * Implementation note:
 * - key is treated as unsigned to support extended ASCII indices (e.g. 0x80..0xFF).
 * - action should be a value from the current input method action table, where
 *   special sentinel values indicate no-op or mode change.
 */
struct UkKeyMapping
{
  /**
   * @brief Raw input key code (unsigned) for mapping.
   *
   * Typically 0..255, taken from keyboard scan/character input layer.
   */
  unsigned char key;

  /**
   * @brief Action code to execute for this key.
   *
   * The exact meaning is method-specific and resolved by UkInputProcessor
   * to set event types (e.g., vneRoof_a) or character insertion descriptors.
   */
  int action;
};

///////////////////////////////////////////
/**
 * @brief Core Vietnamese input method key processor.
 *
 * Handles input method selection, character classification, and key-to-event
 * conversion according to a selected keyboard layout (Telex, VNI, etc.).
 */
class UkInputProcessor
{

public:
  /**
   * @brief Default object state is uninitialized, use init().
   *
   * NOTE: This object may be placed in shared memory. Avoid constructing
   * heavy members here, and use init() to establish necessary state.
   */
  // UkInputProcessor();

  /**
   * @brief Initialize internal mapping tables and static state.
   */
  void init();

  /**
   * @brief Get currently active input method.
   */
  UkInputMethod getIM()
  {
    return m_im;
  }

  /**
   * @brief Convert keycode into semantic input event (tone/hook/bowl/etc.).
   *
   * @param keyCode Raw key code from keyboard input.
   * @param[out] ev Populated UkKeyEvent with event details.
   */
  void keyCodeToEvent(unsigned int keyCode, UkKeyEvent &ev);

  /**
   * @brief Convert keycode into symbolic event (e.g., diacritic character mapping).
   */
  void keyCodeToSymbol(unsigned int keyCode, UkKeyEvent &ev);

  /**
   * @brief Set active input method by enum.
   */
  int setIM(UkInputMethod im);

  /**
   * @brief Set active input method from key mapping table.
   */
  int setIM(int map[256]);

  /**
   * @brief Export current key map to caller-provided array.
   *
   * @param map Output map array (size 256).
   */
  void getKeyMap(int map[256]);

  /**
   * @brief Classify key code as Vietnamese word/non-Vietnamese/break.
   */
  UkCharType getCharType(unsigned int keyCode);

protected:
  static bool m_classInit; ///< Class-level initialization guard

  UkInputMethod m_im; ///< Current input method
  int m_keyMap[256];  ///< Current key map lookup table

  /**
   * @brief Populate m_keyMap using built-in method mapping.
   *
   * @param map Null-terminated method map.
   */
  void useBuiltIn(UkKeyMapping *map);
};

/**
 * @brief Reset the key map to the default unmodified state.
 *
 * @param keyMap Array of 256 mapping values.
 */
void UkResetKeyMap(int keyMap[256]);

/**
 * @brief Initialize internal classifier tables for Unicode inputs.
 */
void SetupInputClassifierTable();

/**
 * @brief Telex input method key mapping.
 *
 * Standard Telex mappings for Vietnamese typing (e.g., "s" for sắc, "f" for huyền).
 * Used by UkInputProcessor when UkInputMethod is set to the Telex layout.
 */
DllInterface extern UkKeyMapping TelexMethodMapping[];

/**
 * @brief Simple Telex input method mapping.
 *
 * A reduced variant of Telex with fewer special keys and simpler heuristics,
 * designed for users who prefer minimal key combinations.
 */
DllInterface extern UkKeyMapping SimpleTelexMethodMapping[];

/**
 * @brief VNI input method key mapping.
 *
 * Uses numeric tone and diacritic keys (e.g., 1..5 for tones, 6..9 for hooks).
 * Set with UkInputMethod VNI.
 */
DllInterface extern UkKeyMapping VniMethodMapping[];

/**
 * @brief VIQR input method key mapping.
 *
 * Maps VIQR syntax sequences to Vietnamese characters (e.g., "a^" -> â).
 */
DllInterface extern UkKeyMapping VIQRMethodMapping[];

/**
 * @brief Microsoft Vietnamese (MSVI) key mapping.
 *
 * Compatibility mode for Microsoft VI keyboard layout, used by older legacy
 * applications and to support users familiar with MSVI conventions.
 */
DllInterface extern UkKeyMapping MsViMethodMapping[];

extern VnLexiName IsoVnLexiMap[];

/**
 * @brief Map ASCII code to Vietnamese lexical enumeration.
 *
 * @param keyCode Unicode code point or ASCII value.
 * @return Corresponding VnLexiName.
 */
inline VnLexiName IsoToVnLexi(unsigned int keyCode)
{
  return (keyCode >= 256) ? vnl_nonVnChar : IsoVnLexiMap[keyCode];
}

#endif
