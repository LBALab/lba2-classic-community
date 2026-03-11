/* Test: ScaleBox — scale rectangular region */
#include "test_harness.h"
#include <SVGA/SCALEBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

extern "C" void asm_ScaleBox(S32 xs0, S32 ys0, S32 xs1, S32 ys1, void *ptrs, S32 xd0, S32 yd0, S32 xd1, S32 yd1, void *ptrd);

/* ScaleBox reads TabOffLine[] for both source and destination Y indexing.
   We use a 640-wide virtual screen so TabOffLine[y] = y * 640. */
static U8 srcbuf[640 * 480];
static U8 dstbuf[640 * 480];

static void setup(void)
{
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++) TabOffLine[i] = i * 640;
    memset(srcbuf, 0, sizeof(srcbuf));
    memset(dstbuf, 0, sizeof(dstbuf));
}

static void test_same_size_copy(void)
{
    setup();
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++)
            srcbuf[y * 640 + x] = (U8)((x + y * 10) & 0xFF);
    ScaleBox(0, 0, 9, 9, srcbuf, 100, 100, 109, 109, dstbuf);
    /* Verify some pixels were written */
    int nonzero = 0;
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++)
            if (dstbuf[(100 + y) * 640 + 100 + x] != 0) nonzero++;
    ASSERT_TRUE(nonzero > 0);
}

static void test_asm_equiv_downscale(void)
{
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];

    setup();
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            srcbuf[y * 640 + x] = (U8)(y * 4 + x);

    memset(cpp_dst, 0, sizeof(cpp_dst));
    ScaleBox(0, 0, 3, 3, srcbuf, 0, 300, 1, 301, cpp_dst);

    memset(asm_dst, 0, sizeof(asm_dst));
    asm_ScaleBox(0, 0, 3, 3, srcbuf, 0, 300, 1, 301, asm_dst);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "ScaleBox downscale 4x4→2x2");
}

static void test_asm_equiv_same_size(void)
{
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];

    setup();
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++)
            srcbuf[y * 640 + x] = (U8)((x * 7 + y * 3) & 0xFF);

    memset(cpp_dst, 0, sizeof(cpp_dst));
    ScaleBox(0, 0, 19, 19, srcbuf, 50, 50, 69, 69, cpp_dst);

    memset(asm_dst, 0, sizeof(asm_dst));
    asm_ScaleBox(0, 0, 19, 19, srcbuf, 50, 50, 69, 69, asm_dst);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "ScaleBox same-size");
}

static void test_asm_equiv_upscale(void)
{
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];

    setup();
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++)
            srcbuf[y * 640 + x] = (U8)((x + y) & 0xFF);

    memset(cpp_dst, 0, sizeof(cpp_dst));
    ScaleBox(0, 0, 9, 9, srcbuf, 0, 100, 29, 129, cpp_dst);

    memset(asm_dst, 0, sizeof(asm_dst));
    asm_ScaleBox(0, 0, 9, 9, srcbuf, 0, 100, 29, 129, asm_dst);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "ScaleBox upscale 10x10→30x30");
}

int main(void)
{
    RUN_TEST(test_same_size_copy);
    RUN_TEST(test_asm_equiv_downscale);
    RUN_TEST(test_asm_equiv_same_size);
    RUN_TEST(test_asm_equiv_upscale);
    TEST_SUMMARY();
    return test_failures != 0;
}
