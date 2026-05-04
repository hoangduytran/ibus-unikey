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

extern VnLexiName AZLexiUpper[];
extern VnLexiName AZLexiLower[];

//------------------------------------------------
/**
 * @brief Initialize static engine lookup tables and shared vowel data.
 *
 * Populates the sorted sequence tables and the vowel lookup map used by
 * the UniKey engine across instances.
 */
void engineClassInit()
{
    int i, j;

    for (i = 0; i < VSeqCount; i++)
    {
        for (j = 0; j < 3; j++)
            SortedVSeqList[i].v[j] = VSeqList[i].v[j];
        SortedVSeqList[i].vs = (VowelSeq)i;
    }

    for (i = 0; i < CSeqCount; i++)
    {
        for (j = 0; j < 3; j++)
            SortedCSeqList[i].c[j] = CSeqList[i].c[j];
        SortedCSeqList[i].cs = (ConSeq)i;
    }

    qsort(SortedVSeqList, VSeqCount, sizeof(VSeqPair), tripleVowelCompare);
    qsort(SortedCSeqList, CSeqCount, sizeof(CSeqPair), tripleConCompare);
    qsort(VCPairList, VCPairCount, sizeof(VCPair), VCPairCompare);

    for (i = 0; i < vnl_lastChar; i++)
        IsVnVowel[i] = true;

    unsigned char ch;
    for (ch = 'a'; ch <= 'z'; ch++)
    {
        if (ch != 'a' && ch != 'e' && ch != 'i' &&
            ch != 'o' && ch != 'u' && ch != 'y')
        {
            IsVnVowel[AZLexiLower[ch - 'a']] = false;
            IsVnVowel[AZLexiUpper[ch - 'a']] = false;
        }
    }
    IsVnVowel[vnl_dd] = false;
    IsVnVowel[vnl_DD] = false;
}
