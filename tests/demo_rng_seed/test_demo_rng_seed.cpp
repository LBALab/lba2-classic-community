/* Host-only test for SOURCES/DEMO_SEED.{H,CPP} (issue #176).
 *
 * Pins the cross-platform demo determinism contract: while DemoSlide is set,
 * ChangeCube must seed srand from the cube number, NOT from TimerRefHR. The
 * earlier "zero TimerRefHR in BeginDemoSlide()" mechanism (PR #249) was
 * structurally insufficient -- ManageTime() banks (SDL_GetTicks() - LastTime)
 * back into TimerRefHR before the first ChangeCube on the SHIFT+D / idle-
 * attract path, so the seed varies by host speed. macOS reported the bat-vs-
 * Twinsen Desert Island scene still diverging after #249 landed -- this test
 * pins the new contract so that can't regress.
 *
 * Pure logic, no engine dependencies.
 */

#include <cassert>
#include <cstdio>

#include "DEMO_SEED.H"

int main() {
    size_t fails = 0;

    /* (1) Retail behavior: while NOT in demo mode, the seed is TimerRefHR
     * regardless of cube. Byte-identical to the pre-fix engine -- ordinary
     * gameplay must be unaffected. */
    {
        const U32 timer_values[] = {0u, 1u, 42u, 12345u, 0x7FFFFFFFu, 0xFFFFFFFFu};
        const S32 cubes[] = {0, 1, 193, 218, 999};
        for (size_t i = 0; i < sizeof(timer_values) / sizeof(timer_values[0]); i++) {
            for (size_t j = 0; j < sizeof(cubes) / sizeof(cubes[0]); j++) {
                U32 got = Demo_RngSeed(FALSE, timer_values[i], cubes[j]);
                if (got != timer_values[i]) {
                    std::fprintf(stderr,
                                 "FAIL: retail seed: TimerRefHR=%u cube=%d -> %u (want %u)\n",
                                 timer_values[i], cubes[j], got, timer_values[i]);
                    fails++;
                }
            }
        }
    }

    /* (2) Demo mode: the seed is the cube number regardless of TimerRefHR.
     * This is the bug fix -- the host's wall-clock between BeginDemoSlide()
     * and ChangeCube cannot influence the demo's RNG branch. */
    {
        const U32 timer_values[] = {0u, 1u, 17u, 42u, 12345u, 0x7FFFFFFFu, 0xFFFFFFFFu};
        const S32 cubes[] = {0, 1, 193, 196, 200, 208, 218};
        for (size_t j = 0; j < sizeof(cubes) / sizeof(cubes[0]); j++) {
            U32 want = (U32)cubes[j];
            for (size_t i = 0; i < sizeof(timer_values) / sizeof(timer_values[0]); i++) {
                U32 got = Demo_RngSeed(TRUE, timer_values[i], cubes[j]);
                if (got != want) {
                    std::fprintf(stderr,
                                 "FAIL: demo seed: TimerRefHR=%u cube=%d -> %u (want %u)\n",
                                 timer_values[i], cubes[j], got, want);
                    fails++;
                }
            }
        }
    }

    /* (3) Cross-machine reproduction property: for any pair of TimerRefHR
     * values a host might observe between BeginDemoSlide() and ChangeCube
     * (Linux-fast ~5 ms, macOS-slow ~50 ms, anything in between), the demo
     * seed must be identical. This is the property issue #176 actually asks
     * the fix to guarantee. */
    {
        const U32 fast_host_timer = 5u;     /* representative Linux delta */
        const U32 slow_host_timer = 50u;    /* representative macOS delta */
        const S32 desert_island_demo = 200; /* "bat scene" class in #176 */
        U32 a = Demo_RngSeed(TRUE, fast_host_timer, desert_island_demo);
        U32 b = Demo_RngSeed(TRUE, slow_host_timer, desert_island_demo);
        if (a != b) {
            std::fprintf(stderr,
                         "FAIL: demo seed must match across hosts: fast=%u slow=%u\n",
                         a, b);
            fails++;
        }
    }

    /* (4) Distinct cubes give distinct seeds within a single demo run, so each
     * cube's RNG-driven wanderers don't all share one stream. Not strictly
     * required for the bug fix, but documents the chosen seeding policy. */
    {
        U32 a = Demo_RngSeed(TRUE, 0u, 193);
        U32 b = Demo_RngSeed(TRUE, 0u, 196);
        if (a == b) {
            std::fprintf(stderr,
                         "FAIL: distinct cubes should yield distinct demo seeds (both=%u)\n",
                         a);
            fails++;
        }
    }

    assert(fails == 0);
    std::printf("test_demo_rng_seed: ok\n");
    return 0;
}
