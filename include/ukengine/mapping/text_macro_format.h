// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef UKENGINE_TEXT_MACRO_FORMAT_H
#define UKENGINE_TEXT_MACRO_FORMAT_H

#include <ukengine/mapping/macro_format.h>

/**
 * @brief UniKey legacy UTF-8 / VIQR text format: version header, `key:text` lines, last-wins fold for ASCII.
 *
 * Does not read or write the binary `.ukmcache` sidecar; CacheManagement handles that.
 */
class TextMacroFormat : public MacroFormat
{
public:
    /** UTF-8 macro file header `version=` value written by export. */
    static const int kUtf8Version = 1;

    MacroFormatId id() const override;

    int importFromPath(const char *path, CMacroTable &table, int *outSourceVersion = nullptr) override;

    int exportToPath(const char *path, CMacroTable &table) override;
};

#endif
