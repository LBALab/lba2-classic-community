/* Test: ScaleSprite — ASM-vs-CPP equivalence.
 *
 * The original ASM PROC only declared `uses ebp` but clobbers esi/edi/ebx
 * (callee-saved in cdecl). Fixed by adding `uses esi edi ebx` to the proc.
 *
 * CPP implementation ignores factorx/factory (always 1:1 blit).
 * ASM does real fixed-point scaling. ASM-vs-CPP equiv only at 1:1 scale.
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

static U32 build_multi_bank(int n) {
    memset(g_bank, 0, sizeof(g_bank));
    U32 *off = (U32 *)g_bank;
    U32 pos = (U32)(n * 4);
    for (int i = 0; i < n; i++) {
        off[i] = pos;
        U8 w = (U8)((i % 8) + 2);
        U8 h = (U8)((i % 6) + 2);
        g_bank[pos] = w;
        g_bank[pos + 1] = h;
        g_bank[pos + 2] = (U8)(i % 3);
        g_bank[pos + 3] = (U8)(i % 2);
        U8 *px = g_bank + pos + 4;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                px[y * w + x] = ((x + y) % 2 == 0) ? (U8)(i + 1 + y * w + x) : 0;
        pos += 4 + (U32)(w * h);
    }
    return pos;
}

static void setup(void) {
    memset(framebuf, 0, sizeof(framebuf));
    Log = framebuf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = 639;
    ClipYMax = 479;
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
    setup();
    printf("# test_asm_minimal: calling asm_ScaleSprite...\n");
    fflush(stdout);
    call_asm_ScaleSprite(0, 100, 100, 0x10000, 0x10000, g_bank);
    printf("# test_asm_minimal: returned OK\n");
    fflush(stdout);
    ASSERT_TRUE(1);
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
    setup();
    ScaleSprite(0, -100, -100, 0x10000, 0x10000, g_bank);
    ASSERT_TRUE(ScreenXMin > ScreenXMax);
}

static void test_clip_left(void) {
    build_multi_bank(4);
    setup();
    ScaleSprite(3, -2, 100, 0x10000, 0x10000, g_bank);
    ASSERT_TRUE(ScreenXMin >= 0);
}

static void test_clip_right(void) {
    build_multi_bank(4);
    setup();
    ScaleSprite(3, 636, 100, 0x10000, 0x10000, g_bank);
    ASSERT_TRUE(ScreenXMax <= 639);
}

static void test_clip_top(void) {
    build_multi_bank(4);
    setup();
    ScaleSprite(3, 100, -2, 0x10000, 0x10000, g_bank);
    ASSERT_TRUE(ScreenYMin >= 0);
}

static void test_clip_bottom(void) {
    build_multi_bank(4);
    setup();
    ScaleSprite(3, 100, 477, 0x10000, 0x10000, g_bank);
    ASSERT_TRUE(ScreenYMax <= 479);
}

static void test_hotspot(void) {
    build_multi_bank(4);
    setup();
    /* Sprite 2: w=4, h=4, hx=2, hy=0. Hot_X adds 2 to x position */
    ScaleSprite(2, 200, 200, 0x10000, 0x10000, g_bank);
    ASSERT_EQ_INT(202, ScreenXMin); /* 200 + hx(2) */
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
    for (int i = 0; i < 30 && test_failures == prev; i++) {
        setup();
        S32 num = (S32)(rng_next() % 8);
        S32 sx = (S32)(rng_next() % 680) - 20;
        S32 sy = (S32)(rng_next() % 520) - 20;
        ScaleSprite(num, sx, sy, 0x10000, 0x10000, g_bank);
        if (ScreenXMin <= ScreenXMax) {
            ASSERT_TRUE(ScreenXMin >= 0);
            ASSERT_TRUE(ScreenXMax <= 639);
            ASSERT_TRUE(ScreenYMin >= 0);
            ASSERT_TRUE(ScreenYMax <= 479);
        }
    }
}

static void test_asm_equiv_1to1(void) {
    static U8 cpp_buf[640 * 480];
    static U8 asm_buf[640 * 480];
    build_multi_bank(1);

    setup();
    ScaleSprite(0, 50, 50, 0x10000, 0x10000, g_bank);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup();
    call_asm_ScaleSprite(0, 50, 50, 0x10000, 0x10000, g_bank);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "ScaleSprite 1:1");
}

static void test_asm_equiv_clipped(void) {
    static U8 cpp_buf[640 * 480];
    static U8 asm_buf[640 * 480];
    build_multi_bank(4);

    setup();
    ScaleSprite(3, -1, -1, 0x10000, 0x10000, g_bank);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup();
    call_asm_ScaleSprite(3, -1, -1, 0x10000, 0x10000, g_bank);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "ScaleSprite clipped");
}

static void test_asm_equiv_random(void) {
    static U8 cpp_buf[640 * 480];
    static U8 asm_buf[640 * 480];
    build_multi_bank(8);
    rng_seed(0xA5E01234);
    int prev = test_failures;
    for (int i = 0; i < 20 && test_failures == prev; i++) {
        S32 num = (S32)(rng_next() % 8);
        S32 sx = (S32)(rng_next() % 600) + 10;
        S32 sy = (S32)(rng_next() % 440) + 10;

        setup();
        ScaleSprite(num, sx, sy, 0x10000, 0x10000, g_bank);
        memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

        setup();
        call_asm_ScaleSprite(num, sx, sy, 0x10000, 0x10000, g_bank);
        memcpy(asm_buf, framebuf, sizeof(asm_buf));

        char label[64];
        snprintf(label, sizeof(label), "ScaleSprite random #%d spr=%d (%d,%d)", i, num, sx, sy);
        ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), label);
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
    RUN_TEST(test_multi_sprites);
    RUN_TEST(test_random_batch);
    RUN_TEST(test_asm_equiv_1to1);
    RUN_TEST(test_asm_equiv_clipped);
    RUN_TEST(test_asm_equiv_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
