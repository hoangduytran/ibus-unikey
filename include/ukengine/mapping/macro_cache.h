// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef UKENGINE_MACRO_CACHE_H
#define UKENGINE_MACRO_CACHE_H

class CMacroTable;

/**
 * @brief Precomputed binary cache next to the macro text file (same directory,
 *        `.ukmcache` suffix). FNV-1a 64 fingerprint of the text file must match.
 */
class MacroBinaryCache
{
public:
    /** @return true if cache was loaded into @a table. */
    static bool tryLoadForTextFile(const char *macroTextPath, CMacroTable &table);

    /** Write sidecar after a successful text file state is known. */
    static void persistForTextFile(const char *macroTextPath, const CMacroTable &table);
};

#endif
