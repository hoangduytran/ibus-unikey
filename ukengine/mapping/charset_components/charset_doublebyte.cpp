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
#include <memory.h>
#include <stdlib.h>

#include "charset.h"
#include "charset_components/charset_internal.h"

/////////////////////////////////
// Double-byte charsets        //
/////////////////////////////////
/**
 * @brief Constructs a double-byte charset converter.
 *
 * Initializes the mapping from double-byte encoded characters to standard
 * Vietnamese characters.
 *
 * @param[in] vnChars  Pointer to the array of double-byte Vietnamese
 * characters.
 *
 * @usage
 *   UKWORD *vnChars = ...;
 *   DoubleByteCharset cs(vnChars);
 */
DoubleByteCharset::DoubleByteCharset(UKWORD *vnChars)
{
	// Keep the caller-provided double-byte mapping table for output conversion.
	m_toDoubleChar = vnChars;

	// Clear the reverse lookup table so every byte starts out unmapped.
	memset(m_stdMap, 0, 256 * sizeof(UKWORD));

	// Build both the reverse lookup table and the searchable character table.
	for (int i = 0; i < TOTAL_VNCHARS; i++)
	{
		// If the mapped character spans two bytes, mark its high byte as invalid
		// for single-byte lookup so it will not be treated as an ordinary char.
		if (vnChars[i] >> 8)					// a 2-byte character
			m_stdMap[vnChars[i] >> 8] = 0xFFFF; // INVALID_STD_CHAR;
		// Otherwise record the single-byte value only if it has not been seen yet.
		else if (m_stdMap[vnChars[i]] == 0)
			m_stdMap[vnChars[i]] = i + 1;

		// Store the standard-char index in the high word so qsort/bsearch can
		// later recover the VN index after sorting by the low word value.
		m_vnChars[i] =
			(i << 16) + vnChars[i]; // high word is used for StdChar index
	}

	// Sort once up front so lookups during input parsing can use binary search.
	qsort(m_vnChars, TOTAL_VNCHARS, sizeof(UKDWORD), wideCharCompare);
}

//---------------------------------------------
/**
 * @brief Reads the next double-byte character from the input stream and
 * converts it to a standard Vietnamese character.
 *
 * Handles both single and double-byte sequences, mapping them to the internal
 * standard character.
 *
 * @param[in,out] is      Input byte stream to read from.
 * @param[out] stdChar    Receives the standard Vietnamese character.
 * @param[out] bytesRead  Receives the number of bytes read (1 or 2 if
 * successful).
 * @return 1 if a character was read, 0 if end of stream or error.
 *
 * @usage
 *   DoubleByteCharset cs(...);
 *   ByteInStream is(...);
 *   StdVnChar ch;
 *   int bytes;
 *   while (cs.nextInput(is, ch, bytes)) {
 *       // process ch
 *   }
 */
int DoubleByteCharset::nextInput(ByteInStream &is, StdVnChar &stdChar,
								 int &bytesRead)
{
	unsigned char ch;

	// read first byte
	bytesRead = 0;
	if (!is.getNext(ch))
		return 0;
	bytesRead = 1;
	/*
	 * Progression and steps:
	 * 1) Read one input byte into `ch`. If none available, return 0.
	 * 2) Initialize `bytesRead` to 1 since we've consumed the first byte.
	 * 3) Look up `ch` in `m_stdMap`:
	 *    - If the map entry is 0: `ch` is not a lead byte for a double-byte
	 *      sequence and represents an ordinary single-byte character; return
	 *      it directly by setting `stdChar = ch`.
	 *    - If the map entry is 0xFFFF: this byte value is marked invalid for
	 *      this charset; set `stdChar = INVALID_STD_CHAR`.
	 *    - Otherwise the map contains a non-zero index indicating `ch` may be
	 *      the first byte (lead) of a double-byte character. In that case we
	 *      convert the stored value to a `StdVnChar` base and try to read the
	 *      second byte to resolve a full two-byte character.
	 */
	stdChar = m_stdMap[ch];
	if (stdChar == 0)
	{
		// Single-byte character: no mapping in high-byte table
		stdChar = ch;
	}
	else if (stdChar == 0xFFFF)
	{
		// Explicitly marked invalid lead byte for this double-byte encoding
		stdChar = INVALID_STD_CHAR;
	}
	else
	{
		/*
		 * The lead byte indicates a possible double-byte sequence. The stored
		 * value in m_stdMap is an index (i+1) that we convert to the internal
		 * StdVnChar range by adding `VnStdCharOffset - 1`.
		 */
		stdChar += VnStdCharOffset - 1;
		UKBYTE hi;

		/*
		 * Peek at the next byte (do not consume it yet). If peek succeeds and
		 * the next byte is non-zero, form a composite word (low=first byte,
		 * high=second byte) and binary-search the sorted `m_vnChars` table to
		 * see if this two-byte pair corresponds to a mapped Vietnamese char.
		 */
		if (is.peekNext(hi) && hi > 0)
		{
			// Build lookup key and search the mapping table
			UKDWORD key = MAKEWORD(ch, hi);
			UKDWORD *pChar = (UKDWORD *)bsearch(&key, m_vnChars, TOTAL_VNCHARS,
												sizeof(UKDWORD), wideCharCompare);
			if (pChar)
			{
				/*
				 * Found a matching double-byte character entry. The high word of
				 * *pChar holds the StdChar index; translate it to the engine's
				 * StdVnChar encoding and consume the second byte from the stream.
				 */
				stdChar = VnStdCharOffset + HIWORD(*pChar);
				bytesRead = 2;	// two bytes consumed for this character
				is.getNext(hi); // actually remove the second byte from the stream
			}
		}
	}
	return 1;
}

//---------------------------------------------
/**
 * @brief Writes a standard Vietnamese character to the output stream in
 * double-byte charset format.
 *
 * Converts the standard character to its double-byte representation and writes
 * it to the output stream. If the character is not representable, writes a
 * padding character.
 *
 * @param[out] os         Output byte stream to write to.
 * @param[in] stdChar     The standard Vietnamese character to write.
 * @param[out] outLen     Receives the number of bytes written (1 or 2).
 * @return The result of the write operation (nonzero if successful).
 *
 * @usage
 *   DoubleByteCharset cs(...);
 *   ByteOutStream os(...);
 *   StdVnChar ch = ...;
 *   int written;
 *   cs.putChar(os, ch, written);
 */
int DoubleByteCharset::putChar(ByteOutStream &os, StdVnChar stdChar,
							   int &outLen)
{
	/**
	 * @brief Emit a standard Vietnamese character using double-byte encoding.
	 *
	 * Progression and behavior:
	 * - If `stdChar` is in the mapped VN range (>= VnStdCharOffset):
	 *     * Look up the corresponding double-byte value `wCh` from
	 * `m_toDoubleChar`.
	 *     * If `wCh` has a high byte (wCh & 0xFF00):
	 *         - Emit the low byte followed by the high byte (two output bytes).
	 *         - Set `outLen` to 2 and return the result of the last write.
	 *     * Otherwise `wCh` fits in one byte:
	 *         - Check whether this single-byte value is marked invalid in
	 *           `m_stdMap` (value 0xFFFF). If invalid, emit `PadChar` instead.
	 *         - Emit the single byte and set `outLen` to 1.
	 *
	 * - If `stdChar` is not a mapped VN index (raw char value):
	 *     * If the value is out of single-byte range (>255) or maps to a
	 *       lead byte in `m_stdMap` (meaning it cannot be emitted alone),
	 *       emit `PadChar` instead (outLen=1).
	 *     * Otherwise emit the raw single byte value (outLen=1).
	 */

	int ret;

	// Case A: stdChar is a mapped Vietnamese character index.
	if (stdChar >= VnStdCharOffset)
	{
		// Resolve to the configured double-byte representation.
		UKWORD wCh = m_toDoubleChar[stdChar - VnStdCharOffset];

		// If high byte is present, this is a true 2-byte encoding.
		if (wCh & 0xFF00)
		{
			outLen = 2;						   // two bytes will be written
			os.putB((UKBYTE)(wCh & 0x00FF));   // write low byte first
			ret = os.putB((UKBYTE)(wCh >> 8)); // then high byte
		}
		else
		{
			// Single-byte representation stored in wCh's low byte.
			unsigned char b = (unsigned char)wCh;

			// If this byte is flagged as an invalid lead byte for this
			// double-byte charset, emit the PadChar instead.
			if (m_stdMap[b] == 0xFFFF)
				b = PadChar;

			outLen = 1;
			ret = os.putB(b);
		}
	}
	else
	{
		// Case B: stdChar is a raw value (not a VN index).
		// If it's outside the single-byte range or would collide with
		// a lead byte in this double-byte encoding, emit PadChar.
		if (stdChar > 255 || m_stdMap[stdChar])
		{
			outLen = 1;
			ret = os.putB((UKBYTE)PadChar);
		}
		else
		{
			// Safe to emit the raw single byte value.
			outLen = 1;
			ret = os.putB((UKBYTE)stdChar);
		}
	}

	return ret;
}
