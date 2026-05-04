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
#include "charset.h"

/////////////////////////////////////////////
// Class: UTF8VIQRCharset                  //
/////////////////////////////////////////////

//-----------------------------------------
/**
 * @brief Construct a UTF-8 / VIQR hybrid charset dispatcher.
 *
 * Stores pointers to the UTF-8 and VIQR sub-converters so this object
 * can dispatch input parsing (and delegate output) to the appropriate
 * implementation depending on the stream content.
 *
 * @param[in] pUtf  Pointer to an existing `UnicodeUTF8Charset` instance
 *                  (used for UTF-8 decoding).
 * @param[in] pViqr Pointer to an existing `VIQRCharset` instance
 *                  (used for VIQR decoding/output).
 *
 * Usage example:
 *   auto hybrid = new UTF8VIQRCharset(utf8Obj, viqrObj);
 */
UTF8VIQRCharset::UTF8VIQRCharset(UnicodeUTF8Charset *pUtf, VIQRCharset *pViqr)
{
	m_pUtf = pUtf;
	m_pViqr = pViqr;
}

//-----------------------------------------
/**
 * @brief Prepare both UTF-8 and VIQR sub-converters for input.
 *
 * Forwards `startInput()` to both `m_pUtf` and `m_pViqr` so each
 * implementation can reset parser state (pending UTF-8 bytes,
 * escape flags, suspicious-sequence tracking, etc.) before reading a
 * new input stream.
 *
 * Usage:
 *   UTF8VIQRCharset hybrid(...);
 *   hybrid.startInput(); // resets both sub-converters
 */
void UTF8VIQRCharset::startInput()
{
	m_pUtf->startInput();
	m_pViqr->startInput();
}

//-----------------------------------------
/**
 * @brief Prepare both UTF-8 and VIQR sub-converters for output.
 *
 * This method forwards the output-start call to both the UTF-8 and
 * VIQR implementations so they can reset any per-conversion output state
 * (escape flags, previous-hex markers, etc.) before emission begins.
 *
 * Usage:
 *   UTF8VIQRCharset hybrid(...);
 *   hybrid.startOutput(); // resets both sub-converters
 */
void UTF8VIQRCharset::startOutput()
{
	m_pUtf->startOutput();
	m_pViqr->startOutput();
}

//-----------------------------------------
/**
 * @brief Read the next input and dispatch between UTF-8 and VIQR parsing.
 *
 * Inspects the next byte to decide whether to treat the incoming stream as
 * UTF-8 (multi-byte sequences) or as VIQR (escape-based) and forwards to the
 * appropriate sub-converter.
 *
 * @param[in,out] is Input byte stream
 * @param[out] stdChar Resulting standard Vietnamese character
 * @param[out] bytesRead Number of input bytes consumed
 * @return 1 if a character was read successfully, 0 on end-of-stream
 */
int UTF8VIQRCharset::nextInput(ByteInStream &is, StdVnChar &stdChar,
							   int &bytesRead)
{
	/* Progression:
	 * 1) Peek the next input byte without consuming it. If none available
	 *    return 0 (end-of-stream).
	 * 2) If the peeked byte looks like a UTF-8 lead byte (0xC0..0xFD),
	 *    treat the stream as UTF-8: reset VIQR sub-state, mark it
	 *    suspicious to avoid mis-parsing, and delegate to the UTF-8
	 *    sub-converter's `nextInput` which will consume the full
	 *    multi-byte sequence and set `bytesRead`/`stdChar`.
	 * 3) Otherwise, dispatch to the VIQR parser which interprets
	 *    escape-based sequences and sets `bytesRead`/`stdChar`.
	 * 4) This dispatcher does not itself consume bytes or set
	 *    `bytesRead` — the delegated sub-converter does that.
	 * 5) Return the delegated parser's return value (1 on success, 0
	 *    on end-of-stream).
	 */

	UKBYTE ch;

	if (!is.peekNext(ch))
		return 0;

	/* If the byte is in the UTF-8 lead-byte range, use UTF-8 parser. */
	if (ch > 0xBF && ch < 0xFE)
	{
		m_pViqr->startInput(); // reset VIQR state to avoid leftover heuristics
		m_pViqr->m_suspicious = 1;
		return m_pUtf->nextInput(is, stdChar, bytesRead);
	}

	/* Otherwise treat as VIQR-encoded input. */
	return m_pViqr->nextInput(is, stdChar, bytesRead);
}

//-----------------------------------------
/**
 * @brief Emit a standard Vietnamese character using the VIQR output codec.
 *
 * This hybrid charset delegates output to the VIQR implementation so the
 * same escaping and output rules are applied consistently.
 *
 * @param[out] os Output byte stream
 * @param[in] stdChar Standard Vietnamese character to encode
 * @param[out] outLen Number of bytes written
 * @return Result of the last write operation (nonzero on success)
 */
int UTF8VIQRCharset::putChar(ByteOutStream &os, StdVnChar stdChar,
							 int &outLen)
{
	return m_pViqr->putChar(os, stdChar, outLen);
}

