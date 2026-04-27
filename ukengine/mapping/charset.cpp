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
#include <search.h>
#include <stddef.h>
#include <stdlib.h>

#include "charset.h"

/**
 * @brief Lowercase vowel lookup table.
 *
 * Each index corresponds to a lowercase ASCII letter 'a'..'z'.
 * Nonzero means the letter is a Vietnamese vowel.
 *
 * Usage:
 *   if (LoVowel['a'-'a']) // 'a' is a vowel
 */
int LoVowel['z' - 'a' + 1];

/**
 * @brief Uppercase vowel lookup table.
 *
 * Mirrors LoVowel for 'A'..'Z'.
 * Usage is identical to LoVowel, but for uppercase.
 */
int HiVowel['Z' - 'A' + 1];

/**
 * @brief Macro to check if a character is a Vietnamese vowel (upper or lower).
 *
 * Usage:
 *   if (IS_VOWEL(c)) { ... }
 */
#define IS_VOWEL(x)                                \
	((x >= 'a' && x <= 'z' && LoVowel[x - 'a']) || \
	 (x >= 'A' && x <= 'Z' && HiVowel[x - 'A']))

/**
 * @brief Array of pointers to single-byte charset converters.
 *
 * Indexed by CONV_* IDs. Used by the engine to convert byte streams to
 * StdVnChar. Example: SingleByteCharset *latin1 = SgCharsets[CONV_LATIN1]; if
 * (latin1) latin1->nextInput(is, stdChar, bytesRead);
 */
SingleByteCharset *SgCharsets[CONV_TOTAL_SINGLE_CHARSETS];

/**
 * @brief Array of pointers to double-byte charset converters.
 *
 * Used for multi-byte legacy encodings.
 */
DoubleByteCharset *DbCharsets[CONV_TOTAL_DOUBLE_CHARSETS];

/**
 * @brief Central charset library object exported from the mapping module.
 *
 * Provides registration, lookup, and lifetime management for available charset
 * converters. Example: CVnCharsetLib &lib = VnCharsetLibObj;
 */
DllExport CVnCharsetLib VnCharsetLibObj;

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

/**
 * @brief Comparison function for sorting/searching arrays of wide (double-word)
 * characters.
 *
 * Compares the low word (16 bits) of two 32-bit values, typically used for
 * sorting or searching arrays of wide characters (UKDWORD) where the low word
 * represents the character value.
 *
 * @param ele1 Pointer to the first element (const void*), expected to point to
 * a UKDWORD.
 * @param ele2 Pointer to the second element (const void*), expected to point to
 * a UKDWORD.
 * @return 0 if equal, 1 if first > second, -1 if first < second (for use with
 * qsort/bsearch).
 *
 * @usage
 *   // Example: Sorting an array of UKDWORDs by their low word
 *   qsort(array, count, sizeof(UKDWORD), wideCharCompare);
 *
 *   // Example: Searching for a character in a sorted array
 *   UKDWORD key = ...;
 *   UKDWORD *found = (UKDWORD*)bsearch(&key, array, count, sizeof(UKDWORD),
 * wideCharCompare);
 */
int wideCharCompare(const void *ele1, const void *ele2)
{
	UKWORD ch1 = LOWORD(*((UKDWORD *)ele1));
	UKWORD ch2 = LOWORD(*((UKDWORD *)ele2));
	return (ch1 == ch2) ? 0 : ((ch1 > ch2) ? 1 : -1);
}

//-------------------------------------------
/**
 * @brief Constructs a Unicode charset converter.
 *
 * Initializes the mapping from Unicode characters to standard Vietnamese
 * characters.
 *
 * @param[in] vnChars  Pointer to the array of Unicode Vietnamese characters.
 *
 * @usage
 *   UnicodeChar *vnChars = ...;
 *   UnicodeCharset cs(vnChars);
 */
UnicodeCharset::UnicodeCharset(UnicodeChar *vnChars)
{
	UKDWORD i;
	m_toUnicode = vnChars;
	for (i = 0; i < TOTAL_VNCHARS; i++)
		m_vnChars[i] = (i << 16) + vnChars[i]; // high word is used for index
	qsort(m_vnChars, TOTAL_VNCHARS, sizeof(UKDWORD), wideCharCompare);
}

//-------------------------------------------
/**
 * @brief Reads the next Unicode character from the input stream and converts it
 * to a standard Vietnamese character.
 *
 * @param[in,out] is      Input byte stream to read from.
 * @param[out] stdChar    Receives the standard Vietnamese character.
 * @param[out] bytesRead  Receives the number of bytes read (always 2 if
 * successful).
 * @return 1 if a character was read, 0 if end of stream or error.
 *
 * @usage
 *   UnicodeCharset cs(...);
 *   ByteInStream is(...);
 *   StdVnChar ch;
 *   int bytes;
 *   while (cs.nextInput(is, ch, bytes)) {
 *       // process ch
 *   }
 */
int UnicodeCharset::nextInput(ByteInStream &is, StdVnChar &stdChar,
							  int &bytesRead)
{
	UnicodeChar uniCh;
	if (!is.getNextW(uniCh))
	{
		bytesRead = 0;
		return 0;
	}
	bytesRead = sizeof(UnicodeChar);
	UKDWORD key = uniCh;
	UKDWORD *pChar = (UKDWORD *)bsearch(&key, m_vnChars, TOTAL_VNCHARS,
										sizeof(UKDWORD), wideCharCompare);
	if (pChar)
		stdChar = VnStdCharOffset + HIWORD(*pChar);
	else
		stdChar = uniCh;
	return 1;
}

//-------------------------------------------
/**
 * @brief Writes a standard Vietnamese character to the output stream in Unicode
 * format.
 *
 * Converts the standard character to its Unicode representation and writes it
 * to the output stream.
 *
 * @param[out] os         Output byte stream to write to.
 * @param[in] stdChar     The standard Vietnamese character to write.
 * @param[out] outLen     Receives the number of bytes written (always 2).
 * @return The result of the write operation (nonzero if successful).
 *
 * @usage
 *   UnicodeCharset cs(...);
 *   ByteOutStream os(...);
 *   StdVnChar ch = ...;
 *   int written;
 *   cs.putChar(os, ch, written);
 */
int UnicodeCharset::putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen)
{
	outLen = sizeof(UnicodeChar);
	return os.putW((stdChar >= VnStdCharOffset)
					   ? m_toUnicode[stdChar - VnStdCharOffset]
					   : (UnicodeChar)stdChar);
}

//-------------------------------------------
/**
 * @brief Returns the element size (in bytes) for the Unicode charset encoding.
 *
 * Always returns 2, as Unicode uses 2 bytes per character.
 *
 * @return Number of bytes per character element (2).
 *
 * @usage
 *   UnicodeCharset cs(...);
 *   int size = cs.elementSize(); // size == 2
 */
int UnicodeCharset::elementSize() { return 2; }

////////////////////////////////////////
// Unicode decomposed
////////////////////////////////////////

/* VIQR tone markers and escape patterns are declared later near
 * the VIQRCharset implementation to keep related code together. */
int uniCompInfoCompare(const void *ele1, const void *ele2)
{
	UKDWORD ch1 = ((UniCompCharInfo *)ele1)->compChar;
	UKDWORD ch2 = ((UniCompCharInfo *)ele2)->compChar;
	return (ch1 == ch2) ? 0 : ((ch1 > ch2) ? 1 : -1);
}

//-------------------------------------------
/**
 * @brief Construct a composed Unicode charset converter with support for both
 * decomposed and precomposed Vietnamese characters.
 *
 * The constructor builds an internal lookup table of composed character values
 * and their standard Vietnamese indices, then sorts the table to enable fast
 * binary search during input conversion.
 *
 * @param[in] uniChars      Unicode mapping table for base standard Vietnamese
 * chars.
 * @param[in] uniCompChars  Unicode mapping table for composed (precomposed)
 * chars.
 *
 * @usage
 *   UnicodeCompCharset cs(UnicodeTable, UnicodeComposite);
 *   // cs.nextInput() will accept both decomposed and composed Unicode forms.
 */
UnicodeCompCharset::UnicodeCompCharset(UnicodeChar *uniChars,
									   UKDWORD *uniCompChars)
{

	int i, k;
	/*
	 * Store the provided composed/unicode tables and build an internal
	 * UniCompCharInfo array (`m_info`) that contains both the precomposed
	 * composite codepoints (`uniCompChars`) and any distinct base unicode
	 * codepoints (`uniChars`) that differ from the composed table. The
	 * resulting `m_info` array is sorted to allow binary-search lookups
	 * during input conversion.
	 */
	m_uniCompChars = uniCompChars;
	m_totalChars = 0;

	/* First, copy all precomposed entries from `uniCompChars` into m_info.
	 * Each entry's `compChar` holds the (possibly two-word) composed value
	 * and `stdIndex` stores the index into the standard Vietnamese table.
	 */
	for (i = 0; i < TOTAL_VNCHARS; i++)
	{
		m_info[i].compChar = uniCompChars[i];
		m_info[i].stdIndex = i;
		m_totalChars++;
	}

	/*
	 * Next, append any Unicode base characters from `uniChars` that are not
	 * identical to the corresponding `uniCompChars` entry. These are the
	 * decomposed or alternate forms which must also be recognized by the
	 * converter. We start writing these additional entries at index
	 * `TOTAL_VNCHARS` in `m_info`.
	 */
	for (k = 0, i = TOTAL_VNCHARS; k < TOTAL_VNCHARS; k++)
	{
		if (uniChars[k] != uniCompChars[k])
		{
			/* Add the distinct base unicode codepoint as an entry. */
			m_info[i].compChar = uniChars[k];
			m_info[i].stdIndex = k;
			m_totalChars++;
			i++;
		}
	}

	/* Finally, sort the table by `compChar` so lookups via bsearch are fast. */
	qsort(m_info, m_totalChars, sizeof(UniCompCharInfo), uniCompInfoCompare);
}

//---------------------------------------------
/**
 * @brief Read the next Unicode character from the input stream and resolve
 * composed forms.
 *
 * This method supports both single UTF-16 code units and composed Unicode pairs
 * by looking up the first code unit in the prebuilt composed-character table.
 * If the first unit matches a base character, it also peeks at the next unit to
 * resolve a combined character sequence.
 *
 * @param[in,out] is      Byte input stream providing UTF-16 code units.
 * @param[out] stdChar    Receives the resulting standard Vietnamese character.
 * @param[out] bytesRead  Receives the number of bytes consumed (2 or 4).
 * @return 1 if a character was successfully read, 0 on end-of-stream or error.
 *
 * @usage
 *   UnicodeCompCharset cs(...);
 *   StdVnChar ch;
 *   int bytes;
 *   if (cs.nextInput(is, ch, bytes)) {
 *       // process ch
 *   }
 */
int UnicodeCompCharset::nextInput(ByteInStream &is, StdVnChar &stdChar,
								  int &bytesRead)
{
	// read first UTF-16 code unit from the input stream
	UniCompCharInfo key;
	UKWORD w;
	if (!is.getNextW(w))
	{
		bytesRead = 0;
		return 0;
	}

	// Start with the low word as the candidate composed character.
	key.compChar = w;
	bytesRead = 2;

	// Lookup the first code unit in the composed-character table.
	UniCompCharInfo *pInfo = (UniCompCharInfo *)bsearch(
		&key, m_info, m_totalChars, sizeof(UniCompCharInfo), uniCompInfoCompare);

	if (!pInfo)
	{
		// No composed match; treat this code unit as a raw Unicode character.
		stdChar = key.compChar;
	}
	else
	{
		// Found a base mapping for the first code unit.
		stdChar = pInfo->stdIndex + VnStdCharOffset;

		// Peek the next unit to see if this is actually a two-unit composed
		// character.
		if (is.peekNextW(w))
		{
			UKDWORD hi = w;
			if (hi > 0)
			{
				// Combine the high word and low word into a full composed codepoint.
				key.compChar += hi << 16;

				// Re-run the lookup with the full composed sequence.
				pInfo = (UniCompCharInfo *)bsearch(&key, m_info, m_totalChars,
												   sizeof(UniCompCharInfo),
												   uniCompInfoCompare);
				if (pInfo)
				{
					// Replace the result with the composed character's standard index.
					is.getNextW(w);
				}
			}
		}
	}
	return 1;
}

//---------------------------------------------
/**
 * @brief Write a standard Vietnamese character to the output stream using
 * composed Unicode encoding.
 *
 * This method converts a standard character index back into its composed
 * Unicode code units. If the input is a mapped Vietnamese character, it writes
 * the precomputed composed form (lo/hi words). Otherwise it emits the character
 * value directly as a single UTF-16 unit.
 *
 * @param[out] os         Output stream to write Unicode code units into.
 * @param[in] stdChar     Standard Vietnamese character to encode.
 * @param[out] outLen     Receives bytes written (2 or 4).
 * @return The result of the final write operation (nonzero if successful).
 *
 * @usage
 *   UnicodeCompCharset cs(...);
 *   int len;
 *   cs.putChar(os, stdChar, len);
 */
int UnicodeCompCharset::putChar(ByteOutStream &os, StdVnChar stdChar,
								int &outLen)
{
	int ret;

	// If the character is a mapped standard Vietnamese char, emit the composed
	// Unicode form.
	if (stdChar >= VnStdCharOffset)
	{
		// Lookup the composed Unicode codepoint for this standard char index.
		UKDWORD uniCompCh = m_uniCompChars[stdChar - VnStdCharOffset];

		// Split the composed value into low and high UTF-16 words.
		UKWORD lo = LOWORD(uniCompCh);
		UKWORD hi = HIWORD(uniCompCh);

		// Always write the first UTF-16 unit.
		outLen = 2;
		ret = os.putW(lo);

		// If the composed character requires a second UTF-16 unit, write it too.
		if (hi > 0)
		{
			outLen += 2;
			ret = os.putW(hi);
		}
	}
	else
	{
		// Non-mapped values are emitted directly as a single UTF-16 unit.
		outLen = 2;
		ret = os.putW((UKWORD)stdChar);
	}

	return ret;
}

//-------------------------------------------
/**
 * @brief Returns the byte size of a Unicode element for composed Unicode
 * output.
 *
 * This charset always uses UTF-16 code units, so the logical element size is 2
 * bytes. The engine uses this value when allocating output buffers and
 * interpreting stream lengths.
 *
 * @return Number of bytes per Unicode element (2).
 *
 * @usage
 *   UnicodeCompCharset cs(...);
 *   int size = cs.elementSize(); // size == 2
 */
int UnicodeCompCharset::elementSize() { return 2; }

////////////////////////////////
// Unicode UTF-8              //
////////////////////////////////
/**
 * @brief Reads the next UTF-8 encoded character from the input stream and
 * converts it to a standard Vietnamese character.
 *
 * Handles 1, 2, or 3-byte UTF-8 sequences and translates them to the internal
 * standard character.
 *
 * @param[in,out] is      Input byte stream to read from.
 * @param[out] stdChar    Receives the standard Vietnamese character.
 * @param[out] bytesRead  Receives the number of bytes read (1, 2, or 3 if
 * successful).
 * @return 1 if a character was read, 0 if end of stream or error.
 *
 * @usage
 *   UnicodeUTF8Charset cs(...);
 *   ByteInStream is(...);
 *   StdVnChar ch;
 *   int bytes;
 *   while (cs.nextInput(is, ch, bytes)) {
 *       // process ch
 *   }
 */
int UnicodeUTF8Charset::nextInput(ByteInStream &is, StdVnChar &stdChar,
								  int &bytesRead)
{
	// Temporary UTF-16 words used during decoding of multi-byte UTF-8 sequences.
	UKWORD w1, w2, w3;

	// Raw UTF-8 input bytes read from the stream.
	UKBYTE first, second, third;

	// Decoded Unicode codepoint assembled from the UTF-8 sequence.
	UnicodeChar uniCh;

	// Read the first byte and start counting bytes consumed.
	bytesRead = 0;
	if (!is.getNext(first))
		return 0;
	bytesRead = 1;

	// Determine UTF-8 sequence length from the first byte.
	if (first < 0x80)
		uniCh = first; // 1-byte ASCII sequence

	/* checks whether first is the leading byte of a 2-byte UTF-8 sequence.
					first & 0xE0 masks the top 3 bits of the byte.
					0xC0 is binary 11000000.
	So the condition is true when first has the form 110xxxxx.
	In other words, it detects a UTF-8 start byte for a 2-byte encoded
	character.*/
	else if ((first & 0xE0) == 0xC0)
	{
		// 2-byte UTF-8 sequence: validate and decode the second byte.
		// Peek the next byte without consuming it first. If there is no next byte,
		// the sequence is incomplete and the function returns end-of-stream.
		if (!is.peekNext(second))
			return 0;
		// Continuation bytes in UTF-8 must have the form 10xxxxxx.
		// Mask the top two bits and verify that they equal 0x80.
		if ((second & 0xC0) != 0x80)
		{
			// Invalid continuation byte.
			stdChar = INVALID_STD_CHAR;
			return 1;
		}
		is.getNext(second);
		bytesRead = 2;
		w1 = first;
		w2 = second;
		// Decode the 2-byte UTF-8 sequence:
		//   first  = 110xxxxx
		//   second = 10yyyyyy
		// Resulting Unicode code point is xxxxx yyyyyy.
		uniCh = ((w1 & 0x001F) << 6) | (w2 & 0x3F);
	}
	else if ((first & 0xF0) == 0xE0)
	{
		// This checks whether the first byte is the leading byte of a 3-byte UTF-8
		// sequence. Mask the top 4 bits and compare against 0xE0 (1110xxxx), which
		// is the prefix for a UTF-8 start byte that encodes a code point in the
		// U+0800..U+FFFF range. In other words, it detects the pattern 1110xxxx.
		// 3-byte UTF-8 sequence: validate both continuation bytes.
		if (!is.peekNext(second))
			return 0;
		// The second byte of a UTF-8 continuation must have the form 10xxxxxx.
		// Mask the top two bits and ensure they equal 0x80.
		if ((second & 0xC0) != 0x80)
		{
			stdChar = INVALID_STD_CHAR;
			return 1;
		}
		is.getNext(second);
		bytesRead = 2;
		// Check whether a third byte exists for the 3-byte UTF-8 sequence.
		// If the stream ends before the third byte is available, the sequence is
		// incomplete.
		if (!is.peekNext(third))
			return 0;
		// The third byte of a 3-byte UTF-8 sequence must be a continuation byte
		// with the bit pattern 10xxxxxx. If it is not, the sequence is invalid.
		if ((third & 0xC0) != 0x80)
		{
			stdChar = INVALID_STD_CHAR;
			return 1;
		}
		is.getNext(third);
		bytesRead = 3;
		w1 = first;
		w2 = second;
		w3 = third;
		// Reconstruct the Unicode code point from the 3-byte UTF-8 sequence:
		// first byte = 1110xxxx, second = 10yyyyyy, third = 10zzzzzz
		// The actual code point bits are xxxx yyyy yyzz zzzz.
		uniCh = ((w1 & 0x000F) << 12) | ((w2 & 0x003F) << 6) | (w3 & 0x003F);
	}
	else
	{
		// Unsupported UTF-8 start byte; emit invalid marker.
		stdChar = INVALID_STD_CHAR;
		return 1;
	}

	// Translate the decoded Unicode codepoint to a standard Vietnamese character.
	UKDWORD key = uniCh;
	UKDWORD *pChar = (UKDWORD *)bsearch(&key, m_vnChars, TOTAL_VNCHARS,
										sizeof(UKDWORD), wideCharCompare);
	if (pChar)
		stdChar = VnStdCharOffset + HIWORD(*pChar);
	else
		stdChar = uniCh;
	return 1;
}

//-------------------------------------------
/**
 * @brief Writes a standard Vietnamese character to the output stream in UTF-8
 * encoding.
 *
 * Converts the standard character to its UTF-8 representation and writes it to
 * the output stream.
 *
 * @param[out] os         Output byte stream to write to.
 * @param[in] stdChar     The standard Vietnamese character to write.
 * @param[out] outLen     Receives the number of bytes written (1, 2, or 3).
 * @return The result of the write operation (nonzero if successful).
 *
 * @usage
 *   UnicodeUTF8Charset cs(...);
 *   ByteOutStream os(...);
 *   StdVnChar ch = ...;
 *   int written;
 *   cs.putChar(os, ch, written);
 */
int UnicodeUTF8Charset::putChar(ByteOutStream &os, StdVnChar stdChar,
								int &outLen)
{
	// Convert the standard or mapped Vietnamese character to a Unicode code
	// point.
	UnicodeChar uChar = (stdChar < VnStdCharOffset)
							? (UnicodeChar)stdChar
							: m_toUnicode[stdChar - VnStdCharOffset];
	int ret;

	// Encode as UTF-8 in 1, 2, or 3 bytes depending on the code point range.
	if (uChar < 0x0080)
	{
		// ASCII range: single-byte UTF-8 output.
		outLen = 1;
		ret = os.putB((UKBYTE)uChar);
	}
	else if (uChar < 0x0800)
	{
		// U+0080..U+07FF: two-byte UTF-8 sequence.
		outLen = 2;
		os.putB(0xC0 | (UKBYTE)(uChar >> 6));			// first byte: 110xxxxx
		ret = os.putB(0x80 | (UKBYTE)(uChar & 0x003F)); // second byte: 10xxxxxx
	}
	else
	{
		// U+0800..U+FFFF: three-byte UTF-8 sequence.
		outLen = 3;
		os.putB(0xE0 | (UKBYTE)(uChar >> 12));			 // first byte: 1110xxxx
		os.putB(0x80 | (UKBYTE)((uChar >> 6) & 0x003F)); // second byte: 10yyyyyy
		ret = os.putB(0x80 | (UKBYTE)(uChar & 0x003F));	 // third byte: 10zzzzzz
	}
	return ret;
}

////////////////////////////////////////
// Unicode character reference &#D;   //
/**
 * @brief Returns the element size (in bytes) for the UTF-8 charset encoding.
 *
 * Returns the maximum number of bytes per character (3 for UTF-8).
 *
 * @return Maximum number of bytes per character element (3).
 *
 * @usage
 *   UnicodeUTF8Charset cs(...);
 *   int size = cs.elementSize(); // size == 3
 */
int UnicodeUTF8Charset::elementSize() { return 3; }
////////////////////////////////////////
/**
 * @brief Convert a single hexadecimal digit character to its numeric value.
 *
 * Accepts '0'..'9', 'a'..'f', 'A'..'F' and returns the corresponding
 * integer 0..15. Any other input returns 0.
 *
 * @param digit ASCII character representing a hex digit.
 * @return Integer value of the hex digit (0-15), or 0 for invalid input.
 */
/**
 * @brief Convert a single hexadecimal digit character to its numeric value.
 *
 * Accepts '0'..'9', 'a'..'f', 'A'..'F' and returns the corresponding
 * integer 0..15. Any other input returns 0.
 *
 * @param digit ASCII character representing a hex digit.
 * @return Integer value of the hex digit (0-15), or 0 for invalid input.
 */
int hexDigitValue(unsigned char digit)
{
	// Check for lowercase hexadecimal digits 'a' to 'f'
	if (digit >= 'a' && digit <= 'f')
		// Convert 'a'-'f' to 10-15
		return digit - 'a' + 10;

	// Check for uppercase hexadecimal digits 'A' to 'F'
	if (digit >= 'A' && digit <= 'F')
		// Convert 'A'-'F' to 10-15
		return digit - 'A' + 10;

	// Check for decimal digits '0' to '9'
	if (digit >= '0' && digit <= '9')
		// Convert '0'-'9' to 0-9
		return digit - '0';

	// For any other character, return 0 (invalid hex digit)
	return 0;
}

//--------------------------------------
/**
 * @brief Read the next character from an input stream that may contain
 *        numeric character references (&#DDDD; or &#xHHHH;).
 *
 * This function recognizes both decimal and hexadecimal references and
 * converts them into the internal Unicode value before mapping to StdVnChar.
 *
 * @param[in,out] is Input byte stream
 * @param[out] stdChar Resulting standard Vietnamese character
 * @param[out] bytesRead Number of input bytes consumed
 * @return 1 if a character was read successfully, 0 on end-of-stream
 */
int UnicodeRefCharset::nextInput(ByteInStream &is, StdVnChar &stdChar,
								 int &bytesRead)
{
	// Step 1: Prepare to read the next byte from the input stream.
	// This will be used to check for numeric character references like "&#123;"
	// or "&#x1EA1;".
	unsigned char ch;

	// Step 2: Variable to hold the Unicode codepoint assembled from the input.
	// It starts as the raw byte value, but may be overwritten if a numeric
	// reference is parsed.
	UnicodeChar uniCh;

	// Step 3: Reset the byte counter for how many bytes are consumed from the
	// input.
	bytesRead = 0;

	// Step 4: Attempt to read one byte from the input stream.
	// If no byte is available, return 0 to indicate end-of-stream.
	if (!is.getNext(ch))
		return 0;
	bytesRead = 1;
	uniCh = ch;

	// Step 5: Check if the byte is an ampersand '&', which may indicate the start
	// of a numeric reference.
	if (ch == '&')
	{
		// Step 6: Peek ahead to see if the next character is '#', confirming a
		// numeric reference.
		if (is.peekNext(ch) && ch == '#')
		{
			// Step 7: Consume the '#' character and increment the byte counter.
			is.getNext(ch);
			bytesRead++;

			// Step 8: If not at end-of-stream, proceed to parse the numeric reference
			// (decimal or hex).
			if (!is.eos())
			{
				is.peekNext(ch);
				// Step 8a: If the next character is not 'x' or 'X', parse as decimal
				// (e.g., "&#123;").
				if (ch != 'x' && ch != 'X')
				{
					UKWORD code = 0;
					int digits = 0;
					// Step 8a.1: Read up to 5 decimal digits.
					while (is.peekNext(ch) && isdigit(ch) && digits < 5)
					{
						is.getNext(ch);
						bytesRead++;
						code = code * 10 + (ch - '0');
						digits++;
					}
					// Step 8a.2: If the next character is ';', consume it and set uniCh
					// to the parsed code.
					if (is.peekNext(ch) && ch == ';')
					{
						is.getNext(ch);
						bytesRead++;
						uniCh = code;
					}
				}
				// Step 8b: If the next character is 'x' or 'X', parse as hexadecimal
				// (e.g., "&#x1EA1;").
				else
				{
					is.getNext(ch); // consume 'x' or 'X'
					bytesRead++;
					UKWORD code = 0;
					int digits = 0;
					// Step 8b.1: Read up to 4 hexadecimal digits.
					while (is.peekNext(ch) && isxdigit(ch) && digits < 4)
					{
						is.getNext(ch);
						bytesRead++;
						code = (code << 4) + hexDigitValue(ch);
						digits++;
					}
					// Step 8b.2: If the next character is ';', consume it and set uniCh
					// to the parsed code.
					if (is.peekNext(ch) && ch == ';')
					{
						is.getNext(ch);
						bytesRead++;
						uniCh = code;
					}
				}
			}
		}
	}

	// Step 9: Map the resulting Unicode codepoint to a standard Vietnamese
	// character if possible.
	UKDWORD key = uniCh;
	UKDWORD *pChar = (UKDWORD *)bsearch(&key, m_vnChars, TOTAL_VNCHARS,
										sizeof(UKDWORD), wideCharCompare);
	if (pChar)
		stdChar = VnStdCharOffset +
				  HIWORD(*pChar); // Found a mapping, use the standard index
	else
		stdChar = uniCh; // No mapping, use the Unicode value directly

	// Step 10: Return 1 to indicate a character was successfully read and
	// processed.
	return 1;
}

/**
 * @brief Write a standard Vietnamese character as a numeric character reference
 * (&#DDDD;).
 *
 * For characters above ASCII, this emits an HTML-style decimal reference of the
 * form "&#nnnn;". For ASCII values it simply writes the byte.
 *
 * @param[out] os     Output byte stream to write into.
 * @param[in] stdChar Standard Vietnamese character or raw Unicode value.
 * @param[out] outLen Receives the number of bytes written.
 * @return Result of the final write operation (nonzero on success).
 *
 * @usage
 *   UnicodeRefCharset cs(...);
 *   int len;
 *   cs.putChar(os, stdChar, len);
 */
int UnicodeRefCharset::putChar(ByteOutStream &os, StdVnChar stdChar,
							   int &outLen)
{
	// Convert `stdChar` (internal index or raw codepoint) to an actual
	// Unicode code point that can be emitted. If `stdChar` is a mapped
	// Vietnamese index, look up the corresponding Unicode value; otherwise
	// treat `stdChar` as a raw Unicode value.
	UnicodeChar uChar = (stdChar < VnStdCharOffset)
							? (UnicodeChar)stdChar
							: m_toUnicode[stdChar - VnStdCharOffset];

	int ret;

	// Fast path: ASCII characters are emitted directly as single bytes.
	if (uChar < 128)
	{
		outLen = 1;					  // one output byte written
		ret = os.putB((UKBYTE)uChar); // emit ASCII byte
	}
	else
	{
		// For non-ASCII characters emit an HTML-style decimal numeric
		// character reference: "&#nnnn;". Start by writing the leading
		// sequence "&#" and account for the bytes written in `outLen`.
		outLen = 2; // for '&' and '#'
		os.putB((UKBYTE)'&');
		os.putB((UKBYTE)'#');

		// Emit decimal digits for the code point without leading zeros.
		// `base` iterates 10000,1000,100,10,1 to extract five digits;
		// `prev` becomes true once a non-zero digit has been emitted so
		// subsequent zeros are preserved (avoids leading zeros suppression).
		int i, digit, prev, base;
		prev = 0;
		base = 10000;
		for (i = 0; i < 5; i++)
		{
			digit = uChar / base; // extract current decimal digit
			if (digit || prev)
			{
				prev = 1; // we've started emitting digits
				outLen++; // one more output byte will be written
				os.putB('0' + (unsigned char)digit);
			}
			uChar %= base; // remove the digit just emitted
			base /= 10;	   // move to next lower decimal place
		}

		// Terminate the numeric reference with a semicolon and count it.
		ret = os.putB((UKBYTE)';');
		outLen++;
	}

	return ret;
}

#define HEX_DIGIT(x) ((x < 10) ? ('0' + x) : ('A' + x - 10))

/**
 * @brief Write a standard Vietnamese character as a hexadecimal character
 * reference (&#xHHHH;).
 *
 * Emits an HTML-style hexadecimal reference for non-ASCII values (e.g.
 * "&#x1EA1;"). ASCII values are written directly. The function writes only
 * significant hex digits (no leading zeros) up to four digits.
 *
 * @param[out] os     Output byte stream.
 * @param[in] stdChar Standard Vietnamese character or Unicode value.
 * @param[out] outLen Receives number of bytes written (1 or 3+).
 * @return Result of the final write operation (nonzero on success).
 *
 * @usage
 *   UnicodeHexCharset cs(...);
 *   int len;
 *   cs.putChar(os, stdChar, len);
 */
int UnicodeHexCharset::putChar(ByteOutStream &os, StdVnChar stdChar,
							   int &outLen)
{
	// Resolve the actual Unicode code point to emit. If `stdChar` is
	// a mapped standard Vietnamese index, look up its Unicode value;
	// otherwise treat the value as a raw Unicode code point.
	UnicodeChar uChar = (stdChar < VnStdCharOffset)
							? (UnicodeChar)stdChar
							: m_toUnicode[stdChar - VnStdCharOffset];

	int ret;

	// If the code point fits in a single byte (0..255) emit it directly.
	// This covers ASCII and single-byte encodings; the caller expects
	// `outLen` to contain the number of bytes written.
	if (uChar < 256)
	{
		outLen = 1;
		ret = os.putB((UKBYTE)uChar);
	}
	else
	{
		// For larger code points emit an HTML-style hexadecimal numeric
		// character reference of the form "&#xHHHH;". Start by emitting
		// the leading sequence "&#x" and account for those three bytes.
		outLen = 3; // '&' '#' 'x'
		os.putB('&');
		os.putB('#');
		os.putB('x');

		// Emit significant hex digits from most-significant nibble to
		// least-significant. `shifts` starts at 12 to cover up to 4 hex
		// digits (16-bit code points). `prev` tracks whether we've
		// already emitted a non-zero digit so subsequent zeros are kept.
		int i, digit;
		int prev = 0;
		int shifts = 12;

		for (i = 0; i < 4; i++)
		{
			// Extract the current 4-bit nibble after shifting.
			digit = ((uChar >> shifts) & 0x000F);
			// Emit digit if non-zero or we've already started emitting
			// (to avoid suppressing interior/trailing zeros), and account
			// for the emitted byte in `outLen`.
			if (digit > 0 || prev)
			{
				prev = 1;
				outLen++;
				os.putB((UKBYTE)HEX_DIGIT(digit));
			}
			shifts -= 4; // move to next nibble
		}

		// Terminate the reference with a semicolon and account for it.
		ret = os.putB(';');
		outLen++;
	}

	return ret;
}

/////////////////////////////////
// Class UnicodeCStringCharset  /
/////////////////////////////////
/**
 * @brief Reset internal state for C-style escaped string parsing.
 *
 * Clears the `m_prevIsHex` flag so that subsequent `\x` escapes are parsed
 * correctly at the start of a new input sequence.
 */
void UnicodeCStringCharset::startInput() { m_prevIsHex = 0; }

//----------------------------------------
/**
 * @brief Parse the next input character, interpreting C-style `\xHHHH` escapes.
 *
 * If an escape is encountered ("\\x" or "\\X"), up to four hex digits
 * are consumed and combined into a Unicode value. Otherwise a single byte is
 * returned as-is.
 *
 * @param[in,out] is Input byte stream
 * @param[out] stdChar Resulting standard Vietnamese character
 * @param[out] bytesRead Number of input bytes consumed
 * @return 1 if a character was read successfully, 0 on end-of-stream
 */
int UnicodeCStringCharset::nextInput(ByteInStream &is, StdVnChar &stdChar,
									 int &bytesRead)
{
	unsigned char ch; // raw input byte read from the stream (0..255)
	UnicodeChar
		uniCh; // candidate Unicode code point (from byte or parsed \xHHHH)

	/* Initialize consumption counter and read one byte from stream.
	   If there is no byte available, return 0 (end-of-stream). */
	bytesRead = 0;
	if (!is.getNext(ch))
		return 0;

	bytesRead = 1; // we've consumed the first byte

	/* By default treat the single byte as the Unicode candidate. If
	   we detect a C-style escape sequence starting with '\\x' or
	   '\\X' we will parse up to four hexadecimal digits and
	   overwrite `uniCh` with the parsed value. */
	uniCh = ch;

	/* Handle C-style hexadecimal escape \xHHHH (case-insensitive).
	   We only enter parsing mode if the first byte was a backslash. */
	if (ch == '\\')
	{
		/* Peek the next byte to avoid consuming it unless it is an
		   'x' or 'X'. peekNext does not advance the stream. */
		if (is.peekNext(ch) && (ch == 'x' || ch == 'X'))
		{
			/* Consume the 'x'/'X' marker and account for the byte. */
			is.getNext(ch);
			bytesRead++;

			/* Accumulate up to four hex digits into `code`. The loop
			   peeks before reading to ensure we stop at non-hex
			   characters or after four digits. */
			UKWORD code = 0;
			int digits = 0;
			while (is.peekNext(ch) && isxdigit(ch) && digits < 4)
			{
				is.getNext(ch);							// consume the hex digit
				bytesRead++;							// update consumed-byte count
				code = (code << 4) + hexDigitValue(ch); // append nibble
				digits++;
			}

			/* Replace the candidate Unicode value with the parsed code
			   (if any digits were read, otherwise code==0). */
			uniCh = code;
		}
	}

	// translate to StdVnChar
	UKDWORD key = uniCh;
	UKDWORD *pChar = (UKDWORD *)bsearch(&key, m_vnChars, TOTAL_VNCHARS,
										sizeof(UKDWORD), wideCharCompare);
	if (pChar)
		stdChar = VnStdCharOffset + HIWORD(*pChar);
	else
		stdChar = uniCh;
	return 1;
}

/**
 * @brief Emit `stdChar` as a C-style escaped string (\xHHHH) when needed.
 *
 * Behavior summary / progression:
 * - If `uChar` is a safe ASCII byte (value < 128), is not a hex digit
 *   and is not 'x'/'X', the character is emitted directly as a single byte.
 *   outLen is set to 1 and the function returns the result of `os.putB`.
 *
 * - Otherwise the function emits a C-style hexadecimal escape sequence:
 *     - Writes the two-character prefix "\\x" (outLen starts at 2),
 *     - Emits up to four significant hex digits (most-significant nibble
 *       first). For each hex digit emitted `outLen` is incremented.
 *     - The stream status is then read via `os.isOK()` and stored in `ret`.
 *     - `m_prevIsHex` is set to 1 to indicate the last output was a hex escape.
 *
 * Notes:
 * - `stdChar` may be either a raw Unicode code point or an index into the
 *   Vietnamese mapping table; this is resolved to `uChar` at the start.
 * - Only significant hex digits are emitted (leading zeros are suppressed).
 * - `outLen` counts actual bytes written to the output stream.
 *
 * @param[out] os Output byte stream
 * @param[in] stdChar Standard-VN character or raw Unicode value
 * @param[out] outLen Number of bytes written
 * @return nonzero if the output stream reports OK, zero otherwise
 */
int UnicodeCStringCharset::putChar(ByteOutStream &os, StdVnChar stdChar,
								   int &outLen)
{

	UnicodeChar uChar = (stdChar < VnStdCharOffset)
							? (UnicodeChar)stdChar
							: m_toUnicode[stdChar - VnStdCharOffset];
	int ret;

	/* Fast path: safe ASCII byte that does not conflict with hex escapes.
	   Conditions: value < 128, not a hex digit, not 'x'/'X'. In this case
	   we can emit the byte directly and be done. */
	if (uChar < 128 && !isxdigit(uChar) && uChar != 'x' && uChar != 'X')
	{
		outLen = 1;					  // single output byte
		ret = os.putB((UKBYTE)uChar); // write directly
	}
	else
	{
		/* Escape path: emit "\\x" then the hex digits for the code point.
		   Start with outLen=2 for the two prefix bytes. */
		outLen = 2;
		os.putB('\\');
		os.putB('x');

		/* Emit up to 4 hex digits, starting from the most-significant nibble
		   (shifts = 12 covers a 16-bit code point). `prev` tracks whether
		   we've emitted a non-zero digit already so we avoid leading zeros. */
		int i, digit;
		int prev = 0;
		int shifts = 12;

		for (i = 0; i < 4; i++)
		{
			digit = ((uChar >> shifts) & 0x000F); // extract current nibble
			if (digit > 0 || prev)
			{
				prev = 1;						   // once set, keep emitting
				outLen++;						   // account for this hex digit
				os.putB((UKBYTE)HEX_DIGIT(digit)); // write hex char ('0'..'9','A'..'F')
			}
			shifts -= 4; // move to next nibble
		}

		/* After writing the escape and any hex digits, capture stream status
		   and mark that previous output was a hex escape. */
		ret = os.isOK();
		m_prevIsHex = 1;
	}

	return ret;
}

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
const int VIQREscCount = sizeof(VIQREscapes) / sizeof(char *);

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

//-----------------------------------------
/**
 * @brief Initialize the global charset library instance.
 *
 * This constructor prepares lookup tables, clears cached converter
 * pointers, sets defaults for conversion options, and initializes
 * the VIQR input/output escape pattern matchers.
 *
 * No parameters.
 *
 * Per-line comments in the body explain each initialization step.
 */
CVnCharsetLib::CVnCharsetLib()
{
	/* Local iterator used for filling vowel lookup tables. */
	unsigned char ch;

	/* Initialize lowercase vowel lookup table: clear all entries
	 * then mark known vowels as true. LoVowel[i] corresponds to
	 * character ('a' + i). */
	for (ch = 'a'; ch < 'z'; ch++)
		LoVowel[ch - 'a'] = 0; // clear
	LoVowel['a' - 'a'] = 1;	   // mark 'a' as vowel
	LoVowel['e' - 'a'] = 1;	   // mark 'e'
	LoVowel['i' - 'a'] = 1;	   // mark 'i'
	LoVowel['o' - 'a'] = 1;	   // mark 'o'
	LoVowel['u' - 'a'] = 1;	   // mark 'u'
	LoVowel['y' - 'a'] = 1;	   // mark 'y'

	/* Initialize uppercase vowel lookup table similarly. HiVowel[i]
	 * corresponds to character ('A' + i). */
	for (ch = 'A'; ch < 'Z'; ch++)
		HiVowel[ch - 'A'] = 0; // clear
	HiVowel['A' - 'A'] = 1;	   // mark 'A' as vowel
	HiVowel['E' - 'A'] = 1;	   // mark 'E'
	HiVowel['I' - 'A'] = 1;	   // mark 'I'
	HiVowel['O' - 'A'] = 1;	   // mark 'O'
	HiVowel['U' - 'A'] = 1;	   // mark 'U'
	HiVowel['Y' - 'A'] = 1;	   // mark 'Y'

	/* Clear cached charset pointers. These are lazily created by
	 * `getVnCharset()` and must start NULL to indicate 'not created'. */
	m_pUniCharset = NULL;	  // Unicode charset instance
	m_pUniCompCharset = NULL; // decomposed Unicode
	m_pUniUTF8 = NULL;		  // UTF-8 handler
	m_pUniRef = NULL;		  // named-reference handler
	m_pUniHex = NULL;		  // hex-ref handler
	m_pVIQRCharObj = NULL;	  // VIQR converter
	m_pUVIQRCharObj = NULL;	  // UTF8+VIQR hybrid
	m_pWinCP1258 = NULL;	  // Win-1258 converter
	m_pVnIntCharset = NULL;	  // internal VN charset

	/* Initialize arrays of single- and double-byte cached converters
	 * to NULL to indicate 'not yet created'. */
	int i;
	for (i = 0; i < CONV_TOTAL_SINGLE_CHARSETS; i++)
		m_sgCharsets[i] = NULL; // per-single-byte table

	for (i = 0; i < CONV_TOTAL_DOUBLE_CHARSETS; i++)
		m_dbCharsets[i] = NULL; // per-double-byte table

	/* Reset conversion options to library defaults. This populates
	 * `m_options` with sensible defaults for VIQR handling, casing, etc. */
	VnConvResetOptions(&m_options);

	/* Initialize VIQR escape-pattern matchers used to detect tokens
	 * (URLs, emails, schemes) where VIQR interpretation should be
	 * suppressed. Both input and output pattern matchers use the same
	 * pattern table `VIQREscapes` with `VIQREscCount` entries. */
	m_VIQREscPatterns.init((char **)VIQREscapes, VIQREscCount);
	m_VIQROutEscPatterns.init((char **)VIQREscapes, VIQREscCount);
}

/**
 * @brief Construct and initialize the global charset library.
 *
 * Initializes vowel lookup tables, clears cached converter objects, resets
 * conversion options, and prepares escape pattern matchers used by VIQR logic.
 * This object is intended to be a single global instance exported as
 * `VnCharsetLibObj`.
 *
 * @usage
 *   // global instance is available as VnCharsetLibObj
 */
CVnCharsetLib::~CVnCharsetLib()
{
	/*
	 * Member pointer cleanup sequence (each pointer may be NULL):
	 * - m_pUniCharset: Unicode (UTF-16) input/output converter object
	 * - m_pUniUTF8: UTF-8 converter object
	 * - m_pUniRef: Numeric character reference converter (&#nnnn;)
	 * - m_pUniHex: Hexadecimal numeric reference converter (&#xHHHH;)
	 * - m_pVIQRCharObj: VIQR input/output converter (ASCII tone markers)
	 * - m_pUVIQRCharObj: Upper-case variant of VIQR converter
	 * - m_pWinCP1258: Windows CP1258 single-byte converter
	 * - m_pUniCString: C-style escaped string converter (\xHHHH)
	 * - m_pVnIntCharset: internal 4-byte engine charset representation
	 *
	 * Each pointer is checked for non-NULL and deleted to free heap
	 * memory allocated by this library. Deleting a NULL pointer is
	 * safe but the explicit checks make the sequence clear.
	 */

	if (m_pUniCharset)
		delete m_pUniCharset; // free UnicodeCharset instance
	if (m_pUniUTF8)
		delete m_pUniUTF8; // free UnicodeUTF8Charset instance
	if (m_pUniRef)
		delete m_pUniRef; // free UnicodeRefCharset instance
	if (m_pUniHex)
		delete m_pUniHex; // free UnicodeHexCharset instance
	if (m_pVIQRCharObj)
		delete m_pVIQRCharObj; // free VIQRCharset (ASCII mapping) instance
	if (m_pUVIQRCharObj)
		delete m_pUVIQRCharObj; // free uppercase VIQR variant
	if (m_pWinCP1258)
		delete m_pWinCP1258; // free CP1258 SingleByteCharset
	if (m_pUniCString)
		delete m_pUniCString; // free CString escape parser
	if (m_pVnIntCharset)
		delete m_pVnIntCharset; // free internal charset (StdVnChar formatter)

	/*
	 * Delete any cached single-byte charset converter objects that were
	 * lazily created and stored in the m_sgCharsets table. Each entry
	 * represents a pointer to a SingleByteCharset or NULL if unused.
	 */
	int i;
	for (i = 0; i < CONV_TOTAL_SINGLE_CHARSETS; i++)
		if (m_sgCharsets[i])
			delete m_sgCharsets[i]; // free each allocated SingleByteCharset

	/*
	 * Similarly free any allocated double-byte charset converter objects
	 * stored in the m_dbCharsets table.
	 */
	for (i = 0; i < CONV_TOTAL_DOUBLE_CHARSETS; i++)
		if (m_dbCharsets[i])
			delete m_dbCharsets[i]; // free each allocated DoubleByteCharset
}

//-----------------------------------------
VnCharset *CVnCharsetLib::getVnCharset(int charsetIdx)
{
	switch (charsetIdx)
	{

	case CONV_CHARSET_UNICODE:
		if (m_pUniCharset == NULL)
			m_pUniCharset = new UnicodeCharset(UnicodeTable);
		return m_pUniCharset;
	case CONV_CHARSET_UNIDECOMPOSED:
		if (m_pUniCompCharset == NULL)
			m_pUniCompCharset =
				new UnicodeCompCharset(UnicodeTable, UnicodeComposite);
		return m_pUniCompCharset;
	case CONV_CHARSET_UNIUTF8:
	case CONV_CHARSET_XUTF8:
		if (m_pUniUTF8 == NULL)
			m_pUniUTF8 = new UnicodeUTF8Charset(UnicodeTable);
		return m_pUniUTF8;

	case CONV_CHARSET_UNIREF:
		if (m_pUniRef == NULL)
			m_pUniRef = new UnicodeRefCharset(UnicodeTable);
		return m_pUniRef;

	case CONV_CHARSET_UNIREF_HEX:
		if (m_pUniHex == NULL)
			m_pUniHex = new UnicodeHexCharset(UnicodeTable);
		return m_pUniHex;

	case CONV_CHARSET_UNI_CSTRING:
		if (m_pUniCString == NULL)
			m_pUniCString = new UnicodeCStringCharset(UnicodeTable);
		return m_pUniCString;

	case CONV_CHARSET_WINCP1258:
		if (m_pWinCP1258 == NULL)
			m_pWinCP1258 = new WinCP1258Charset(WinCP1258, WinCP1258Pre);
		return m_pWinCP1258;

	case CONV_CHARSET_VIQR:
		if (m_pVIQRCharObj == NULL)
			m_pVIQRCharObj = new VIQRCharset(VIQRTable);
		return m_pVIQRCharObj;

	case CONV_CHARSET_VNSTANDARD:
		if (m_pVnIntCharset == NULL)
			m_pVnIntCharset = new VnInternalCharset();
		return m_pVnIntCharset;

	case CONV_CHARSET_UTF8VIQR:
		if (m_pUVIQRCharObj == NULL)
		{
			if (m_pVIQRCharObj == NULL)
				m_pVIQRCharObj = new VIQRCharset(VIQRTable);

			if (m_pUniUTF8 == NULL)
				m_pUniUTF8 = new UnicodeUTF8Charset(UnicodeTable);
			m_pUVIQRCharObj = new UTF8VIQRCharset(m_pUniUTF8, m_pVIQRCharObj);
		}
		return m_pUVIQRCharObj;

	default:
		if (IS_SINGLE_BYTE_CHARSET(charsetIdx))
		{
			int i = charsetIdx - CONV_CHARSET_TCVN3;
			if (m_sgCharsets[i] == NULL)
				m_sgCharsets[i] = new SingleByteCharset(SingleByteTables[i]);
			return m_sgCharsets[i];
		}
		else if (IS_DOUBLE_BYTE_CHARSET(charsetIdx))
		{
			int i = charsetIdx - CONV_CHARSET_VNIWIN;
			if (m_dbCharsets[i] == NULL)
				m_dbCharsets[i] = new DoubleByteCharset(DoubleByteTables[i]);
			return m_dbCharsets[i];
		}
	}
	return NULL;
}

/**
 * @brief Lookup or create a charset converter by index.
 *
 * Returns a pointer to a `VnCharset` implementation for the given
 * `charsetIdx`. Lazily constructs the appropriate converter and caches it
 * inside the library for subsequent calls.
 *
 * @param[in] charsetIdx Index identifying the desired charset (CONV_* enum).
 * @return Pointer to a `VnCharset` instance, or NULL if the index is invalid.
 *
 * @usage
 *   VnCharset *cs = VnCharsetLibObj.getVnCharset(CONV_CHARSET_UNIUTF8);
 */

//-------------------------------------------------
DllExport void VnConvSetOptions(VnConvOptions *pOptions)
{
	VnCharsetLibObj.m_options = *pOptions;
}

/**
 * @brief Set global conversion options for the charset library.
 *
 * Copies the provided `VnConvOptions` into the global `VnCharsetLibObj` so
 * that subsequent conversions respect the new settings (VIQR escaping, casing,
 * tone removal, etc.).
 *
 * @param[in] pOptions Pointer to a `VnConvOptions` structure with desired
 * options.
 */

//-------------------------------------------------
DllExport void VnConvGetOptions(VnConvOptions *pOptions)
{
	*pOptions = VnCharsetLibObj.m_options;
}

/**
 * @brief Retrieve the current global conversion options.
 *
 * Copies the library's active `VnConvOptions` into the caller-provided struct.
 *
 * @param[out] pOptions Destination buffer to receive the current options.
 */

//-------------------------------------------------
DllExport void VnConvResetOptions(VnConvOptions *pOptions)
{
	pOptions->viqrEsc = 1;
	pOptions->viqrMixed = 0;
	pOptions->toUpper = 0;
	pOptions->toLower = 0;
	pOptions->removeTone = 0;
	pOptions->smartViqr = 1;
}

/**
 * @brief Initialize a `VnConvOptions` structure with default values.
 *
 * Provides a convenient way to reset options to the library defaults.
 * @param[out] pOptions Pointer to options structure to initialize.
 */

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

#define IS_ODD(x) (x & 1)
#define IS_EVEN(x) (!(x & 1))

StdVnChar StdVnToUpper(StdVnChar ch)
{
	if (ch >= VnStdCharOffset && ch < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) &&
		IS_ODD(ch))
		ch -= 1;
	return ch;
}

/**
 * @brief Convert an internal StdVnChar to its uppercase partner when
 * applicable.
 *
 * The internal alphabet uses adjacent paired values for lowercase/uppercase
 * (even/odd). This helper returns the uppercase partner if `ch` is a
 * mapped Vietnamese alphabetic character encoded with the odd/even scheme.
 * Otherwise the input is returned unchanged.
 *
 * @param ch Input standard Vietnamese character.
 * @return Uppercase StdVnChar or the original `ch` if not applicable.
 *
 * @usage
 *   StdVnChar up = StdVnToUpper(ch);
 */

//----------------------------------------
/**
 * @brief Convert an internal StdVnChar to its lowercase form when applicable.
 *
 * If the character is a mapped Vietnamese alphabetic character and its
 * internal encoding uses an even/odd paired scheme for case, adjust the
 * value to the lowercase partner. Otherwise returns the input unchanged.
 *
 * @param ch Input standard Vietnamese character
 * @return Lowercase StdVnChar value or the original `ch` if not applicable
 */
StdVnChar StdVnToLower(StdVnChar ch)
{
	if (ch >= VnStdCharOffset && ch < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) &&
		IS_EVEN(ch))
		ch += 1;
	return ch;
}

//----------------------------------------
/**
 * @brief Return the root (base) StdVnChar for a possibly derived character.
 *
 * Uses the `StdVnRootChar` lookup to map derived/combined characters back to
 * their canonical base character within the standard character table.
 *
 * @param ch Input standard Vietnamese character
 * @return The root StdVnChar, or the input unchanged if out of range
 */
StdVnChar StdVnGetRoot(StdVnChar ch)
{
	if (ch >= VnStdCharOffset && ch < VnStdCharOffset + TOTAL_VNCHARS)
		ch = VnStdCharOffset + StdVnRootChar[ch - VnStdCharOffset];
	return ch;
}
