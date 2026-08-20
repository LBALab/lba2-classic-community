/* Host test for LIB386/SYSTEM/RANDOM.CPP, the engine's one pseudo-random stream.
 *
 * The stream is what decides whether a recorded session can replay on another
 * platform, and libc rand() is a different function per platform: glibc returns
 * 1804289383 first from seed 0 and tops out at 2147483647, while the Windows
 * UCRT returns 38 and tops out at 32767. Different order, and a different range,
 * so `rand() % n` is differently distributed as well as differently ordered.
 * Drawing from libc is therefore not an option, and this test is what holds the
 * replacement to the one sequence.
 *
 * Two oracles, and they check different things:
 *
 *   1. The committed vector below runs on every platform and is what actually
 *      pins portability. If Linux and Windows and macOS all reproduce it, they
 *      all draw the same numbers, whatever their libc does.
 *
 *   2. The live comparison against the system rand() only builds on glibc, and
 *      checks the other half: that the committed vector really is glibc's
 *      sequence rather than some sequence we agreed on among ourselves. That
 *      matters because every committed baseline, corpus save and reference
 *      recording in this repository was made on Linux against glibc. Matching
 *      it is what lets those keep their meaning.
 *
 * Neither oracle claims the retail sequence. Watcom's rand() is a third
 * implementation again, which no port reproduces. See docs/BIT_EXACTNESS.md.
 */

#include <cstdio>
#include <cstdlib>

#include "test_harness.h"

#include <SYSTEM/RANDOM.H>

/* For the Rnd(n) macro itself. The aggregator is headers only here: nothing below
   calls into it except through Rnd, which resolves to Rnd_Next. */
#include <SYSTEM.H>

/* FNV-1a over the little-endian bytes of each draw. A digest rather than more
 * literals because the failure this guards against (a wrong ring index, a wrong
 * discard count) shows up across the whole run, not in the first few values. */
static U32 digest_draws(int n) {
    U32 h = 2166136261u;
    int i, b;
    for (i = 0; i < n; i++) {
        U32 v = (U32)Rnd_Next();
        for (b = 0; b < 4; b++) {
            h ^= (v >> (b * 8)) & 0xFFu;
            h *= 16777619u;
        }
    }
    return h;
}

struct Vector {
    U32 seed;
    S32 first[8];
    U32 digest; /* of the 100000 draws after those eight */
};

/* Generated from glibc and checked against it by the live comparison below.
 *
 * If you are here because you changed the generator and this failed: that is the
 * test working. Regenerating these numbers is not the whole job. A recording
 * declares the stream it was made against by name, so Rnd_StreamName() in
 * RANDOM.CPP has to change in the same commit, or recordings made against the old
 * sequence will claim to be compatible with the new one and diverge silently on
 * replay instead of saying why. */
static const Vector VECTORS[] = {
    {0u,
     {1804289383, 846930886, 1681692777, 1714636915, 1957747793, 424238335, 719885386, 1649760492},
     0xE838AE09U},
    {1u,
     {1804289383, 846930886, 1681692777, 1714636915, 1957747793, 424238335, 719885386, 1649760492},
     0xE838AE09U},
    {2u,
     {1505335290, 1738766719, 190686788, 260874575, 747983061, 906156498, 1502820864, 142559277},
     0x0EA70F90U},
    {12345u,
     {383100999, 858300821, 357768173, 455528251, 133005921, 116285904, 591987137, 102557902},
     0xCB32B9F0U},
    {0x7FFFFFFFu,
     {1065668062, 2142264300, 1066566375, 1064012770, 2141034222, 1065509725, 2135810236, 2139491828},
     0xC7FE1BD9U},
    {0x80000000u,
     {1336741213, 1210407648, 1447044896, 337392383, 82502902, 538660432, 1313908778, 370221063},
     0xF81867A8U},
    {0xFFFFFFFFu,
     {254925627, 1205188300, 366127624, 1401405153, 76053476, 1604170158, 1302235366, 362229243},
     0x29A2EE33U},
};

#define NVECTORS ((int)(sizeof VECTORS / sizeof VECTORS[0]))

/* The portability oracle: same numbers on every platform. */
static void test_committed_vector(void) {
    int v, i;
    for (v = 0; v < NVECTORS; v++) {
        Rnd_Seed(VECTORS[v].seed);
        for (i = 0; i < 8; i++)
            ASSERT_EQ_INT(VECTORS[v].first[i], Rnd_Next());
        ASSERT_EQ_UINT(VECTORS[v].digest, digest_draws(100000));
    }
}

/* Seeds 0 and 1 are the same stream. Not relied on by anything, but it is the
 * documented behaviour and it makes the seed-to-stream mapping total. */
static void test_zero_seed_is_one(void) {
    S32 fromZero[16], i;
    Rnd_Seed(0);
    for (i = 0; i < 16; i++)
        fromZero[i] = Rnd_Next();
    Rnd_Seed(1);
    for (i = 0; i < 16; i++)
        ASSERT_EQ_INT(fromZero[i], Rnd_Next());
}

/* Every draw is non-negative, which is what `Rnd(n) = Rnd_Next() % n` needs to
 * stay in range for a positive n. A negative draw would come back as a negative
 * index, and the upper bound needs no test: the shift that produces the value
 * leaves 31 bits, so S32's own maximum is the bound. */
static void test_range(void) {
    S32 worst = 0;
    int i;
    Rnd_Seed(20250820u);
    for (i = 0; i < 200000; i++) {
        S32 v = Rnd_Next();
        if (v < worst)
            worst = v;
    }
    ASSERT_EQ_INT(0, worst);
}

/* With no Rnd_Seed at all the stream reads as if seeded with 1, which is what a
 * C program gets from rand() before any srand. Nothing seeds before the first
 * ChangeCube, so this is the state the boot path actually draws from, including
 * the menu plasma. Checked against the committed vector rather than against the
 * system rand() so that it runs everywhere: the boot state is not a property to
 * verify only on the one host that happens to have glibc. */
static void test_unseeded_default(void) {
    int i;
    /* Deliberately no Rnd_Seed. Runs first, before any other test touches the
     * module, which is what makes it a test of the initial state. */
    ASSERT_EQ_UINT(1u, VECTORS[1].seed); /* pins the index this reads from */
    for (i = 0; i < 8; i++)
        ASSERT_EQ_INT(VECTORS[1].first[i], Rnd_Next());
}

/* `Rnd(n)` is what the engine actually calls, and the whole of the portability
 * fix is which generator that macro resolves to. Everything above tests
 * Rnd_Next; this tests the wiring, which is the line that would silently undo
 * the fix if it were changed back. A build whose Rnd went to libc rand() would
 * pass every other test in this file and still play a different game on Windows.
 *
 * 1804289383 is the first draw from seed 1, so the modulus of it is what the
 * macro has to produce. */
static void test_rnd_macro_is_wired_to_the_engine_generator(void) {
    Rnd_Seed(1);
    ASSERT_EQ_INT(1804289383 % 1000, Rnd(1000));
    Rnd_Seed(1);
    ASSERT_EQ_INT(1804289383 % 7, Rnd(7));
    Rnd_Seed(1);
    ASSERT_EQ_INT(0, Rnd(1)); /* modulus 1 is always 0, whatever the draw */

    /* And it stays inside the range its callers index arrays with. */
    {
        int i;
        Rnd_Seed(99);
        for (i = 0; i < 20000; i++) {
            S32 v = Rnd(37);
            if (v < 0 || v >= 37) {
                ASSERT_TRUE(v >= 0 && v < 37);
                return;
            }
        }
        ASSERT_TRUE(1);
    }
}

#ifdef __GLIBC__
/* The other oracle: the committed vector is glibc's own sequence.
 *
 * Only meaningful where the system rand() is glibc's, so it is compiled out
 * everywhere else rather than skipped at runtime. A skipped check exits 0 and
 * reads exactly like a passing one, and this is the check that gives the
 * committed vector its authority. */
static void test_matches_system_glibc(void) {
    static const unsigned seeds[] = {0u, 1u, 2u, 3u,
                                     42u, 12345u, 65535u, 1000000u,
                                     0x7FFFFFFFu, 0x80000000u, 0xDEADBEEFu, 0xFFFFFFFFu};
    int k, i;
    long mismatches = 0;

    for (k = 0; k < (int)(sizeof seeds / sizeof seeds[0]); k++) {
        srand(seeds[k]);
        Rnd_Seed(seeds[k]);
        for (i = 0; i < 50000; i++) {
            int sys = rand();
            S32 ours = Rnd_Next();
            if (sys != ours) {
                if (mismatches == 0)
                    std::fprintf(stderr,
                                 "  first divergence: seed %u draw %d, glibc %d, ours %d\n",
                                 seeds[k], i, sys, (int)ours);
                mismatches++;
            }
        }
    }
    ASSERT_EQ_INT(0, (int)mismatches);
}

#endif

int main(void) {
    /* First, while the module is still untouched. */
    RUN_TEST(test_unseeded_default);
    RUN_TEST(test_committed_vector);
    RUN_TEST(test_zero_seed_is_one);
    RUN_TEST(test_range);
    RUN_TEST(test_rnd_macro_is_wired_to_the_engine_generator);
#ifdef __GLIBC__
    RUN_TEST(test_matches_system_glibc);
#else
    std::printf("# system rand() comparison not built: not a glibc host\n");
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
