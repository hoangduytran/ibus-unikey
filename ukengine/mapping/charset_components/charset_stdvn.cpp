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

#define IS_ODD(x) (x & 1)
#define IS_EVEN(x) (!(x & 1))

StdVnChar StdVnToUpper(StdVnChar ch)
{
	if (ch >= VnStdCharOffset && ch < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) &&
		IS_ODD(ch))
		ch -= 1;
	return ch;
}

/**
 * @brief Convert an internal StdVnChar to its uppercase partner when
 * applicable.
 *
 * The internal alphabet uses adjacent paired values for lowercase/uppercase
 * (even/odd). This helper returns the uppercase partner if `ch` is a
 * mapped Vietnamese alphabetic character encoded with the odd/even scheme.
 * Otherwise the input is returned unchanged.
 *
 * @param ch Input standard Vietnamese character.
 * @return Uppercase StdVnChar or the original `ch` if not applicable.
 *
 * @usage
 *   StdVnChar up = StdVnToUpper(ch);
 */

//----------------------------------------
/**
 * @brief Convert an internal StdVnChar to its lowercase form when applicable.
 *
 * If the character is a mapped Vietnamese alphabetic character and its
 * internal encoding uses an even/odd paired scheme for case, adjust the
 * value to the lowercase partner. Otherwise returns the input unchanged.
 *
 * @param ch Input standard Vietnamese character
 * @return Lowercase StdVnChar value or the original `ch` if not applicable
 */
StdVnChar StdVnToLower(StdVnChar ch)
{
	if (ch >= VnStdCharOffset && ch < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) &&
		IS_EVEN(ch))
		ch += 1;
	return ch;
}

//----------------------------------------
/**
 * @brief Return the root (base) StdVnChar for a possibly derived character.
 *
 * Uses the `StdVnRootChar` lookup to map derived/combined characters back to
 * their canonical base character within the standard character table.
 *
 * @param ch Input standard Vietnamese character
 * @return The root StdVnChar, or the input unchanged if out of range
 */
StdVnChar StdVnGetRoot(StdVnChar ch)
{
	if (ch >= VnStdCharOffset && ch < VnStdCharOffset + TOTAL_VNCHARS)
		ch = VnStdCharOffset + StdVnRootChar[ch - VnStdCharOffset];
	return ch;
}
