// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef UKENGINE_MACRO_FORMAT_H
#define UKENGINE_MACRO_FORMAT_H

class CMacroTable;

/**
 * Identifiers for pluggable macro file codecs (transport only; see TextMacroFormat).
 */
enum class MacroFormatId {
    TextUniKey,
    JsonMacros,
    YamlMacros,
    PlistMacTextReplacement,
    CsvMacros,
    TsvMacros,
};

/**
 * @brief Abstract macro import/export by file path.
 *
 * Human-readable export must not embed binary digests or hashes; those belong to
 * CacheManagement sidecar files only.
 */
class MacroFormat
{
public:
    virtual ~MacroFormat() {}

    virtual MacroFormatId id() const = 0;

    /**
     * Replace macro rows from a file. Caller must reset @a table first if a full reload is intended.
     * @param outSourceVersion if non-null, receives detected text header version (0 = legacy / no UTF-8 header).
     * @return 1 on success, 0 on failure (see CMacroTable error state).
     */
    virtual int importFromPath(const char *path, CMacroTable &table, int *outSourceVersion = nullptr) = 0;

    /**
     * Write human-readable macro text only (no sidecar).
     * @return 1 on success, 0 on failure.
     */
    virtual int exportToPath(const char *path, CMacroTable &table) = 0;
};

#endif
