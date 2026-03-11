/* Test: AffString — display string at screen position using embedded Font8x8 */
#include "test_harness.h"
#include <SVGA/AFFSTR.H>
#include <SVGA/SCREEN.H>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
    TextPaper = 0xFF;  /* 0xFF = transparent paper (no background fill) */
    asm_SizeChar = 8;
    asm_TextInk = 0xFF;
    asm_TextPaper = 0xFF;
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

static void test_asm_equiv_hello(void)
{
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];

    setup_screen();
    AffString(10, 10, (char *)"Hello");
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    asm_AffString(10, 10, (char *)"Hello");
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "AffString Hello");
}

static void test_asm_equiv_special(void)
{
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];

    setup_screen();
    AffString(50, 50, (char *)"09!@#");
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    asm_AffString(50, 50, (char *)"09!@#");
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "AffString special chars");
}

static void test_asm_equiv_at_origin(void)
{
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];

    setup_screen();
    AffString(0, 0, (char *)"AB");
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    asm_AffString(0, 0, (char *)"AB");
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "AffString at origin");
}

int main(void)
{
    RUN_TEST(test_cpp_renders_char);
    RUN_TEST(test_empty_string);
    RUN_TEST(test_asm_equiv_hello);
    RUN_TEST(test_asm_equiv_special);
    RUN_TEST(test_asm_equiv_at_origin);
    TEST_SUMMARY();
    return test_failures != 0;
}
