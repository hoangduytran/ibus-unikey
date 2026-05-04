/**
 * @file byteio.cpp
 * @brief Implements byte-oriented input/output stream classes for ukengine.
 *
 * This module provides in-memory and file-backed byte streams used by the
 * Vietnamese input engine for buffered reads, writes, marking, and peeking.
 */
// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#include <string.h>
#include "byteio.h"

//------------------------------------------------
/**
 * @brief Construct a memory-backed byte input stream.
 *
 * @param data Buffer to read from; caller retains ownership.
 * @param len Number of bytes in the buffer, or -1 for NUL-terminated data.
 * @param elementSize If 2 or 4, treat the termination test as 16-bit or 32-bit.
 */
StringBIStream::StringBIStream(UKBYTE *data, int len, int elementSize)
{
	m_data = m_current = data;
	m_len = m_left = len;

	const bool lenIsNegative = len == -1;
	if (lenIsNegative)
	{
		if (elementSize == 2)
			m_eos = (*(UKWORD *)data == 0);
		else if (elementSize == 4)
			m_eos = (*(UKDWORD *)data == 4);
		else
			m_eos = (*data == 0);
	}
	else
		m_eos = (len <= 0);
	m_didBookmark = 0;
}

//------------------------------------------------
/**
 * @brief Check whether the stream has reached end-of-data.
 *
 * @return Non-zero if no more bytes are available.
 */
int StringBIStream::eos()
{
	return m_eos;
}

//------------------------------------------------
/**
 * @brief Read the next byte from the stream.
 *
 * Advances the internal cursor and updates EOS state.
 *
 * @param[out] b Output byte value.
 * @return 1 if a byte was read, 0 on EOF.
 */
int StringBIStream::getNext(UKBYTE &b)
{
	if (m_eos)
		return 0;
	b = *m_current++;
	if (m_len == -1)
	{
		m_eos = (b == 0);
	}
	else
	{
		m_left--;
		m_eos = (m_left <= 0);
	}
	return 1;
}

//------------------------------------------------
/**
 * @brief Push a byte back so it can be read again.
 *
 * @param b Byte to unget.
 * @return 1 always; fails silently if already at buffer start.
 */
int StringBIStream::unget(UKBYTE b)
{
	if (m_current != m_data)
	{
		*--m_current = b;
		m_eos = 0;
		if (m_len != -1)
			m_left++;
	}
	return 1;
}

//------------------------------------------------
/**
 * @brief Read next 16-bit little-endian word from the stream.
 *
 * @param[out] w Output word value.
 * @return 1 if successful, 0 on EOF.
 */
int StringBIStream::getNextW(UKWORD &w)
{
	if (m_eos)
		return 0;
	w = *((UKWORD *)m_current);
	m_current += 2;
	if (m_len == -1)
		m_eos = (w == 0);
	else
	{
		m_left -= 2;
		m_eos = (m_left <= 0);
	}
	return 1;
}

//------------------------------------------------
/**
 * @brief Read next 32-bit little-endian double word from the stream.
 *
 * @param[out] dw Output double word value.
 * @return 1 if successful, 0 on EOF.
 */
int StringBIStream::getNextDW(UKDWORD &dw)
{
	if (m_eos)
		return 0;

	dw = *((UKDWORD *)m_current);
	m_current += 4;
	if (m_len == -1)
		m_eos = (dw == 0);
	else
	{
		m_left -= 4;
		m_eos = (m_left <= 0);
	}
	return 1;
}

//------------------------------------------------
/**
 * @brief Peek at the next byte without advancing the stream.
 *
 * @param[out] b Next byte value.
 * @return 1 if available, 0 on EOF.
 */
int StringBIStream::peekNext(UKBYTE &b)
{
	if (m_eos)
		return 0;
	b = *m_current;
	return 1;
}

//------------------------------------------------
/**
 * @brief Peek at the next 16-bit word without consuming it.
 *
 * @param[out] w Next word value.
 * @return 1 if available, 0 on EOF.
 */
int StringBIStream::peekNextW(UKWORD &w)
{
	if (m_eos)
		return 0;
	w = *((UKWORD *)m_current);
	return 1;
}

/*
//------------------------------------------------
int StringBIStream::peekNextDW(UKDWORD & dw)
{
	if (m_eos)
		return 0;
	dw = *((UKDWORD *)m_current);
	return 1;
}
*/

//------------------------------------------------
/**
 * @brief Rewind the stream to the beginning of the buffer.
 */
void StringBIStream::reopen()
{
	m_current = m_data;
	m_left = m_len;
	if (m_len == -1)
		m_eos = (m_data == 0);
	else
		m_eos = (m_len <= 0);
	m_didBookmark = 0;
}

//------------------------------------------------
/**
 * @brief Save the current stream position for later restoration.
 *
 * @return 1 always.
 */
int StringBIStream::bookmark()
{
	m_didBookmark = 1;
	m_bookmark.current = m_current;
	m_bookmark.data = m_data;
	m_bookmark.eos = m_eos;
	m_bookmark.left = m_left;
	m_bookmark.len = m_len;
	return 1;
}

//------------------------------------------------
/**
 * @brief Restore the stream cursor to the last bookmarked state.
 *
 * @return 1 if bookmark exists, 0 otherwise.
 */
int StringBIStream::gotoBookmark()
{
	if (!m_didBookmark)
		return 0;
	m_current = m_bookmark.current;
	m_data = m_bookmark.data;
	m_eos = m_bookmark.eos;
	m_left = m_bookmark.left;
	m_len = m_bookmark.len;
	return 1;
}

//------------------------------------------------
/**
 * @brief Close the memory stream.
 *
 * This implementation is a no-op because the caller owns the buffer.
 *
 * @return 1 always.
 */
int StringBIStream::close()
{
	return 1;
}

//////////////////////////////////////////////////
// Class StringBOStream
//////////////////////////////////////////////////

//------------------------------------------------
/**
 * @brief Construct an in-memory byte output stream.
 *
 * @param buf Output buffer provided by the caller.
 * @param len Capacity of the output buffer in bytes.
 */
StringBOStream::StringBOStream(UKBYTE *buf, int len)
{
	m_current = m_buf = buf;
	m_len = len;
	m_out = 0;
	m_bad = 0;
}

//------------------------------------------------
/**
 * @brief Write a single byte to the output buffer.
 *
 * @param b Byte value to write.
 * @return 1 on success, 0 on overflow.
 */
int StringBOStream::putB(UKBYTE b)
{
	m_out++;
	/*
		if (m_out >= 2147483647) {
			int err;
			err = 1;
		}
	*/
	if (m_bad)
		return 0;
	/*
		if (m_out < 0) {
			int i;
			i = 1;
		}
	*/
	if (m_out <= m_len)
	{
		*m_current++ = b;
		return 1;
	}
	m_bad = 1;
	return 0;
}

//------------------------------------------------
/**
 * @brief Write a 16-bit little-endian value to the buffer.
 *
 * @param w Value to write.
 * @return 1 on success, 0 on overflow.
 */
int StringBOStream::putW(UKWORD w)
{
	m_out += 2;
	if (m_bad)
		return 0;
	if (m_out <= m_len)
	{
		*((UKWORD *)m_current) = w;
		m_current += 2;
		return 1;
	}
	m_bad = 1;
	return 0;
}

//------------------------------------------------
/**
 * @brief Write text to the output buffer.
 *
 * @param s Source string data.
 * @param size Number of bytes to write, or -1 for NUL-terminated.
 * @return 1 on success, 0 on overflow.
 */
int StringBOStream::puts(const char *s, int size)
{
	if (size == -1)
	{
		while (*s)
		{
			m_out++;
			if (m_out <= m_len)
				*m_current++ = *s;
			s++;
		}
		if (!m_bad && m_out > m_len)
			m_bad = 1;
		return (!m_bad);
	}

	int n;
	if (!m_bad && m_out <= m_len)
	{
		n = m_len - m_out;
		if (n > size)
			n = size;
		memcpy(m_current, s, n);
		m_current += n;
	}

	m_out += size;
	if (!m_bad && m_out > m_len)
		m_bad = 1;
	return (!m_bad);
}

//------------------------------------------------
/**
 * @brief Reset the output cursor to the beginning of the buffer.
 *
 * Does not clear existing buffer contents.
 */
void StringBOStream::reopen()
{
	m_current = m_buf;
	m_out = 0;
	m_bad = 0;
}

//------------------------------------------------
/**
 * @brief Query whether the stream is still in a good state.
 *
 * @return 1 when no write error has occurred, 0 otherwise.
 */
int StringBOStream::isOK()
{
	return !m_bad;
}

////////////////////////////////////////////////////
// Class FileBIStream                             //
////////////////////////////////////////////////////

//----------------------------------------------------
/**
 * @brief Construct a buffered file input stream.
 *
 * @param bufSize Read buffer size.
 * @param buf Optional buffer supplied by caller.
 */
FileBIStream::FileBIStream(int bufSize, char *buf)
{
	m_file = NULL;
	m_buf = buf;
	m_bufSize = bufSize;
	m_own = 1;
	m_didBookmark = 0;

	m_readAhead = 0;
	m_lastIsAhead = 0;
}

//----------------------------------------------------
/**
 * @brief Destroy the file input stream, closing owned file handles.
 */
FileBIStream::~FileBIStream()
{
	if (m_own)
		close();
}

//----------------------------------------------------
/**
 * @brief Open a file for binary reading.
 *
 * @param fileName Path to the file to open.
 * @return 1 on success, 0 on failure.
 */
int FileBIStream::open(const char *fileName)
{
	m_file = fopen(fileName, "rb");
	if (m_file == NULL)
		return 0;
	setvbuf(m_file, m_buf, _IOFBF, m_bufSize);
	m_own = 0;
	m_readAhead = 0;
	m_lastIsAhead = 0;
	return 1;
}

//----------------------------------------------------
/**
 * @brief Close the current file handle.
 *
 * @return 1 always.
 */
int FileBIStream::close()
{
	if (m_file != NULL)
	{
		fclose(m_file);
		m_file = NULL;
	}
	return 1;
}

//----------------------------------------------------
/**
 * @brief Attach a pre-opened FILE* to the stream.
 *
 * @param f FILE pointer owned by the caller.
 */
void FileBIStream::attach(FILE *f)
{
	m_file = f;
	m_own = 0;
	m_readAhead = 0;
	m_lastIsAhead = 0;
}

//----------------------------------------------------
/**
 * @brief Return end-of-stream status for the file.
 *
 * @return 1 if EOF, 0 otherwise.
 */
int FileBIStream::eos()
{
	if (m_readAhead)
		return 0;
	return feof(m_file);
}

//----------------------------------------------------
/**
 * @brief Read the next byte from the file stream.
 *
 * Supports one-byte lookahead when necessary.
 *
 * @param[out] b Output byte.
 * @return 1 on success, 0 on EOF.
 */
int FileBIStream::getNext(UKBYTE &b)
{
	if (m_readAhead)
	{
		m_readAhead = 0;
		b = m_readByte;
		m_lastIsAhead = 1;
		return 1;
	}

	m_lastIsAhead = 0;
	b = (UKBYTE)fgetc(m_file);
	const bool byteAvailablePastEofProbe = (!feof(m_file));
	return byteAvailablePastEofProbe ? 1 : 0;
}

//----------------------------------------------------
/**
 * @brief Peek at the next byte without consuming it.
 *
 * @param[out] b Output byte value.
 * @return 1 if a byte is available, 0 on EOF.
 */
int FileBIStream::peekNext(UKBYTE &b)
{
	if (m_readAhead)
	{
		b = m_readByte;
		return 1;
	}

	b = (UKBYTE)fgetc(m_file);
	const bool streamAtEofAfterPeekRead = feof(m_file) != 0;
	if (streamAtEofAfterPeekRead)
		return 0;
	ungetc(b, m_file);
	return 1;
}

//----------------------------------------------------
/**
 * @brief Push a byte back onto the file stream.
 *
 * @param b Byte to unget.
 * @return 1 always.
 */
int FileBIStream::unget(UKBYTE b)
{
	if (m_lastIsAhead)
	{
		m_lastIsAhead = 0;
		m_readAhead = 1;
		m_readByte = b;
		return 1;
	}

	ungetc(b, m_file);
	return 1;
}

//----------------------------------------------------
/**
 * @brief Read a 16-bit little-endian word from the file stream.
 *
 * @param[out] w Output word.
 * @return 1 on success, 0 on failure.
 */
int FileBIStream::getNextW(UKWORD &w)
{
	UKBYTE b1, b2;
	if (!getNext(b1) || !getNext(b2))
		return 0;
	*((UKBYTE *)&w) = b1;
	*(((UKBYTE *)&w) + 1) = b2;
	return 1;
}

//----------------------------------------------------
/**
 * @brief Read a 32-bit little-endian double word from the file stream.
 *
 * @param[out] dw Output double word.
 * @return 1 on success, 0 on failure.
 */
int FileBIStream::getNextDW(UKDWORD &dw)
{
	UKWORD w1, w2;
	if (!getNextW(w1) || !getNextW(w2))
		return 0;
	*((UKWORD *)&dw) = w1;
	*(((UKWORD *)&dw) + 1) = w2;
	return 1;
}
//----------------------------------------------------
/**
 * @brief Peek the next 16-bit word without consuming it.
 *
 * @param[out] w Output word.
 * @return 1 if available, 0 otherwise.
 */
int FileBIStream::peekNextW(UKWORD &w)
{
	UKBYTE hi, low;
	if (!getNext(low))
		return 0;
	const bool hiByteReadOk = getNext(hi);
	if (!hiByteReadOk) {
		m_readAhead = 1;
		m_readByte = low;
		m_lastIsAhead = 0;
		return 0;
	}
	unget(hi);
	w = hi;
	w = (w << 8) + low;
	m_readAhead = 1;
	m_readByte = low;
	m_lastIsAhead = 0;
	return 1;
}

//----------------------------------------------------
/**
 * @brief Bookmark the current file position for later rewinding.
 *
 * @return 1 when bookmark is recorded.
 */
int FileBIStream::bookmark()
{
	m_didBookmark = 1;
	m_bookmark.pos = ftell(m_file);
	return 1;
}

//----------------------------------------------------
/**
 * @brief Rewind to the last bookmarked file position.
 *
 * @return 1 on success, 0 if no bookmark exists.
 */
int FileBIStream::gotoBookmark()
{
	if (!m_didBookmark)
		return 0;
	fseek(m_file, m_bookmark.pos, SEEK_SET);
	return 1;
}

////////////////////////////////////////////////////
// Class FileBOStream                             //
////////////////////////////////////////////////////
//----------------------------------------------------
/**
 * @brief Construct a buffered file output stream.
 *
 * @param bufSize Size of internal buffer for write operations.
 * @param buf Optional external buffer, not owned if provided.
 */
FileBOStream::FileBOStream(int bufSize, char *buf)
{
	m_file = NULL;
	m_buf = buf;
	m_bufSize = bufSize;
	m_own = 1;
	m_bad = 1;
}

//----------------------------------------------------
/**
 * @brief Destroy the file output stream and close owned file handles.
 */
FileBOStream::~FileBOStream()
{
	if (m_own)
		close();
}

//----------------------------------------------------
/**
 * @brief Open the file for binary output.
 *
 * @param fileName Path to the output file.
 * @return 1 on success, 0 on failure.
 */
int FileBOStream::open(const char *fileName)
{
	m_file = fopen(fileName, "wb");
	if (m_file == NULL)
		return 0;
	m_bad = 0;
	setvbuf(m_file, m_buf, _IOFBF, m_bufSize);
	m_own = 1;
	return 1;
}

//----------------------------------------------------
/**
 * @brief Attach an existing FILE* handle to the stream.
 *
 * @param f FILE* handle provided by the caller.
 */
void FileBOStream::attach(FILE *f)
{
	m_file = f;
	m_own = 0;
	m_bad = 0;
}

//----------------------------------------------------
/**
 * @brief Close the stream and flush buffered output.
 *
 * @return 1 on success, 0 on failure.
 */
int FileBOStream::close()
{
	if (m_file != NULL)
	{
		fclose(m_file);
		m_file = NULL;
	}
	return 1;
}

//----------------------------------------------------
/**
 * @brief Write a byte to the output file.
 *
 * @param b Byte to write.
 * @return 1 on success, 0 on failure.
 */
int FileBOStream::putB(UKBYTE b)
{
	if (m_bad)
		return 0;
	m_bad = (fputc(b, m_file) == EOF);
	return (!m_bad);
}

//----------------------------------------------------
/**
 * @brief Write a 16-bit little-endian value to the file.
 *
 * @param w Value to write.
 * @return 1 on success, 0 on failure.
 */
int FileBOStream::putW(UKWORD w)
{
	if (m_bad)
		return 0;
	//	m_bad = (fputwc(w, m_file) == WEOF);
	m_bad = (fputc((UKBYTE)w, m_file) == EOF);
	if (m_bad)
		return 0;
	m_bad = (fputc((UKBYTE)(w >> 8), m_file) == EOF);
	return (!m_bad);
}

//----------------------------------------------------
/**
 * @brief Write a block of bytes to the file.
 *
 * @param s Source buffer.
 * @param size Number of bytes to write, or -1 for NUL-terminated string.
 * @return 1 on success, 0 on failure.
 */
int FileBOStream::puts(const char *s, int size)
{
	if (m_bad)
		return 0;
	if (size == -1)
	{
		m_bad = (fputs(s, m_file) == EOF);
		return (!m_bad);
	}
	int out = fwrite(s, 1, size, m_file);
	m_bad = (out != size);
	return (!m_bad);
}

//----------------------------------------------------
/**
 * @brief Query whether the file stream is still operational.
 *
 * @return 1 when no prior write errors have occurred, 0 otherwise.
 */
int FileBOStream::isOK()
{
	return !m_bad;
}
