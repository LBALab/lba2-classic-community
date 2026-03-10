/* Test: Box — draw rectangle outline with clipping */
#include "test_harness.h"

#include <SVGA/BOX.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <string.h>
#include <stdlib.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_Box(S32 x0, S32 y0, S32 x1, S32 y1, S32 col);
#endif

static U8 framebuf[640 * 480];

static void setup_screen(void)
{
    memset(framebuf, 0, sizeof(framebuf));
    Log = framebuf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++) TabOffLine[i] = i * 640;
    ClipXMin = 0; ClipYMin = 0; ClipXMax = 639; ClipYMax = 479;
}

static void test_simple_box(void)
{
    setup_screen();
    Box(10, 10, 20, 20, 0xFF);
    /* Top-left corner pixel should be set */
    ASSERT_EQ_UINT(0xFF, framebuf[10 * 640 + 10]);
    /* Verify the box drew something in the region */
    ASSERT_EQ_UINT(0xFF, framebuf[10 * 640 + 15]);
}

static void test_clipped_box(void)
{
    setup_screen();
    /* Box partially outside clip region */
    Box(-5, -5, 5, 5, 0xAA);
    /* Pixels at (0,0) should be set (clamped to clip) */
    ASSERT_EQ_UINT(0xAA, framebuf[0]);
}

static void test_zero_size_box(void)
{
    setup_screen();
    Box(100, 100, 100, 100, 0x42);
    /* Single pixel box */
    ASSERT_EQ_UINT(0x42, framebuf[100 * 640 + 100]);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    U8 cpp_buf[640 * 480], asm_buf[640 * 480];

    setup_screen();
    Box(10, 10, 50, 50, 0xCC);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    Log = framebuf;
    asm_Box(10, 10, 50, 50, 0xCC);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "Box");
}
#endif

int main(void)
{
    RUN_TEST(test_simple_box);
    RUN_TEST(test_clipped_box);
    RUN_TEST(test_zero_size_box);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
