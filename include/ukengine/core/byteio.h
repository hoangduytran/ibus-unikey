/**
 * @file byteio.h
 * @brief Provides a set of abstract and concrete byte stream classes.
 *
 * These classes are used throughout ukengine for buffered I/O over
 * in-memory and file-backed sources. The design favors simple, minimal
 * stream semantics with support for peeking, bookmarking, and byte-wide
 * reads to avoid reallocations in performance-sensitive text conversion paths.
 *
 * Ownership:
 * - For StringBIStream/StringBOStream, caller owns input/output buffer.
 * - For FileBIStream/FileBOStream, stream owns file pointer when opened by
 *   open(), and may attach external FILE* without ownership transfer.
 */
// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef BYTE_IO_STREAM_H
#define BYTE_IO_STREAM_H


//#include "vnconv.h"
#include <stdio.h>

typedef unsigned char UKBYTE;
typedef unsigned short UKWORD;
typedef unsigned int UKDWORD;

//----------------------------------------------------
/**
 * @brief Base interface for all byte stream abstractions.
 *
 * Provides virtual destructor for proper cleanup via polymorphic pointers.
 */
class ByteStream {
 public:
  virtual ~ByteStream(){};
};

//----------------------------------------------------
/**
 * @brief Read-only stream interface for byte-oriented input.
 *
 * Implementations must provide sequential read operations and optionally
 * support bookmarks for rewinding. Return conventions are implementation-
 * specific but typically >=1 for success and 0 for EOF/error.
 */
class ByteInStream: public ByteStream
{
public:
	/**
	 * @brief Read next byte and consume it from stream.
	 *
	 * @param[out] b Output byte value.
	 * @return non-zero on success, zero on failure or EOF.
	 */
	virtual int getNext(UKBYTE &b) = 0;

	/**
	 * @brief Preview next byte without consuming.
	 *
	 * @param[out] b Output byte value.
	 * @return non-zero on success, zero on EOF.
	 */
	virtual int peekNext(UKBYTE &b) = 0;

	/**
	 * @brief Push a byte back to stream for subsequent read.
	 *
	 * @param b Byte to unget.
	 * @return non-zero on success, zero on failure.
	 */
	virtual int unget(UKBYTE b) = 0;

	/**
	 * @brief Read next word (16-bit) as little-endian.
	 *
	 * @param[out] w Output word.
	 * @return non-zero on success.
	 */
	virtual int getNextW(UKWORD &w) = 0;

	/**
	 * @brief Peek next word without consuming.
	 */
	virtual int peekNextW(UKWORD &w) = 0;

	/**
	 * @brief Read next double word (32-bit) as little-endian.
	 */
	virtual int getNextDW(UKDWORD &dw) = 0;

	/**
	 * @brief Save current stream state for later rewind.
	 *
	 * Default: unsupported, return 0.
	 */
	virtual int bookmark() //no support for bookmark by default
	{
		return 0;
	}

	/**
	 * @brief Restore stream state to last bookmark.
	 *
	 * Default: unsupported, return 0.
	 */
	virtual int gotoBookmark()
	{
		return 0;
	}

	/**
	 * @brief Return non-zero when end-of-stream has been reached.
	 */
	virtual int eos() = 0; //end of stream

	/**
	 * @brief Close the stream and release backend resources.
	 *
	 * After close(), stream objects should not be used unless reopened.
	 */
	virtual int close() = 0;
};

//----------------------------------------------------
/**
 * @brief Write-only stream interface for byte-oriented output.
 *
 * Implementations may be in-memory buffers or file-backed outputs.
 */
class ByteOutStream: public ByteStream
{
public:
	/**
	 * @brief Write single byte to stream.
	 *
	 * @param b Byte value
	 * @return non-zero on success, zero on failure.
	 */
	virtual int putB(UKBYTE b) = 0;

	/**
	 * @brief Write 16-bit word to stream (little-endian).
	 */
	virtual int putW(UKWORD w) = 0;

	/**
	 * @brief Write string length bytes or full NUL-terminated string.
	 *
	 * @param s Input string.
	 * @param size Number of bytes to write; -1 for strlen(s).
	 * @return non-zero on success.
	 */
	virtual int puts(const char *s, int size = -1) = 0; // write an 8-bit string

	/**
	 * @brief True if stream is still operational (no error state).
	 */
	virtual int isOK() = 0;// get current stream state
};

//----------------------------------------------------
/**
 * @brief In-memory byte input stream with optional bookmark support.
 *
 * This is used by the parser to read from a contiguous byte array without
 * performing heap allocations. It supports one bookmark slot to rewind state.
 */
class StringBIStream : public ByteInStream
{
protected:
	int m_eos;            ///< True if end of stream is reached; prevents further reads
	UKBYTE *m_data;       ///< Base data pointer (owned by caller, must stay alive)
	UKBYTE *m_current;    ///< Current read cursor, increments on each byte consumed
	int m_len;            ///< Total length of input buffer in bytes
	int m_left;           ///< Remaining bytes available for reading

	struct {
		int eos;            ///< End-of-stream state for bookmarked position
		UKBYTE *data;       ///< Bookmark snapshot of m_data
		UKBYTE *current;    ///< Bookmark snapshot of m_current
		int len;            ///< Bookmark snapshot of m_len
		int left;           ///< Bookmark snapshot of m_left
	} m_bookmark;         ///< Saved position for bookmark/goto

	int m_didBookmark;    ///< Non-zero if a bookmark has been set
		
public:
	/**
	 * @param data Buffer to read from (caller retains ownership)
	 * @param len Byte length of buffer
	 * @param elementSize 1 for bytes, set 2/4 to advance by wider units in
	 *        multi-byte interpretations (implementation-specific behavior).
	 */
	StringBIStream(UKBYTE *data, int len, int elementSize = 1);

	/**
	 * @brief Read and consume next byte.
	 *
	 * @param[out] b Next byte value.
	 * @return non-zero on success, zero if EOF.
	 */
	virtual int getNext(UKBYTE &b);

	/**
	 * @brief Peek next byte without advancing stream.
	 */
	virtual int peekNext(UKBYTE &b);

	/**
	 * @brief Put one byte back (unread) to support limited pushback.
	 */
	virtual int unget(UKBYTE b);

	/**
	 * @brief Read next 16-bit word from stream in little-endian order.
	 */
	virtual int getNextW(UKWORD &w);

	/**
	 * @brief Peek next 16-bit word without consuming.
	 */
	virtual int peekNextW(UKWORD &w);

	/**
	 * @brief Read next 32-bit double word in little-endian order.
	 */
	virtual int getNextDW(UKDWORD &dw);

    /**
     * @brief Return non-zero when no bytes remain.
     */
    virtual int eos(); //end of stream

	/**
	 * @brief Close stream; for memory stream this is no-op and does not free buffer.
	 */
	virtual int close();

	/**
	 * @brief Save current position for later gotoBookmark().
	 */
	virtual int bookmark();

	/**
	 * @brief Seek back to last bookmarked state.
	 */
	virtual int gotoBookmark();

	/**
	 * @brief Reopen stream at start of buffer without changing buffer contents.
	 */
	void reopen();

	/**
	 * @brief Number of bytes remaining in the stream.
	 */
	int left() {
		return m_left;
	}
};

//----------------------------------------------------
/**
 * @brief Buffered file-based input stream.
 *
 * Supports minimal Unicode and byte reading, with an optional internal
 * lookahead to handle systems without native wide-char file APIs.
 */
class FileBIStream : public ByteInStream
{
protected:
	FILE *m_file;       ///< Underlying FILE* handle (NULL when closed)
	int m_bufSize;      ///< Size of internal read buffer (bytes)
	char *m_buf;        ///< Buffer allocation for file read operations
	int m_own;          ///< Non-zero if object owns m_buf and must free on close
	int m_didBookmark;  ///< Non-zero if bookmark has been set via bookmark()

	struct {
		long pos;      ///< File position recorded at bookmark
	} m_bookmark;

	// Some systems don't have wide-char IO support, so we emulate using
	// one-byte lookahead buffering. This allows pixel-perfect powe
	UKBYTE m_readByte; ///< Lookahead byte for unget/peek when needed
	int m_readAhead;   ///< Number of bytes currently in lookahead state (0/1)
	int m_lastIsAhead; ///< Flag representing whether last read came from lookahead

public:
	/**
	 * @brief Construct a file input stream with optional preexisting buffer.
	 *
	 * @param bufsize Requested internal buffer size.
	 * @param buf If non-NULL, uses this buffer instead of allocating one.
	 */
	FileBIStream(int bufsize = 8192, char *buf = NULL);

	/**
	 * @brief Open a file for reading and initialize buffer state.
	 *
	 * @param fileName UTF-8 or current locale filename depending on platform.
	 * @return non-zero on success, zero on failure.
	 */
	int open(const char *fileName);
	
	/**
	 * @brief Attach to an already-open file handle.
	 *
	 * The stream does not take ownership of external handles by default.
	 * @param f Open FILE* handle.
	 */
	void attach(FILE *f);

	/**
	 * @brief Close stream and release resources.
	 *
	 * Closes file if opened by this object and frees buffer if owned.
	 * @return non-zero on success.
	 */
	virtual int close();

	/**
	 * @brief Read and consume next byte.
	 */
	virtual int getNext(UKBYTE &b);

	/**
	 * @brief Peek next byte without moving cursor.
	 */
	virtual int peekNext(UKBYTE &b);

	/**
	 * @brief Push one byte back onto stream for re-read.
	 */
	virtual int unget(UKBYTE b);

	/**
	 * @brief Read next 16-bit value from stream (little-endian).
	 */
	virtual int getNextW(UKWORD &w);

	/**
	 * @brief Peek next 16-bit value without consuming.
	 */
	virtual int peekNextW(UKWORD &w);

    /**
     * @brief Read next 32-bit value from stream (little-endian).
     */
    virtual int getNextDW(UKDWORD &dw);

    /**
     * @brief Determine stream end-of-file state.
     */
    virtual int eos(); //end of stream

	/**
	 * @brief Memorize position for gotoBookmark().
	 */
	virtual int bookmark();

	/**
	 * @brief Rewind to location stored by last bookmark().
	 */
	virtual int gotoBookmark();

	/**
	 * @brief Dtor.
	 */
	virtual ~FileBIStream();
};


//----------------------------------------------------
/**
 * @brief In-memory buffered output stream.
 *
 * The caller provides the output buffer. This class does not allocate or
 * free the buffer, but it tracks written bytes and failure state.
 */
class StringBOStream : public ByteOutStream
{
protected:
	UKBYTE *m_buf;      ///< Output buffer pointer (external ownership)
	UKBYTE *m_current;  ///< Write cursor position
	int m_out;          ///< Bytes written so far
	int m_len;          ///< Buffer length
	int m_bad;          ///< Error state flag
public:
	/**
	 * @param buf Target buffer (caller owned)
	 * @param len Buffer capacity in bytes
	 */
	StringBOStream(UKBYTE *buf, int len);

	/**
	 * @brief Write a single byte to output buffer.
	 *
	 * Checks capacity and sets `m_bad` on overflow.
	 */
	virtual int putB(UKBYTE b);

	/**
	 * @brief Write 16-bit value in little-endian order.
	 */
	virtual int putW(UKWORD w);

	/**
	 * @brief Write string data to output buffer.
	 *
	 * @param s Source data
	 * @param size Length to write; -1 means NUL-terminated string length
	 * @return non-zero on success, 0 on overflow/error.
	 */
	virtual int puts(const char *s, int size = -1);

	/**
	 * @brief Returns non-zero when stream is operational.
	 *
	 * @return 1 when no prior write errors occurred, else 0.
	 */
	virtual int isOK();

	/**
	 * @brief No-op close for in-memory stream.
	 *
	 * @return 1 always, buffer remains available to caller.
	 */
	virtual int close()   
	{
		return 1;
	};

	/**
	 * @brief Reset write cursor to start (keeping existing data intact).
	 *
	 * Does not clear the underlying buffer contents.
	 */
	void reopen();

	/**
	 * @brief Get number of bytes written so far.
	 *
	 * @return Number of bytes already emitted to output buffer.
	 */
	int getOutBytes() {
		return m_out;
	}
};

//----------------------------------------------------
/**
 * @brief Buffered file output stream.
 *
 * Writes bytes and words to an underlying FILE* with optional internal
 * buffer management.
 */
class FileBOStream : public ByteOutStream
{
protected:
	FILE *m_file;       ///< Back-end file pointer
	int m_bufSize;      ///< Internal buffer size for batch writes
	char *m_buf;        ///< Buffer memory (may be external)
	int m_own;          ///< Owns buffer memory if non-zero
	int m_bad;          ///< Error state indicator

public:
	/**
	 * @brief Create a file output stream with optional external buffer.
	 *
	 * @param bufsize Write buffer size to use.
	 * @param buf Optional preallocated buffer (not owned if provided).
	 */
	FileBOStream(int bufsize = 8192, char *buf = NULL);

	/**
	 * @brief Open a file for writing.
	 *
	 * @param fileName Name of file to open.
	 * @return non-zero on success; zero on failure.
	 */
	int open(const char *fileName);

	/**
	 * @brief Attach to an existing FILE* handle (no ownership transfer).
	 *
	 * @param f FILE handle; caller owns it.
	 */
	void attach(FILE *);

	/**
	 * @brief Close stream and flush any buffered output.
	 *
	 * @return non-zero on success; zero on failure.
	 */
	virtual int close();

	/**
	 * @brief Write single byte to file buffer.
	 */
	virtual int putB(UKBYTE b);

	/**
	 * @brief Write 16-bit word as little-endian to file buffer.
	 */
	virtual int putW(UKWORD w);

	/**
	 * @brief Write string data to file buffer.
	 */
	virtual int puts(const char *s, int size = -1);

	/**
	 * @brief Return non-zero when stream state is OK.
	 */
	virtual int isOK(); // get current stream state

	/**
	 * @brief Destructor, ensures close() is called.
	 */
	virtual ~FileBOStream();
};


#endif
