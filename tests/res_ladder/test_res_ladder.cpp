/* Host conformance test for the head-mounted resolution ladder math
 * (SOURCES/RES_CATALOG.H, Res_BuildHeadMountLadderFor).
 *
 * The Display menu and the `resolution` console verb build the headset ladder
 * from the runtime's live per-eye recommendation, so the rungs are per-device.
 * The pins below lock the historical Quest 3 / 3S table (the values the fixed
 * ladder used to carry), the Quest 2 / Pro table, and the Quest 1 case where
 * the 0.25x rung falls below the engine's minimum width and is dropped. The
 * invariants cover what the menu relies on: ascending height, multiple-of-8
 * width, width following the recommendation's aspect, and a native rung equal
 * to the recommendation.
 */

#include "RES_CATALOG.H"

#include <cstdio>
#include <cstring>

static int failures = 0;

struct Step {
    unsigned w, h;
    const char *tag;
};

static void expectLadder(const char *name, unsigned sw, unsigned sh,
                         const Step *want, int wantCount) {
    ResModeEntry got[8];
    const int n = Res_BuildHeadMountLadderFor(sw, sh, got, 8);
    if (n != wantCount) {
        std::printf("FAIL %s: got %d entries, want %d\n", name, n, wantCount);
        failures++;
        return;
    }
    for (int i = 0; i < n; i++) {
        if (got[i].w != want[i].w || got[i].h != want[i].h ||
            std::strcmp(got[i].tag, want[i].tag) != 0) {
            std::printf("FAIL %s[%d]: got %ux%u (%s), want %ux%u (%s)\n", name,
                        i, got[i].w, got[i].h, got[i].tag, want[i].w, want[i].h,
                        want[i].tag);
            failures++;
        }
    }
}

static void checkInvariants(const char *name, unsigned sw, unsigned sh) {
    ResModeEntry got[8];
    const int n = Res_BuildHeadMountLadderFor(sw, sh, got, 8);
    if (n <= 0) {
        std::printf("FAIL %s: no rungs for %ux%u\n", name, sw, sh);
        failures++;
        return;
    }
    for (int i = 0; i < n; i++) {
        if ((got[i].w & 7u) != 0 || got[i].w < RES_MIN_W ||
            got[i].w > RES_MAX_W || got[i].h < RES_MIN_H ||
            got[i].h > RES_MAX_H) {
            std::printf("FAIL %s[%d]: %ux%u outside the engine range\n", name,
                        i, got[i].w, got[i].h);
            failures++;
        }
        if (i > 0 && got[i].h <= got[i - 1].h) {
            std::printf("FAIL %s[%d]: heights not ascending (%u after %u)\n",
                        name, i, got[i].h, got[i - 1].h);
            failures++;
        }
        /* Width follows the recommendation's aspect, allowing the independent
           multiple-of-8 rounding of the width (<= 7 px). */
        const double want = (double)sw * (double)got[i].h / (double)sh;
        const double diff = (double)got[i].w - want;
        if (diff < -7.0 || diff > 1.0) {
            std::printf("FAIL %s[%d]: %ux%u does not follow %ux%u aspect "
                        "(want width %.1f)\n",
                        name, i, got[i].w, got[i].h, sw, sh, want);
            failures++;
        }
    }
    const ResModeEntry &native = got[n - 1];
    if (native.h != sh || native.w != (sw & ~7u)) {
        std::printf("FAIL %s: native rung %ux%u, want %ux%u\n", name, native.w,
                    native.h, sw & ~7u, sh);
        failures++;
    }
}

int main() {
    /* Quest 3 / 3S: the historical fixed table, unchanged. */
    static const Step quest3[] = {
        {416, 440, "0.25x"},
        {560, 587, "0.33x"},
        {840, 880, "0.5x"},
        {1120, 1173, "0.67x"},
        {1256, 1320, "0.75x"},
        {1680, 1760, "native"},
    };
    expectLadder("Quest 3 / 3S", 1680, 1760, quest3, 6);
    checkInvariants("Quest 3 / 3S", 1680, 1760);

    /* Quest 2 / Pro: same fractions of their 1440x1584 eye buffer. */
    static const Step quest2[] = {
        {360, 396, "0.25x"},
        {480, 528, "0.33x"},
        {720, 792, "0.5x"},
        {960, 1056, "0.67x"},
        {1080, 1188, "0.75x"},
        {1440, 1584, "native"},
    };
    expectLadder("Quest 2 / Pro", 1440, 1584, quest2, 6);
    checkInvariants("Quest 2 / Pro", 1440, 1584);

    /* Quest 1: 0.25x would be 304x336, below RES_MIN_W, so the ladder starts
       at 0.33x. */
    static const Step quest1[] = {
        {400, 448, "0.33x"},
        {608, 672, "0.5x"},
        {808, 896, "0.67x"},
        {912, 1008, "0.75x"},
        {1216, 1344, "native"},
    };
    expectLadder("Quest 1", 1216, 1344, quest1, 5);
    checkInvariants("Quest 1", 1216, 1344);

    /* The engine's smallest supported mode: only the native rung survives. */
    static const Step smallest[] = {{320, 200, "native"}};
    expectLadder("320x200", 320, 200, smallest, 1);
    checkInvariants("320x200", 320, 200);

    /* The cap is honoured (the menu's fixed slot budget). */
    {
        ResModeEntry got[8];
        const int n = Res_BuildHeadMountLadderFor(1680, 1760, got, 2);
        if (n != 2 || got[0].w != 416 || got[1].w != 560) {
            std::printf("FAIL cap: got %d entries (want 2: 416, 560)\n", n);
            failures++;
        }
    }

    if (failures != 0) {
        std::printf("%d ladder failure(s)\n", failures);
        return 1;
    }
    std::printf("head-mounted ladder OK\n");
    return 0;
}
