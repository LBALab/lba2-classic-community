#include "test_harness.h"

#include "EXTERN.H"

#include <POLYGON/POLY.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>
#include <SYSTEM/LIMITS.H>

#include <stdio.h>
#include <string.h>

extern "C" S32 asm_LineRain(S32 x0, S32 y0, S32 z0,
                            S32 x1, S32 y1, S32 z1,
                            S32 coul);

void *Log = NULL;
PTR_U16 PtrZBuffer = NULL;
PTR_U32 PTR_TabOffLine = NULL;
U32 ScreenPitch = 0;
U32 ModeDesiredX = 0;
U32 ModeDesiredY = 0;
U32 TabOffLine[ADELINE_MAX_Y_RES];

S32 ClipXMin = 0;
S32 ClipYMin = 0;
S32 ClipXMax = 0;
S32 ClipYMax = 0;

S32 ScreenXMin = 0;
S32 ScreenYMin = 0;
S32 ScreenXMax = 0;
S32 ScreenYMax = 0;

U8 Fill_Flag_Fog = 0;
U8 Fill_Flag_ZBuffer = 0;
U8 Fill_Logical_Palette[256];

static const U32 kWidth = 96;
static const U32 kHeight = 72;
static const U32 kPixelCount = kWidth * kHeight;

static U8 initial_log[kPixelCount];
static U16 initial_zbuf[kPixelCount];
static U8 initial_palette[256];

static U8 cpp_log[kPixelCount];
static U16 cpp_zbuf[kPixelCount];

static U8 asm_log[kPixelCount];
static U16 asm_zbuf[kPixelCount];

static U32 rng_state;

static void rng_seed(U32 s) { rng_state = s; }

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

struct RunResult {
    S32 ret;
    S32 screen_xmin;
    S32 screen_ymin;
    S32 screen_xmax;
    S32 screen_ymax;
};

static void init_palette_identity(void) {
    for (U32 i = 0; i < 256; ++i) {
        initial_palette[i] = (U8)i;
    }
}

static void init_default_buffers(void) {
    for (U32 i = 0; i < kPixelCount; ++i) {
        initial_log[i] = (U8)((i * 23u + 17u) & 0xFFu);
        initial_zbuf[i] = (U16)((i * 61u + 19u) & 0xFFFFu);
    }
    init_palette_identity();
}

static void randomize_state(void) {
    for (U32 i = 0; i < kPixelCount; ++i) {
        initial_log[i] = (U8)rng_next();
        initial_zbuf[i] = (U16)(((rng_next() << 1) ^ rng_next()) & 0xFFFFu);
    }
    for (U32 i = 0; i < 256; ++i) {
        initial_palette[i] = (U8)rng_next();
    }
}

static void clone_initial_state(void) {
    memcpy(cpp_log, initial_log, sizeof(initial_log));
    memcpy(cpp_zbuf, initial_zbuf, sizeof(initial_zbuf));
    memcpy(asm_log, initial_log, sizeof(initial_log));
    memcpy(asm_zbuf, initial_zbuf, sizeof(initial_zbuf));
}

static void fill_rect_z(U16 value, U32 xmin, U32 ymin, U32 xmax, U32 ymax) {
    for (U32 y = ymin; y <= ymax; ++y) {
        for (U32 x = xmin; x <= xmax; ++x) {
            initial_zbuf[y * kWidth + x] = value;
        }
    }
}

static void setup_globals(U8 *log_buf, U16 *zbuf_buf,
                          S32 clip_xmin, S32 clip_ymin,
                          S32 clip_xmax, S32 clip_ymax,
                          U8 fog_enabled) {
    Log = log_buf;
    PtrZBuffer = zbuf_buf;
    PTR_TabOffLine = TabOffLine;
    ScreenPitch = kWidth;
    ModeDesiredX = kWidth;
    ModeDesiredY = kHeight;

    for (U32 y = 0; y < kHeight; ++y) {
        TabOffLine[y] = y * kWidth;
    }

    ClipXMin = clip_xmin;
    ClipYMin = clip_ymin;
    ClipXMax = clip_xmax;
    ClipYMax = clip_ymax;

    Fill_Flag_Fog = fog_enabled;
    Fill_Flag_ZBuffer = 1;
    memcpy(Fill_Logical_Palette, initial_palette, sizeof(initial_palette));

    ScreenXMin = 0x12345678;
    ScreenYMin = 0x23456789;
    ScreenXMax = 0x3456789A;
    ScreenYMax = 0x456789AB;
}

static RunResult run_cpp(S32 x0, S32 y0, S32 z0,
                         S32 x1, S32 y1, S32 z1,
                         S32 color,
                         S32 clip_xmin, S32 clip_ymin,
                         S32 clip_xmax, S32 clip_ymax,
                         U8 fog_enabled) {
    setup_globals(cpp_log, cpp_zbuf,
                  clip_xmin, clip_ymin, clip_xmax, clip_ymax,
                  fog_enabled);

    RunResult result;
    result.ret = LineRain(x0, y0, z0, x1, y1, z1, color);
    result.screen_xmin = ScreenXMin;
    result.screen_ymin = ScreenYMin;
    result.screen_xmax = ScreenXMax;
    result.screen_ymax = ScreenYMax;
    return result;
}

static RunResult run_asm(S32 x0, S32 y0, S32 z0,
                         S32 x1, S32 y1, S32 z1,
                         S32 color,
                         S32 clip_xmin, S32 clip_ymin,
                         S32 clip_xmax, S32 clip_ymax,
                         U8 fog_enabled) {
    setup_globals(asm_log, asm_zbuf,
                  clip_xmin, clip_ymin, clip_xmax, clip_ymax,
                  fog_enabled);

    RunResult result;
    result.ret = asm_LineRain(x0, y0, z0, x1, y1, z1, color);
    result.screen_xmin = ScreenXMin;
    result.screen_ymin = ScreenYMin;
    result.screen_xmax = ScreenXMax;
    result.screen_ymax = ScreenYMax;
    return result;
}

static void assert_case_equivalence(const char *label,
                                    S32 x0, S32 y0, S32 z0,
                                    S32 x1, S32 y1, S32 z1,
                                    S32 color,
                                    S32 clip_xmin, S32 clip_ymin,
                                    S32 clip_xmax, S32 clip_ymax,
                                    U8 fog_enabled) {
    clone_initial_state();

    RunResult cpp_result = run_cpp(x0, y0, z0, x1, y1, z1, color,
                                   clip_xmin, clip_ymin, clip_xmax, clip_ymax,
                                   fog_enabled);
    RunResult asm_result = run_asm(x0, y0, z0, x1, y1, z1, color,
                                   clip_xmin, clip_ymin, clip_xmax, clip_ymax,
                                   fog_enabled);

    ASSERT_ASM_CPP_EQ_INT(asm_result.ret, cpp_result.ret, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.screen_xmin, cpp_result.screen_xmin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.screen_ymin, cpp_result.screen_ymin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.screen_xmax, cpp_result.screen_xmax, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.screen_ymax, cpp_result.screen_ymax, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_log, cpp_log, sizeof(cpp_log), label);
    ASSERT_ASM_CPP_MEM_EQ((U8 *)asm_zbuf, (U8 *)cpp_zbuf, sizeof(cpp_zbuf), label);
}

static void test_linerain_horizontal_visible(void) {
    init_default_buffers();
    fill_rect_z(1200, 12, 15, 40, 15);

    assert_case_equivalence("LineRain horizontal visible",
                            12, 15, 180,
                            40, 15, 180,
                            0x34,
                            0, 0, kWidth - 1, kHeight - 1,
                            0);
}

static void test_linerain_diagonal_fog_intersection(void) {
    init_default_buffers();
    fill_rect_z(150, 20, 10, 40, 30);
    initial_palette[0x5A] = 0xC7;

    assert_case_equivalence("LineRain diagonal fog intersection",
                            20, 10, 170,
                            40, 30, 200,
                            0x5A,
                            0, 0, kWidth - 1, kHeight - 1,
                            1);
}

static void test_linerain_vertical_occluded(void) {
    init_default_buffers();
    fill_rect_z(10, 30, 12, 30, 26);

    assert_case_equivalence("LineRain vertical occluded",
                            30, 12, 180,
                            30, 26, 180,
                            0x91,
                            0, 0, kWidth - 1, kHeight - 1,
                            0);
}

static void test_linerain_single_pixel(void) {
    init_default_buffers();
    initial_zbuf[18 * kWidth + 14] = 220;

    assert_case_equivalence("LineRain single pixel",
                            14, 18, 120,
                            14, 18, 120,
                            0x66,
                            0, 0, kWidth - 1, kHeight - 1,
                            0);
}

static void test_linerain_fully_clipped(void) {
    init_default_buffers();

    assert_case_equivalence("LineRain fully clipped",
                            -20, -10, 100,
                            -5, -2, 160,
                            0x7B,
                            0, 0, kWidth - 1, kHeight - 1,
                            0);
}

static void test_linerain_random_stress(void) {
    rng_seed(0xDEADBEEFu);

    for (int round = 0; round < 300; ++round) {
        randomize_state();

        S32 x0 = (S32)(rng_next() % (kWidth + 40)) - 20;
        S32 y0 = (S32)(rng_next() % (kHeight + 40)) - 20;
        S32 x1 = (S32)(rng_next() % (kWidth + 40)) - 20;
        S32 y1 = (S32)(rng_next() % (kHeight + 40)) - 20;
        S32 z0 = (S32)(rng_next() % 400) - 50;
        S32 z1 = (S32)(rng_next() % 400) - 50;
        S32 color = (S32)(rng_next() & 0xFF);
        S32 clip_xmin = (S32)(rng_next() % (kWidth / 2));
        S32 clip_ymin = (S32)(rng_next() % (kHeight / 2));
        S32 clip_xmax = clip_xmin + (S32)(rng_next() % (kWidth - clip_xmin));
        S32 clip_ymax = clip_ymin + (S32)(rng_next() % (kHeight - clip_ymin));
        U8 fog_enabled = (U8)(rng_next() & 1u);
        char msg[96];

        snprintf(msg, sizeof(msg), "LineRain random round %d", round);
        assert_case_equivalence(msg,
                                x0, y0, z0,
                                x1, y1, z1,
                                color,
                                clip_xmin, clip_ymin,
                                clip_xmax, clip_ymax,
                                fog_enabled);
    }
}

int main(void) {
    RUN_TEST(test_linerain_horizontal_visible);
    RUN_TEST(test_linerain_diagonal_fog_intersection);
    RUN_TEST(test_linerain_vertical_occluded);
    RUN_TEST(test_linerain_single_pixel);
    RUN_TEST(test_linerain_fully_clipped);
    RUN_TEST(test_linerain_random_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}