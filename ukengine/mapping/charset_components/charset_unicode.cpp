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
#include <stdlib.h>

#include "charset.h"
#include "charset_components/charset_internal.h"

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
	const bool streamHasWideChar = is.getNextW(uniCh);
	if (!streamHasWideChar)
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

/**
 * @brief If @a leadingByte is backslash, optionally consume `x`/`X` and up to four hex digits from @a is.
 *
 * When the stream does not continue with C-style hex, leaves @a uniCh as set by the caller and does
 * not read further. Otherwise replaces @a uniCh with the parsed code point and increments
 * @a bytesRead for each additional byte consumed after @a leadingByte.
 */
static void tryConsumeCStyleHexEscapeAfterBackslash(ByteInStream &is,
													unsigned char leadingByte,
													UnicodeChar &uniCh,
													int &bytesRead)
{
	if (leadingByte != '\\')
		return;
	unsigned char ch;
	if (!is.peekNext(ch) || (ch != 'x' && ch != 'X'))
		return;
	is.getNext(ch);
	bytesRead++;
	UKWORD code = 0;
	int digits = 0;
	while (is.peekNext(ch) && isxdigit(ch) && digits < 4)
	{
		is.getNext(ch);
		bytesRead++;
		code = (code << 4) + hexDigitValue(ch);
		digits++;
	}
	uniCh = (UnicodeChar)code;
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
	tryConsumeCStyleHexEscapeAfterBackslash(is, ch, uniCh, bytesRead);

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
