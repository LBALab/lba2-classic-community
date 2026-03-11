/* Test: RestoreBlock — restore saved screen region */
#include "test_harness.h"
#include <SVGA/RESBLOCK.H>
#include <SVGA/SAVBLOCK.H>
#include <SVGA/SCREEN.H>
#include <string.h>

extern "C" void asm_RestoreBlock(void *screen, void *buffer, S32 x0, S32 y0, S32 x1, S32 y1);

static U8 screen_buf[640 * 480];
static U8 save_buf[640 * 480];

static void setup(void)
{
    ModeDesiredX = 640; ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++) TabOffLine[i] = i * 640;
    memset(screen_buf, 0, sizeof(screen_buf));
    memset(save_buf, 0, sizeof(save_buf));
}

static void test_restore_simple(void)
{
    setup();
    /* Paint a pattern on screen, save it, clear screen, restore */
    for (int y = 10; y <= 30; y++)
        for (int x = 10; x <= 30; x++)
            screen_buf[y * 640 + x] = (U8)((x + y) & 0xFF);
    SaveBlock(screen_buf, save_buf, 10, 10, 30, 30);
    memset(screen_buf, 0, sizeof(screen_buf));
    RestoreBlock(screen_buf, save_buf, 10, 10, 30, 30);
    /* Verify a few pixels */
    ASSERT_EQ_UINT((10 + 10) & 0xFF, screen_buf[10 * 640 + 10]);
    ASSERT_EQ_UINT((20 + 20) & 0xFF, screen_buf[20 * 640 + 20]);
    ASSERT_EQ_UINT((30 + 30) & 0xFF, screen_buf[30 * 640 + 30]);
}

static void test_restore_single_pixel(void)
{
    setup();
    screen_buf[50 * 640 + 50] = 0xAB;
    SaveBlock(screen_buf, save_buf, 50, 50, 50, 50);
    screen_buf[50 * 640 + 50] = 0;
    RestoreBlock(screen_buf, save_buf, 50, 50, 50, 50);
    ASSERT_EQ_UINT(0xAB, screen_buf[50 * 640 + 50]);
}

static void test_restore_full_width(void)
{
    setup();
    for (int x = 0; x < 640; x++)
        screen_buf[0 * 640 + x] = (U8)(x & 0xFF);
    SaveBlock(screen_buf, save_buf, 0, 0, 639, 0);
    memset(screen_buf, 0, 640);
    RestoreBlock(screen_buf, save_buf, 0, 0, 639, 0);
    ASSERT_EQ_UINT(0, screen_buf[0]);
    ASSERT_EQ_UINT(100 & 0xFF, screen_buf[100]);
    ASSERT_EQ_UINT(639 & 0xFF, screen_buf[639]);
}

static void test_asm_equiv(void)
{
    U8 cpp_screen[640 * 480];
    U8 asm_screen[640 * 480];
    U8 buf[640 * 480];

    setup();
    /* Paint a pattern */
    for (int y = 5; y <= 25; y++)
        for (int x = 5; x <= 25; x++)
            screen_buf[y * 640 + x] = (U8)(x * y);
    SaveBlock(screen_buf, buf, 5, 5, 25, 25);

    /* CPP RestoreBlock */
    memset(cpp_screen, 0, sizeof(cpp_screen));
    RestoreBlock(cpp_screen, buf, 5, 5, 25, 25);

    /* ASM RestoreBlock */
    memset(asm_screen, 0, sizeof(asm_screen));
    asm_RestoreBlock(asm_screen, buf, 5, 5, 25, 25);

    ASSERT_ASM_CPP_MEM_EQ(asm_screen, cpp_screen, sizeof(cpp_screen), "RestoreBlock");
}

static void test_asm_equiv_large(void)
{
    U8 cpp_screen[640 * 480];
    U8 asm_screen[640 * 480];
    U8 buf[640 * 480];

    setup();
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++)
            screen_buf[y * 640 + x] = (U8)((x ^ y) & 0xFF);
    SaveBlock(screen_buf, buf, 0, 0, 639, 479);

    memset(cpp_screen, 0, sizeof(cpp_screen));
    RestoreBlock(cpp_screen, buf, 0, 0, 639, 479);

    memset(asm_screen, 0, sizeof(asm_screen));
    asm_RestoreBlock(asm_screen, buf, 0, 0, 639, 479);

    ASSERT_ASM_CPP_MEM_EQ(asm_screen, cpp_screen, sizeof(cpp_screen), "RestoreBlock full-screen");
}

int main(void)
{
    RUN_TEST(test_restore_simple);
    RUN_TEST(test_restore_single_pixel);
    RUN_TEST(test_restore_full_width);
    RUN_TEST(test_asm_equiv);
    RUN_TEST(test_asm_equiv_large);
    TEST_SUMMARY();
    return test_failures != 0;
}
