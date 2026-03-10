/* Test: CopyBlock — fast memory block copy between screen regions */
#include "test_harness.h"
#include <SVGA/CPYBLOCK.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_CopyBlock(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd, void *dst);
#endif

static U8 srcbuf[640 * 480];
static U8 dstbuf[640 * 480];

static void setup(void)
{
    ModeDesiredX = 640; ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++) TabOffLine[i] = i * 640;
    ClipXMin = 0; ClipYMin = 0; ClipXMax = 639; ClipYMax = 479;
    memset(srcbuf, 0, sizeof(srcbuf));
    memset(dstbuf, 0, sizeof(dstbuf));
}

static void test_simple_copy(void)
{
    setup();
    /* Fill a region in srcbuf */
    for (int y = 10; y <= 20; y++)
        for (int x = 10; x <= 20; x++)
            srcbuf[y * 640 + x] = 0xAB;
    CopyBlock(10, 10, 20, 20, srcbuf, 50, 50, dstbuf);
    /* Check that dst has the data at (50,50)-(60,60) */
    ASSERT_EQ_UINT(0xAB, dstbuf[50 * 640 + 50]);
}

static void test_zero_size(void)
{
    setup();
    CopyBlock(10, 10, 10, 10, srcbuf, 50, 50, dstbuf);
    ASSERT_TRUE(1); /* Should not crash */
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    setup();
    for (int y = 5; y <= 15; y++)
        for (int x = 5; x <= 15; x++)
            srcbuf[y * 640 + x] = 0xCD;

    U8 cpp_dst[640 * 480], asm_dst[640 * 480];
    memset(cpp_dst, 0, sizeof(cpp_dst));
    memset(asm_dst, 0, sizeof(asm_dst));
    CopyBlock(5, 5, 15, 15, srcbuf, 30, 30, cpp_dst);
    asm_CopyBlock(5, 5, 15, 15, srcbuf, 30, 30, asm_dst);
    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "CopyBlock");
}
#endif

int main(void)
{
    RUN_TEST(test_simple_copy);
    RUN_TEST(test_zero_size);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
