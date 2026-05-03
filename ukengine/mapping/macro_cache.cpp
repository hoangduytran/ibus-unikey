// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/**
 * @file macro_cache.cpp
 * @brief Binary sidecar cache for CMacroTable (see project_planning ukengine macro plan).
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <ukengine/mapping/macro_cache.h>
#include <ukengine/mapping/mactab.h>

namespace {

const char kMagic[8] = {'U', 'K', 'M', 'C', 'C', 'H', '0', '1'};
const uint16_t kFormatVersion = 1;

uint64_t fnv1a64File(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    uint64_t h = 14695981039346656037ULL;
    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            h ^= (uint64_t)buf[i];
            h *= 1099511628211ULL;
        }
    }
    fclose(f);
    return h;
}

bool readU16(FILE *f, uint16_t *o)
{
    unsigned char b[2];
    if (fread(b, 1, 2, f) != 2)
        return false;
    *o = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
    return true;
}

bool readU32(FILE *f, uint32_t *o)
{
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4)
        return false;
    *o = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return true;
}

bool readU64(FILE *f, uint64_t *o)
{
    unsigned char b[8];
    if (fread(b, 1, 8, f) != 8)
        return false;
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t)b[i] << (8 * i);
    *o = v;
    return true;
}

void writeU16(FILE *f, uint16_t v)
{
    unsigned char b[2] = {(unsigned char)(v & 0xFF), (unsigned char)((v >> 8) & 0xFF)};
    fwrite(b, 1, 2, f);
}

void writeU32(FILE *f, uint32_t v)
{
    unsigned char b[4] = {(unsigned char)(v & 0xFF), (unsigned char)((v >> 8) & 0xFF),
                          (unsigned char)((v >> 16) & 0xFF), (unsigned char)((v >> 24) & 0xFF)};
    fwrite(b, 1, 4, f);
}

void writeU64(FILE *f, uint64_t v)
{
    for (int i = 0; i < 8; i++) {
        unsigned char b = (unsigned char)((v >> (8 * i)) & 0xFF);
        fwrite(&b, 1, 1, f);
    }
}

} // namespace

std::string CacheManagement::sidecarPathFor(const char *macroTextPath)
{
    if (!macroTextPath)
        return std::string();
    return std::string(macroTextPath) + ".ukmcache";
}

bool CacheManagement::tryLoad(const char *macroTextPath, CMacroTable &table)
{
    const std::string cpath = sidecarPathFor(macroTextPath);
    FILE *cf = fopen(cpath.c_str(), "rb");
    if (!cf)
        return false;

    char mag[8];
    if (fread(mag, 1, 8, cf) != 8 || memcmp(mag, kMagic, 8) != 0) {
        fclose(cf);
        return false;
    }

    uint16_t ver = 0;
    uint16_t flags = 0;
    uint64_t fpFile = 0;
    uint32_t n = 0;
    if (!readU16(cf, &ver) || !readU16(cf, &flags) || !readU64(cf, &fpFile) || !readU32(cf, &n)) {
        fclose(cf);
        return false;
    }

    if (ver != kFormatVersion) {
        fclose(cf);
        return false;
    }

    const uint64_t actual = fnv1a64File(macroTextPath);
    if (actual != fpFile || actual == 0) {
        fclose(cf);
        return false;
    }

    std::vector<MacroEntry> loaded;
    loaded.reserve(n);

    for (uint32_t i = 0; i < n; i++) {
        uint32_t ku = 0, tu = 0;
        if (!readU32(cf, &ku) || !readU32(cf, &tu) || ku == 0 || tu == 0) {
            fclose(cf);
            return false;
        }
        const size_t kbytes = (size_t)ku * sizeof(StdVnChar);
        const size_t tbytes = (size_t)tu * sizeof(StdVnChar);
        std::vector<char> kbuf(kbytes);
        std::vector<char> tbuf(tbytes);
        if (fread(kbuf.data(), 1, kbytes, cf) != kbytes || fread(tbuf.data(), 1, tbytes, cf) != tbytes) {
            fclose(cf);
            return false;
        }

        MacroEntry e;
        e.key.resize(ku);
        e.text.resize(tu);
        memcpy(e.key.data(), kbuf.data(), kbytes);
        memcpy(e.text.data(), tbuf.data(), tbytes);
        loaded.push_back(std::move(e));
    }

    fclose(cf);

    table.m_entries = std::move(loaded);
    table.rebuildLookupMap();
    return true;
}

void CacheManagement::persist(const char *macroTextPath, const CMacroTable &table)
{
    const uint64_t fp = fnv1a64File(macroTextPath);
    if (fp == 0)
        return;

    const std::string sidecar = sidecarPathFor(macroTextPath);
    const std::string tmp = sidecar + ".tmp";
    const std::string finalPath = sidecar;

    FILE *cf = fopen(tmp.c_str(), "wb");
    if (!cf)
        return;

    fwrite(kMagic, 1, 8, cf);
    writeU16(cf, kFormatVersion);
    writeU16(cf, 0);
    writeU64(cf, fp);

    const uint32_t n = (uint32_t)table.getCount();
    writeU32(cf, n);

    for (int i = 0; i < (int)n; i++) {
        const StdVnChar *pk = table.getKey(i);
        const StdVnChar *pt = table.getText(i);
        if (!pk || !pt) {
            fclose(cf);
            remove(tmp.c_str());
            return;
        }

        size_t kunits = 0;
        while (pk[kunits] != 0)
            kunits++;
        kunits++; // trailing NUL
        size_t tunits = 0;
        while (pt[tunits] != 0)
            tunits++;
        tunits++;

        writeU32(cf, (uint32_t)kunits);
        writeU32(cf, (uint32_t)tunits);
        fwrite(pk, sizeof(StdVnChar), kunits, cf);
        fwrite(pt, sizeof(StdVnChar), tunits, cf);
    }

    fclose(cf);
    remove(finalPath.c_str());
    rename(tmp.c_str(), finalPath.c_str());
}
