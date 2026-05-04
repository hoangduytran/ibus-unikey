#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

#include <ukengine/engine/unikey.h>
#include <ukengine/engine/ukengine.h>
#include <ukengine/mapping/vnlexi.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/charset.h>
#include <ukengine/mapping/vnconv.h>

extern UkSharedMem *pShMem;
extern UkEngine MyKbEngine;
extern ConSeq lookupCSeq(VnLexiName c1, VnLexiName c2, VnLexiName c3);

namespace {

bool stdVnToUtf8Str(const StdVnChar *src, std::string &out)
{
    if (!src)
        return false;
    std::vector<char> buf;
    buf.resize(256);
    for (int attempt = 0; attempt < 24; attempt++) {
        int inLen = -1;
        int maxOut = (int)buf.size();
        int ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8, (UKBYTE *)src, (UKBYTE *)buf.data(),
                            &inLen, &maxOut);
        if (ret == 0) {
            while (maxOut > 0 && buf[(size_t)maxOut - 1] == '\0')
                maxOut--;
            out.assign(buf.data(), (size_t)maxOut);
            return true;
        }
        if (buf.size() > (size_t)64 * 1024 * 1024)
            return false;
        buf.resize(buf.size() * 2);
    }
    return false;
}

/** Create empty temp file; returns path or empty on failure. */
std::string tempMacroPath()
{
    char tmpl[] = "/tmp/ibus_ukengine_test_XXXXXX";
    const int fd = mkstemp(tmpl);
    if (fd < 0)
        return "";
    if (close(fd) != 0) {
        unlink(tmpl);
        return "";
    }
    unlink(tmpl);
    return std::string(tmpl);
}

void writeMacroFile(const std::string &path, const char *bodyAfterHeader)
{
    FILE *wf = fopen(path.c_str(), "w");
    assert(wf);
    fprintf(wf, "DO NOT DELETE THIS LINE*** version=1 ***\n%s", bodyAfterHeader);
    fclose(wf);
}

} // namespace

int main()
{
    UnikeySetup();
    assert(UnikeyAtWordBeginning() == true || UnikeyAtWordBeginning() == false);
    UnikeySetCapsState(0, 0);
    UnikeySetInputMethod(UkTelex);
    UnikeySetOutputCharset(CONV_CHARSET_XUTF8);

    // Verify consonant sequence lookup works correctly
    ConSeq csD = lookupCSeq(vnl_d, vnl_nonVnChar, vnl_nonVnChar);
    assert(csD == cs_d && "lookupCSeq(vnl_d) should return cs_d");

    UnikeyFilter('a');
    assert(UnikeyBufChars >= 0);

    UnikeyResetBuf();

    // Test dd -> đ (Telex method)
    UnikeyFilter('d');
    UnikeyFilter('d');

    // In UTF-8, đ is encoded as 0xC4 0x91 (2 bytes)
    // The engine should produce backspaces and output đ
    assert(UnikeyBufChars >= 2 && "dd should produce output");
    assert(UnikeyBuf[0] == 0xC4 && UnikeyBuf[1] == 0x91 && "dd should convert to đ (UTF-8: 0xC4 0x91)");

    UnikeyResetBuf();
    UnikeyCleanup();

    std::cout << "ukengine basic flow test passed\n";

    /* Regression: CMacroTable must accept more than the old 1024 entry cap. */
    CMacroTable mt;
    mt.init();
    for (int i = 0; i < 1025; i++)
    {
        char k[32];
        char v[32];
        std::snprintf(k, sizeof k, "k%04d", i);
        std::snprintf(v, sizeof v, "v%04d", i);
        assert(mt.addItem(k, v, CONV_CHARSET_UNIUTF8) >= 0);
    }
    assert(mt.getCount() == 1025);

    /* Long UTF-8 key/text must survive VNSTANDARD round-trip in CMacroTable. */
    {
        CMacroTable t;
        t.init();
        std::string longKey(3500, 'm');
        std::string longVal(800, 'n');
        assert(t.addItem(longKey.c_str(), longVal.c_str(), CONV_CHARSET_UNIUTF8) >= 0);
        assert(t.getCount() == 1);
        std::string gotKey, gotVal;
        assert(stdVnToUtf8Str(t.getKey(0), gotKey));
        assert(stdVnToUtf8Str(t.getText(0), gotVal));
        assert(gotKey == longKey);
        assert(gotVal == longVal);
    }

    /* addItem: duplicate key replaces row (last wins; same UTF-8 key bytes). */
    {
        CMacroTable t;
        t.init();
        assert(t.addItem("samekey", "first", CONV_CHARSET_UNIUTF8) >= 0);
        assert(t.addItem("samekey", "second", CONV_CHARSET_UNIUTF8) >= 0);
        assert(t.getCount() == 1);
        std::string got;
        assert(stdVnToUtf8Str(t.getText(0), got));
        assert(got == "second");
    }

    /* File load: ASCII fold last-wins (same as prefix fold for bytes < 128). */
    {
        const std::string path = tempMacroPath();
        assert(!path.empty());
        writeMacroFile(path,
                       "low:lose\n"
                       "LOW:win\n");
        CMacroTable t;
        t.init();
        assert(t.loadFromFile(path.c_str()) == 1);
        assert(t.getCount() == 1);
        std::string got;
        assert(stdVnToUtf8Str(t.getText(0), got));
        assert(got == "win");
        unlink(path.c_str());
        unlink((path + ".ukmcache").c_str());
    }

    /* Binary sidecar: second load uses cache; edited text file invalidates cache. */
    {
        const std::string path = tempMacroPath();
        assert(!path.empty());
        writeMacroFile(path, "q:cached_value\n");

        CMacroTable cold;
        cold.init();
        assert(cold.loadFromFile(path.c_str()) == 1);
        assert(cold.getCount() == 1);
        std::string v1;
        assert(stdVnToUtf8Str(cold.getText(0), v1));
        assert(v1 == "cached_value");

        FILE *cf = fopen((path + ".ukmcache").c_str(), "rb");
        assert(cf && "sidecar should exist after UTF-8 load");
        fclose(cf);

        CMacroTable warm;
        warm.init();
        assert(warm.loadFromFile(path.c_str()) == 1);
        std::string v2;
        assert(stdVnToUtf8Str(warm.getText(0), v2));
        assert(v2 == "cached_value");

        writeMacroFile(path, "q:fresh_value\n");

        CMacroTable afterEdit;
        afterEdit.init();
        assert(afterEdit.loadFromFile(path.c_str()) == 1);
        std::string v3;
        assert(stdVnToUtf8Str(afterEdit.getText(0), v3));
        assert(v3 == "fresh_value");

        unlink(path.c_str());
        unlink((path + ".ukmcache").c_str());
    }

    std::cout << "CMacroTable macro/cache checks passed\n";

    return 0;
}
