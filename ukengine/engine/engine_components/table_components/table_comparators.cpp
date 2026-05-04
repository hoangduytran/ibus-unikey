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
#include "engine_components/engine_tables_shared.h"

//------------------------------------------------
/**
 * @brief Compare two vowel sequence triples for sorting.
 *
 * @param p1 pointer to first VSeqPair object
 * @param p2 pointer to second VSeqPair object
 * @return -1 if first < second, 1 if first > second, 0 if equal
 */
int tripleVowelCompare(const void *p1, const void *p2)
{
    VSeqPair *t1 = (VSeqPair *)p1;
    VSeqPair *t2 = (VSeqPair *)p2;

    for (int i = 0; i < 3; i++)
    {
        if (t1->v[i] < t2->v[i])
            return -1;
        if (t1->v[i] > t2->v[i])
            return 1;
    }
    return 0;
}

//------------------------------------------------
/**
 * @brief Compare two consonant sequence triples for sorting.
 *
 * @param p1 pointer to first CSeqPair object
 * @param p2 pointer to second CSeqPair object
 * @return -1 if first < second, 1 if first > second, 0 if equal
 */
int tripleConCompare(const void *p1, const void *p2)
{
    CSeqPair *t1 = (CSeqPair *)p1;
    CSeqPair *t2 = (CSeqPair *)p2;

    for (int i = 0; i < 3; i++)
    {
        if (t1->c[i] < t2->c[i])
            return -1;
        if (t1->c[i] > t2->c[i])
            return 1;
    }
    return 0;
}

//------------------------------------------------
/**
 * @brief Compare vowel-consonant pairs for binary search ordering.
 *
 * @param p1 pointer to first VCPair object
 * @param p2 pointer to second VCPair object
 * @return -1 if first < second, 1 if first > second, 0 if equal
 */
int VCPairCompare(const void *p1, const void *p2)
{
    VCPair *t1 = (VCPair *)p1;
    VCPair *t2 = (VCPair *)p2;

    if (t1->v < t2->v)
        return -1;
    if (t1->v > t2->v)
        return 1;

    if (t1->c < t2->c)
        return -1;
    if (t1->c > t2->c)
        return 1;
    return 0;
}
