// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/**
 * @file engine_internal.h
 * @brief Shared macros and helpers for `engine_components` translation units.
 *
 * Not part of the public API; include only from ukengine `.cpp` fragments.
 */
#ifndef UKENGINE_ENGINE_COMPONENTS_ENGINE_INTERNAL_H
#define UKENGINE_ENGINE_COMPONENTS_ENGINE_INTERNAL_H

#include "vnlexi.h"

/** Carriage return (used for macro / word-boundary checks). */
#define ENTER_CHAR 13
#define IS_ODD(x) (x & 1)
#define IS_EVEN(x) (!(x & 1))

#define IS_STD_VN_LOWER(x)                                                   \
    ((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && \
     IS_ODD(x))
#define IS_STD_VN_UPPER(x)                                                    \
    ((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && \
     IS_EVEN(x))

inline VnLexiName changeCase(VnLexiName x)
{
    if (x == vnl_nonVnChar)
        return x;
    if (!(x & 0x01))
        return (VnLexiName)(x + 1);
    return (VnLexiName)(x - 1);
}

inline VnLexiName vnToLower(VnLexiName x)
{
    if (x == vnl_nonVnChar)
        return x;
    if (!(x & 0x01)) // even
        return (VnLexiName)(x + 1);
    return x;
}

#endif
