/* Test: AffString — display string at screen position using embedded Font8x8
 *
 * KNOWN DISCREPANCY: ASM and CPP AffString render glyphs differently.
 * ASM starts at (x + SizeChar), iterates bits right-to-left (mirrored).
 * CPP starts at x, iterates bits left-to-right (mask=0x80 >> shift).
 * Also: ASM Font8x8 has 2 modified entries (chars 0xC0, 0xCC) for accented chars.
 * These tests verify both versions render non-zero pixels, but do NOT compare
 * ASM vs CPP framebuffers due to the known rendering offset difference.
 * Marked [~] partial in ASM_VALIDATION_PROGRESS.md.
 */
#include "test_harness.h"
#include <SVGA/AFFSTR.H>
#include <SVGA/SCREEN.H>
#include <string.h>

extern "C" void asm_AffString(S32 x, S32 y, char *str);
extern "C" S32 asm_SizeChar;
extern "C" U8  asm_TextInk;
extern "C" U8  asm_TextPaper;

static U8 framebuf[640 * 480];

static void setup_screen(void)
{
    memset(framebuf, 0, sizeof(framebuf));
    Log = framebuf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++) TabOffLine[i] = i * 640;
    SizeChar = 8;
    TextInk = 0xFF;
    TextPaper = 0;
    asm_SizeChar = 8;
    asm_TextInk = 0xFF;
    asm_TextPaper = 0;
}

static void test_cpp_renders_char(void)
{
    setup_screen();
    AffString(0, 0, (char *)"A");
    int nonzero = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 16; x++)
            if (framebuf[y * 640 + x] == 0xFF) nonzero++;
    ASSERT_TRUE(nonzero > 0);
}

static void test_asm_renders_char(void)
{
    setup_screen();
    asm_AffString(0, 0, (char *)"A");
    int nonzero = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 16; x++)
            if (framebuf[y * 640 + x] == 0xFF) nonzero++;
    ASSERT_TRUE(nonzero > 0);
}

static void test_empty_string(void)
{
    setup_screen();
    AffString(10, 10, (char *)"");
    int nonzero = 0;
    for (int y = 10; y < 18; y++)
        for (int x = 10; x < 26; x++)
            if (framebuf[y * 640 + x] != 0) nonzero++;
    ASSERT_EQ_INT(0, nonzero);
}

static void test_cpp_renders_multi(void)
{
    setup_screen();
    AffString(10, 10, (char *)"Hello");
    int nonzero = 0;
    for (int y = 10; y < 18; y++)
        for (int x = 10; x < 60; x++)
            if (framebuf[y * 640 + x] == 0xFF) nonzero++;
    ASSERT_TRUE(nonzero > 5);
}

static void test_asm_renders_multi(void)
{
    setup_screen();
    asm_AffString(10, 10, (char *)"Hello");
    int nonzero = 0;
    for (int y = 10; y < 18; y++)
        for (int x = 10; x < 60; x++)
            if (framebuf[y * 640 + x] == 0xFF) nonzero++;
    ASSERT_TRUE(nonzero > 5);
}

int main(void)
{
    RUN_TEST(test_cpp_renders_char);
    RUN_TEST(test_asm_renders_char);
    RUN_TEST(test_empty_string);
    RUN_TEST(test_cpp_renders_multi);
    RUN_TEST(test_asm_renders_multi);
    TEST_SUMMARY();
    return test_failures != 0;
}
