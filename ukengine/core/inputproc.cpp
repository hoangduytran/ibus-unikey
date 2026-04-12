// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/**
 * @file inputproc.cpp
 * @brief Core Vietnamese input processing and key mapping logic for UniKey.
 *
 * This module defines the built-in key mapping tables, character classification
 * tables, and the input processor methods used to translate key codes into
 * Vietnamese input events.
 */
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

#include <iostream>
#include "inputproc.h"

using namespace std;

/**
 * @brief Characters treated as word boundaries during input processing.
 */
unsigned char WordBreakSyms[] = {
    ',',  // comma: breaks words, e.g. "xin, chao"
    ';',  // semicolon: breaks phrases, e.g. "a; b"
    ':',  // colon: separates clauses, e.g. "gio: 10h"
    '.',  // period: ends a word boundary, e.g. "abc."
    '"',  // double quote: quotation boundary, e.g. "\"abc\""
    '\'', // apostrophe: e.g. "o'clock"
    '!',  // exclamation mark: sentence boundary, e.g. "xin chao!"
    '?',  // question mark: sentence boundary, e.g. "ban khoe khong?"
    ' ',  // space: standard word separator
    '<',  // less-than: symbol boundary, e.g. "a < b"
    '>',  // greater-than: symbol boundary, e.g. "a > b"
    '=',  // equals sign: separator in assignments, e.g. "x = 1"
    '+',  // plus: operator boundary, e.g. "a + b"
    '-',  // hyphen: compound-word separator, e.g. "co-so"
    '*',  // asterisk: operator or wildcard, e.g. "a * b"
    '/',  // slash: path or division separator, e.g. "a/b"
    '\\', // backslash: path separator, e.g. "C:\\tmp"
    '_',  // underscore: token separator, e.g. "ten_nguoi_dung"
    '@',  // at sign: address separator, e.g. "name@example.com"
    '#',  // hash: tag marker, e.g. "#viet"
    '$',  // dollar sign: currency or variable marker, e.g. "$100"
    '%',  // percent: percentage marker, e.g. "50%"
    '&',  // ampersand: conjunction/operator, e.g. "a & b"
    '(',  // left parenthesis: grouping boundary, e.g. "(abc)"
    ')',  // right parenthesis: grouping boundary, e.g. "(abc)"
    '{',  // left brace: block boundary, e.g. "{abc}"
    '}',  // right brace: block boundary, e.g. "{abc}"
    '[',  // left bracket: list boundary, e.g. "[abc]"
    ']',  // right bracket: list boundary, e.g. "[abc]"
    '|'}; // pipe: alternate or separator, e.g. "a|b"; we excluded ~, `, ^

/**
 * @brief Mapping from ASCII uppercase letters to Vietnamese lexical names.
 */
VnLexiName AZLexiUpper[] =
    {vnl_A,  // 'A'
     vnl_B,  // 'B'
     vnl_C,  // 'C'
     vnl_D,  // 'D'
     vnl_E,  // 'E'
     vnl_F,  // 'F'
     vnl_G,  // 'G'
     vnl_H,  // 'H'
     vnl_I,  // 'I'
     vnl_J,  // 'J'
     vnl_K,  // 'K'
     vnl_L,  // 'L'
     vnl_M,  // 'M'
     vnl_N,  // 'N'
     vnl_O,  // 'O'
     vnl_P,  // 'P'
     vnl_Q,  // 'Q'
     vnl_R,  // 'R'
     vnl_S,  // 'S'
     vnl_T,  // 'T'
     vnl_U,  // 'U'
     vnl_V,  // 'V'
     vnl_W,  // 'W'
     vnl_X,  // 'X'
     vnl_Y,  // 'Y'
     vnl_Z}; // 'Z'

/**
 * @brief Mapping from ASCII lowercase letters to Vietnamese lexical names.
 */
VnLexiName AZLexiLower[] =
    {vnl_a,  // 'a'
     vnl_b,  // 'b'
     vnl_c,  // 'c'
     vnl_d,  // 'd'
     vnl_e,  // 'e'
     vnl_f,  // 'f'
     vnl_g,  // 'g'
     vnl_h,  // 'h'
     vnl_i,  // 'i'
     vnl_j,  // 'j'
     vnl_k,  // 'k'
     vnl_l,  // 'l'
     vnl_m,  // 'm'
     vnl_n,  // 'n'
     vnl_o,  // 'o'
     vnl_p,  // 'p'
     vnl_q,  // 'q'
     vnl_r,  // 'r'
     vnl_s,  // 's'
     vnl_t,  // 't'
     vnl_u,  // 'u'
     vnl_v,  // 'v'
     vnl_w,  // 'w'
     vnl_x,  // 'x'
     vnl_y,  // 'y'
     vnl_z}; // 'z'

/**
 * @brief Lookup table that classifies each byte value as Vietnamese, non-Vietnamese, or word break.
 */
UkCharType UkcMap[256];

/**
 * @brief Associates an ASCII code with a Vietnamese lexical symbol.
 */
struct _ascVnLexi
{
    /** @brief ASCII code point value. */
    int asc;
    /** @brief Matching Vietnamese lexical symbol. */
    VnLexiName lexi;
};

/**
 * @brief Western characters outside A-Z that are still treated as Vietnamese letters.
 */
_ascVnLexi AscVnLexiList[] = {
    {0xC0, vnl_A2},         // À -> uppercase A with grave tone
    {0xC1, vnl_A1},         // Á -> uppercase A with acute tone
    {0xC2, vnl_Ar},         // Â -> uppercase A with circumflex
    {0xC2, vnl_A4},         // Â -> uppercase A with hook tone (legacy alias)
    {0xC8, vnl_E2},         // È -> uppercase E with grave tone
    {0xC9, vnl_E1},         // É -> uppercase E with acute tone
    {0xCA, vnl_Er},         // Ê -> uppercase E with circumflex
    {0xCC, vnl_I2},         // Ì -> uppercase I with grave tone
    {0xCD, vnl_I1},         // Í -> uppercase I with acute tone
    {0xD2, vnl_O2},         // Ò -> uppercase O with grave tone
    {0xD3, vnl_O1},         // Ó -> uppercase O with acute tone
    {0xD4, vnl_Or},         // Ô -> uppercase O with circumflex
    {0xD5, vnl_O4},         // Õ -> uppercase O with tilde tone
    {0xD9, vnl_U2},         // Ù -> uppercase U with grave tone
    {0xDA, vnl_U1},         // Ú -> uppercase U with acute tone
    {0xDD, vnl_Y1},         // Ý -> uppercase Y with acute tone
    {0xE0, vnl_a2},         // à -> lowercase a with grave tone
    {0xE1, vnl_a1},         // á -> lowercase a with acute tone
    {0xE2, vnl_ar},         // â -> lowercase a with circumflex
    {0xE3, vnl_a4},         // ã -> lowercase a with tilde tone
    {0xE8, vnl_e2},         // è -> lowercase e with grave tone
    {0xE9, vnl_e1},         // é -> lowercase e with acute tone
    {0xEA, vnl_er},         // ê -> lowercase e with circumflex
    {0xEC, vnl_i2},         // ì -> lowercase i with grave tone
    {0xED, vnl_i1},         // í -> lowercase i with acute tone
    {0xF2, vnl_o2},         // ò -> lowercase o with grave tone
    {0xF3, vnl_o1},         // ó -> lowercase o with acute tone
    {0xF4, vnl_or},         // ô -> lowercase o with circumflex
    {0xF5, vnl_o4},         // õ -> lowercase o with tilde tone
    {0xF9, vnl_u2},         // ù -> lowercase u with grave tone
    {0xFA, vnl_u1},         // ú -> lowercase u with acute tone
    {0xFD, vnl_y1},         // ý -> lowercase y with acute tone
    {0x00, vnl_nonVnChar}}; // terminator: no more mapped entries

/**
 * @brief Lookup table mapping ISO byte values to Vietnamese lexical symbols.
 */
VnLexiName IsoVnLexiMap[256];

/**
 * @brief Tracks whether the classification tables have already been initialized.
 */
bool ClassifierTableInitialized = false;

/**
 * @brief Telex input method mapping table.
 *
 * Each row maps a raw typed key into an internal Vietnamese input event.
 * The rules here follow the standard UniKey/Telex conventions.
 */
DllExport UkKeyMapping TelexMethodMapping[] = {
    // Tone keys:
    {'Z', vneTone0}, // z: remove/reset tone, e.g. "á" + z -> "a"
    {'S', vneTone1}, // s: sắc tone, e.g. as -> á, es -> é, os -> ó
    {'F', vneTone2}, // f: huyền tone, e.g. af -> à, ef -> è, of -> ò
    {'R', vneTone3}, // r: hỏi tone, e.g. ar -> ả, er -> ẻ, or -> ỏ
    {'X', vneTone4}, // x: ngã tone, e.g. ax -> ã, ex -> ẽ, ox -> õ
    {'J', vneTone5}, // j: nặng tone, e.g. aj -> ạ, ej -> ẹ, oj -> ọ

    // Telex vowel modifiers:
    {'W', vne_telex_w}, // w: special Telex handling for ă/ơ/ư, e.g. aw -> ă
    {'A', vneRoof_a},   // a: roof on a -> â, e.g. aa -> â
    {'E', vneRoof_e},   // e: roof on e -> ê, e.g. ee -> ê
    {'O', vneRoof_o},   // o: roof on o -> ô, e.g. oo -> ô

    // Special consonant and direct mapping entries:
    {'D', vneDd},             // d: double d -> đ, e.g. dd -> đ
    {'[', vneCount + vnl_oh}, // [: insert lowercase ô directly
    {']', vneCount + vnl_uh}, // ]: insert lowercase ư directly
    {'{', vneCount + vnl_Oh}, // {: insert uppercase Ô directly
    {'}', vneCount + vnl_Uh}, // }: insert uppercase Ư directly

    {0, vneNormal}}; // terminator

/**
 * @brief Simplified Telex mapping table.
 */
DllExport UkKeyMapping SimpleTelexMethodMapping[] = {
    {'Z', vneTone0},   // z: reset tone
    {'S', vneTone1},   // s: sắc tone
    {'F', vneTone2},   // f: huyền tone
    {'R', vneTone3},   // r: hỏi tone
    {'X', vneTone4},   // x: ngã tone
    {'J', vneTone5},   // j: nặng tone
    {'W', vneHookAll}, // w: hook all vowels, e.g. aw -> ă, ow -> ơ, uw -> ư
    {'A', vneRoof_a},  // a: roof on a -> â
    {'E', vneRoof_e},  // e: roof on e -> ê
    {'O', vneRoof_o},  // o: roof on o -> ô
    {'D', vneDd},      // d: double d -> đ
    {0, vneNormal}};   // terminator

/**
 * @brief Alternate simplified Telex mapping table.
 */
DllExport UkKeyMapping SimpleTelex2MethodMapping[] = {
    {'Z', vneTone0},    // z: reset tone
    {'S', vneTone1},    // s: sắc tone
    {'F', vneTone2},    // f: huyền tone
    {'R', vneTone3},    // r: hỏi tone
    {'X', vneTone4},    // x: ngã tone
    {'J', vneTone5},    // j: nặng tone
    {'W', vne_telex_w}, // w: Telex special handling, e.g. aw -> ă
    {'A', vneRoof_a},   // a: roof on a -> â
    {'E', vneRoof_e},   // e: roof on e -> ê
    {'O', vneRoof_o},   // o: roof on o -> ô
    {'D', vneDd},       // d: double d -> đ
    {0, vneNormal}};    // terminator

/**
 * @brief VNI input method mapping table.
 */
DllExport UkKeyMapping VniMethodMapping[] = {
    {'0', vneTone0},   // 0: no tone / reset
    {'1', vneTone1},   // 1: sắc tone, e.g. a1 -> á
    {'2', vneTone2},   // 2: huyền tone, e.g. a2 -> à
    {'3', vneTone3},   // 3: hỏi tone, e.g. a3 -> ả
    {'4', vneTone4},   // 4: ngã tone, e.g. a4 -> ã
    {'5', vneTone5},   // 5: nặng tone, e.g. a5 -> ạ
    {'6', vneRoofAll}, // 6: roof modifier, e.g. a6 -> â
    {'7', vneHook_uo}, // 7: hook modifier, e.g. o7 -> ơ
    {'8', vneBowl},    // 8: bowl modifier, e.g. u8 -> ư
    {'9', vneDd},      // 9: d-to-đ, e.g. d9 -> đ
    {0, vneNormal}};   // terminator

/**
 * @brief VIQR input method mapping table.
 */
DllExport UkKeyMapping VIQRMethodMapping[] = {
    {'0', vneTone0},    // 0: reset tone
    {'\'', vneTone1},   // apostrophe: sắc tone, e.g. a' -> á
    {'`', vneTone2},    // backtick: huyền tone, e.g. a` -> à
    {'?', vneTone3},    // question mark: hỏi tone, e.g. a? -> ả
    {'~', vneTone4},    // tilde: ngã tone, e.g. a~ -> ã
    {'.', vneTone5},    // dot: nặng tone, e.g. a. -> ạ
    {'^', vneRoofAll},  // caret: roof modifier, e.g. a^ -> â
    {'+', vneHook_uo},  // plus: hook modifier, e.g. o+ -> ơ
    {'*', vneHook_uo},  // asterisk: hook modifier, e.g. o* -> ơ
    {'(', vneBowl},     // left parenthesis: bowl modifier, e.g. u( -> ư
    {'D', vneDd},       // D: d-to-đ, e.g. D -> Đ
    {'\\', vneEscChar}, // backslash: escape character
    {0, vneNormal}};    // terminator

/**
 * @brief Microsoft Vietnamese input method mapping table.
 */
DllExport UkKeyMapping MsViMethodMapping[] = {
    {'5', vneTone2},          // 5: huyền tone
    {'%', vneTone2},          // %: alternate huyền tone
    {'6', vneTone3},          // 6: hỏi tone
    {'^', vneTone3},          // ^: alternate hỏi tone
    {'7', vneTone4},          // 7: ngã tone
    {'&', vneTone4},          // &: alternate ngã tone
    {'8', vneTone1},          // 8: sắc tone
    {'*', vneTone1},          // *: alternate sắc tone
    {'9', vneTone5},          // 9: nặng tone
    {'(', vneTone5},          // (: alternate nặng tone
    {'1', vneCount + vnl_ab}, // 1: insert lowercase a with breve, e.g. ă
    {'!', vneCount + vnl_Ab}, // !: insert uppercase A with breve, e.g. Ă
    {'2', vneCount + vnl_ar}, // 2: insert lowercase a with circumflex, e.g. â
    {'@', vneCount + vnl_Ar}, // @: insert uppercase A with circumflex, e.g. Â
    {'3', vneCount + vnl_er}, // 3: insert lowercase e with circumflex, e.g. ê
    {'#', vneCount + vnl_Er}, // #: insert uppercase E with circumflex, e.g. Ê
    {'4', vneCount + vnl_or}, // 4: insert lowercase o with circumflex, e.g. ô
    {'$', vneCount + vnl_Or}, // $: insert uppercase O with circumflex, e.g. Ô
    {'0', vneCount + vnl_dd}, // 0: insert lowercase đ
    {')', vneCount + vnl_DD}, // ): insert uppercase Đ
    {'[', vneCount + vnl_uh}, // [: insert lowercase ư
    {']', vneCount + vnl_oh}, // ]: insert lowercase ơ
    {'{', vneCount + vnl_Uh}, // {: insert uppercase Ư
    {'}', vneCount + vnl_Oh}, // }: insert uppercase Ơ
    {0, vneNormal}};          // terminator

/**
 * @brief Initializes the character classification lookup tables.
 *
 * Populates UkcMap and IsoVnLexiMap so later key processing can classify bytes quickly.
 */
void SetupInputClassifierTable()
{
    unsigned int c;
    int i;

    for (c = 0; c <= 32; c++)
    {
        UkcMap[c] = ukcReset;
    }

    for (c = 33; c < 256; c++)
    {
        UkcMap[c] = ukcNonVn;
    }

    /*
    for (c = '0'; c <= '9'; c++)
      UkcMap[c] = ukcNonVn;
    */

    for (c = 'a'; c <= 'z'; c++)
        UkcMap[c] = ukcVn;
    for (c = 'A'; c <= 'Z'; c++)
        UkcMap[c] = ukcVn;

    for (i = 0; AscVnLexiList[i].asc; i++)
    {
        UkcMap[AscVnLexiList[i].asc] = ukcVn;
    }

    UkcMap[(unsigned char)'j'] = ukcNonVn;
    UkcMap[(unsigned char)'J'] = ukcNonVn;
    UkcMap[(unsigned char)'f'] = ukcNonVn;
    UkcMap[(unsigned char)'F'] = ukcNonVn;
    UkcMap[(unsigned char)'w'] = ukcNonVn;
    UkcMap[(unsigned char)'W'] = ukcNonVn;

    int count = sizeof(WordBreakSyms) / sizeof(unsigned char);
    for (i = 0; i < count; i++)
        UkcMap[WordBreakSyms[i]] = ukcWordBreak;

    // Calculate IsoVnLexiMap
    for (i = 0; i < 256; i++)
    {
        IsoVnLexiMap[i] = vnl_nonVnChar;
    }

    for (i = 0; AscVnLexiList[i].asc; i++)
    {
        IsoVnLexiMap[AscVnLexiList[i].asc] = AscVnLexiList[i].lexi;
    }

    for (c = 'a'; c <= 'z'; c++)
    {
        IsoVnLexiMap[c] = AZLexiLower[c - 'a'];
    }

    for (c = 'A'; c <= 'Z'; c++)
    {
        IsoVnLexiMap[c] = AZLexiUpper[c - 'A'];
    }
}

/**
 * @brief Initializes the input processor state.
 *
 * Ensures classifier tables are available and selects the default Telex input method.
 */
void UkInputProcessor::init()
{
    if (!ClassifierTableInitialized)
    {
        SetupInputClassifierTable();
        ClassifierTableInitialized = true;
    }
    setIM(UkTelex);
}

/**
 * @brief Selects one of the built-in input methods.
 *
 * @param im The input method to activate.
 * @return Always returns 1.
 */
int UkInputProcessor::setIM(UkInputMethod im)
{
    m_im = im;
    switch (im)
    {
    case UkTelex:
        useBuiltIn(TelexMethodMapping);
        break;
    case UkSimpleTelex:
        useBuiltIn(SimpleTelexMethodMapping);
        break;
    case UkSimpleTelex2:
        useBuiltIn(SimpleTelex2MethodMapping);
        break;
    case UkVni:
        useBuiltIn(VniMethodMapping);
        break;
    case UkViqr:
        useBuiltIn(VIQRMethodMapping);
        break;
    case UkMsVi:
        useBuiltIn(MsViMethodMapping);
        break;
    default:
        m_im = UkTelex;
        useBuiltIn(TelexMethodMapping);
    }
    return 1;
}

/**
 * @brief Installs a custom key mapping table.
 *
 * @param map Array of 256 mapping entries.
 * @return Always returns 1.
 */
int UkInputProcessor::setIM(int map[256])
{
    int i;
    m_im = UkUsrIM;
    for (i = 0; i < 256; i++)
        m_keyMap[i] = map[i];
    return 1;
}

/**
 * @brief Resets a key map to the default non-mapped state.
 *
 * @param keyMap Array of 256 entries to reset.
 */
void UkResetKeyMap(int keyMap[256])
{
    unsigned int c;
    for (c = 0; c < 256; c++)
        keyMap[c] = vneNormal;
}

/**
 * @brief Loads a built-in key mapping table into the processor.
 *
 * @param map Pointer to a null-terminated key mapping list.
 */
void UkInputProcessor::useBuiltIn(UkKeyMapping *map)
{
    UkResetKeyMap(m_keyMap);
    for (int i = 0; map[i].key; i++)
    {
        m_keyMap[map[i].key] = map[i].action;
        if (map[i].action < vneCount)
        {
            if (islower(map[i].key))
            {
                m_keyMap[toupper(map[i].key)] = map[i].action;
            }
            else if (isupper(map[i].key))
            {
                m_keyMap[tolower(map[i].key)] = map[i].action;
            }
        }
    }
}

/**
 * @brief Converts a raw key code into a Vietnamese input event.
 *
 * @param keyCode Raw key code from the keyboard.
 * @param ev Output event record to populate.
 */
void UkInputProcessor::keyCodeToEvent(unsigned int keyCode, UkKeyEvent &ev)
{
    ev.keyCode = keyCode;
    if (keyCode > 255)
    {
        ev.evType = vneNormal;
        ev.vnSym = IsoToVnLexi(keyCode);
        ev.chType = (ev.vnSym == vnl_nonVnChar) ? ukcNonVn : ukcVn;
    }
    else
    {
        ev.chType = UkcMap[keyCode];
        ev.evType = m_keyMap[keyCode];

        if (ev.evType >= vneTone0 && ev.evType <= vneTone5)
        {
            ev.tone = ev.evType - vneTone0;
        }

        if (ev.evType >= vneCount)
        {
            ev.chType = ukcVn;
            ev.vnSym = (VnLexiName)(ev.evType - vneCount);
            ev.evType = vneMapChar;
        }
        else
        {
            ev.vnSym = IsoToVnLexi(keyCode);
        }
    }
}

/**
 * @brief Converts a raw key code into a symbol-only input event.
 *
 * This treats the key as character input rather than an action key.
 *
 * @param keyCode Raw key code from the keyboard.
 * @param ev Output event record to populate.
 */
void UkInputProcessor::keyCodeToSymbol(unsigned int keyCode, UkKeyEvent &ev)
{
    ev.keyCode = keyCode;
    ev.evType = vneNormal;
    ev.vnSym = IsoToVnLexi(keyCode);
    if (keyCode > 255)
    {
        ev.chType = (ev.vnSym == vnl_nonVnChar) ? ukcNonVn : ukcVn;
    }
    else
    {
        ev.chType = UkcMap[keyCode];
    }
}

/**
 * @brief Returns the character classification for a key code.
 *
 * @param keyCode Raw key code from the keyboard.
 * @return The inferred character type.
 */
UkCharType UkInputProcessor::getCharType(unsigned int keyCode)
{
    if (keyCode > 255)
        return (IsoToVnLexi(keyCode) == vnl_nonVnChar) ? ukcNonVn : ukcVn;
    return UkcMap[keyCode];
}

/**
 * @brief Copies the current key map into a caller-provided buffer.
 *
 * @param map Destination array of 256 entries.
 */
void UkInputProcessor::getKeyMap(int map[256])
{
    int i;
    for (i = 0; i < 256; i++)
        map[i] = m_keyMap[i];
}
