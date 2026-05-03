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

#include "charset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
	#include <io.h>
	#include <fcntl.h>
#endif

#include "vnconv.h"

/**
 * @brief Converts a file stream from one Vietnamese charset to another.
 *
 * @param inCharset Input charset identifier.
 * @param outCharset Output charset identifier.
 * @param inf Input file stream pointer.
 * @param outf Output file stream pointer.
 * @return 0 if successful, error code otherwise.
 */
int vnFileStreamConvert(int inCharset, int outCharset, FILE * inf, FILE *outf);

/**
 * @brief General conversion function for Vietnamese character sets. Reads from input stream, processes data according to input/output charsets, and writes to output stream
 *
 * @param incs Input character set object.
 * @param outcs Output character set object.
 * @param input Input byte stream.
 * @param output Output byte stream.
 * @return 0 if successful, VNCONV_OUT_OF_MEMORY if memory allocation fails.
 */
DllExport int genConvert(VnCharset & incs, VnCharset & outcs, ByteInStream & input, ByteOutStream & output)
{
	StdVnChar stdChar;	// standard Vietnamese character
	int bytesRead, bytesWritten; // number of bytes read and written

	incs.startInput(); // start input conversion
	outcs.startOutput(); // start output conversion

	int ret = 1; // return value
	while (!input.eos()) {
        stdChar = 0; // standard Vietnamese character
		if (incs.nextInput(input, stdChar, bytesRead)) {
			if (stdChar != INVALID_STD_CHAR) { // if standard Vietnamese character is valid
			  if (VnCharsetLibObj.m_options.toLower) // if to lower option is set
			    stdChar = StdVnToLower(stdChar);	// convert to lower case
			  else if (VnCharsetLibObj.m_options.toUpper) // if to upper option is set
			    stdChar = StdVnToUpper(stdChar);	// convert to upper case
			  if (VnCharsetLibObj.m_options.removeTone) // if remove tone option is set
			    stdChar = StdVnGetRoot(stdChar);	// remove tone
			  ret = outcs.putChar(output, stdChar, bytesWritten);	// write to output stream
			}
		}
		else break; // if standard Vietnamese character is invalid, break the loop
	}
	return (ret? 0 : VNCONV_OUT_OF_MEMORY); // return 0 if successful, VNCONV_OUT_OF_MEMORY if memory allocation fails
}

/**
 * @brief Converts Vietnamese text between two character sets.
 *
 * This function handles memory management for input and output buffers.
 *
 * @param inCharset Input charset identifier.
 * @param outCharset Output charset identifier.
 * @param input Pointer to input buffer.
 * @param output Pointer to output buffer.
 * @param pInLen Pointer to input length. Updated to remaining bytes after conversion. If inLen = -1, input data is null-terminated.
 * @param pMaxOutLen Pointer to output buffer size. Updated to bytes written or required ie. number of bytes output, if enough memory or number of bytes needed for output, if not enough memory
 * @return 0 if successful, error code otherwise.
 */
DllExport int VnConvert(int inCharset, int outCharset, UKBYTE *input, UKBYTE *output, 
	      int * pInLen, int * pMaxOutLen)
{
	int inLen, maxOutLen; // input length and maximum output length
	int ret = -1; // return value

	inLen = *pInLen; // input length
	maxOutLen = *pMaxOutLen; // maximum output length

	if (inLen != -1 && inLen < 0) // invalid inLen
		return ret; // return error code

	VnCharset *pInCharset = VnCharsetLibObj.getVnCharset(inCharset); // get input character set
	VnCharset *pOutCharset = VnCharsetLibObj.getVnCharset(outCharset); // get output character set

	if (!pInCharset || !pOutCharset) // if input or output character set is invalid
		return VNCONV_INVALID_CHARSET;

	StringBIStream is(input, inLen, pInCharset->elementSize()); // create input stream
	StringBOStream os(output, maxOutLen); // create output stream

	ret = genConvert(*pInCharset, *pOutCharset, is, os); // convert characters
	*pMaxOutLen = os.getOutBytes(); // update maximum output length
	*pInLen = is.left(); // update input length
	return ret; // return error code
}

/**
 * @brief Converts Vietnamese text between two character sets using files.
 *
 * This function supports standard input and output streams as well as temporary files
 * to handle cases where input and output files are the same.
 *
 * @param inCharset Input charset identifier.
 * @param outCharset Output charset identifier.
 * @param inFile Input file name. NULL for STDIN.
 * @param outFile Output file name. NULL for STDOUT.
 * @return 0 if successful, error code otherwise.
 */
DllExport int VnFileConvert(int inCharset, int outCharset, const char *inFile, const char *outFile)
{
	FILE *inf = NULL; // input file pointer
	FILE *outf = NULL;
	int ret = 0; // return value
	char tmpName[32]; // temporary file name

	if (inFile == NULL) { // if input file is NULL
		inf = stdin; // set input file pointer to stdin
#if defined(_WIN32)
		_setmode( _fileno(stdin), _O_BINARY); // set input file pointer to stdin
#endif
	}
	else {
		inf = fopen(inFile, "rb"); // open input file
		if (inf == NULL) {
			ret = VNCONV_ERR_INPUT_FILE; // set return value to VNCONV_ERR_INPUT_FILE
			goto end; // goto end
		}
	}

	if (outFile == NULL) // if output file is NULL
		outf = stdout; // set output file pointer to stdout
	else {
		// setup temporary output file (because real output file may be the same as input file
		char outDir[256]; // output directory
		strcpy(outDir, outFile); // copy output file name to output directory string

#if defined(_WIN32)
		char *p = strrchr(outDir, '\\'); // find last backslash in output directory string
#else
		char *p = strrchr(outDir, '/'); // find last forward slash in output directory string
#endif

		if (p == NULL) // if last backslash or forward slash is not found
			outDir[0] = 0; // set first character of output directory string to NULL
		else
			*p = 0; // set last backslash or forward slash to NULL

		strcpy(tmpName, outDir); // copy output directory string to temporary file name string
        strcat(tmpName, "XXXXXX"); // append "XXXXXX" to temporary file name string

		if (mkstemp(tmpName) == -1) { // create temporary file
			fclose(inf);
			ret = VNCONV_ERR_OUTPUT_FILE; // set return value to VNCONV_ERR_OUTPUT_FILE
			goto end; // goto end
		}
		outf = fopen(tmpName, "wb"); // open output file

		if (outf == NULL) { // if output file is not opened
			fclose(inf);
			ret = VNCONV_ERR_OUTPUT_FILE; // set return value to VNCONV_ERR_OUTPUT_FILE
			goto end; // goto end
		}
	}


	ret = vnFileStreamConvert(inCharset, outCharset, inf, outf); // convert file stream
	if (inf != stdin) // if input file is not stdin
		fclose(inf); // close input file
	if (outf != stdout) { // if output file is not stdout
		fclose(outf); // close output file
		if (ret == 0) { // if conversion is successful
			remove(outFile); // remove output file
			remove(tmpName); // remove temporary file
		}
	}

end:
#if defined(_WIN32)
	if (inf == stdin) { // if input file is stdin
		_setmode( _fileno(stdin), _O_BINARY); // set input file pointer to stdin
	}
#endif
	return ret; // return error code
}

/**
 * @brief Converts a file stream between Vietnamese character sets.
 *
 * This function attaches input and output streams to the provided file pointers
 * and performs the conversion using the specified character sets.
 *
 * @param inCharset Input charset identifier.
 * @param outCharset Output charset identifier.
 * @param inf Input file stream pointer.
 * @param outf Output file stream pointer.
 * @return 0 if successful, error code otherwise.
 */
int vnFileStreamConvert(int inCharset, int outCharset, FILE * inf, FILE *outf)
{
	VnCharset *pInCharset = VnCharsetLibObj.getVnCharset(inCharset);	// get input character set
	VnCharset *pOutCharset = VnCharsetLibObj.getVnCharset(outCharset);	// get output character set

	if (!pInCharset || !pOutCharset) // if input or output character set is invalid
		return VNCONV_INVALID_CHARSET;

	if (outCharset == CONV_CHARSET_UNICODE) { // if output character set is Unicode
		UKWORD sign = 0xFEFF; // Unicode sign
		fwrite(&sign, sizeof(UKWORD), 1, outf); // write Unicode sign to output file
	}

	FileBIStream is; // create input stream
	FileBOStream os; // create output stream

	is.attach(inf); // attach input stream to input file
	os.attach(outf); // attach output stream to output file

	return genConvert(*pInCharset, *pOutCharset, is, os); // convert characters
}

/**
 * @brief Error messages for Vietnamese conversion errors.
 *
 * This array maps error codes to human-readable error messages.
 */
const char *ErrTable[VNCONV_LAST_ERROR] = 
{"No error", 						// 0
 "Unknown error", 					// 1
 "Invalid charset", 				// 2
 "Error opening input file", 		// 3
 "Error opening output file", 		// 4
 "Error writing to output stream", 	// 5
 "Not enough memory", 				// 6
};


/**
 * @brief Retrieves the error message corresponding to an error code.
 *
 * @param errCode Error code.
 * @return Pointer to the error message string.
 */
DllExport const char * VnConvErrMsg(int errCode)
{
	if (errCode < 0 || errCode >= VNCONV_LAST_ERROR) 	// if error code is invalid, set error code to VNCONV_UNKNOWN_ERROR
		errCode = VNCONV_UNKNOWN_ERROR; 				
	return ErrTable[errCode]; 							// return error message
}

