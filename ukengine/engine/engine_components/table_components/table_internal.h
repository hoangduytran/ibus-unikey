// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/**
 * @file table_internal.h
 * @brief qsort/bsearch comparators shared across table_components translation units.
 */
#ifndef UKENGINE_TABLE_COMPONENTS_TABLE_INTERNAL_H
#define UKENGINE_TABLE_COMPONENTS_TABLE_INTERNAL_H

int tripleVowelCompare(const void *p1, const void *p2);
int tripleConCompare(const void *p1, const void *p2);
int VCPairCompare(const void *p1, const void *p2);

#endif
