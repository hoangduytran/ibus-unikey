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
#include "charset_components/charset_internal.h"

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
