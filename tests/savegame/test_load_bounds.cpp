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

    /* Both stride positions yield an in-range patch count (zero buffer reads as 0) and
     * no scene hint is given — the heuristic cannot disambiguate, so it returns 0
     * (caller defaults to native; the LoadContexte retry handles the wrong-stride case). */
    {
        const S32 nb = 1;
        const S32 stride64 = 142 + 164;
        std::vector<U8> ctx(4000, 0);
        U8 *p = &ctx[0];
        U8 *after_nb = p + 200;
        U8 *end = p + ctx.size();
        S32 nb_patches = 3;
        memcpy(after_nb + (size_t)nb * (size_t)stride64, &nb_patches, sizeof(S32));
        S32 g = SaveLoadGuessObjectWireStride(nb, after_nb, end, stride64, -1);
        assert(g == 0);
    }

    /* Scene NbPatches hint picks the stride whose sniff matches it exclusively. */
    {
        const S32 nb = 1;
        const S32 stride64 = 142 + 164;
        std::vector<U8> ctx(4000, 0);
        U8 *after_nb = &ctx[200];
        U8 *end = &ctx[0] + ctx.size();
        S32 nb_at32 = 7;
        S32 nb_at64 = 5;
        memcpy(after_nb + (size_t)nb * 278u, &nb_at32, sizeof(S32));
        memcpy(after_nb + (size_t)nb * (size_t)stride64, &nb_at64, sizeof(S32));
        S32 g = SaveLoadGuessObjectWireStride(nb, after_nb, end, stride64, 5);
        assert(g == stride64);
        S32 g2 = SaveLoadGuessObjectWireStride(nb, after_nb, end, stride64, 7);
        assert(g2 == 278);
    }

    /* Both strides match the scene hint — heuristic returns 0 (ambiguous); the retry
     * in LoadContexte chooses by post-read validation. */
    {
        const S32 nb = 2;
        const S32 stride64 = 142 + 164;
        std::vector<U8> ctx(8000, 0xCD);
        U8 *after_nb = &ctx[100];
        U8 *end = &ctx[0] + ctx.size();
        S32 scene = 3;
        memcpy(after_nb + (size_t)nb * 278u, &scene, sizeof(S32));
        memcpy(after_nb + (size_t)nb * (size_t)stride64, &scene, sizeof(S32));
        S32 g = SaveLoadGuessObjectWireStride(nb, after_nb, end, stride64, scene);
        assert(g == 0);
        assert(SaveLoadSniffNbPatchesAtStride(nb, after_nb, end, 278u) == scene);
    }

    /* One stride sniffs an absurd value; the other reads cleanly — pick the clean one. */
    {
        const S32 nb = 1;
        const S32 stride64 = 142 + 164;
        std::vector<U8> ctx(5000, 0);
        U8 *after = &ctx[100];
        U8 *end = after + 4000;
        S32 scene = 34;
        memcpy(after + 278u, &scene, sizeof(S32));
        S32 huge = 25000000;
        memcpy(after + (size_t)stride64, &huge, sizeof(S32));
        S32 g = SaveLoadGuessObjectWireStride(nb, after, end, stride64, scene);
        assert(g == 278);
    }

    /* Both sniffs OOB / invalid; no raw word at either stride — return 0 (host native default). */
    {
        const S32 nb = 34;
        const S32 stride64 = 142 + 164;
        std::vector<U8> ctx(200, 0xAB);
        U8 *after = &ctx[10];
        U8 *end = after + 80; /* < nb * 278 + 4 — both sniffs fail */
        S32 g = SaveLoadGuessObjectWireStride(nb, after, end, stride64, 12);
        assert(g == 0);
    }

    return 0;
}
