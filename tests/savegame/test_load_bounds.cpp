/* Host-only tests for SOURCES/SAVEGAME_LOAD_BOUNDS.CPP (issue #62 helpers). */

#include <cassert>
#include <cstring>
#include <vector>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/LZ.H>

#include "SAVEGAME_LOAD_BOUNDS.H"

int main() {
    /* Screen-sized scratch buffer */
    const U32 cap = SaveLoadScreenBufferBytes();
    std::vector<U8> buf(cap, 0);
    U8 *base = &buf[0];

    /* Valid minimal staging: compressed starts at base+0, sizefile small, tail fits */
    {
        S32 sizefile = 10;
        U8 *cmp = base;
        U32 compressed = 4;
        memset(cmp + (U32)sizefile + (U32)RECOVER_AREA, 0xAB, compressed);
        S32 ok = SaveLoadValidateCompressedStaging(base, cap, cmp, compressed, sizefile);
        assert(ok == TRUE);
    }

    /* Negative sizefile rejected */
    assert(SaveLoadValidateCompressedStaging(base, cap, base, 4, -1) == FALSE);

    /* Oversize decompressed vs buffer */
    {
        S32 sizefile = (S32)(cap + 1u);
        assert(SaveLoadValidateCompressedStaging(base, cap, base, 0, sizefile) == FALSE);
    }

    /* The per-object stride-sniff tests that used to live here were removed with
     * SaveLoadGuessObjectWireStride: the reader is now canonical-first (32-bit
     * wire, native fallback on validation failure), so there is no first-guess
     * heuristic to unit-test. The wire layout is locked by tests/save_wire/ and
     * the SAVEGAME_WIRE.H size asserts; the read path is exercised by the corpus
     * harness (tests/savegame/corpus). */

    return 0;
}
