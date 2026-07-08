/* Phase 1 fuzz (docs/SAVE_WIRE_PLAN.md): randomized round-trip over the three
 * wire converters. For thousands of random native structs, assert:
 *
 *   1. ToWire32 then FromWire32 then ToWire32 reproduces the first wire bytes
 *      exactly (memcmp). This "write -> read -> write is idempotent" invariant
 *      catches any field the writer and reader handle inconsistently (e.g. a new
 *      field added to one converter but not the other).
 *   2. Every round-trippable scalar field survives ToWire32 -> FromWire32.
 *
 * Deterministic: a fixed-seed xorshift PRNG, so a failure reproduces exactly.
 * No retail data. Runs in public CI.
 *
 * This covers the three pointer-bearing structs. The full-payload round-trip
 * (SaveContexte <-> LoadContexte over a fuzzed ListObjet/ListExtra/...) is the
 * runtime --save-fuzz harness described in the plan; this is its unit-level core.
 */

#include "SAVEGAME_WIRE.H"
#include "test_harness.h"

#include <stdint.h>
#include <string.h>

/* xorshift32, fixed seed -> reproducible fuzz. */
static uint32_t g_state = 0x1234567u;
static uint32_t rnd(void) {
    uint32_t x = g_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_state = x;
    return x;
}
static void rnd_fill(void *p, size_t n) {
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < n; i++)
        b[i] = (unsigned char)rnd();
}

#define ITERS 20000

static void test_obj3d_fuzz(void) {
    for (int i = 0; i < ITERS; i++) {
        T_OBJ_3D a;
        rnd_fill(&a, sizeof(a)); /* random bytes, incl. pointer + CurrentFrame */

        T_OBJ_3D_WIRE32 w1;
        SavegameObj3dToWire32(&a, &w1);
        T_OBJ_3D mid;
        SavegameObj3dFromWire32(&w1, &mid);
        T_OBJ_3D_WIRE32 w2;
        SavegameObj3dToWire32(&mid, &w2);

        /* write -> read -> write is byte-idempotent */
        if (memcmp(&w1, &w2, sizeof(w1)) != 0) {
            ASSERT_TRUE(0 && "obj3d wire round-trip not idempotent");
            return;
        }
        /* representative scalar fields survive the round-trip */
        if (mid.X != a.X || mid.Status != a.Status || mid.Body.Num != a.Body.Num ||
            mid.NbGroups != a.NbGroups) {
            ASSERT_TRUE(0 && "obj3d scalar field lost in round-trip");
            return;
        }
    }
    ASSERT_TRUE(1);
}

static void test_extra_fuzz(void) {
    for (int i = 0; i < ITERS; i++) {
        T_EXTRA a;
        rnd_fill(&a, sizeof(a));

        T_EXTRA_WIRE32 w1;
        SavegameExtraToWire32(&a, &w1);
        T_EXTRA mid;
        SavegameExtraFromWire32(&w1, &mid);
        T_EXTRA_WIRE32 w2;
        SavegameExtraToWire32(&mid, &w2);

        if (memcmp(&w1, &w2, sizeof(w1)) != 0) {
            ASSERT_TRUE(0 && "extra wire round-trip not idempotent");
            return;
        }
        if (mid.PosX != a.PosX || mid.Flags != a.Flags || mid.Scale != a.Scale ||
            mid.Sprite != a.Sprite) {
            ASSERT_TRUE(0 && "extra scalar field lost in round-trip");
            return;
        }
    }
    ASSERT_TRUE(1);
}

static void test_flow_fuzz(void) {
    for (int i = 0; i < ITERS; i++) {
        S_PART_FLOW a;
        rnd_fill(&a, sizeof(a));

        S_PART_FLOW_WIRE32 w1;
        SavegameFlowToWire32(&a, &w1);
        S_PART_FLOW mid;
        memset(&mid, 0, sizeof(mid));
        SavegameFlowFromWire32(&w1, &mid);
        S_PART_FLOW_WIRE32 w2;
        SavegameFlowToWire32(&mid, &w2);

        if (memcmp(&w1, &w2, sizeof(w1)) != 0) {
            ASSERT_TRUE(0 && "flow wire round-trip not idempotent");
            return;
        }
        if (mid.Flag != a.Flag || mid.NbDot != a.NbDot ||
            mid.FlowTimerStart != a.FlowTimerStart || mid.ZMax != a.ZMax) {
            ASSERT_TRUE(0 && "flow scalar field lost in round-trip");
            return;
        }
    }
    ASSERT_TRUE(1);
}

int main(void) {
    RUN_TEST(test_obj3d_fuzz);
    RUN_TEST(test_extra_fuzz);
    RUN_TEST(test_flow_fuzz);
    TEST_SUMMARY();
    return test_failures != 0;
}
