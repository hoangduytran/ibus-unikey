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

/////////////////////////////////////////////
// Class WinCP1258Charset
/////////////////////////////////////////////
WinCP1258Charset::WinCP1258Charset(UKWORD *compositeChars,
								   UKWORD *precomposedChars)
{
	int i, k;
	m_toDoubleChar = compositeChars;
	memset(m_stdMap, 0, 256 * sizeof(UKWORD));

	// encode composite chars
	for (i = 0; i < TOTAL_VNCHARS; i++)
	{
		if (compositeChars[i] >> 8)					   // a 2-byte character
			m_stdMap[compositeChars[i] >> 8] = 0xFFFF; // INVALID_STD_CHAR;
		else if (m_stdMap[compositeChars[i]] == 0)
			m_stdMap[compositeChars[i]] = i + 1;

		m_vnChars[i] =
			(i << 16) + compositeChars[i]; // high word is used for StdChar index
	}

	m_totalChars = TOTAL_VNCHARS;

	// add precomposed chars to the table
	for (k = 0, i = TOTAL_VNCHARS; k < TOTAL_VNCHARS; k++)
		if (precomposedChars[k] != compositeChars[k])
		{
			if (precomposedChars[k] >> 8)					 // a 2-byte character
				m_stdMap[precomposedChars[k] >> 8] = 0xFFFF; // INVALID_STD_CHAR;
			else if (m_stdMap[precomposedChars[k]] == 0)
				m_stdMap[precomposedChars[k]] = k + 1;

			m_vnChars[i] = (k << 16) + precomposedChars[k];
			m_totalChars++;
			i++;
		}

	qsort(m_vnChars, m_totalChars, sizeof(UKDWORD), wideCharCompare);
}

// This function is basically the same as that of DoubleByteCharset
// with m_totalChars used instead of the constant TOTAL_VNCHARS
int WinCP1258Charset::nextInput(ByteInStream &is, StdVnChar &stdChar,
								int &bytesRead)
{
	unsigned char ch;

	// read first byte
	bytesRead = 0;
	if (!is.getNext(ch))
		return 0;
	bytesRead = 1;
	stdChar = m_stdMap[ch];
	if (stdChar == 0)
		stdChar = ch;
	else if (stdChar == 0xFFFF)
		stdChar = INVALID_STD_CHAR;
	else
	{
		stdChar += VnStdCharOffset - 1;
		UKBYTE hi;
		if (is.peekNext(hi) && hi > 0)
		{
			// test if a double-byte character is encountered
			UKDWORD key = MAKEWORD(ch, hi);
			UKDWORD *pChar = (UKDWORD *)bsearch(&key, m_vnChars, m_totalChars,
												sizeof(UKDWORD), wideCharCompare);
			if (pChar)
			{
				stdChar = VnStdCharOffset + HIWORD(*pChar);
				bytesRead = 2;
				is.getNext(hi);
			}
		}
	}
	return 1;
}

// This fuction is exactly the same as that of DoubleByteCharset
int WinCP1258Charset::putChar(ByteOutStream &os, StdVnChar stdChar,
							  int &outLen)
{
	int ret;
	if (stdChar >= VnStdCharOffset)
	{
		UKWORD wCh = m_toDoubleChar[stdChar - VnStdCharOffset];

		if (wCh & 0xFF00)
		{
			outLen = 2;
			os.putB((UKBYTE)(wCh & 0x00FF));
			ret = os.putB((UKBYTE)(wCh >> 8));
		}
		else
		{
			unsigned char b = (unsigned char)wCh;
			if (m_stdMap[b] == 0xFFFF)
				b = PadChar;
			outLen = 1;
			ret = os.putB(b);
		}
	}
	else
	{
		if (stdChar > 255 || m_stdMap[stdChar])
		{
			outLen = 1;
			ret = os.putB((UKBYTE)PadChar);
		}
		else
		{
			outLen = 1;
			ret = os.putB((UKBYTE)stdChar);
		}
	}
	return ret;
}

