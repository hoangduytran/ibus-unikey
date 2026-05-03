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

#include "charset.h"

//////////////////////////////////////////////////////
// Generic VnCharset class
//////////////////////////////////////////////////////
/**
 * @brief Returns the element size (in bytes) for the current charset encoding.
 *
 * The default implementation returns 1, which is suitable for single-byte
 * encodings. Subclasses override this method to report their actual element
 * size (e.g., 2 for UTF-16, 4 for internal formats).
 *
 * @return Number of bytes per character element for this charset.
 *
 * @usage
 *   VnCharset *cs = ...;
 *   int size = cs->elementSize();
 *   // Use 'size' to allocate buffers or process streams accordingly
 */
int VnCharset::elementSize() { return 1; }

//-------------------------------------------
/**
 * @brief Reads the next character from the input stream in internal charset
 * format.
 *
 * This method attempts to read the next double word (4 bytes) from the input
 * stream and stores it in stdChar. If successful, bytesRead is set to the
 * number of bytes read (4), otherwise 0.
 *
 * @param[in,out] is      Input byte stream to read from.
 * @param[out] stdChar    Receives the standard Vietnamese character read from
 * the stream.
 * @param[out] bytesRead  Receives the number of bytes read (4 if successful, 0
 * otherwise).
 * @return 1 if a character was read, 0 if end of stream or error.
 *
 * @usage
 *   VnInternalCharset cs;
 *   ByteInStream is(...);
 *   StdVnChar ch;
 *   int bytes;
 *   while (cs.nextInput(is, ch, bytes)) {
 *       // process ch
 *   }
 */
int VnInternalCharset::nextInput(ByteInStream &is, StdVnChar &stdChar,
								 int &bytesRead)
{
	if (!is.getNextDW(stdChar))
	{
		bytesRead = 0;
		return 0;
	}
	bytesRead = sizeof(UKDWORD);
	return 1;
}

//-------------------------------------------
/**
 * @brief Writes a standard Vietnamese character to the output stream in
 * internal charset format.
 *
 * This method writes the character as a double word (4 bytes) to the output
 * stream.
 *
 * @param[out] os         Output byte stream to write to.
 * @param[in] stdChar     The standard Vietnamese character to write.
 * @param[out] outLen     Receives the number of bytes written (always 4).
 * @return The result of the last write operation (nonzero if successful).
 *
 * @usage
 *   VnInternalCharset cs;
 *   ByteOutStream os(...);
 *   StdVnChar ch = ...;
 *   int written;
 *   cs.putChar(os, ch, written);
 */
int VnInternalCharset::putChar(ByteOutStream &os, StdVnChar stdChar,
							   int &outLen)
{
	outLen = sizeof(StdVnChar);
	os.putW((UKWORD)stdChar);
	return os.putW((UKWORD)(stdChar >> (sizeof(UKWORD) * 8)));
}

//-------------------------------------------
/**
 * @brief Returns the element size (in bytes) for the internal charset.
 *
 * The internal representation stores one character in a 4-byte `StdVnChar`.
 *
 * @return Number of bytes per element, always 4.
 */
int VnInternalCharset::elementSize() { return 4; }

//-------------------------------------------
/**
 * @brief Constructs a single-byte charset converter.
 *
 * Builds a reverse lookup table from encoded single-byte values to internal
 * standard Vietnamese character indices.
 *
 * @param[in] vnChars Mapping table from standard Vietnamese index to encoded
 * single-byte value.
 */
SingleByteCharset::SingleByteCharset(unsigned char *vnChars)
{
	int i;
	m_vnChars = vnChars;
	memset(m_stdMap, 0, 256 * sizeof(UKWORD));
	for (i = 0; i < TOTAL_VNCHARS; i++)
	{
		if (vnChars[i] != 0 && (i == TOTAL_VNCHARS - 1 || vnChars[i] != vnChars[i + 1]))
			m_stdMap[vnChars[i]] = i + 1;
	}
}

//-------------------------------------------
/**
 * @brief Reads the next single-byte character from the input stream and
 * converts it to a standard Vietnamese character.
 *
 * @param[in,out] is      Input byte stream to read from.
 * @param[out] stdChar    Receives the standard Vietnamese character.
 * @param[out] bytesRead  Receives the number of bytes read (always 1 if
 * successful).
 * @return 1 if a character was read, 0 if end of stream or error.
 *
 * @usage
 *   SingleByteCharset cs(...);
 *   ByteInStream is(...);
 *   StdVnChar ch;
 *   int bytes;
 *   while (cs.nextInput(is, ch, bytes)) {
 *       // process ch
 *   }
 */
int SingleByteCharset::nextInput(ByteInStream &is, StdVnChar &stdChar,
								 int &bytesRead)
{
	unsigned char ch;
	if (!is.getNext(ch))
	{
		bytesRead = 0;
		return 0;
	}

	stdChar = (m_stdMap[ch]) ? (VnStdCharOffset + m_stdMap[ch] - 1) : ch;
	bytesRead = 1;
	return 1;
}

//-------------------------------------------
/**
 * @brief Writes a standard Vietnamese character to the output stream in
 * single-byte charset format.
 *
 * Converts the standard character to its single-byte representation and writes
 * it to the output stream. If the character is not representable, writes a
 * padding character.
 *
 * @param[out] os         Output byte stream to write to.
 * @param[in] stdChar     The standard Vietnamese character to write.
 * @param[out] outLen     Receives the number of bytes written (always 1).
 * @return The result of the write operation (nonzero if successful).
 *
 * @usage
 *   SingleByteCharset cs(...);
 *   ByteOutStream os(...);
 *   StdVnChar ch = ...;
 *   int written;
 *   cs.putChar(os, ch, written);
 */
int SingleByteCharset::putChar(ByteOutStream &os, StdVnChar stdChar,
							   int &outLen)
{
	int ret;
	unsigned char ch;
	if (stdChar >= VnStdCharOffset)
	{
		outLen = 1;
		ch = m_vnChars[stdChar - VnStdCharOffset];
		if (ch == 0)
			ch = (stdChar == StdStartQuote)
					 ? PadStartQuote
					 : ((stdChar == StdEndQuote)
							? PadEndQuote
							: ((stdChar == StdEllipsis) ? PadEllipsis : PadChar));
		ret = os.putB(ch);
	}
	else
	{
		if (stdChar > 255 || m_stdMap[stdChar])
		{
			// this character is missing in the charset
			//  output padding character
			outLen = 1;
			ret = os.putB(PadChar);
		}
		else
		{
			outLen = 1;
			ret = os.putB((UKBYTE)stdChar);
		}
	}
	return ret;
}
