// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
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

#ifndef __CHARSET_CONVERT_H
#define __CHARSET_CONVERT_H

/**
 * @file charset.h
 * @brief Vietnamese character set conversion abstractions and concrete encodings.
 *
 * Defines the VnCharset interface and implementations for single-byte, Unicode,
 * UTF-8/VIQR combinations, and platform-specific code pages.
 *
 * This module is used by the engine to translate between internal standard
 * character codes and external encoding formats.
 */

#if !defined(_WIN32)
#include <stdint.h>
#endif

#if defined(_WIN32)
/**
 * @brief DLL import/export annotation for symbol visibility in Windows builds.
 *
 * - When building UniKey itself (`UNIKEYHOOK` defined), symbols are exported.
 * - When using UniKey as a library, symbols are imported.
 */
#if defined(UNIKEYHOOK)
#define DllInterface __declspec(dllexport)
#else
#define DllInterface __declspec(dllimport)
#endif
#else
/**
 * @brief Fallback visibility for non-Windows platforms; no-op in static/shared builds.
 */
#define DllInterface // not used
#define DllExport
#define DllImport
#endif

#include "vnconv.h"
#include "byteio.h"
#include "pattern.h"

/**
 * @brief Total number of mapped Vietnamese std char entries.
 */
#define TOTAL_VNCHARS 213

/**
 * @brief Number of alphabetic Vietnamese characters (subset of TOTAL_VNCHARS).
 */
#define TOTAL_ALPHA_VNCHARS 186

#if defined(_WIN32)
typedef unsigned __int32 StdVnChar;	  /**< 32-bit internal standard Vietnamese character code.
									   *  Used as canonical index in mapping tables across encodings.
									   */
typedef unsigned __int16 UnicodeChar; /**< UTF-16 code unit type for Unicode charset conversion. */
typedef unsigned __int16 UKWORD;	  /**< 16-bit container used in legacy single/double-byte mapping tables. */
typedef unsigned __int32 UKDWORD;	  /**< 32-bit container used in wide conversion tables and lookup data. */
#else
// typedef unsigned int StdVnChar; //the size should be more specific
typedef uint32_t StdVnChar;	  /**< 32-bit internal standard Vietnamese char value on non-Windows. */
typedef uint16_t UnicodeChar; /**< UTF-16 representation for Unicode conversion path. */
typedef uint16_t UKWORD;	  /**< Conversion pair index type for 16-bit tables. */
typedef uint32_t UKDWORD;	  /**< Conversion pair index type for 32-bit tables / combined values. */
#endif

// typedef unsigned short UnicodeChar;
// typedef unsigned short UKWORD;

// typedef unsigned int UKDWORD; //the size should be more specific

#ifndef LOWORD
#define LOWORD(l) ((UKWORD)(l))
#endif

#ifndef HIWORD
#define HIWORD(l) ((UKWORD)(((UKDWORD)(l) >> 16) & 0xFFFF))
#endif

#ifndef MAKEWORD
/**
 * @brief Construct a word from two bytes in little-endian order.
 */
#define MAKEWORD(a, b) ((UKWORD)(((UKBYTE)(a)) | ((UKWORD)((UKBYTE)(b))) << 8))
#endif

/** @brief Offset to move standard Vietnamese characters into a private range. */
const StdVnChar VnStdCharOffset = 0x10000;

/** @brief Sentinel for invalid or unmapped standard Vietnamese characters. */
const StdVnChar INVALID_STD_CHAR = 0xFFFFFFFF;

/** @brief Padding character used by conversions for non-renderable positions. */
// const unsigned char PadChar = '?'; //? is used for VIQR charset
const unsigned char PadChar = '#';
const unsigned char PadStartQuote = '"'; /**< verbatim quote begin char for unicode escape */
const unsigned char PadEndQuote = '"';	 /**< verbatim quote end char for unicode escape */
const unsigned char PadEllipsis = '.';	 /**< ellipsis placeholder char for missing output */

/**
 * @class VnCharset
 * @brief Abstract base class for Vietnamese encoding conversions.
 *
 * Implementations convert between the internal StdVnChar representation
 * and byte streams for various input/output encodings.
 */
class DllInterface VnCharset
{
public:
	/**
	 * @brief Prepare for a new input conversion sequence.
	 */
	virtual void startInput() {};

	/**
	 * @brief Prepare for a new output conversion sequence.
	 */
	virtual void startOutput() {};

	/**
	 * @brief Read next character from input stream and map to StdVnChar.
	 *
	 * @param is source byte stream
	 * @param stdChar output standard char
	 * @param bytesRead number of input bytes consumed
	 * @return 0 on success, or error code on failure.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead) = 0;

	/**
	 * @brief Write an output representation for a standard Vietnamese char.
	 *
	 * @param os destination byte stream
	 * @param stdChar source standard char code
	 * @param outLen output length written
	 * @return next write position in output buffer.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen) = 0;

	/**
	 * @brief Character element size in bytes for this encoding.
	 *
	 * Default may be overridden by subclasses.
	 */
	virtual int elementSize();

	/**
	 * @brief Virtual destructor to ensure derived charset objects are safely released.
	 *
	 * Required for proper polymorphic cleanup when using base-class pointers.
	 */
	virtual ~VnCharset() {}
};

//--------------------------------------------------
/**
 * @class SingleByteCharset
 * @brief Charset implementation for legacy single-byte encodings.
 *
 * Uses direct mapping tables from byte values to StdVnChar.
 */
class SingleByteCharset : public VnCharset
{
protected:
	UKWORD m_stdMap[256];	  /**< map from 8-bit input byte to standard character code */
	unsigned char *m_vnChars; /**< pointer to output character table initialized from charset data */
public:
	/**
	 * @brief Constructor requires a static mapping table of 213 standard chars.
	 *
	 * @param vnChars pointer to the 8-bit to StdVnChar map data (owned by caller).
	 */
	SingleByteCharset(unsigned char *vnChars);

	/**
	 * @brief Convert a byte stream into the internal StdVnChar sequence.
	 *
	 * @param is source input stream with single-byte encoding.
	 * @param stdChar output standard Vietnamese char value.
	 * @param bytesRead number of bytes consumed (usually 1, 0 on failure).
	 * @return 0 on success or negative on error (e.g., EOF).
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Convert a StdVnChar value back to encoded byte sequence.
	 *
	 * @param os destination output stream.
	 * @param stdChar canonical char to render.
	 * @param outLen number of output bytes written (usually 1 or 0 if unmapped).
	 * @return 0 on success or negative on error (e.g., no mapping exists).
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);
};

//--------------------------------------------------
/**
 * @class VnInternalCharset
 * @brief Internal Unicode-like operational charset representation.
 *
 * Used for internal engine transitions where char values are already mapped
 * to a standard VN char space, bypassing external encoding details.
 */
class VnInternalCharset : public VnCharset
{
public:
	/**
	 * @brief Default constructor for phased internal conversion.
	 */
	VnInternalCharset() {};

	/**
	 * @brief Read next character from byte stream assumed to encode StdVnChar directly.
	 *
	 * This path is used when the engine has pre-normalized internal representation.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Output internal StdVnChar value to byte stream without external remap.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);

	/**
	 * @brief Report logical element size in bytes for this internal format.
	 */
	virtual int elementSize();
};

//--------------------------------------------------
/**
 * @class UnicodeCharset
 * @brief UTF-16-based Unicode charset conversion implementation.
 *
 * This class maps between StdVnChar and UnicodeChar sequences for input and output.
 */
class UnicodeCharset : public VnCharset
{
protected:
	UKDWORD m_vnChars[TOTAL_VNCHARS]; /**< map from standard char index to Unicode codepoints */
	UnicodeChar *m_toUnicode;		  /**< conversion table for writing output in 16-bit units */
public:
	/**
	 * @brief Construct a Unicode charset converter.
	 *
	 * @param vnChars table of UnicodeChar values corresponding to standard VN indices.
	 */
	UnicodeCharset(UnicodeChar *vnChars);

	/**
	 * @brief Convert from input bytes to StdVnChar using UTF-16 mapping.
	 *
	 * The engine calls this when input has to be normalized into internal VN code points.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Convert from StdVnChar to Unicode output bytes.
	 *
	 * Uses `m_toUnicode` to produce valid UTF-16 units from standard char index.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);

	/**
	 * @brief Returns UTF-16 element size for Unicode output (normally 2 bytes).
	 */
	virtual int elementSize();
};

//--------------------------------------------------
/**
 * @class DoubleByteCharset
 * @brief Charset converter for legacy double-byte encodings.
 *
 * Handles two-byte output mappings and inverse input mapping for composite sets.
 */
class DoubleByteCharset : public VnCharset
{
protected:
	UKWORD m_stdMap[256];			  /**< mapping table for first-byte input to StdVnChar indices */
	UKDWORD m_vnChars[TOTAL_VNCHARS]; /**< mapping table for standard char indexes to double-byte values */
	UKWORD *m_toDoubleChar;			  /**< pointer to combined two-byte mapping table used for output */
public:
	/**
	 * @brief Constructor taking prepopulated composite char mapping table.
	 *
	 * @param vnChars pointer to table of double-byte characters for conversion (owner remains external).
	 */
	DoubleByteCharset(UKWORD *vnChars);

	/**
	 * @brief Decode a double-byte input sequence into a StdVnChar.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Encode a StdVnChar as a two-byte sequence in output stream.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);
};

//--------------------------------------------------
/**
 * @class UnicodeUTF8Charset
 * @brief UTF-8 specialized variant of Unicode charset converter.
 *
 * Builds on UnicodeCharset, translating between internal StdVnChar and UTF-8 encoded output
 * while inheriting common UTF-16 conversion behavior.
 */
class UnicodeUTF8Charset : public UnicodeCharset
{
public:
	/**
	 * @brief Construct UTF-8 converter using base Unicode table.
	 */
	UnicodeUTF8Charset(UnicodeChar *vnChars) : UnicodeCharset(vnChars) {}

	/**
	 * @brief Read UTF-8 byte stream and return normalized StdVnChar.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Write normalized StdVnChar as UTF-8 bytes to output stream.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);

	/**
	 * @brief Return the logical UTF-8 element size used by this charset.
	 */
	virtual int elementSize();
};

//--------------------------------------------------
/**
 * @class UnicodeRefCharset
 * @brief Unicode charset converting with reference formatting (e.g. &#D; sequences).
 *
 * Extends UTF-16 conversion with text-format-specific output encoding semantics.
 */
class UnicodeRefCharset : public UnicodeCharset
{
public:
	/**
	 * @brief Constructor for ref-style Unicode conversion.
	 *
	 * @param vnChars UTF-16 mapping table for standard VN indices.
	 */
	UnicodeRefCharset(UnicodeChar *vnChars) : UnicodeCharset(vnChars) {}

	/**
	 * @brief Convert input (UTF-16 or ref input) to StdVnChar.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Emit output as reference-style Unicode sequence mapping.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);
};

//--------------------------------------------------
/**
 * @class UnicodeHexCharset
 * @brief Reference Unicode converter with hex-encoded output style.
 *
 * Produces hex-based output sequences (e.g. \uXXXX) for the VS/legacy converter layer.
 */
class UnicodeHexCharset : public UnicodeRefCharset
{
public:
	/**
	 * @brief Construct hex-style converter using base Unicode ref mappings.
	 */
	UnicodeHexCharset(UnicodeChar *vnChars) : UnicodeRefCharset(vnChars) {}

	/**
	 * @brief Convert StdVnChar to hex-style Unicode output text.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);
};

//--------------------------------------------------
/**
 * @class UnicodeCStringCharset
 * @brief Unicode converter for C-string escaping style.
 *
 * Emits \uXXXX escapes and tracks previous hex output context to ensure correct encoding.
 */
class UnicodeCStringCharset : public UnicodeCharset
{
protected:
	int m_prevIsHex; /**< state flag to avoid duplicate escape sequences in continuous output */
public:
	UnicodeCStringCharset(UnicodeChar *vnChars) : UnicodeCharset(vnChars) {}

	/**
	 * @brief Convert UTF-8/C-string style input to StdVnChar.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Convert StdVnChar to C-style escape output.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);

	/**
	 * @brief Reset internal escape state when starting a new input sequence.
	 */
	virtual void startInput();
};

//--------------------------------------------------
/**
 * @class WinCP1258Charset
 * @brief WinCP1258 (Vietnamese code page) charset converter.
 *
 * Handles both composite and precomposed char forms and performs round-trip
 * encoding/decoding with platform-specific tables.
 */
class WinCP1258Charset : public VnCharset
{
protected:
	UKWORD m_stdMap[256];				  /**< byte->StdVnChar index mapping for CP1258 input */
	UKDWORD m_vnChars[TOTAL_VNCHARS * 2]; /**< CP1258 char->StdVnChar output mapping table */
	UKWORD *m_toDoubleChar;				  /**< optional map for 2-byte sequences (composed chars) */
	int m_totalChars;					  /**< number of valid chars in m_vnChars table */

public:
	/**
	 * @brief Construct using given composite and precomposed mapping tables.
	 *
	 * @param compositeChars table for combined two-byte sequences.
	 * @param precomposedChars table for precomposed single chars.
	 */
	WinCP1258Charset(UKWORD *compositeChars, UKWORD *precomposedChars);

	/**
	 * @brief Decode CP1258 input to internal StdVnChar representation.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Encode internal StdVnChar to CP1258 output bytes.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);
};

//--------------------------------------------------
/**
 * @struct UniCompCharInfo
 * @brief Compounded character information for Unicode decomposition/combination.
 */
struct UniCompCharInfo
{
	UKDWORD compChar; /**< combined Unicode codepoint for composition */
	int stdIndex;	  /**< standard VN index representing this composed char */
};

/**
 * @class UnicodeCompCharset
 * @brief Unicode converter that supports composed + decomposed character handling.
 */
class UnicodeCompCharset : public VnCharset
{
protected:
	UniCompCharInfo m_info[TOTAL_VNCHARS * 2]; /**< table of combined char information */
	UKDWORD *m_uniCompChars;				   /**< pointer to external composed character table */
	int m_totalChars;						   /**< number of composed entries loaded */
public:
	/**
	 * @brief Construct a composed-character-aware Unicode converter.
	 *
	 * @param uniChars base Unicode character table.
	 * @param uniCompChars composed/unicode decomposition mapping table.
	 */
	UnicodeCompCharset(UnicodeChar *uniChars, UKDWORD *uniCompChars);

	/**
	 * @brief Convert input stream to StdVnChar with composition logic.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Convert StdVnChar to output, applying composed-char rules.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);

	/**
	 * @brief Byte size of the element representation in this charset.
	 */
	virtual int elementSize();
};

//--------------------------------------------------
/**
 * @class VIQRCharset
 * @brief VIQR conversion with escape sequence state machine.
 *
 * Handles Vietnamese VIQR text entry with conversion markers and generator
 * for export forms that may include escape sequences.
 */
class VIQRCharset : public VnCharset
{
protected:
	UKDWORD *m_vnChars;	   /**< VIQR character mapping table used for lookup and output resolution */
	UKWORD m_stdMap[256];  /**< direct map from input byte value to standard VN symbol index */
	int m_atWordBeginning; /**< indicates current parser position at a word boundary */
	int m_escapeBowl;	   /**< state tracking bowl-accent-like VIQR escape sequence */
	int m_escapeRoof;	   /**< state tracking roof (^) escape sequence */
	int m_escapeHook;	   /**< state tracking hook (`) escape sequence */
	int m_escapeTone;	   /**< state tracking tone mark escape sequence */
	int m_gotTone;		   /**< flag set when tone has been applied for current syllable */
	int m_escAll;		   /**< enables output escaping for all mapped characters */
	int m_noOutEsc;		   /**< disables output escaping even when escape sequences are recognized */
public:
	int m_suspicious; /**< heuristic flag for ambiguous VIQR parse cases requiring verification */

	/**
	 * @brief Construct a VIQR charset converter with base mapping.
	 *
	 * @param vnChars pointer to the VIQR mapping table.
	 */
	VIQRCharset(UKDWORD *vnChars);

	/**
	 * @brief Initialize state for a new conversion input stream.
	 */
	virtual void startInput();

	/**
	 * @brief Initialize state for a new conversion output stream.
	 */
	virtual void startOutput();

	/**
	 * @brief Parse incoming VIQR stream to standardized StdVnChar.
	 *
	 * Handles marker sequences and tone transformation state.
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Render StdVnChar to VIQR output encoding.
	 *
	 * Honors escape mode flags and current output style settings.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);
};

//--------------------------------------------------
/**
 * @class UTF8VIQRCharset
 * @brief Hybrid converter that integrates UTF-8 and VIQR output modes.
 *
 * Used when engines need to maintain UTF-8 data while providing VIQR fallback
 * or compatibility behavior.
 */
class UTF8VIQRCharset : public VnCharset
{

protected:
	VIQRCharset *m_pViqr;		/**< nested VIQR engine instance; not owned */
	UnicodeUTF8Charset *m_pUtf; /**< nested UTF8 engine instance; not owned */

public:
	/**
	 * @brief Construct hybrid UTF8/VIQR converter.
	 *
	 * @param pUtf pointer to UTF8 charset instance
	 * @param pViqr pointer to VIQR charset instance
	 */
	UTF8VIQRCharset(UnicodeUTF8Charset *pUtf, VIQRCharset *pViqr);

	/**
	 * @brief Reset hybrid converter state before input sequence.
	 */
	virtual void startInput();

	/**
	 * @brief Reset hybrid converter state before output sequence.
	 */
	virtual void startOutput();

	/**
	 * @brief Parse input stream using UTF-8 then VIQR hybrid rules (if required).
	 */
	virtual int nextInput(ByteInStream &is, StdVnChar &stdChar, int &bytesRead);

	/**
	 * @brief Emit output by applying VIQR escaping on UTF-8 normalization as needed.
	 */
	virtual int putChar(ByteOutStream &os, StdVnChar stdChar, int &outLen);
};

//--------------------------------------------------
/**
 * @class CVnCharsetLib
 * @brief Central registry of available charset converters.
 *
 * Manages lifecycle of converter implementations and provides lookup by charset ID.
 */
class DllInterface CVnCharsetLib
{
protected:
	SingleByteCharset *m_sgCharsets[CONV_TOTAL_SINGLE_CHARSETS]; /**< single-byte encodings index, lazily created and reused. */
	DoubleByteCharset *m_dbCharsets[CONV_TOTAL_DOUBLE_CHARSETS]; /**< double-byte encodings index, lazily created and reused. */
	UnicodeCharset *m_pUniCharset;								 /**< UTF-16 unicode converter instance (singleton inside lib). */
	UnicodeCompCharset *m_pUniCompCharset;						 /**< Unicode composite/decomposed converter instance. */
	UnicodeUTF8Charset *m_pUniUTF8;								 /**< UTF-8 Unicode converter instance for UTF8 input/output. */
	UnicodeRefCharset *m_pUniRef;								 /**< Unicode reference (&#NNN;) converter instance. */
	UnicodeHexCharset *m_pUniHex;								 /**< Unicode hex (\uXXXX) converter instance. */
	VIQRCharset *m_pVIQRCharObj;								 /**< VIQR input/output converter instance. */
	UTF8VIQRCharset *m_pUVIQRCharObj;							 /**< Hybrid UTF8+VIQR converter instance. */
	WinCP1258Charset *m_pWinCP1258;								 /**< Windows CP1258 converter instance. */
	UnicodeCStringCharset *m_pUniCString;						 /**< C-style Unicode escape converter instance. */
	VnInternalCharset *m_pVnIntCharset;							 /**< Internal standard VN converter instance (no external encoding). */

public:
	PatternList m_VIQREscPatterns, m_VIQROutEscPatterns; /**< compiled patterns to match VIQR escape sequences for input/output. */
	VnConvOptions m_options;							 /**< current runtime options that control converter behavior (e.g., mode flags). */

	/**
	 * @brief Default ctor initializes all converter pointers and state.
	 *
	 * Typically responsible for setting pointers to nullptr and preconfiguring
	 * fallback character mapping options.
	 */
	CVnCharsetLib();

	/**
	 * @brief Destructor cleans up all owned converter instances.
	 */
	~CVnCharsetLib();

	/**
	 * @brief Retrieve or create converter for a given charset ID.
	 *
	 * @param charsetIdx identifier from VnConv repository to select mapping.
	 * @return pointer to active VnCharset object (allocated once per type).
	 */
	VnCharset *getVnCharset(int charsetIdx);
};

/**
 * @brief Static tables for charset translation.
 */
extern unsigned char SingleByteTables[][TOTAL_VNCHARS]; /**< table of standard-VN-index mappings for each single-byte legacy charset. */
extern UKWORD DoubleByteTables[][TOTAL_VNCHARS];		/**< table of standard-VN-index mappings for each double-byte legacy charset. */
extern UnicodeChar UnicodeTable[TOTAL_VNCHARS];			/**< canonical UTF-16 characters for each standard Vietnamese index. */
extern UKDWORD VIQRTable[TOTAL_VNCHARS];				/**< mapping from standard Vietnamese index to VIQR codepoint values. */
extern UKDWORD UnicodeComposite[TOTAL_VNCHARS];			/**< composed Unicode codepoints used for combined diacritic forms. */
extern UKWORD WinCP1258[TOTAL_VNCHARS];					/**< WinCP1258 8-bit output mapping for standard indexes. */
extern UKWORD WinCP1258Pre[TOTAL_VNCHARS];				/**< precomposed WinCP1258 mapping entries for accent generation fallback. */

extern DllInterface CVnCharsetLib VnCharsetLibObj; /**< global charset library instance */
extern VnConvOptions VnConvGlobalOptions;		   /**< global conversion options */
extern int StdVnNoTone[TOTAL_VNCHARS];			   /**< soft mapping of non-tone base chars */
extern int StdVnRootChar[TOTAL_VNCHARS];		   /**< root character mapping table */

/**
 * @brief Generic converter pipeline util.
 *
 * Orchestrates conversion from source charset to target charset through
 * an input/output conversion loop. Handles streaming in/out and error propagation.
 *
 * @param incs source charset converter instance.
 * @param outcs destination charset converter instance.
 * @param input input byte stream.
 * @param output output byte stream.
 * @return number of converted characters or negative error code.
 */
DllInterface int genConvert(VnCharset &incs, VnCharset &outcs, ByteInStream &input, ByteOutStream &output);

/**
 * @brief Convert standard char to uppercase variant.
 *
 * Uses standard Vietnamese case mapping table (or identity for non-alphabetic characters).
 *
 * @param ch standard Vietnamese char code.
 * @return uppercase standard Vietnamese char code.
 */
StdVnChar StdVnToUpper(StdVnChar ch);

/**
 * @brief Convert standard char to lowercase variant.
 *
 * Uses standard Vietnamese case mapping table (or identity for non-alphabetic characters).
 *
 * @param ch standard Vietnamese char code.
 * @return lowercase standard Vietnamese char code.
 */
StdVnChar StdVnToLower(StdVnChar ch);

/**
 * @brief Strip tone elements from a standard Vietnamese char to compute its root form.
 *
 * Removes diacritic tone markers but keeps base vowel components for lexical operations.
 *
 * @param ch standard Vietnamese char code.
 * @return root standard Vietnamese char code without tone element.
 */
StdVnChar StdVnRemoveTone(StdVnChar ch);

/**
 * @brief Compatibility alias for tone removal used by older converter code.
 *
 * @param ch standard Vietnamese char code.
 * @return root standard Vietnamese char code without tone element.
 */
StdVnChar StdVnGetRoot(StdVnChar ch);

#endif
