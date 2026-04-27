#include <cassert>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <ukengine/engine/unikey.h>
#include <ukengine/engine/ukengine.h>
#include <ukengine/mapping/vnlexi.h>
#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/charset.h>

extern UkSharedMem *pShMem;
extern UkEngine MyKbEngine;
extern ConSeq lookupCSeq(VnLexiName c1, VnLexiName c2, VnLexiName c3);

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

    return 0;
}
