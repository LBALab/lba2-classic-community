/* Test: ClearBox / SetClearColor — clear rectangular region */
#include "test_harness.h"

#include <SVGA/CLRBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

extern "C" void asm_ClearBox(void *dst, U32 *TabOffDst, T_BOX *box);
extern "C" void asm_SetClearColor(U32 color);

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

static void test_set_clear_color(void)
{
    SetClearColor(0x42);
    /* Just verify it doesn't crash; the color is used by ClearBox internally */
    ASSERT_TRUE(1);
}

static void test_clear_box_region(void)
{
    setup_screen();
    memset(framebuf, 0xAA, sizeof(framebuf));
    SetClearColor(0x00);
    T_BOX box = {10, 10, 20, 20, NULL};
    ClearBox(framebuf, TabOffLine, &box);
    /* Interior should be cleared to 0 */
    ASSERT_EQ_UINT(0x00, framebuf[15 * 640 + 15]);
    /* Outside should remain 0xAA */
    ASSERT_EQ_UINT(0xAA, framebuf[0]);
}

static void test_asm_equiv(void)
{
    U8 cpp_buf[640 * 480], asm_buf[640 * 480];
    setup_screen();

    memset(framebuf, 0xFF, sizeof(framebuf));
    SetClearColor(0x00);
    T_BOX box = {5, 5, 30, 30, NULL};
    ClearBox(framebuf, TabOffLine, &box);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    memset(framebuf, 0xFF, sizeof(framebuf));
    asm_SetClearColor(0x00);
    asm_ClearBox(framebuf, TabOffLine, &box);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "ClearBox");
}

int main(void)
{
    RUN_TEST(test_set_clear_color);
    RUN_TEST(test_clear_box_region);
    RUN_TEST(test_asm_equiv);
    TEST_SUMMARY();
    return test_failures != 0;
}
