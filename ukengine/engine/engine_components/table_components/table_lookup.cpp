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
#include <stdlib.h>

#include "engine_components/engine_tables_shared.h"
#include "engine_components/table_components/table_internal.h"

//------------------------------------------------
/**
 * @brief Implementation of lookupVSeq.
 *
 * Maps up to three VnLexiName letters to a VowelSeq enum using binary search.
 *
 * @param v1 First vowel letter (required)
 * @param v2 Second vowel letter (optional, default vnl_nonVnChar)
 * @param v3 Third vowel letter (optional, default vnl_nonVnChar)
 * @return The canonical VowelSeq enum value, or vs_nil if not found.
 *
 * Example usage:
 *   VowelSeq seq = lookupVSeq(vnl_a, vnl_i); // "ai" → vs_ai
 *   VowelSeq seq2 = lookupVSeq(vnl_o);       // "o"  → vs_o
 */
VowelSeq lookupVSeq(VnLexiName v1, VnLexiName v2, VnLexiName v3)
{
    VSeqPair key;
    key.v[0] = v1;
    key.v[1] = v2;
    key.v[2] = v3;

    VSeqPair *pInfo = (VSeqPair *)bsearch(&key, SortedVSeqList, VSeqCount, sizeof(VSeqPair), tripleVowelCompare);
    if (pInfo == 0)
        return vs_nil;
    return pInfo->vs;
}

//------------------------------------------------
/**
 * @brief Implementation of lookupCSeq.
 *
 * Maps up to three VnLexiName letters to a ConSeq enum using binary search.
 *
 * @param c1 First consonant letter (required)
 * @param c2 Second consonant letter (optional, default vnl_nonVnChar)
 * @param c3 Third consonant letter (optional, default vnl_nonVnChar)
 * @return The canonical ConSeq enum value, or cs_nil if not found.
 *
 * Example usage:
 *   ConSeq cs = lookupCSeq(vnl_n, vnl_g); // "ng" → cs_ng
 *   ConSeq cs2 = lookupCSeq(vnl_b);       // "b"  → cs_b
 */
ConSeq lookupCSeq(VnLexiName c1, VnLexiName c2, VnLexiName c3)
{
    CSeqPair key;
    key.c[0] = c1;
    key.c[1] = c2;
    key.c[2] = c3;

    CSeqPair *pInfo = (CSeqPair *)bsearch(&key, SortedCSeqList, CSeqCount, sizeof(CSeqPair), tripleConCompare);
    if (pInfo == 0)
        return cs_nil;
    return pInfo->cs;
}
