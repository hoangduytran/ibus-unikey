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

#ifndef __PATTERN_H
#define __PATTERN_H

#if defined(_WIN32)
#if defined(UNIKEYHOOK)
#define DllInterface __declspec(dllexport)
#else
#define DllInterface __declspec(dllimport)
#endif
#else
#define DllInterface // not used
#endif

#define MAX_PATTERN_LEN 40

// PatternState tracks the matching state for a single search pattern.
// It uses a border table so input may be consumed one character at a time
// and matches are found efficiently without restarting from scratch.
class DllInterface PatternState
{
public:
	char *m_pattern;				   // null-terminated pattern string being searched
	int m_border[MAX_PATTERN_LEN + 1]; // KMP border/failure table for the pattern
	int m_pos;						   // current scan position in the pattern
	int m_found;					   // nonzero once the pattern has been matched
	void init(char *pattern);		   // initialize state and build the border table
	void reset();					   // reset scan position to begin a new input sequence
	int foundAtNextChar(char ch);	   // consume next char and return 1 if matched
};

// PatternList manages a collection of PatternState objects so several patterns
// can be scanned in parallel against the same input stream.
class DllInterface PatternList
{
public:
	PatternState *m_patterns;			   // dynamic array of active pattern states
	int m_count;						   // number of patterns in the list
	void init(char **patterns, int count); // allocate and initialize the list
	int foundAtNextChar(char ch);		   // update every pattern with the next char
	void reset();						   // reset all contained pattern states

	PatternList()
	{
		m_count = 0;
		m_patterns = 0;
	}

	~PatternList()
	{
		if (m_patterns)
			delete[] m_patterns;
	}
};

#endif
