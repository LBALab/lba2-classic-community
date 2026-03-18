/* Test: CopyBlock — fast memory block copy between screen regions */
#include "test_harness.h"

#include <SVGA/CPYBLOCK.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>
#include <string.h>

extern "C" void asm_CopyBlock(S32 x0, S32 y0, S32 x1, S32 y1, void *src,
                               S32 xd, S32 yd, void *dst);

static const U32 kScreenWidth = 640;
static const U32 kScreenHeight = 480;
static const U32 kBufferSize = kScreenWidth * kScreenHeight;

static U8 src_init[kBufferSize];
static U8 cpp_src[kBufferSize];
static U8 asm_src[kBufferSize];
static U8 dst_init[kBufferSize];
static U8 cpp_dst[kBufferSize];
static U8 asm_dst[kBufferSize];
static U32 rng_state;

static void rng_seed(U32 seed)
{
    rng_state = seed;
}

static U32 rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void setup_screen(void)
{
    ModeDesiredX = (S32)kScreenWidth;
    ModeDesiredY = (S32)kScreenHeight;
    for (U32 i = 0; i < kScreenHeight; ++i) {
        TabOffLine[i] = i * kScreenWidth;
    }
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = (S32)kScreenWidth - 1;
    ClipYMax = (S32)kScreenHeight - 1;
}

static void fill_source_pattern(U32 salt)
{
    for (U32 i = 0; i < kBufferSize; ++i) {
        src_init[i] = (U8)(((i * 29u) + (salt * 17u) + (i >> 2)) & 0xFFu);
    }
}

static void fill_dest_pattern(U32 salt)
{
    for (U32 i = 0; i < kBufferSize; ++i) {
        dst_init[i] = (U8)(((i * 13u) + (salt * 31u) + 0x66u) & 0xFFu);
    }
}

static void assert_copyblock_case(const char *label,
                                  S32 x0, S32 y0, S32 x1, S32 y1,
                                  S32 xd, S32 yd,
                                  S32 clip_x_min, S32 clip_y_min,
                                  S32 clip_x_max, S32 clip_y_max,
                                  U32 salt)
{
    setup_screen();
    fill_source_pattern(salt);
    fill_dest_pattern(salt + 1u);

    memcpy(cpp_src, src_init, sizeof(cpp_src));
    memcpy(asm_src, src_init, sizeof(asm_src));
    memcpy(cpp_dst, dst_init, sizeof(cpp_dst));
    memcpy(asm_dst, dst_init, sizeof(asm_dst));

    ClipXMin = clip_x_min;
    ClipYMin = clip_y_min;
    ClipXMax = clip_x_max;
    ClipYMax = clip_y_max;
    CopyBlock(x0, y0, x1, y1, cpp_src, xd, yd, cpp_dst);

    setup_screen();
    ClipXMin = clip_x_min;
    ClipYMin = clip_y_min;
    ClipXMax = clip_x_max;
    ClipYMax = clip_y_max;
    asm_CopyBlock(x0, y0, x1, y1, asm_src, xd, yd, asm_dst);

    ASSERT_ASM_CPP_MEM_EQ(src_init, cpp_src, sizeof(cpp_src), label);
    ASSERT_ASM_CPP_MEM_EQ(src_init, asm_src, sizeof(asm_src), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), label);
}

static void test_equivalence(void)
{
    assert_copyblock_case("CopyBlock fixed simple",
                          10, 10, 20, 20, 50, 50,
                          0, 0, 639, 479, 1u);
    assert_copyblock_case("CopyBlock fixed clipped source",
                          -8, -3, 12, 6, 40, 20,
                          0, 0, 639, 479, 2u);
    assert_copyblock_case("CopyBlock fixed clipped destination",
                          20, 20, 80, 45, 610, 460,
                          0, 0, 639, 479, 3u);
    assert_copyblock_case("CopyBlock fixed custom clip",
                          50, 30, 120, 70, 180, 100,
                          64, 32, 255, 191, 4u);
    assert_copyblock_case("CopyBlock fixed outside clip",
                          100, 100, 140, 120, 20, 20,
                          200, 200, 300, 260, 5u);
}

static void test_random_equivalence(void)
{
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 120; ++i) {
        S32 x0 = (S32)(rng_next() % 760u) - 60;
        S32 y0 = (S32)(rng_next() % 560u) - 40;
        S32 x1 = (S32)(rng_next() % 760u) - 60;
        S32 y1 = (S32)(rng_next() % 560u) - 40;
        S32 xd = (S32)(rng_next() % 760u) - 60;
        S32 yd = (S32)(rng_next() % 560u) - 40;
        S32 clip_x_min = (S32)(rng_next() % 320u);
        S32 clip_y_min = (S32)(rng_next() % 240u);
        S32 clip_x_max = clip_x_min + (S32)(rng_next() % (640u - (U32)clip_x_min));
        S32 clip_y_max = clip_y_min + (S32)(rng_next() % (480u - (U32)clip_y_min));
        char label[64];

        if (x0 > x1) {
            S32 tmp = x0;
            x0 = x1;
            x1 = tmp;
        }
        if (y0 > y1) {
            S32 tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        snprintf(label, sizeof(label), "CopyBlock rand %d", i);
        assert_copyblock_case(label, x0, y0, x1, y1, xd, yd,
                              clip_x_min, clip_y_min, clip_x_max, clip_y_max,
                              (U32)i + 100u);
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
