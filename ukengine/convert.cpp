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

--------------------------------------------------------------------------------
Purpose:
This file is part of the Vietnamese Encoding Converter Library (VnConv) within the UniKey project. It provides functionality for converting text between different Vietnamese character sets. The file includes functions for stream, buffer, and file-based conversions, as well as character set transformations such as case conversion and tone removal.

Key Functions:
1. vnFileStreamConvert: Converts a file stream from one Vietnamese charset to another.
2. genConvert: A general-purpose function for converting Vietnamese character sets using input and output streams.
3. VnConvert: Converts Vietnamese text between two character sets, managing memory for input and output buffers.
4. VnFileConvert: Handles file-based conversions, including support for standard input/output and temporary files.
5. VnConvErrMsg: Retrieves human-readable error messages for conversion errors.

Usage Context:
This file operates as a backend utility for encoding conversions. Its functionality might be indirectly accessible through:
- Input Method Settings: If the UniKey engine is integrated into an input method framework like IBus, users might configure encoding options in the settings.
- Command-Line Tools: If the project includes command-line utilities, this file's functionality could be invoked for batch conversions or testing.
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
int vnFileStreamConvert(int inCharset, int outCharset, FILE *inf, FILE *outf);

/**
 * @brief General conversion function for Vietnamese character sets.
 *
 * This function reads from an input stream, processes the data according to the specified
 * input and output character sets, and writes the converted data to an output stream.
 *
 * @param incs Input character set object.
 * @param outcs Output character set object.
 * @param input Input byte stream.
 * @param output Output byte stream.
 * @return 0 if successful, VNCONV_OUT_OF_MEMORY if memory allocation fails.
 */
// General conversion function for Vietnamese character sets.
// Reads from input stream, processes data according to input/output charsets, and writes to output stream.
DllExport int genConvert(VnCharset &incs, VnCharset &outcs, ByteInStream &input, ByteOutStream &output)
{
	StdVnChar stdChar;			 // Holds the current standard Vietnamese character being processed
	int bytesRead, bytesWritten; // Number of bytes read from input and written to output

	// Initialize the input character set for reading
	incs.startInput();
	// Initialize the output character set for writing
	outcs.startOutput();

	int ret = 1; // Return value: 1 for success, 0 for out-of-memory
	// Main loop: process input stream until end-of-stream
	while (!input.eos())
	{
		stdChar = 0; // Reset character for each iteration
		// Read next character from input stream using input charset
		if (incs.nextInput(input, stdChar, bytesRead))
		{
			// If the character is valid, process it
			if (stdChar != INVALID_STD_CHAR)
			{
				// Apply lowercase transformation if requested
				if (VnCharsetLibObj.m_options.toLower)
					stdChar = StdVnToLower(stdChar);
				// Otherwise, apply uppercase transformation if requested
				else if (VnCharsetLibObj.m_options.toUpper)
					stdChar = StdVnToUpper(stdChar);
				// Remove tone marks if requested
				if (VnCharsetLibObj.m_options.removeTone)
					stdChar = StdVnGetRoot(stdChar);
				// Write the transformed character to the output stream
				ret = outcs.putChar(output, stdChar, bytesWritten);
			}
		}
		else
			break; // Stop if no more input can be read
	}
	// Return 0 if successful, or VNCONV_OUT_OF_MEMORY if output failed
	return (ret ? 0 : VNCONV_OUT_OF_MEMORY);
}

//----------------------------------------------
// Arguments:
//       inCharset: charset of input
//       outCharset: charset of output
//       input: input data
//       output: output data
//       inLen: [in]  size of input. if inLen = -1, input data is null-terminated.
//              [out] if input inLen != -1, output iLen is the numbers of byte left in input.
//       maxOutLen: [in] size of output.
//                  [out] number of bytes output, if enough memory
//                        number of bytes needed for output, if not enough memory
// Returns:  0 if successful
//           error code: if failed
//----------------------------------------------
// int VnConvert(int inCharset, int outCharset, UKBYTE *input, UKBYTE *output, int & inLen, int & maxOutLen)

/**
 * @brief Converts Vietnamese text between two character sets.
 *
 * This function handles memory management for input and output buffers.
 *
 * @param inCharset Input charset identifier.
 * @param outCharset Output charset identifier.
 * @param input Pointer to input buffer.
 * @param output Pointer to output buffer.
 * @param pInLen Pointer to input length. Updated to remaining bytes after conversion.
 * @param pMaxOutLen Pointer to output buffer size. Updated to bytes written or required.
 * @return 0 if successful, error code otherwise.
 */
// Converts Vietnamese text between two character sets in memory buffers.
// Handles memory management for input and output buffers.
DllExport int VnConvert(int inCharset, int outCharset, UKBYTE *input, UKBYTE *output,
						int *pInLen, int *pMaxOutLen)
{
	int inLen, maxOutLen; // Local copies of input and output buffer sizes
	int ret = -1;		  // Return value: 0 for success, error code otherwise

	inLen = *pInLen;		 // Get input buffer length from pointer
	maxOutLen = *pMaxOutLen; // Get output buffer length from pointer

	// Check for invalid input length (negative but not -1)
	if (inLen != -1 && inLen < 0)
		return ret; // Return error if input length is invalid

	// Retrieve input charset object for the given input charset ID
	VnCharset *pInCharset = VnCharsetLibObj.getVnCharset(inCharset);
	// Retrieve output charset object for the given output charset ID
	VnCharset *pOutCharset = VnCharsetLibObj.getVnCharset(outCharset);

	// If either charset is invalid, return error code
	if (!pInCharset || !pOutCharset)
		return VNCONV_INVALID_CHARSET;

	// Initialize input stream with input buffer, length, and element size
	StringBIStream is(input, inLen, pInCharset->elementSize());
	// Initialize output stream with output buffer and max output length
	StringBOStream os(output, maxOutLen);

	// Perform the conversion using the charset objects and streams
	ret = genConvert(*pInCharset, *pOutCharset, is, os);
	// Update output buffer size with number of bytes written
	*pMaxOutLen = os.getOutBytes();
	// Update input length with number of bytes left after conversion
	*pInLen = is.left();
	// Return conversion result (0 for success, error code otherwise)
	return ret;
}

//---------------------------------------
// Arguments:
//   inFile: input file name. NULL if STDIN is used
//   outFile: output file name, NULL if STDOUT is used
// Returns:
//     0: successful
//     errCode: if failed
//---------------------------------------
/**
 * @brief Converts Vietnamese text between two character sets using files.
 *
 * This function supports standard input and output streams as well as temporary files
 * to handle cases where input and output files are the same.
 *
 * @param inCharset Input charset identifier.
 * @param outCharset Output charset identifier.
 * @param inFile Input file name. NULL for standard input.
 * @param outFile Output file name. NULL for standard output.
 * @return 0 if successful, error code otherwise.
 */
// Converts Vietnamese text between two character sets using files.
// Handles standard input/output and uses a temporary file if input and output files may overlap.
DllExport int VnFileConvert(int inCharset, int outCharset, const char *inFile, const char *outFile)
{
	FILE *inf = NULL;  // Input file pointer
	FILE *outf = NULL; // Output file pointer
	int ret = 0;	   // Return code for conversion status
	char tmpName[32];  // Buffer for temporary output file name

	// Open input file or use stdin if inFile is NULL
	if (inFile == NULL)
	{
		inf = stdin; // Use standard input
#if defined(_WIN32)
		_setmode(_fileno(stdin), _O_BINARY); // Set stdin to binary mode on Windows
#endif
	}
	else
	{
		inf = fopen(inFile, "rb"); // Open input file for reading in binary mode
		if (inf == NULL)
		{
			ret = VNCONV_ERR_INPUT_FILE; // Set error code if input file can't be opened
			goto end;					 // Exit function
		}
	}

	// Open output file or use stdout if outFile is NULL
	if (outFile == NULL)
		outf = stdout; // Use standard output
	else
	{
		// Prepare to use a temporary file for output to avoid overwriting input
		char outDir[256];		 // Buffer for output directory
		strcpy(outDir, outFile); // Copy output file name

#if defined(_WIN32)
		char *p = strrchr(outDir, '\\'); // Find last backslash for directory on Windows
#else
		char *p = strrchr(outDir, '/'); // Find last slash for directory on Unix
#endif

		if (p == NULL)
			outDir[0] = 0; // No directory part, use current directory
		else
			*p = 0; // Terminate string at directory separator

		strcpy(tmpName, outDir);   // Start temporary file name with directory
		strcat(tmpName, "XXXXXX"); // Append XXXXXX for mkstemp template

		// Create a unique temporary file
		if (mkstemp(tmpName) == -1)
		{
			fclose(inf);				  // Close input file if open
			ret = VNCONV_ERR_OUTPUT_FILE; // Set error code if temp file can't be created
			goto end;					  // Exit function
		}
		outf = fopen(tmpName, "wb"); // Open temporary file for writing in binary mode

		if (outf == NULL)
		{
			fclose(inf);				  // Close input file if open
			ret = VNCONV_ERR_OUTPUT_FILE; // Set error code if temp file can't be opened
			goto end;					  // Exit function
		}
	}

	// Perform the file stream conversion
	ret = vnFileStreamConvert(inCharset, outCharset, inf, outf);
	// Close input file if it is not stdin
	if (inf != stdin)
		fclose(inf);
	// If output is not stdout, handle the temporary file
	if (outf != stdout)
	{
		fclose(outf); // Close output file

		// If conversion succeeded, move temp file to final output
		if (ret == 0)
		{
			remove(outFile); // Remove existing output file if it exists
#if !defined(_WIN32)
			char cmd[256];
			sprintf(cmd, "mv %s %s", tmpName, outFile); // Prepare move command
			cmd[0] = system(cmd);						// Execute move command
#else
			if (rename(tmpName, outFile) != 0)
			{
				remove(tmpName);			  // Remove temp file if rename fails
				ret = VNCONV_ERR_OUTPUT_FILE; // Set error code
				goto end;					  // Exit function
			}
#endif
		}
		else
			remove(tmpName); // Remove temp file if conversion failed
	}

end:
#if defined(_WIN32)
	// Restore stdin to binary mode if needed (Windows only)
	if (inf == stdin)
	{
		_setmode(_fileno(stdin), _O_BINARY);
	}
#endif
	return ret; // Return conversion result
}

//------------------------------------------------
// Returns:
//     0: successful
//     errCode: if failed
//---------------------------------------
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
// Converts a file stream between Vietnamese character sets.
// Attaches input/output streams to file pointers and performs the conversion.
int vnFileStreamConvert(int inCharset, int outCharset, FILE *inf, FILE *outf)
{
	// Get the input charset object for the specified input charset ID
	VnCharset *pInCharset = VnCharsetLibObj.getVnCharset(inCharset);
	// Get the output charset object for the specified output charset ID
	VnCharset *pOutCharset = VnCharsetLibObj.getVnCharset(outCharset);

	// If either charset is invalid, return an error code
	if (!pInCharset || !pOutCharset)
		return VNCONV_INVALID_CHARSET;

	// If output charset is Unicode, write BOM (Byte Order Mark) to output file
	if (outCharset == CONV_CHARSET_UNICODE)
	{
		UKWORD sign = 0xFEFF;					// UTF-16 BOM
		fwrite(&sign, sizeof(UKWORD), 1, outf); // Write BOM to output
	}

	FileBIStream is; // Input file byte stream wrapper
	FileBOStream os; // Output file byte stream wrapper

	is.attach(inf);	 // Attach input file pointer to input stream
	os.attach(outf); // Attach output file pointer to output stream

	// Perform the conversion using the charset objects and streams
	return genConvert(*pInCharset, *pOutCharset, is, os);
}

/**
 * @brief Error messages for Vietnamese conversion errors.
 *
 * This array maps error codes to human-readable error messages.
 */
const char *ErrTable[VNCONV_LAST_ERROR] =
	{
		"No error",
		"Unknown error",
		"Invalid charset",
		"Error opening input file",
		"Error opening output file",
		"Error writing to output stream",
		"Not enough memory",
};

/**
 * @brief Retrieves the error message corresponding to an error code.
 *
 * @param errCode Error code.
 * @return Pointer to the error message string.
 */
DllExport const char *VnConvErrMsg(int errCode)
{
	if (errCode < 0 || errCode >= VNCONV_LAST_ERROR)
		errCode = VNCONV_UNKNOWN_ERROR;
	return ErrTable[errCode];
}
