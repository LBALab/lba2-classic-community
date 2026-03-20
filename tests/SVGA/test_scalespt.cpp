/* Test: ScaleSpriteTransp — ASM-vs-CPP equivalence. */
#include "test_harness.h"

#include <SVGA/SCALESPT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREENXY.H>

#include <string.h>
#include <stdio.h>

extern "C" void asm_ScaleSpriteTransp(S32 num, S32 x, S32 y, S32 factorx, S32 factory, void *ptrbank, void *ptr_transp);

static U32 rng_state;

static U8 g_bank[4096];
static U8 g_cpp_framebuf[640 * 480];
static U8 g_asm_framebuf[640 * 480];
static U8 g_transp[256 * 256];

static void rng_seed(U32 seed_value) {
    rng_state = seed_value;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static U32 build_multi_bank(int count) {
    static const S8 hot_x_values[] = {0, 1, -1, 2, -2, 3, -3, 1};
    static const S8 hot_y_values[] = {0, -1, 1, -2, 2, -3, 3, 0};

    memset(g_bank, 0, sizeof(g_bank));

    U32 *offsets = (U32 *)g_bank;
    U32 pos = (U32)(count * 4);
    for (int index = 0; index < count; ++index) {
        const U8 width = (U8)((index % 5) + 2);
        const U8 height = (U8)((index % 4) + 2);
        offsets[index] = pos;
        g_bank[pos + 0] = width;
        g_bank[pos + 1] = height;
        g_bank[pos + 2] = (U8)hot_x_values[index & 7];
        g_bank[pos + 3] = (U8)hot_y_values[index & 7];

        U8 *pixels = g_bank + pos + 4;
        for (U8 y = 0; y < height; ++y) {
            for (U8 x = 0; x < width; ++x) {
                U8 value = (U8)((index * 17 + x * 5 + y * 11 + 1) & 0xFF);
                if (((x + y + index) & 1) != 0) {
                    value = 0;
                }
                pixels[y * width + x] = value;
            }
        }

        pos += 4 + (U32)(width * height);
    }

    return pos;
}

static void init_transp_table(void) {
    for (U32 src = 0; src < 256; ++src) {
        for (U32 dst = 0; dst < 256; ++dst) {
            g_transp[(src << 8) | dst] = (U8)((src + (dst * 3) + 7) & 0xFF);
        }
    }
}

static void fill_background(U8 *buffer) {
    for (U32 y = 0; y < 480; ++y) {
        for (U32 x = 0; x < 640; ++x) {
            buffer[y * 640 + x] = (U8)((x * 3 + y * 5 + 0x21) & 0xFF);
        }
    }
}

static void setup_screen(U8 *framebuf) {
    fill_background(framebuf);
    Log = framebuf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 row = 0; row < 480; ++row) {
        TabOffLine[row] = row * 640;
    }

    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = 639;
    ClipYMax = 479;
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
}

static void run_scalespt_case(const char *label,
                              S32 num, S32 x, S32 y,
                              S32 factorx, S32 factory) {
    S32 cpp_xmin, cpp_xmax, cpp_ymin, cpp_ymax;
    S32 asm_xmin, asm_xmax, asm_ymin, asm_ymax;

    setup_screen(g_cpp_framebuf);
    ScaleSpriteTransp(num, x, y, factorx, factory, g_bank, g_transp);
    cpp_xmin = ScreenXMin;
    cpp_xmax = ScreenXMax;
    cpp_ymin = ScreenYMin;
    cpp_ymax = ScreenYMax;

    setup_screen(g_asm_framebuf);
    asm_ScaleSpriteTransp(num, x, y, factorx, factory, g_bank, g_transp);
    asm_xmin = ScreenXMin;
    asm_xmax = ScreenXMax;
    asm_ymin = ScreenYMin;
    asm_ymax = ScreenYMax;

    ASSERT_ASM_CPP_EQ_INT(asm_xmin, cpp_xmin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_xmax, cpp_xmax, label);
    ASSERT_ASM_CPP_EQ_INT(asm_ymin, cpp_ymin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_ymax, cpp_ymax, label);
    ASSERT_ASM_CPP_MEM_EQ(g_asm_framebuf, g_cpp_framebuf, sizeof(g_cpp_framebuf), label);
}

static void test_scalespt_fixed_cases(void) {
    build_multi_bank(8);
    init_transp_table();

    run_scalespt_case("ScaleSpriteTransp 1:1 interior", 0, 120, 80, 0x10000, 0x10000);
    run_scalespt_case("ScaleSpriteTransp scaled up", 3, 210, 140, 0x18000, 0x14000);
    run_scalespt_case("ScaleSpriteTransp scaled down", 5, 320, 200, 0x08000, 0x0C000);
}

static void test_scalespt_edge_cases(void) {
    build_multi_bank(8);
    init_transp_table();

    run_scalespt_case("ScaleSpriteTransp clipped top left", 4, -3, -2, 0x10000, 0x10000);
    run_scalespt_case("ScaleSpriteTransp fully clipped right", 2, 900, 50, 0x10000, 0x10000);
    run_scalespt_case("ScaleSpriteTransp negative factor fallback", 6, 180, 150, -3, 0x10000);
    run_scalespt_case("ScaleSpriteTransp zero factor early exit", 1, 70, 60, 0, 0x10000);
}

static void test_scalespt_random_stress(void) {
    build_multi_bank(8);
    init_transp_table();
    rng_seed(0xDEADBEEFu);

    for (int round = 0; round < 300; ++round) {
        S32 num = (S32)(rng_next() % 8);
        S32 x = (S32)(rng_next() % 760) - 60;
        S32 y = (S32)(rng_next() % 600) - 60;
        S32 factorx_choices[] = {0x10000, 0x08000, 0x18000, 0x14000, -5, 0};
        S32 factory_choices[] = {0x10000, 0x0C000, 0x16000, 0x09000, -7, 0};
        S32 factorx = factorx_choices[rng_next() % 6];
        S32 factory = factory_choices[rng_next() % 6];

        char label[96];
        snprintf(label, sizeof(label),
                 "ScaleSpriteTransp random #%d spr=%d pos=(%d,%d) fx=%d fy=%d",
                 round, num, x, y, factorx, factory);
        run_scalespt_case(label, num, x, y, factorx, factory);
    }
}

int main(void) {
    RUN_TEST(test_scalespt_fixed_cases);
    RUN_TEST(test_scalespt_edge_cases);
    RUN_TEST(test_scalespt_random_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}
