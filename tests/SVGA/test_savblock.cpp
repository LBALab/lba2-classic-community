/* Test: SaveBlock — save screen region to buffer */
#include "test_harness.h"
#include <SVGA/SAVBLOCK.H>
#include <SVGA/RESBLOCK.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_SaveBlock(void *screen, void *buffer, S32 x0, S32 y0, S32 x1, S32 y1);
#endif

static U8 screen[640 * 480];
static U8 buffer[640 * 480];

static void setup(void)
{
    ModeDesiredX = 640; ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++) TabOffLine[i] = i * 640;
    memset(screen, 0, sizeof(screen));
    memset(buffer, 0, sizeof(buffer));
}

static void test_save_and_restore(void)
{
    setup();
    /* Draw something on screen */
    for (int y = 10; y <= 20; y++)
        for (int x = 10; x <= 20; x++)
            screen[y * 640 + x] = 0xAB;
    SaveBlock(screen, buffer, 10, 10, 20, 20);
    /* Clear screen */
    memset(screen, 0, sizeof(screen));
    RestoreBlock(screen, buffer, 10, 10, 20, 20);
    /* Verify restored */
    ASSERT_EQ_UINT(0xAB, screen[15 * 640 + 15]);
}

int main(void)
{
    RUN_TEST(test_save_and_restore);
    TEST_SUMMARY();
    return test_failures != 0;
}
