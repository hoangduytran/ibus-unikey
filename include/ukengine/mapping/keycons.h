// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/*------------------------------------------------------------------------------
UniKey - Open-source Vietnamese Keyboard
Copyright (C) 1998-2004 Pham Kim Long
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
#ifndef __KEY_CONS_H
#define __KEY_CONS_H

/**
 * @file keycons.h
 * @brief UniKey input method key constants and options structures.
 *
 * Defines shared constants and enum types for input method selection and
 * keyboard mapping options.
 */

// macro table constants
#define MAX_MACRO_KEY_LEN 16
//#define MAX_MACRO_TEXT_LEN 256
#define MAX_MACRO_TEXT_LEN 1024 /**< maximum macro replacement text length */
#define MAX_MACRO_ITEMS 1024     /**< maximum number of macro entries supported */
#define MAX_MACRO_LINE (MAX_MACRO_TEXT_LEN + MAX_MACRO_KEY_LEN) /**< max line buffer length for macro storage */

#define MACRO_MEM_SIZE (1024*128) /**< macro table memory buffer size (128KB) */

#define CP_US_ANSI 1252 /**< default Windows ANSI code page */

/**
 * @brief Supported input method enumeration.
 */
typedef enum {
    UkTelex,        /**< Telex input method (classic UniKey rules). */
    UkVni,          /**< VNI input method (numeric tone marks via digits). */
    UkViqr,         /**< VIQR input method (ASCII-based textual accent codes). */
    UkMsVi,         /**< Microsoft Vietnamese IME compatibility mode. */
    UkUsrIM,        /**< User-defined custom mapping input method. */
    UkSimpleTelex,  /**< Simplified Telex variant (fewer keystrokes). */
    UkSimpleTelex2  /**< Another simplified Telex variant with alternate behavior. */
} UkInputMethod;

/**
 * @brief Runtime configuration options for UniKey engine.
 */
typedef struct _UnikeyOptions UnikeyOptions;

struct _UnikeyOptions
{
  int freeMarking;           /**< allow non-standard accent placement */
  int modernStyle;           /**< modern style key transformations */
  int macroEnabled;          /**< macro substitution enabled */
  int useUnicodeClipboard;   /**< exchange with Unicode clipboard on Windows */
  int alwaysMacro;           /**< always apply macro mapping even in non-VN contexts */
  int strictSpellCheck;      /**< enforce strict spell check semantics */
  int useIME;                /**< use native IME in Win32 builds */
  int spellCheckEnabled;     /**< enable built-in spell checker */
  int autoNonVnRestore;      /**< auto-restore non-VN text after typo correction */
};

#define UKOPT_FLAG_ALL                   0xFFFFFFFF /**< all options flags */
#define UKOPT_FLAG_FREE_STYLE            0x00000001 /**< free marking mode */
//#define UKOPT_FLAG_MANUAL_TONE           0x00000002
#define UKOPT_FLAG_MODERN                0x00000004 /**< modern style mode */
#define UKOPT_FLAG_MACRO_ENABLED         0x00000008 /**< macro processing enabled */
#define UKOPT_FLAG_USE_CLIPBOARD         0x00000010 /**< use unicode clipboard */
#define UKOPT_FLAG_ALWAYS_MACRO          0x00000020 /**< always apply macros */
#define UKOPT_FLAG_STRICT_SPELL          0x00000040 /**< strict spell-check mode */
#define UKOPT_FLAG_USE_IME               0x00000080 /**< use native IME (Win32 only) */
#define UKOPT_FLAG_SPELLCHECK_ENABLED   0x00000100 /**< enable spell-checker */

#if defined(WIN32)
typedef struct _UnikeySysInfo UnikeySysInfo;
struct _UnikeySysInfo
{
  int switchKey;           /**< virtual key code used to toggle UniKey mode. */
  HHOOK keyHook;           /**< handle to keyboard hook installed for interception. */
  HHOOK mouseHook;         /**< handle to mouse hook used during composition mode in some UIs. */
  HWND hMainDlg;           /**< main window/dialog handle used for event dispatch. */
  UINT iconMsgId;          /**< notification icon message ID for tray icon actions. */
  HICON hVietIcon, hEnIcon;/**< icons used in system tray for Vietnamese/English mode states. */
  int unicodePlatform;     /**< detected Unicode support level on this Windows platform. */
  DWORD winMajorVersion, winMinorVersion; /**< Windows version at startup. */
};
#endif

typedef enum {
    UkCharOutput,           /**< engine delivers final Vietnamese character stream (text output). */
    UkKeyOutput             /**< engine returns transformed keystrokes for IME-style input. */
} UkOutputType;

#endif
