// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4;
// indent-tabs-mode:nil -*-
/*------------------------------------------------------------------------------
Shared declarations for charset implementation split across translation units.
--------------------------------------------------------------------------------*/

#ifndef __UKENGINE_CHARSET_INTERNAL_H
#define __UKENGINE_CHARSET_INTERNAL_H

extern int LoVowel['z' - 'a' + 1];
extern int HiVowel['Z' - 'A' + 1];

extern const char *VIQREscapes[];
extern int VIQREscCount;

/**
 * @brief qsort/bsearch comparator for UKDWORD tables keyed by LOWORD.
 */
int wideCharCompare(const void *ele1, const void *ele2);

#endif
