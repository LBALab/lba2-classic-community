/* Test: CopyBlockIncrust — copy block with incrustation (transparency) */
#include "test_harness.h"
#include <SVGA/CPYBLOCI.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

extern "C" void asm_CopyBlockIncrust(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd, void *dst);

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

static void test_copy_with_transparency(void)
{
    setup();
    /* Byte 0 = transparent in incrust mode */
    srcbuf[10 * 640 + 10] = 0xAB;
    srcbuf[10 * 640 + 11] = 0x00; /* transparent */
    dstbuf[50 * 640 + 50] = 0x99;
    dstbuf[50 * 640 + 51] = 0x99;
    CopyBlockIncrust(10, 10, 11, 10, srcbuf, 50, 50, dstbuf);
    ASSERT_EQ_UINT(0xAB, dstbuf[50 * 640 + 50]);
    /* Transparent pixel should preserve destination */
    ASSERT_EQ_UINT(0x99, dstbuf[50 * 640 + 51]);
}

int main(void)
{
    RUN_TEST(test_copy_with_transparency);
    TEST_SUMMARY();
    return test_failures != 0;
}
