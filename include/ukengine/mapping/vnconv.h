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

//
#ifndef __VN_CONVERT_H
#define __VN_CONVERT_H

/**
 * @file vnconv.h
 * @brief Vietnamese character set conversion helper API.
 *
 * Exposes generic conversion entrypoints and error handling for a broad
 * range of legacy and Unicode encodings.
 */

#if defined(_WIN32)
    #if defined(UNIKEYHOOK)
        #define DllInterface   __declspec( dllexport )
    #else
        #define DllInterface   __declspec( dllimport )
    #endif
    #define DllExport   __declspec( dllexport )
    #define DllImport   __declspec( dllimport )
#else
    #define DllInterface //not used
    #define DllExport
    #define DllImport
#endif

/**
 * @brief Input/output charset identifiers for VnConvert runtime.
 */
#define CONV_CHARSET_UNICODE	0   /**< UTF-16 Unicode mode */
#define CONV_CHARSET_UNIUTF8    1   /**< UTF-8 Uni style */
#define CONV_CHARSET_UNIREF     2   /**< Uni reference format (&#D;) */
#define CONV_CHARSET_UNIREF_HEX 3   /**< Uni reference hex format */
#define CONV_CHARSET_UNIDECOMPOSED 4/**< decomposed Unicode sequence */
#define CONV_CHARSET_WINCP1258	5   /**< Windows CP1258 */
#define CONV_CHARSET_UNI_CSTRING 6  /**< C-string escaped Unicode */
#define CONV_CHARSET_VNSTANDARD 7   /**< internal standard VN encoding */

#define CONV_CHARSET_VIQR		10  /**< VIQR input/output */
#define CONV_CHARSET_UTF8VIQR 11  /**< UTF-8 VIQR combo */
#define CONV_CHARSET_XUTF8  12    /**< extended UTF-8 with tone markers */

#define CONV_CHARSET_TCVN3		20 /**< TCVN3 legacy encoding */
#define CONV_CHARSET_VPS		21 /**< VPS legacy encoding */
#define CONV_CHARSET_VISCII		22 /**< VISCII legacy encoding */
#define CONV_CHARSET_BKHCM1		23 /**< BK HCM1 legacy encoding */
#define CONV_CHARSET_VIETWAREF	24 /**< Vietware F legacy encoding */
#define CONV_CHARSET_ISC        25 /**< ISC legacy encoding */

#define CONV_CHARSET_VNIWIN		40 /**< VNI Windows legacy encoding. */
#define CONV_CHARSET_BKHCM2		41 /**< BK HCM2 legacy encoding. */
#define CONV_CHARSET_VIETWAREX	42 /**< Vietware X legacy encoding. */
#define CONV_CHARSET_VNIMAC		43 /**< VNIMAC legacy encoding. */

#define CONV_TOTAL_SINGLE_CHARSETS 6 /**< Count of legacy single-byte charsets (TCVN3/VPS/VISCII/BKHCM1/VIETWAREF/ISC). */
#define CONV_TOTAL_DOUBLE_CHARSETS 4 /**< Count of legacy double-byte charsets (VNIWIN/BKHCM2/VIETWAREX/VNIMAC). */


/**
 * @brief Predicate to test if a charset ID is a supported single-byte encoding.
 *
 * @param x charset ID to test.
 * @return non-zero true when x falls within configured single-byte range.
 */
#define IS_SINGLE_BYTE_CHARSET(x) (x >= CONV_CHARSET_TCVN3 && x < CONV_CHARSET_TCVN3 + CONV_TOTAL_SINGLE_CHARSETS)

/**
 * @brief Predicate to test if a charset ID is a supported double-byte encoding.
 *
 * @param x charset ID to test.
 * @return non-zero true when x falls within configured double-byte range.
 */
#define IS_DOUBLE_BYTE_CHARSET(x) (x >= CONV_CHARSET_VNIWIN && x < CONV_CHARSET_VNIWIN + CONV_TOTAL_DOUBLE_CHARSETS)

/**
 * @typedef UKBYTE
 * @brief Byte type used for conversion buffers in VnConvert APIs.
 */
typedef unsigned char UKBYTE;

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Convert a memory buffer from one charset to another.
 *
 * @param inCharset source charset type id
 * @param outCharset destination charset type id
 * @param input source byte array
 * @param output destination byte array (must be sized by caller)
 * @param pInLen in/out source buffer length consumed
 * @param pMaxOutLen in/out max output buffer length written
 * @return result code (0 success, negative on error)
 */
DllInterface  int VnConvert(int inCharset, int outCharset, UKBYTE *input, UKBYTE *output, 
		int * pInLen, int * pMaxOutLen);

/**
 * @brief Convert contents of an input file to an output file.
 *
 * Reads data from @p inFile and writes converted data to @p outFile
 * applying the given source and destination charset IDs.
 *
 * @param inCharset source charset ID (CONV_CHARSET_* constant)
 * @param outCharset destination charset ID (CONV_CHARSET_* constant)
 * @param inFile path to the input file to convert (must be readable)
 * @param outFile path to the output file to write (created/truncated)
 * @return conversion status code (0 for success; negative for errors)
 */
DllInterface  int VnFileConvert(int inCharset, int outCharset, const char *inFile, const char *outFile);

#if defined(__cplusplus)
}
#endif

/**
 * @brief Get human-readable VnConv error text.
 */
DllInterface const char * VnConvErrMsg(int errCode);

/**
 * @brief Error codes returned by VnConv functions.
 *
 * Each enum value is returned by conversion routines to indicate
 * the status and error type.  `VnConvErrMsg()` can be used to convert
 * these into human-readable text.
 */
enum VnConvError {
	VNCONV_NO_ERROR,      /**< conversion completed successfully. */
	VNCONV_UNKNOWN_ERROR, /**< unknown or unclassified conversion error. */
	VNCONV_INVALID_CHARSET, /**< input or output charset ID is not recognized. */
	VNCONV_ERR_INPUT_FILE, /**< input file path cannot be opened or read. */
	VNCONV_ERR_OUTPUT_FILE, /**< output file path cannot be created or written. */
	VNCONV_OUT_OF_MEMORY, /**< insufficient memory to complete conversion. */
	VNCONV_ERR_WRITING,   /**< error encountered during output write operations. */
	VNCONV_LAST_ERROR     /**< sentinel value, not returned; used for bounds checks. */
};

/**
 * @brief Mapping of charset name to charset identifier.
 */
typedef struct _CharsetNameId CharsetNameId;

struct _CharsetNameId {
	const char *name; /**< name string for charset */
	int id;          /**< charset ID constant */
};

/**
 * @brief Conversion engine options for VnConv mix mode.
 */
typedef struct _VnConvOptions VnConvOptions;

struct _VnConvOptions {
	int viqrMixed;   /**< allow VIQR mixed-mode conversion */
	int viqrEsc;     /**< escape-sequence mode in VIQR processing */
	int toUpper;     /**< convert output to uppercase */
	int toLower;     /**< convert output to lowercase */
	int removeTone;  /**< remove tone marks in output */
    int smartViqr;   /**< smart VIQR mode heuristics */
};

/**
 * @brief Apply conversion options to the global VnConv engine.
 *
 * Copies fields from user-provided VnConvOptions into internal settings.
 *
 * @param pOptions pointer to option structure; must not be nullptr.
 */
DllInterface void VnConvSetOptions(VnConvOptions *pOptions);

/**
 * @brief Retrieve current engine options into caller-provided structure.
 *
 * @param pOptions pointer to output option structure; must not be nullptr.
 */
DllInterface void VnConvGetOptions(VnConvOptions *pOptions);

/**
 * @brief Reset engine options to defaults and optionally return them.
 *
 * @param pOptions pointer to option structure to receive default values; optional but preferred.
 */
DllInterface void VnConvResetOptions(VnConvOptions *pOptions);

#endif
