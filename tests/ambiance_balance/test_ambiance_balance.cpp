/*
 * Unit tests for SOURCES/AUDIO_BALANCE.CPP — the stereo pan + distance
 * attenuation math used by the ambient-sound mixer (HQ_3D_MixSample /
 * HQ_3D_ChangePanSample).
 *
 * Pins the contract end-to-end at multiple framebuffer widths without
 * linking the audio backend.  The math is pure: it reads (xp, yp,
 * distance, ModeDesiredX, ModeDesiredY, CubeMode) and writes (*pan,
 * *vol, return), nothing else.
 *
 * Why this matters: at ModeDesiredX > 640 the 4:3 literals (-320, 160,
 * 480, 640+320) would still make pan ramps anchor to the 4:3 region,
 * so sounds at the visible edge of a 768/1024 frame wouldn't hard-pan.
 * The recent C2 routing PR fixes that; this test is what catches the
 * next someone-reorders-the-quarter-points regression.
 */

#include <SYSTEM/ADELINE_TYPES.H>

/* Forward-declare GiveBalance directly rather than including AMBIANCE.H,
   which transitively pulls in C_EXTERN.H -> OBJECT.H -> T_OBJET — none
   of which the math needs. The signature must match SOURCES/AMBIANCE.H. */
extern S32 GiveBalance(S32 xp, S32 yp, S32 distance, U32 *pan, S32 *vol);

#include <stdio.h>
#include <stdlib.h>

/* Re-declared engine globals so the host test links without dragging in
   SVGA/SCREEN.CPP or the rest of the engine.  AUDIO_BALANCE.CPP reads
   these at runtime; we set them per-case to drive the math. */
U32 ModeDesiredX = 640;
U32 ModeDesiredY = 480;
S32 CubeMode = 0; /* 0 == CUBE_INTERIEUR per COMMON.H */

/* AUDIO_BALANCE.CPP also pulls in C_EXTERN.H which forward-declares a
   handful of globals.  ListMem-allocated bits (BufSpeak, Screen, etc.)
   never get referenced by GiveBalance so we don't need to define them —
   but the linker insists on a definition for any *referenced* extern.
   GiveBalance's references are ModeDesiredX/Y + CubeMode only; the rest
   stay undefined and unreferenced. */

/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK_PAN(xp, yp, expected_pan, label)                                \
    do {                                                                      \
        U32 _pan = 0xDEAD;                                                    \
        S32 _vol = 0;                                                         \
        S32 _ret = GiveBalance((xp), (yp), 0, &_pan, &_vol);                  \
        if (!_ret || _pan != (U32)(expected_pan)) {                           \
            fprintf(stderr,                                                   \
                    "FAIL: %s: GiveBalance(%d, %d) ret=%d pan=%u "            \
                    "(expected ret=1 pan=%d) [MDX=%u MDY=%u]\n",              \
                    (label), (S32)(xp), (S32)(yp), (int)_ret, (unsigned)_pan, \
                    (int)(expected_pan), (unsigned)ModeDesiredX,              \
                    (unsigned)ModeDesiredY);                                  \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

#define CHECK_REJECT(xp, yp, label)                                            \
    do {                                                                       \
        U32 _pan = 0xDEAD;                                                     \
        S32 _vol = 0xDEAD;                                                     \
        S32 _ret = GiveBalance((xp), (yp), 0, &_pan, &_vol);                   \
        if (_ret) {                                                            \
            fprintf(stderr,                                                    \
                    "FAIL: %s: GiveBalance(%d, %d) ret=1 but expected reject " \
                    "[MDX=%u MDY=%u]\n",                                       \
                    (label), (S32)(xp), (S32)(yp),                             \
                    (unsigned)ModeDesiredX, (unsigned)ModeDesiredY);           \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_VOL(distance, mode, expected_vol, label)                      \
    do {                                                                    \
        U32 _pan = 0;                                                       \
        S32 _vol = 0xDEAD;                                                  \
        CubeMode = (mode);                                                  \
        /* xp=mdx/2 -> centre, in middle pan zone, so pan calc succeeds. */ \
        S32 _ret = GiveBalance((S32)ModeDesiredX / 2, 0, (distance),        \
                               &_pan, &_vol);                               \
        (void)_ret;                                                         \
        if (_vol != (expected_vol)) {                                       \
            fprintf(stderr,                                                 \
                    "FAIL: %s: GiveBalance(distance=%d, mode=%d) vol=%d "   \
                    "(expected %d)\n",                                      \
                    (label), (S32)(distance), (S32)(mode),                  \
                    (int)_vol, (int)(expected_vol));                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

/* ------------------------------------------------------------------ */
/* Pan zones at the canonical 640x480 width — these pin the byte-identical
   degenerate values the routed code is required to produce. */

static void test_pan_640_left_edge(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    CubeMode = 0;
    CHECK_PAN(-320, 0, 0, "640: xp=-320 (off-left edge) -> pan=0");
}

static void test_pan_640_left_ramp_end(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    CubeMode = 0;
    /* xp=159 is the last value in the left branch (xp < 160).
       RegleTrois(0, 32, 479, 159+320=479) = 32. */
    CHECK_PAN(159, 0, 32, "640: xp=159 (end of left ramp) -> pan=32");
}

static void test_pan_640_middle_centre(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    CubeMode = 0;
    /* xp=320 is screen-centre. Middle branch: RegleTrois(32, 96, 320, xp-160).
       xp=320 -> (32 + (96-32) * 160 / 320) = 32 + 32 = 64. */
    CHECK_PAN(320, 0, 64, "640: xp=320 (screen centre) -> pan=64");
}

static void test_pan_640_right_ramp_start(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    CubeMode = 0;
    /* xp=481 is the first value past the middle (xp > 480).
       RegleTrois(96, 127, 479, 481-480=1) = 96 + (127-96)*1/479 = 96. */
    CHECK_PAN(481, 0, 96, "640: xp=481 (start of right ramp) -> pan=96");
}

static void test_pan_640_right_edge(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    CubeMode = 0;
    /* xp=959 is the last value in-range (xp < 640+320=960).
       RegleTrois(96, 127, 479, 959-480=479) = 127. */
    CHECK_PAN(959, 0, 127, "640: xp=959 (off-right edge) -> pan=127");
}

/* ------------------------------------------------------------------ */
/* Off-screen rejects — exactly one past each boundary. */

static void test_reject_640_off_left(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    CHECK_REJECT(-321, 0, "640: xp=-321 (one past off-left)");
}

static void test_reject_640_off_right(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    CHECK_REJECT(960, 0, "640: xp=960 (at off-right boundary)");
}

static void test_reject_640_off_below(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    CHECK_REJECT(320, 961, "640: yp=961 (past 480+480)");
}

static void test_reject_640_off_above(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    CHECK_REJECT(320, -481, "640: yp=-481 (past -480)");
}

/* ------------------------------------------------------------------ */
/* Wider framebuffers — pan zones scale with ModeDesiredX. */

static void test_pan_768_centre(void) {
    ModeDesiredX = 768;
    ModeDesiredY = 480;
    CubeMode = 0;
    /* mdx/2 = 384 -> screen centre. Middle branch with innerSteps=384.
       xp-192 = 192 -> RegleTrois(32, 96, 384, 192) = 32 + 64*192/384 = 64. */
    CHECK_PAN(384, 0, 64, "768: xp=384 (screen centre) -> pan=64");
}

static void test_pan_768_left_ramp_end(void) {
    ModeDesiredX = 768;
    ModeDesiredY = 480;
    CubeMode = 0;
    /* mdx/4 = 192 -> end of left ramp. xp=191 is last value in left branch.
       outerSteps = 3*768/4 - 1 = 575. xp+384 = 575 -> pan=32. */
    CHECK_PAN(191, 0, 32, "768: xp=191 (end of left ramp) -> pan=32");
}

static void test_pan_768_right_ramp_start(void) {
    ModeDesiredX = 768;
    ModeDesiredY = 480;
    CubeMode = 0;
    /* mdx*3/4 = 576 -> start of right ramp. xp=577 is first in right branch.
       RegleTrois(96, 127, 575, 577-576=1) = 96 + 31*1/575 = 96. */
    CHECK_PAN(577, 0, 96, "768: xp=577 (start of right ramp) -> pan=96");
}

static void test_reject_768_off_right(void) {
    ModeDesiredX = 768;
    ModeDesiredY = 480;
    CHECK_REJECT(1152, 0, "768: xp=1152 (mdx + mdx/2)");
}

static void test_pan_1024_centre(void) {
    ModeDesiredX = 1024;
    ModeDesiredY = 480;
    CubeMode = 0;
    /* mdx/2 = 512. innerSteps = 512. xp-256 = 256 -> RegleTrois(32, 96, 512, 256) = 64. */
    CHECK_PAN(512, 0, 64, "1024: xp=512 (screen centre) -> pan=64");
}

static void test_reject_1024_off_left(void) {
    ModeDesiredX = 1024;
    ModeDesiredY = 480;
    CHECK_REJECT(-513, 0, "1024: xp=-513 (one past mdx/2 reject)");
}

/* ------------------------------------------------------------------ */
/* Volume attenuation — interior cube uses Distance2D + DISTANCE_SAMPLE_MAX. */

static void test_vol_interior_at_listener(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    /* distance=0: BoundRegleTrois(127, 0, 700, 0) = 127 (loudest). */
    CHECK_VOL(0, 0 /* CUBE_INTERIEUR */, 127, "interior: distance=0 -> vol=127");
}

static void test_vol_interior_past_max(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    /* distance > DISTANCE_SAMPLE_MAX (700) -> vol=0 (silent). */
    CHECK_VOL(701, 0 /* CUBE_INTERIEUR */, 0, "interior: distance=701 -> vol=0");
}

static void test_vol_interior_halfway(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    /* distance=350 -> BoundRegleTrois(127, 0, 700, 350) = 127 - 127*350/700 = 64. */
    CHECK_VOL(350, 0 /* CUBE_INTERIEUR */, 64, "interior: distance=350 -> vol=64");
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_pan_640_left_edge();
    test_pan_640_left_ramp_end();
    test_pan_640_middle_centre();
    test_pan_640_right_ramp_start();
    test_pan_640_right_edge();

    test_reject_640_off_left();
    test_reject_640_off_right();
    test_reject_640_off_below();
    test_reject_640_off_above();

    test_pan_768_centre();
    test_pan_768_left_ramp_end();
    test_pan_768_right_ramp_start();
    test_reject_768_off_right();

    test_pan_1024_centre();
    test_reject_1024_off_left();

    test_vol_interior_at_listener();
    test_vol_interior_past_max();
    test_vol_interior_halfway();

    if (g_failures == 0) {
        printf("PASS: test_ambiance_balance — all 18 cases\n");
        return 0;
    }
    fprintf(stderr, "FAIL: test_ambiance_balance — %d failure(s)\n",
            g_failures);
    return 1;
}
