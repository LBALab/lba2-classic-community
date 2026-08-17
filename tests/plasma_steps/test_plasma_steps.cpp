// Do_Plasma's evolution, pinned so it runs in CI on every platform.
//
// tests/test_plasma.cpp already covers this function, and covers it better: it
// compares against the original PLASMA.ASM across fixed, edge and random cases.
// It needs an assembler and objcopy to build, so it sits behind
// LBA2_BUILD_ASM_EQUIV_TESTS, which every workflow turns OFF. The algorithm has
// therefore never run in CI. This is the cheap half that can: no assembler, no
// engine, no retail data, one committed digest.
//
// It exists because the menu goldens stopped covering the plasma. Those goldens
// exclude the strip, since its starting state comes from Rnd() and libc rand()
// differs per platform (#530), and excluding it left the evolution with nothing
// watching it. The starting state is what is unportable, not the arithmetic, so
// this seeds the state itself and pins the arithmetic alone. That split is the
// point: it holds on every platform precisely because it does not touch rand().

#include "test_harness.h"

#include <PLASMA.H>

#include <string.h>

// The interleave and control-point count InitPlasmaMenu asks for (ANIMTEX.CPP:
// texdest[2] = 4 control points, and a width/nbactivepoints that resolves to
// this interleave). Pinning the shape the menu actually uses, not an arbitrary one.
#define INTERLEAVE 4
#define NB_ACTIVE_POINTS 4

#define NB_PTS_INTER (1u << INTERLEAVE)
#define WIDTH (NB_ACTIVE_POINTS * NB_PTS_INTER)
#define NB_POINTS (NB_ACTIVE_POINTS * NB_ACTIVE_POINTS)
#define NB_COLORS 12
#define TEX_SIZE (256u * WIDTH)

extern "C" U32 Nb_Pts_Inter;
extern "C" U32 Nb_Pts_Control;

// A fixed starting state, chosen to look like one InitPlasmaMenu would produce
// without asking rand() for it: vertices spread across the colour range, speeds
// alternating in sign, accumulators at the 500 the engine seeds them with.
static void seed_state(S16 *virgule, S16 *speed, S32 *acc, U8 *colors) {
    memset(virgule, 0, NB_POINTS * NB_PTS_INTER * sizeof(S16));
    for (U32 i = 0; i < NB_POINTS; ++i) {
        virgule[i * NB_PTS_INTER] = (S16)(((i * 977u) % ((NB_COLORS - 1) * 256)));
        S32 va = (S32)(2 * 512 + ((i * 313u) % (2 * 512)));
        speed[i] = (S16)((i & 1u) ? -va : va);
        acc[i] = 500;
    }
    for (U32 i = 0; i < NB_COLORS; ++i) {
        colors[i] = (U8)(i + 12 * 16);
    }
}

static U32 fnv1a(const U8 *data, U32 len) {
    U32 h = 2166136261u;
    for (U32 i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

// Thirty steps: what a menu capture runs through before it is photographed,
// measured on both platforms while chasing #530. Enough for the vertices to
// have wrapped and the accumulators to have changed sign more than once, so a
// digest over it is sensitive to the parts of Do_Plasma a single step is not.
#define STEPS 30

static U32 run_steps(void) {
    static S16 virgule[NB_POINTS * NB_PTS_INTER];
    static S16 speed[NB_POINTS];
    static S32 acc[NB_POINTS];
    static U8 colors[NB_COLORS];
    static U8 tex[TEX_SIZE];

    seed_state(virgule, speed, acc, colors);
    memset(tex, 0, sizeof(tex));

    T_PLASMA plasma;
    memset(&plasma, 0, sizeof(plasma));
    plasma.TabVirgule = virgule;
    plasma.TabSpeed = speed;
    plasma.TabAcc = acc;
    plasma.TabColors = colors;
    plasma.TexOffset = tex;
    plasma.Interleave = (U8)INTERLEAVE;
    plasma.NbActivePoints = (U8)NB_ACTIVE_POINTS;
    plasma.NbColors = (U8)NB_COLORS;
    plasma.Speed = 0;

    Nb_Pts_Inter = NB_PTS_INTER;
    Nb_Pts_Control = NB_ACTIVE_POINTS;

    for (U32 i = 0; i < STEPS; ++i) {
        Do_Plasma(&plasma);
    }

    // The texture is what reaches the screen; the vertex table is what carries
    // state between steps. Hashing both means a change to either is a failure,
    // rather than one that happens to leave the visible bytes alone.
    U32 h = fnv1a(tex, TEX_SIZE);
    h ^= fnv1a((const U8 *)virgule, sizeof(virgule));
    h ^= fnv1a((const U8 *)speed, sizeof(speed));
    h ^= fnv1a((const U8 *)acc, sizeof(acc));
    return h;
}

// Regenerate deliberately, and only for a change to Do_Plasma you meant to make:
// this digest is the whole assertion. tests/test_plasma.cpp against PLASMA.ASM is
// the authority on whether a new value is still the original algorithm.
#define EXPECTED_DIGEST 0xcce88029u

static void test_plasma_steps_are_pinned(void) {
    const U32 got = run_steps();
    if (EXPECTED_DIGEST == 0u) {
        printf("    plasma digest after %d steps: 0x%08xu\n", STEPS, got);
    }
    ASSERT_EQ_UINT(EXPECTED_DIGEST, got);
}

// Same input, same output, twice in the same process: guards the case where a
// step leaks state through a static and the digest above only holds on a fresh run.
static void test_plasma_steps_are_repeatable(void) {
    ASSERT_EQ_UINT(run_steps(), run_steps());
}

int main(void) {
    RUN_TEST(test_plasma_steps_are_pinned);
    RUN_TEST(test_plasma_steps_are_repeatable);
    TEST_SUMMARY();
}
