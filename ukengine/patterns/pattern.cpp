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

#include "pattern.h"

/**
 * @file pattern.cpp
 * @brief Implements pattern matching using the Knuth-Morris-Pratt (KMP) algorithm for Vietnamese input processing.
 *
 * Provides PatternState and PatternList classes for efficient multi-pattern search, used in UniKey's Vietnamese input engine.
 *
 * Patterns are sequences of characters (e.g., "dd", "aw", "aa", "ee", "ow", "uw", "as", "af")
 * that represent Vietnamese letters or diacritics. For example:
 *   - "dd" → "đ"
 *   - "aw" → "ă"
 *   - "aa" → "â"
 *   - "ee" → "ê"
 *   - "ow" → "ơ"
 *   - "uw" → "ư"
 *   - "as" → "á" (sắc tone)
 *   - "af" → "à" (huyền tone)
 *
 * These patterns are detected in user input and converted to the appropriate Vietnamese characters.
 *
 * @note The actual set of patterns comes from Vietnamese input method mapping tables (such as Telex, VNI, VIQR),
 *       which are defined elsewhere in the codebase (e.g., core/inputproc.cpp). This file only provides the
 *       pattern matching engine; it does not define the patterns themselves.
 *
 * @note Not thread-safe. Intended for single-threaded input processing.
 */

//////////////////////////////////////////////////
// Pattern matching (based on KMP algorithm)
//////////////////////////////////////////////////

/**
 * @brief Reset the pattern state to initial position.
 *
 * Sets scan position and found flag to zero, ready for a new input sequence.
 */
void PatternState::reset()
{
	m_pos = 0;
	m_found = 0;
}

/**
 * @brief Initialize the pattern state and build the KMP border table.
 *
 * @param pattern Null-terminated pattern string to search for.
 *
 * Builds the border (failure) table for the KMP algorithm, allowing efficient pattern matching.
 */
void PatternState::init(char *pattern)
{
	m_pos = 0;
	m_found = 0;
	m_pattern = pattern;

	// Build KMP border table
	int i = 0, j = -1;
	m_border[i] = j;
	while (m_pattern[i])
	{
		// Find the longest border for the current prefix
		while (j >= 0 && m_pattern[i] != m_pattern[j])
			j = m_border[j];
		i++;
		j++;
		m_border[i] = j;
	}
}

/**
 * @brief Process the next input character and check for a pattern match.
 *
 * @param ch Next character from the input stream.
 * @return 1 if the pattern is matched at this position, 0 otherwise.
 *
 * Advances the scan position using the KMP algorithm. If the pattern is found, increments found count and resets position for overlapping matches.
 */
int PatternState::foundAtNextChar(char ch)
{
	int ret = 0;
	// Progress scan position using KMP border table
	while (m_pos >= 0 && ch != m_pattern[m_pos])
		m_pos = m_border[m_pos];
	m_pos++;
	if (m_pattern[m_pos] == 0)
	{
		m_found++;
		m_pos = m_border[m_pos]; // Prepare for next match (overlapping allowed)
		ret = 1;
	}
	return ret;
}

/**
 * @brief Initialize the pattern list with multiple patterns.
 *
 * @param patterns Array of null-terminated pattern strings.
 * @param count Number of patterns in the array.
 *
 * Allocates and initializes PatternState objects for each pattern.
 * @warning Existing pattern states are deleted and replaced.
 */
void PatternList::init(char **patterns, int count)
{
	m_count = count;
	delete[] m_patterns;
	m_patterns = new PatternState[count];
	for (int i = 0; i < count; i++)
		m_patterns[i].init(patterns[i]);
}

/**
 * @brief Process the next input character for all patterns.
 *
 * @param ch Next character from the input stream.
 * @return Index of a pattern that was matched, or -1 if none matched.
 *
 * Updates all contained PatternState objects. If multiple patterns match, returns the last matched index.
 */
int PatternList::foundAtNextChar(char ch)
{
	int patternFound = -1;
	// Check each pattern for a match
	for (int i = 0; i < m_count; i++)
	{
		if (m_patterns[i].foundAtNextChar(ch))
			patternFound = i;
	}
	return patternFound;
}

/**
 * @brief Reset all pattern states in the list.
 *
 * Sets all contained PatternState objects to their initial state.
 */
void PatternList::reset()
{
	for (int i = 0; i < m_count; i++)
		m_patterns[i].reset();
}
