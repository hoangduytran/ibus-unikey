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

//----------------------------------------------------------
/**
 * @brief Validate a consonant-vowel pairing.
 *
 * @param c consonant sequence to validate
 * @param v vowel sequence to validate
 * @return true when the consonant and vowel form a valid Vietnamese pair
 */
bool isValidCV(ConSeq c, VowelSeq v)
{
    if (c == cs_nil || v == vs_nil)
        return true;

    VowelSeqInfo &vInfo = VSeqList[v];

    if ((c == cs_gi && vInfo.v[0] == vnl_i) ||
        (c == cs_qu && vInfo.v[0] == vnl_u))
        return false; // gi doesn't go with i, qu doesn't go with u

    if (c == cs_k)
    {
        // k can only go with the following vowel sequences
        static VowelSeq kVseq[] = {vs_e, vs_i, vs_y, vs_er, vs_eo, vs_eu,
                                   vs_eru, vs_ia, vs_ie, vs_ier, vs_ieu, vs_ieru, vs_nil};
        int i;
        for (i = 0; kVseq[i] != vs_nil && kVseq[i] != v; i++)
            ;
        return (kVseq[i] != vs_nil);
    }

    // More checks
    return true;
}

//----------------------------------------------------------
/**
 * @brief Validate a vowel-consonant pairing.
 *
 * @param v vowel sequence to validate
 * @param c consonant sequence to validate
 * @return true when the vowel and consonant form a valid ending sequence
 */
bool isValidVC(VowelSeq v, ConSeq c)
{
    if (v == vs_nil || c == cs_nil)
        return true;

    VowelSeqInfo &vInfo = VSeqList[v];
    if (!vInfo.conSuffix)
        return false;

    ConSeqInfo &cInfo = CSeqList[c];
    if (!cInfo.suffix)
        return false;

    VCPair p;
    p.v = v;
    p.c = c;
    if (bsearch(&p, VCPairList, VCPairCount, sizeof(VCPair), VCPairCompare))
        return true;

    return false;
}

//----------------------------------------------------------
/**
 * @brief Validate a consonant-vowel-consonant sequence.
 *
 * @param c1 leading consonant sequence
 * @param v vowel sequence
 * @param c2 trailing consonant sequence
 * @return true when the full CVC sequence is valid in Vietnamese spelling
 */
bool isValidCVC(ConSeq c1, VowelSeq v, ConSeq c2)
{
    if (v == vs_nil)
        return (c1 == cs_nil || c2 != cs_nil);

    if (c1 == cs_nil)
        return isValidVC(v, c2);

    if (c2 == cs_nil)
        return isValidCV(c1, v);

    bool okCV = isValidCV(c1, v);
    bool okVC = isValidVC(v, c2);

    if (okCV && okVC)
        return true;

    if (!okVC)
    {
        // check some exceptions: vc fails but cvc passes

        // quyn, quynh
        if (c1 == cs_qu && v == vs_y && (c2 == cs_n || c2 == cs_nh))
            return true;

        // gieng, gie^ng
        if (c1 == cs_gi && (v == vs_e || v == vs_er) && (c2 == cs_n || c2 == cs_ng))
            return true;
    }
    return false;
}
