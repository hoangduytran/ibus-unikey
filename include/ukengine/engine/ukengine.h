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

#ifndef __UKENGINE_H
#define __UKENGINE_H

/**
 * @file ukengine.h
 * @brief Core UniKey engine state and processing interface.
 *
 * This header exposes the UniKey engine wrapper used by the platform
 * integration layer.
 *
 * - UkSharedMem: shared configuration/state across engine instances.
 * - UkEngine: per-instance key processing and transformation.
 *
 * The engine is not thread-safe; caller must synchronize external access.
 */

#include "charset.h"
#include "vnlexi.h"
#include "inputproc.h"
#include "mactab.h"

#include <vector>

/**
 * @brief Shared immutable/global runtime data for all UniKey engine instances.
 *
 * This structure is kept in shared memory across processes, so it must not
 * contain pointers to process-local memory.
 */
struct UkSharedMem {
    int initialized;       /**< nonzero once engine is initialized */
    int vietKey;           /**< current Vietnamese key mode flag */

    UnikeyOptions options; /**< runtime options (input/output modes, flags) */
    UkInputProcessor input;/**< current input processor state */
    int usrKeyMapLoaded;   /**< true if user custom keymap is loaded */
    int usrKeyMap[256];    /**< direct key map lookup table */
    int charsetId;         /**< active output charset identifier */

    CMacroTable macStore;  /**< loaded macro definitions */
};

/**
 * @brief Maximum number of keystrokes stored in engine history.
 */
#define MAX_UK_ENGINE 128

/**
 * @brief Vietnamese word classification for buffer entries.
 */
enum VnWordForm {
    vnw_nonVn, /**< no Vietnamese sequence active (latin/other text) */
    vnw_empty, /**< empty segment boundary, start-of-word state */
    vnw_c,     /**< consonant-only segment (e.g., beginning of syllable) */
    vnw_v,     /**< vowel-only segment (including combining vowels) */
    vnw_cv,    /**< consonant followed by vowel sequence (partial syllable) */
    vnw_vc,    /**< vowel followed by consonant sequence (rare variant form) */
    vnw_cvc    /**< complete Vietnamese syllable form: consonant-vowel-consonant */
};

/**
 * @brief Callback prototype to query keyboard shift/caps status.
 *
 * @param pShiftPressed output pointer to shift key state
 * @param pCapslockOn output pointer to capslock state
 */
typedef void (* CheckKeyboardCaseCb)(int *pShiftPressed, int *pCapslockOn);

/**
 * @brief Buffer entry for a key event plus conversion status.
 */
struct KeyBufEntry {
    UkKeyEvent ev;      /**< raw key event (scan code, modifiers, etc.) */
    bool converted;     /**< set when this event was consumed by conversion */
};

/**
 * @class UkEngine
 * @brief Core state machine for Vietnamese input processing.
 *
 * Encapsulates keystroke buffering, transformation rules, and output generation.
 * Methods in this class are not thread-safe and caller must serialize access.
 */
class UkEngine
{
public:
    /**
     * @brief Default constructor.
     *
     * Initializes internal state without side-effects on shared config.
     */
    UkEngine();
    /**
     * @brief Attach shared control memory to this engine instance.
     *
     * @param p Pointer to UkSharedMem shared by all engine instances.
     */
    void setCtrlInfo(UkSharedMem *p)
    {
        m_pCtrl = p;
    }

    /**
     * @brief Set platform callback to query keyboard shift/caps state.
     *
     * @param pFunc Callback implementing CheckKeyboardCaseCb.
     */
    void setCheckKbCaseFunc(CheckKeyboardCaseCb pFunc)
    {
        m_keyCheckFunc = pFunc;
    }

    /**
     * @brief Check if the current cursor position is the start of a word.
     *
     * @return true when at beginning of word, false otherwise.
     */
    bool atWordBeginning();

    /**
     * @brief Process a keystroke through UniKey input logic.
     *
     * This is the main engine entrypoint for keyboard events.
     *
     * @param keyCode Unicode or scan code of pressed key
     * @param backs Number of backspaces produced (output parameter)
     * @param outBuf Buffer for generated output text
     * @param outSize[out] Number of bytes written into outBuf
     * @param outType[out] Type of output (e.g., chars, replacement)
     * @return >0 if consumed, 0 if ignored, <0 on error.
     */
    int process(unsigned int keyCode, int & backs, unsigned char *outBuf, int & outSize, UkOutputType & outType);

    /**
     * @brief Pass key through without transformation.
     *
     * Useful for system keys or bypass paths.
     */
    void pass(int keyCode);

    /**
     * @brief Force single-byte mode where Vietnamese input is accepted inside non-VN words.
     */
    void setSingleMode();

    /**
     * @brief Handle backspace key event.
     *
     * Updates internal state and returns text that should be sent to host.
     */
    int processBackspace(int & backs, unsigned char *outBuf, int & outSize, UkOutputType & outType);

    /**
     * @brief Reset engine state (e.g., on focus change).
     */
    void reset();

    /**
     * @brief Restore original keystrokes after conversion, usually on cancellation.
     */
    int restoreKeyStrokes(int & backs, unsigned char *outBuf, int & outSize, UkOutputType & outType);

    // Public method pointers: used by internal state machine hooking.
    // REVIEW: these are intentionally exposed for callback tables and are not
    // part of external API semantics.

    /**
     * @brief Process a tone mark key event (e.g., accent mark in Telex/VNI).
     */
    int processTone(UkKeyEvent & ev);

    /**
     * @brief Process roof mark (ˆ) key event.
     */
    int processRoof(UkKeyEvent & ev);

    /**
     * @brief Process hook mark (ˆ) key event.
     */
    int processHook(UkKeyEvent & ev);

    /**
     * @brief Process character append operation in converter buffer.
     */
    int processAppend(UkKeyEvent & ev);

    /**
     * @brief Append a vowel to current conversion state.
     */
    int appendVowel(UkKeyEvent & ev);

    /**
     * @brief Append a consonant to current conversion state.
     */
    int appendConsonnant(UkKeyEvent & ev);

    /**
     * @brief Process double-character mapping (e.g., dd -> đ).
     */
    int processDd(UkKeyEvent & ev);

    /**
     * @brief Process explicit mapping of a character key via input method tables.
     */
    int processMapChar(UkKeyEvent & ev);

    /**
     * @brief Process special Telex 'w' handling.
     */
    int processTelexW(UkKeyEvent & ev);

    /**
     * @brief Process escape character handling (VIQR sequences, etc.).
     */
    int processEscChar(UkKeyEvent & ev);

protected:
    static bool m_classInit;                  /**< one-time class initialization flag */
    CheckKeyboardCaseCb m_keyCheckFunc;       /**< platform callback for keyboard state */
    UkSharedMem *m_pCtrl;                     /**< pointer to shared engine state */

    int m_changePos;                          /**< position of last transform change */
    int m_backs;                              /**< pending backspaces to emit */
    int m_bufSize;                            /**< current word buffer length */
    int m_current;                            /**< current processing index */
    int m_singleMode;                         /**< single-mode enable flag */

    int m_keyBufSize;                         /**< keystroke buffer size (count) */
    KeyBufEntry m_keyStrokes[MAX_UK_ENGINE];  /**< event history for revert operations */
    int m_keyCurrent;                         /**< current index in key buffer */
    bool m_toEscape;                          /**< escape mode flag for VIQR processing */

    unsigned char *m_pOutBuf;                 /**< output buffer pointer during conversion */
    int *m_pOutSize;                          /**< output buffer size pointer */
    bool m_outputWritten;                     /**< whether output buffer was written */
    bool m_reverted;                          /**< whether last action was reverted */
    bool m_keyRestored;                       /**< whether keystrokes were restored */
    bool m_keyRestoring;                      /**< in-progress keystroke restore state */
    UkOutputType m_outType;                   /**< output type for the current event */
  
    struct WordInfo {
        VnWordForm form;                      /**< classification of Vietnamese word piece */

        int c1Offset;                         /**< offset to first consonant in constructed word frame */
        int vOffset;                          /**< offset to primary vowel position in word frame */
        int c2Offset;                         /**< offset to second consonant (coda) if present */

        union {
            VowelSeq vseq;                   /**< vowel sequence data when current symbol is vowel-centered */
            ConSeq cseq;                     /**< consonant sequence data when current symbol is consonant-centered */
        };

        int caps;                             /**< current capitalization style (0=lower, 1=upper, etc.) */
        int tone;                             /**< tone mark index for this symbol (0..N, -1 none) */
        VnLexiName vnSym;                    /**< canonical lexicon symbol id for normalized form, -1 if none */
        int keyCode;                          /**< original keystroke code used to generate this symbol */
    };

    WordInfo m_buffer[MAX_UK_ENGINE];         /**< working buffer for character composition */

    /** Scratch buffers for macroMatch (unbounded replacement via heap). */
    std::vector<StdVnChar> m_macroKeyScratch;
    std::vector<StdVnChar> m_macroTextScratch;

    /**
     * @brief Process hook mark in combination with user options and output.
     */
    int processHookWithUO(UkKeyEvent & ev);

    /**
     * @brief Attempt macro table match for the current input sequence.
     *
     * Progression:
     * 1. Skip shift+space/enter via `macroMatchPreSkip`.
     * 2. Scan composition buffer via `macroMatchScanForHit`.
     * 3. `markChange`, build cased expansion, flush output charset bytes.
     */
    int macroMatch(UkKeyEvent & ev);

    /**
     * @brief True when shift is held and the key is space or enter (macro match disabled).
     */
    bool macroMatchPreSkip(const UkKeyEvent &ev) const;
    /**
     * @brief Search backward for a `CMacroTable` hit; fills expansion pointer and `markChange` index.
     */
    bool macroMatchScanForHit(const StdVnChar *&pMacText, int &markIndex,
                              StdVnChar *&pKeyStart);
    /**
     * @brief Apply trigger casing to macro text into `m_macroTextScratch`; returns unit count.
     */
    int macroMatchBuildExpansionStdVn(const StdVnChar *pKeyStart,
                                      const StdVnChar *pMacText);
    /**
     * @brief `VnConvert` expansion and trailing key into `m_pOutBuf`, then `reset`.
     */
    void macroMatchFlushOutput(const UkKeyEvent &ev, int expansionUnitCount);

    /** @brief `WordInfo` → VNSTANDARD unit for macro fold key assembly. */
    static StdVnChar macroMatchWordInfoToStdKey(const WordInfo &w);
    /** @brief Span `firstBufferIndex…m_current` within `MAX_UK_ENGINE`. */
    bool macroMatchMacroKeyFitsEngine(int firstBufferIndex) const;
    /** @brief Rewind index to a word boundary or fail if blocked by non-break form. */
    bool macroMatchRewindToWordBoundary(int &wordSpanStartIndex) const;
    /** @brief Write NUL-terminated VN key from `m_buffer` into scratch. */
    void macroMatchBuildFoldLookupKey(int wordSpanStartIndex,
                                      StdVnChar *nulTerminatedKeyOut);
    /**
     * @brief `lookup(key+1)` then `lookup(key)`; sets `markIndex` / `pKeyStart` on hit.
     */
    bool macroMatchLookupAtSpan(int wordSpanStartIndex, StdVnChar *nulTerminatedKey,
                                const StdVnChar *&outMacText, int &markIndex,
                                StdVnChar *&pKeyStart);

    /**
     * @brief Mark a buffer change position for subsequent commit.
     */
    void markChange(int pos);

    /**
     * @brief Ensure the internal word buffer has reserved capacity.
     *
     * precondition: m_buffer has static capacity; this sets the size state.
     */
    void prepareBuffer();

    /**
     * @brief Write generated output into caller-provided buffer.
     *
     * @return number of bytes written.
     */
    int writeOutput(unsigned char *outBuf, int & outSize);

    /**
     * @brief Calculate the number of source steps between two buffer indices.
     */
    int getSeqSteps(int first, int last);

    /**
     * @brief Determine tone insertion position for a vowel sequence.
     */
    int getTonePosition(VowelSeq vs, bool terminated);

    /**
     * @brief Reset internal keystroke event buffer.
     */
    void resetKeyBuf();

    /**
     * @brief Check whether current event should switch to VIQR escape mode.
     */
    int checkEscapeVIQR(UkKeyEvent & ev);

    /**
     * @brief Process a key event path where spell checking is disabled.
     */
    int processNoSpellCheck(UkKeyEvent & ev);

    /**
     * @brief Process end-of-word event and finalize output transformation.
     */
    int processWordEnd(UkKeyEvent & ev);

    /**
     * @brief Synchronize internal key stroke buffer states after output update.
     */
    void synchKeyStrokeBuffer();

    /**
     * @brief Check if the last composed word contains a Vietnamese diacritic marker.
     */
    bool lastWordHasVnMark();

    /**
     * @brief Check if the last composed word is marked as non-VN.
     */
    bool lastWordIsNonVn();
};

/**
 * @brief Initialize global UniKey engine environment.
 *
 * This should be called once before using any dialect-specific interfaces.
 */
void SetupUnikeyEngine();

#endif
