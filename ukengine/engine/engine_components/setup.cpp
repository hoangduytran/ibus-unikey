// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/* Unikey Vietnamese Input Method
 * Copyright (C) 2000-2005 Pham Kim Long
 * Contact:
 *   unikey@gmail.com
 *   UniKey project: http://unikey.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#if defined(_WIN32)
#include "keyhook.h"
#endif

#include "engine_components/engine_tables_shared.h"
//--------------------------------------------------
/**
 * @brief Initialize global UniKey engine tables and character maps.
 *
 * This must be called once prior to using the engine conversion routines.
 */
void SetupUnikeyEngine()
{
    SetupInputClassifierTable();
    int i;
    VnLexiName lexi;

    // Calculate IsoStdVnCharMap
    for (i = 0; i < 256; i++)
    {
        IsoStdVnCharMap[i] = i;
    }

    for (i = 0; SpecialWesternChars[i]; i++)
    {
        IsoStdVnCharMap[SpecialWesternChars[i]] = (vnl_lastChar + i) + VnStdCharOffset;
    }

    for (i = 0; i < 256; i++)
    {
        if ((lexi = IsoToVnLexi(i)) != vnl_nonVnChar)
        {
            IsoStdVnCharMap[i] = lexi + VnStdCharOffset;
        }
    }
}
