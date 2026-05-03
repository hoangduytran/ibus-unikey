// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4;
// indent-tabs-mode:nil -*-
/*------------------------------------------------------------------------------
VnConv: Vietnamese Encoding Converter Library
UniKey Project: http://unikey.sourceforge.net
Copyleft (C) 1998-2002 Pham Kim Long
Contact: longp@cslab.felk.cvut.cz

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

#include "charset.h"
#include "charset_components/charset_internal.h"

#define IS_VOWEL(x)                                \
	((x >= 'a' && x <= 'z' && LoVowel[x - 'a']) || \
	 (x >= 'A' && x <= 'Z' && HiVowel[x - 'A']))

/////////////////////////////////////////////
// Class: VIQRCharset                      //
/////////////////////////////////////////////

/* VIQRTones: Array of ASCII tone marker characters used by the VIQR
 * input/output logic. The sequence/order here matches the offsets used
 * elsewhere in the VIQR mapping tables (acute, grave, hook, tilde, dot).
 */
unsigned char VIQRTones[] = {'\'', '`', '?', '~', '.'};

/* VIQREscapes: Patterns which, when detected at or just before the
 * current input position, should disable VIQR interpretation. This
 * prevents common tokens such as URL schemes, path separators and
 * email addresses from being parsed as VIQR sequences.
 */
const char *VIQREscapes[] = {
	"://", "/", "@", "mailto:", "email:", "news:", "www", "ftp"};

/* VIQREscCount: number of entries in VIQREscapes */
int VIQREscCount = sizeof(VIQREscapes) / sizeof(char *);

/* VIQRCharset constructor
 * Progression:
 * 1) Clear `m_stdMap` (256 entries) used for fast single-byte lookups.
 * 2) Store the incoming `vnChars` pointer in `m_vnChars` for later output
 * mapping. 3) Walk the full VN character table and record reverse mappings for
 *    single-byte entries: `m_stdMap[byte] = i + 256`. This allows the
 *    parser to quickly detect VIQR tokens and convert them to
 *    internal `StdVnChar` indices during input processing.
 * 4) Configure punctuation/tone offsets in `m_stdMap` so that tone
 *    markers and modifiers (e.g. '^', '(', '+', '*') map to their
 *    respective offset values used by VIQR decoding/encoding.
 */
VIQRCharset::VIQRCharset(UKDWORD *vnChars)
{
	/* Initialize the fast single-byte lookup table. All entries start
	 * zero which indicates 'no special mapping' for that byte. */
	memset(m_stdMap, 0, 256 * sizeof(UKWORD));

	/* Keep a local loop index / temp word for table population. */
	int i;
	UKDWORD dw;

	/* Store the VN->Unicode mapping pointer for use by output routines. */
	m_vnChars = vnChars;

	/* Pass 1: Build reverse mappings for any VN characters that are
	 * representable as a single byte. For those entries record the
	 * StdChar index as (i + 256) so values 0..255 remain available for
	 * raw single-byte characters. This allows input parsing to detect
	 * VIQR tokens quickly via m_stdMap[byte]. */
	for (i = 0; i < TOTAL_VNCHARS; i++)
	{
		dw = m_vnChars[i];
		if (!(dw & 0xffffff00))
		{ /* single-byte mapping found */
			/* store reverse lookup: raw byte -> VN index (offset by 256) */
			m_stdMap[dw] = i + 256;
		}
	}

	/* Configure explicit offsets for VIQR tone markers and modifiers.
	 * These values are used by the VIQR parser to compute character
	 * adjustments (tone/diacritic placements) relative to base letters. */
	m_stdMap[(unsigned char)'\''] = 2; /* acute sắc*/
	m_stdMap[(unsigned char)'`'] = 4;  /* grave huyền */
	m_stdMap[(unsigned char)'?'] = 6;  /* hook hỏi */
	m_stdMap[(unsigned char)'~'] = 8;  /* tilde ngã */
	m_stdMap[(unsigned char)'.'] = 10; /* dot nặng */
	m_stdMap[(unsigned char)'^'] = 12; /* roof modifier dấu mũ ^*/
	/* Additional modifier symbols that shift the base index space */
	m_stdMap[(unsigned char)'('] = 24; /* bowl dấu á: ă*/
	m_stdMap[(unsigned char)'+'] =
		26; /* plus as alternative : Dấu móc (+ hoặc '): a+ = ă, o+ = ơ, u+ = ư*/
	m_stdMap[(unsigned char)'*'] =
		26; /* star as alternative :  dấu sao * được dùng để tạo dấu hỏi (?) trên
				   các ký tự. h*a = hỏa, k*e = kể, m*i = mỉ (tỉ mỉ)*/
}

/* VIQRCharset::startInput
 * Reset input parser flags before processing a new input stream.
 * Steps:
 *  - Clear transient state (`m_suspicious`, `m_gotTone`, `m_escAll`).
 *  - Mark that we are at a word beginning so smart-VIQR heuristics
 *    can apply to the next characters.
 *  - Reset the VIQR escape-pattern matcher when escape handling is
 *    enabled.
 */
void VIQRCharset::startInput()
{
	/* Progression:
	 * 1) Clear transient parser state that can affect VIQR decoding
	 *    heuristics (suspicious sequence detection).
	 * 2) Mark that we are at a word beginning so smart-VIQR rules
	 *    may apply to the next characters read from the stream.
	 * 3) Reset the tone-seen flag since no tone has been observed for
	 *    the (new) next word yet.
	 * 4) Clear the global "escape all" flag which disables VIQR parsing
	 *    for certain tokens; it will be re-enabled while parsing if needed.
	 * 5) If VIQR escape-pattern detection is enabled in options, reset
	 *    the pattern matcher so it starts fresh for the new input.
	 */

	m_suspicious = 0;	   // step 1
	m_atWordBeginning = 1; // step 2
	m_gotTone = 0;		   // step 3
	m_escAll = 0;		   // step 4

	if (VnCharsetLibObj.m_options.viqrEsc)
		VnCharsetLibObj.m_VIQREscPatterns.reset(); // step 5
}

/* VIQRCharset::nextInput
 * Parse the next input byte(s) interpreting VIQR escape sequences.
 * High-level progression:
 *  1) Read one byte `ch1` and consult `m_stdMap` for quick single-byte
 *     or modifier mappings.
 *  2) If escape-pattern detection is enabled, possibly enable
 *     `m_escAll` to suppress VIQR parsing for URL/email-like tokens.
 *  3) Handle explicit backslash escapes by consuming the following
 *     byte when present.
 *  4) For multi-byte VIQR sequences peek the next byte and apply
 *     special rules (e.g., double-D mapping, combining modifiers).
 *  5) Update `m_atWordBeginning` and `m_gotTone` tracking flags and
 *     convert mapped VN indices into the internal StdVnChar range.
 */
int VIQRCharset::nextInput(ByteInStream &is, StdVnChar &stdChar,
						   int &bytesRead)
{
	/* Progression (detailed):
	 * 1) Read one input byte `ch1` and increment `bytesRead`.
	 * 2) Use `m_stdMap[ch1]` to quickly detect whether `ch1` is:
	 *      - 0..255: a raw single-byte character (no VIQR token),
	 *      - >255: an encoded VN token or modifier that requires lookahead.
	 * 3) If VIQR escape-pattern detection is enabled, update `m_escAll`
	 *    to temporarily disable VIQR parsing for URL/email-like tokens.
	 * 4) Handle explicit backslash escapes: if the byte was '\\', try to
	 *    consume the following byte and treat it literally.
	 * 5) If `stdChar < 256` it is a raw byte; return it directly.
	 * 6) Otherwise (a VIQR token), perform lookahead on the next byte(s)
	 *    to resolve combined modifiers (tone, roof, bowl, etc.). Special
	 *    handling exists for doubled 'D' (`dd` -> capital Đ/đ mapping).
	 * 7) Update `m_atWordBeginning` and `m_gotTone` tracking flags.
	 * 8) Convert VN token indices into the internal `StdVnChar` range
	 *    before returning.
	 */

	unsigned char ch1;
	bytesRead = 0;

	/* Step 1: read the next byte; return 0 on end-of-stream. */
	if (!is.getNext(ch1))
		return 0;
	bytesRead = 1;

	/* Step 2: use quick lookup table to classify the byte. */
	stdChar = m_stdMap[ch1];

	/* Step 3: detect escape-patterns (URLs, emails) and set `m_escAll`
	 * which suppresses VIQR parsing while the token is being consumed. */
	if (VnCharsetLibObj.m_options.viqrEsc)
	{
		if (VnCharsetLibObj.m_VIQREscPatterns.foundAtNextChar(ch1) != -1)
		{
			m_escAll = 1;
		}
	}

	/* Clear full-escape mode when whitespace is encountered (end of token). */
	if (m_escAll && (ch1 == ' ' || ch1 == '\t' || ch1 == '\r' || ch1 == '\n'))
		m_escAll = 0;

	/* Step 4: explicit backslash escape - if a backslash was read the
	 * following byte (if present) should be treated literally. */
	if (ch1 == '\\')
	{
		/* try to read next; if succeed, update `stdChar` using the table. */
		if (!is.getNext(ch1))
		{
			bytesRead++;
			stdChar = m_stdMap[ch1];
		}
	}

	/* Step 5: raw byte path: if lookup returned <256 it's not a VIQR token. */
	if (stdChar < 256)
	{
		stdChar = ch1;
	}
	/* Step 6: VIQR token requires lookahead and rules application. Only
	 * attempt if we're not in escape-all mode and stream is not at end. */
	else if (!m_escAll && !is.eos())
	{
		/* Peek next byte (ch2) for multi-byte VIQR sequences and rules. */
		unsigned char ch2;
		is.peekNext(ch2);
		unsigned char upper = toupper(ch1);

		/* Handle special double-'D' mapping when enabled by smartViqr
		 * or at word beginning: `Dd`/`dD`/`DD` represent Đ/đ variants. */
		if ((!VnCharsetLibObj.m_options.smartViqr || m_atWordBeginning) &&
			upper == 'D' && (ch2 == 'd' || ch2 == 'D'))
		{
			is.getNext(ch2);
			bytesRead++;
			stdChar += 2; // 'dd' maps two positions after 'd' in table
		}
		else
		{
			/* Evaluate whether the next byte is a valid modifier/tone to
			 * combine with the current letter. `index` is the lookup of
			 * ch2 in the quick-table and drives the conditional rules. */
			StdVnChar index = m_stdMap[ch2];

			int cond;
			if (m_suspicious)
			{
				/* When in suspicious mode we accept a stricter subset of
				 * modifier combinations to avoid accidental mangling. If
				 * an acceptable combination is found clear `m_suspicious`. */
				cond =
					IS_VOWEL(ch1) &&
					(index == 2 || index == 4 || index == 8 ||
					 (index == 12 && (upper == 'A' || upper == 'E' || upper == 'O')) ||
					 (m_stdMap[ch2] == 24 && upper == 'A') ||
					 (m_stdMap[ch2] == 26 && (upper == 'O' || upper == 'U')));
				if (cond)
					m_suspicious = 0;
			}
			else
				/* Normal mode: accept a modifier if the current char is a
				 * vowel and the modifier index fits tone/breve/hook rules,
				 * taking into account whether a tone has already been seen. */
				cond =
					IS_VOWEL(ch1) &&
					((index <= 10 && index > 0 &&
					  (!m_gotTone || (index != 6 && index != 10))) ||
					 (index == 12 && (upper == 'A' || upper == 'E' || upper == 'O')) ||
					 (m_stdMap[ch2] == 24 && upper == 'A') ||
					 (m_stdMap[ch2] == 26 && (upper == 'O' || upper == 'U')));

			if (cond)
			{
				/* Mark that we've seen a tone/breve/hook in this word. */
				if (index > 0)
					m_gotTone = 1;

				/* Accept the modifier: consume ch2 and add its offset. */
				is.getNext(ch2);
				bytesRead++;
				int offset = m_stdMap[ch2];
				if (offset == 26)
					offset = 24;
				if (offset == 24 && (ch1 == 'u' || ch1 == 'U'))
					offset = 12;
				stdChar += offset;

				/* Some sequences allow a third modifier byte (e.g., tone + roof).
				 * Peek and accept an additional byte when appropriate. */
				if (is.peekNext(ch2))
				{
					if (index > 10 && m_stdMap[ch2] > 0 && m_stdMap[ch2] <= 10)
					{
						is.getNext(ch2);
						bytesRead++;
						stdChar += m_stdMap[ch2];
					}
				}
			}
		}
	}

	/* Step 7: update word-beginning tracking and reset tone flag when
	 * we are at the start of a new raw word. */
	m_atWordBeginning = (stdChar < 256);
	if (stdChar < 256)
	{
		m_gotTone =
			0; // reset this flag because we are at the beginning of a new word
	}

	/* Step 8: convert VN token index into internal StdVnChar range. */
	if (stdChar >= 256)
		stdChar += VnStdCharOffset - 256;
	return 1;
}

/* VIQRCharset::startOutput
 * Prepare internal state used for VIQR output encoding. This clears
 * flags that remember previously emitted diacritic/escape markers so
 * output emission decisions are made from a clean state.
 */
void VIQRCharset::startOutput()
{
	/* Progression:
	 * 1) Reset all per-output escape/diacritic flags so output emission
	 *    begins from a known clean state. These flags track whether the
	 *    last emitted character required escaping for bowl (ă), roof (â/ê/ô),
	 *    hook (ơ/ư), or tone markers — and they influence whether the next
	 *    character must be escaped to avoid ambiguity.
	 * 2) Clear the `m_noOutEsc` guard which temporarily suppresses output
	 *    escaping while certain whitespace or separator characters are emitted.
	 * 3) Reset the `m_VIQROutEscPatterns` matcher so patterns used to
	 *    decide when to emit backslash escapes are restarted for the new
	 *    output stream.
	 */

	m_escapeBowl = 0; // step 1: no bowl escape active
	m_escapeRoof = 0; // step 1: no roof escape active
	m_escapeHook = 0; // step 1: no hook escape active
	m_escapeTone = 0; // step 1: no tone escape active
	m_noOutEsc = 0;	  // step 2: allow escapes by default

	/* step 3: reset pattern matcher used to decide when an output byte
	 * should be prefixed with a backslash to avoid accidental VIQR
	 * interpretation by downstream consumers (URLs, email, etc.). */
	VnCharsetLibObj.m_VIQROutEscPatterns.reset();
}

/* VIQRCharset::putChar
 * Emit the provided `StdVnChar` into the VIQR ASCII representation.
 *
 * Progression (stepwise):
 *  1) Determine whether `stdChar` is a mapped Vietnamese index
 *     (>= VnStdCharOffset) or a raw single-byte value.
 *  2) If mapped: fetch the precomputed output word `dw` from `m_vnChars`
 *     and emit its bytes in low->high order (1..3 bytes). While doing
 *     so update `m_escape*` flags that describe the diacritic state
 *     (bowl, roof, hook, tone) for the next characters.
 *     - Reset the output-escape pattern matcher after emitting a
 *       multi-byte VN word so subsequent bytes can be examined.
 *  3) If raw or unmapped: decide whether the byte needs a leading
 *     backslash to avoid forming a VIQR token (based on `m_escape*`
 *     flags and `m_stdMap`), emit the backslash if required, then
 *     emit the byte itself. Update `m_noOutEsc` according to pattern
 *     matcher results and clear `m_escape*` flags for raw bytes.
 *  4) Return the result status from the last `putB` call and set
 *     `outLen` to the number of bytes written.
 *
 * Notes:
 *  - This function never alters the mapping tables; it only reads
 *    `m_vnChars` and `m_stdMap` to make output decisions.
 */
int VIQRCharset::putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen)
{
	int ret;
	UKBYTE b;
	if (stdChar >= VnStdCharOffset)
	{
		outLen = 1;
		UKDWORD dw = m_vnChars[stdChar - VnStdCharOffset];

		unsigned char first = (unsigned char)dw;
		unsigned char firstUpper = toupper(first);

		/* emit low byte of VN word */
		b = (UKBYTE)dw;
		ret = os.putB(b); /* write first byte */
		if (VnCharsetLibObj.m_VIQROutEscPatterns.foundAtNextChar(b) != -1)
			m_noOutEsc = 1; /* next bytes should not force escaping */

		if (m_noOutEsc && (b == ' ' || b == '\t' || b == '\r' || b == '\n'))
			m_noOutEsc = 0;

		if (dw & 0x0000FF00)
		{
			/* second byte is present: emit and account for length */
			unsigned char second = (UKBYTE)(dw >> 8);
			outLen++;
			ret = os.putB(second); /* write second byte */

			if (dw & 0x00FF0000)
			{
				/* third byte present: emit and clear tone escape (explicit diacritic
				 * emitted) */
				outLen++;
				ret = os.putB((UKBYTE)(dw >> 16)); /* write third byte */
				m_escapeTone = 0;
			}
			else
			{
				/* determine whether this second byte indicates a tone/roof/hook */
				UKWORD index = m_stdMap[second];
				m_escapeTone = (index == 12 || index == 24 || index == 26);
			}

			VnCharsetLibObj.m_VIQROutEscPatterns.reset();

			m_escapeBowl = 0;
			m_escapeHook = 0;
			m_escapeRoof = 0;
		}
		else
		{
			/* single-byte VN mapping: set escape flags based on first char */
			m_escapeTone = IS_VOWEL(first);							 /* whether tone escapes apply */
			m_escapeBowl = (firstUpper == 'A');						 /* bowl (ă) follows 'A' */
			m_escapeHook = (firstUpper == 'U' || firstUpper == 'O'); /* hook (ơ/ư) */
			m_escapeRoof = (firstUpper == 'A' || firstUpper == 'E' ||
							firstUpper == 'O'); /* roof (â/ê/ô) */
		}
	}
	else
	{
		if (stdChar > 255)
		{
			outLen = 1;
			ret = os.putB((UKBYTE)PadChar);
			/* unmappable Unicode: emit PadChar as safe fallback */
			if (VnCharsetLibObj.m_VIQROutEscPatterns.foundAtNextChar(
					(UKBYTE)PadChar) != -1)
				m_noOutEsc = 1; /* avoid forcing escapes for immediate next bytes */
		}
		else
		{
			outLen = 1;
			UKWORD index = m_stdMap[stdChar];
			/* decide whether to prefix a backslash to avoid forming VIQR tokens:
			 * - when not in mixed mode, not currently suppressing escapes, and
			 *   the byte either is a backslash or would combine with previous
			 *   escape flags to form a diacritic/tone sequence. */
			if (!VnCharsetLibObj.m_options.viqrMixed && !m_noOutEsc &&
				(stdChar == '\\' || (index > 0 && index <= 10 && m_escapeTone) ||
				 (index == 12 && m_escapeRoof) || (index == 24 && m_escapeBowl) ||
				 (index == 26 && m_escapeHook)))
			{
				/* emit escape prefix to protect this byte from VIQR interpretation */
				outLen++;
				ret = os.putB('\\');
				if (VnCharsetLibObj.m_VIQROutEscPatterns.foundAtNextChar('\\') != -1)
					m_noOutEsc = 1;
			}
			b = (UKBYTE)stdChar;
			/* emit raw byte and update no-out-escape guard if pattern matches */
			ret = os.putB(b);
			if (VnCharsetLibObj.m_VIQROutEscPatterns.foundAtNextChar(b) != -1)
				m_noOutEsc = 1;
			if (m_noOutEsc && (b == ' ' || b == '\t' || b == '\r' || b == '\n'))
				m_noOutEsc = 0; /* whitespace terminates no-escape mode */
		}
		// reset escape marks
		m_escapeBowl = 0;
		m_escapeRoof = 0;
		m_escapeHook = 0;
		m_escapeTone = 0;
	}
	return ret;
}
