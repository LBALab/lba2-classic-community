/* Test: ScaleSprite — ASM-vs-CPP equivalence.
 *
 * The original ASM PROC only declared `uses ebp` but clobbers esi/edi/ebx
 * (callee-saved in cdecl). Fixed by adding `uses esi edi ebx` to the proc.
 *
 * Covers the 1:1 fast path and the 16.16 fixed-point scaled path. The CPP
 * implementation mirrors ScaleSpriteTransp's algorithm minus the
 * transparency-table indirection.
 */
#include "test_harness.h"
#include <SVGA/SCALESPI.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREENXY.H>
#include <string.h>
#include <stdio.h>

/* The raw ASM symbol */
extern "C" void asm_ScaleSprite(S32 num, S32 x, S32 y, S32 factorx, S32 factory, void *ptrbank);
#define call_asm_ScaleSprite asm_ScaleSprite

static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

static U8 g_bank[4096];
static U8 framebuf[640 * 480];
static U8 cpp_framebuf[640 * 480];
static U8 asm_framebuf[640 * 480];

typedef struct SpriteRunResult {
    S32 xmin;
    S32 xmax;
    S32 ymin;
    S32 ymax;
} SpriteRunResult;

static U32 build_multi_bank(int n) {
    static const S8 hot_x_values[] = {0, 1, -1, 2, -2, 3, -3, 1};
    static const S8 hot_y_values[] = {0, -1, 1, -2, 2, -3, 3, 0};

    memset(g_bank, 0, sizeof(g_bank));
    U32 *off = (U32 *)g_bank;
    U32 pos = (U32)(n * 4);
    for (int i = 0; i < n; i++) {
        off[i] = pos;
        U8 w = (U8)((i % 8) + 2);
        U8 h = (U8)((i % 6) + 2);
        g_bank[pos] = w;
        g_bank[pos + 1] = h;
        g_bank[pos + 2] = (U8)hot_x_values[i & 7];
        g_bank[pos + 3] = (U8)hot_y_values[i & 7];
        U8 *px = g_bank + pos + 4;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                px[y * w + x] = ((x + y) % 2 == 0) ? (U8)(i + 1 + y * w + x) : 0;
        pos += 4 + (U32)(w * h);
    }
    return pos;
}

static void setup_screen(U8 *buf) {
    memset(buf, 0, sizeof(framebuf));
    Log = buf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = 639;
    ClipYMax = 479;
}

static void setup(void) {
    setup_screen(framebuf);
}

static SpriteRunResult run_cpp_case(S32 num, S32 x, S32 y) {
    SpriteRunResult result;
    setup_screen(cpp_framebuf);
    ScaleSprite(num, x, y, 0x10000, 0x10000, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static SpriteRunResult run_asm_case(S32 num, S32 x, S32 y) {
    SpriteRunResult result;
    setup_screen(asm_framebuf);
    call_asm_ScaleSprite(num, x, y, 0x10000, 0x10000, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static void assert_case_matches(const char *label, S32 num, S32 x, S32 y) {
    SpriteRunResult cpp_result = run_cpp_case(num, x, y);
    SpriteRunResult asm_result = run_asm_case(num, x, y);

    ASSERT_ASM_CPP_EQ_INT(asm_result.xmin, cpp_result.xmin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.xmax, cpp_result.xmax, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymin, cpp_result.ymin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymax, cpp_result.ymax, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_framebuf, cpp_framebuf, sizeof(cpp_framebuf), label);
}

static SpriteRunResult run_cpp_case_scaled(S32 num, S32 x, S32 y, S32 factorx, S32 factory) {
    SpriteRunResult result;
    setup_screen(cpp_framebuf);
    ScaleSprite(num, x, y, factorx, factory, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static SpriteRunResult run_asm_case_scaled(S32 num, S32 x, S32 y, S32 factorx, S32 factory) {
    SpriteRunResult result;
    setup_screen(asm_framebuf);
    call_asm_ScaleSprite(num, x, y, factorx, factory, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static void assert_case_matches_scaled(const char *label, S32 num, S32 x, S32 y, S32 factorx, S32 factory) {
    SpriteRunResult cpp_result = run_cpp_case_scaled(num, x, y, factorx, factory);
    SpriteRunResult asm_result = run_asm_case_scaled(num, x, y, factorx, factory);

    ASSERT_ASM_CPP_EQ_INT(asm_result.xmin, cpp_result.xmin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.xmax, cpp_result.xmax, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymin, cpp_result.ymin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymax, cpp_result.ymax, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_framebuf, cpp_framebuf, sizeof(cpp_framebuf), label);
}

static void test_basic(void) {
    build_multi_bank(1);
    setup();
    ScaleSprite(0, 100, 100, 0x10000, 0x10000, g_bank);
    ASSERT_EQ_UINT(1, framebuf[100 * 640 + 100]);
    ASSERT_EQ_UINT(0, framebuf[100 * 640 + 101]);
}

static void test_asm_minimal(void) {
    build_multi_bank(1);
    assert_case_matches("ScaleSprite minimal 1:1", 0, 100, 100);
}

static void test_transparency(void) {
    build_multi_bank(1);
    setup();
    for (int y = 99; y < 103; y++)
        for (int x = 99; x < 103; x++)
            framebuf[y * 640 + x] = 0xAA;
    ScaleSprite(0, 100, 100, 0x10000, 0x10000, g_bank);
    ASSERT_EQ_UINT(0xAA, framebuf[100 * 640 + 101]);
    ASSERT_TRUE(framebuf[100 * 640 + 100] != 0xAA);
}

static void test_fully_clipped(void) {
    build_multi_bank(1);
    assert_case_matches("ScaleSprite fully clipped", 0, -100, -100);
}

static void test_clip_left(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clip left", 3, -2, 100);
}

static void test_clip_right(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clip right", 3, 636, 100);
}

static void test_clip_top(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clip top", 3, 100, -2);
}

static void test_clip_bottom(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clip bottom", 3, 100, 477);
}

static void test_hotspot(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite hotspot", 2, 200, 200);
}

static void test_negative_hotspot(void) {
    build_multi_bank(8);
    assert_case_matches("ScaleSprite negative hotspot", 4, 1, 1);
}

static void test_multi_sprites(void) {
    build_multi_bank(8);
    setup();
    for (int i = 0; i < 8; i++) {
        ScaleSprite(i, 50 + i * 50, 50, 0x10000, 0x10000, g_bank);
        ASSERT_TRUE(ScreenXMin >= 0);
    }
}

static void test_random_batch(void) {
    build_multi_bank(8);
    rng_seed(0x5A710001);
    int prev = test_failures;
    for (int i = 0; i < 100 && test_failures == prev; i++) {
        S32 num = (S32)(rng_next() % 8);
        S32 sx = (S32)(rng_next() % 680) - 20;
        S32 sy = (S32)(rng_next() % 520) - 20;
        char label[64];
        snprintf(label, sizeof(label), "ScaleSprite random #%d", i);
        assert_case_matches(label, num, sx, sy);
    }
}

static void test_asm_equiv_1to1(void) {
    build_multi_bank(1);
    assert_case_matches("ScaleSprite 1:1", 0, 50, 50);
}

static void test_asm_equiv_clipped(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clipped", 3, -1, -1);
}

static void test_asm_equiv_random(void) {
    build_multi_bank(8);
    rng_seed(0xA5E01234);
    int prev = test_failures;
    for (int i = 0; i < 20 && test_failures == prev; i++) {
        S32 num = (S32)(rng_next() % 8);
        S32 sx = (S32)(rng_next() % 600) + 10;
        S32 sy = (S32)(rng_next() % 440) + 10;

        char label[64];
        snprintf(label, sizeof(label), "ScaleSprite random #%d spr=%d (%d,%d)", i, num, sx, sy);
        assert_case_matches(label, num, sx, sy);
    }
}

static void test_asm_equiv_scaled_basic(void) {
    build_multi_bank(8);
    assert_case_matches_scaled("ScaleSprite 1:1 baseline", 0, 120, 80, 0x10000, 0x10000);
    assert_case_matches_scaled("ScaleSprite scaled up", 3, 210, 140, 0x18000, 0x14000);
    assert_case_matches_scaled("ScaleSprite scaled down", 5, 320, 200, 0x08000, 0x0C000);
}

static void test_asm_equiv_scaled_edge(void) {
    build_multi_bank(8);
    assert_case_matches_scaled("ScaleSprite scaled clipped top-left", 4, -3, -2, 0x10000, 0x10000);
    assert_case_matches_scaled("ScaleSprite scaled fully clipped right", 2, 900, 50, 0x10000, 0x10000);
    assert_case_matches_scaled("ScaleSprite negative-factor fallback", 6, 180, 150, -3, 0x10000);
    assert_case_matches_scaled("ScaleSprite zero-factor early exit", 1, 70, 60, 0, 0x10000);
}

static void test_asm_equiv_scaled_random(void) {
    build_multi_bank(8);
    rng_seed(0xC0DEC0DE);
    int prev = test_failures;
    for (int round = 0; round < 32 && test_failures == prev; round++) {
        S32 num = (S32)(rng_next() % 8);
        S32 x = (S32)(rng_next() % 700) - 30;
        S32 y = (S32)(rng_next() % 540) - 30;
        S32 factorx_choices[] = {0x10000, 0x08000, 0x18000, 0x14000, -5, 0};
        S32 factory_choices[] = {0x10000, 0x0C000, 0x16000, 0x09000, -7, 0};
        S32 factorx = factorx_choices[rng_next() % 6];
        S32 factory = factory_choices[rng_next() % 6];

        char label[128];
        snprintf(label, sizeof(label),
                 "ScaleSprite scaled random #%d spr=%d pos=(%d,%d) fx=%d fy=%d",
                 round, num, x, y, factorx, factory);
        assert_case_matches_scaled(label, num, x, y, factorx, factory);
    }
}

int main(void) {
    RUN_TEST(test_basic);
    RUN_TEST(test_asm_minimal);
    RUN_TEST(test_transparency);
    RUN_TEST(test_fully_clipped);
    RUN_TEST(test_clip_left);
    RUN_TEST(test_clip_right);
    RUN_TEST(test_clip_top);
    RUN_TEST(test_clip_bottom);
    RUN_TEST(test_hotspot);
    RUN_TEST(test_negative_hotspot);
    RUN_TEST(test_multi_sprites);
    RUN_TEST(test_random_batch);
    RUN_TEST(test_asm_equiv_1to1);
    RUN_TEST(test_asm_equiv_clipped);
    RUN_TEST(test_asm_equiv_random);
    RUN_TEST(test_asm_equiv_scaled_basic);
    RUN_TEST(test_asm_equiv_scaled_edge);
    RUN_TEST(test_asm_equiv_scaled_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
