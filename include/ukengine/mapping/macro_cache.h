// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef UKENGINE_MACRO_CACHE_H
#define UKENGINE_MACRO_CACHE_H

#include <string>

class CMacroTable;

/**
 * @brief Binary sidecar cache next to the canonical macro text file.
 *
 * Path rule: same directory as the text file, suffix `.ukmcache`.
 * Fingerprint: FNV-1a 64 over raw bytes of the macro text file (must match for load).
 *
 * Hashing and binary records exist only here—not in TextMacroFormat export output.
 */
class CacheManagement
{
public:
    /** @return Sidecar path `{macroTextPath}.ukmcache`. */
    static std::string sidecarPathFor(const char *macroTextPath);

    /** @return true if valid cache was loaded into @a table. */
    static bool tryLoad(const char *macroTextPath, CMacroTable &table);

    /** Write sidecar after a successful text file state is known. */
    static void persist(const char *macroTextPath, const CMacroTable &table);
};

#endif
