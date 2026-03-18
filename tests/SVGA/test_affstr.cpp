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

/* ---------- randomized stress tests ---------- */

/* Simple deterministic LCG so we don't depend on libc rand reproducibility */
static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

/* Build a random printable string of length 1..maxlen into buf (NUL-terminated).
   Characters are in the ASCII range 0x20..0x7E. */
static void rng_string(char *buf, int maxlen)
{
    int len = (int)(rng_next() % (U32)maxlen) + 1;
    for (int i = 0; i < len; i++)
        buf[i] = (char)(0x20 + rng_next() % 95);   /* 0x20 .. 0x7E */
    buf[len] = '\0';
}

/* Single randomized round: pick random (x,y), random string, compare ASM vs CPP */
static int random_round(int idx)
{
    char str[32];
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];
    char label[64];

    /* Random position — keep inside safe screen area */
    S32 x = (S32)(rng_next() % 500);        /* 0 .. 499 */
    S32 y = (S32)(rng_next() % 460);        /* 0 .. 459 */

    /* Random ink/paper */
    U8 ink   = (U8)(rng_next() & 0xFF);
    U8 paper = (rng_next() % 4 == 0) ? 0xFF : (U8)(rng_next() & 0xFF);

    rng_string(str, 16);

    /* CPP */
    setup_screen();
    TextInk = ink; TextPaper = paper;
    asm_TextInk = ink; asm_TextPaper = paper;
    AffString(x, y, str);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    /* ASM */
    setup_screen();
    TextInk = ink; TextPaper = paper;
    asm_TextInk = ink; asm_TextPaper = paper;
    asm_AffString(x, y, str);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    snprintf(label, sizeof(label), "random #%d (%d,%d) ink=0x%02X paper=0x%02X \"%s\"",
             idx, (int)x, (int)y, ink, paper, str);
    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), label);
    return test_failures;
}

static void test_random_batch(void)
{
    rng_seed(0xDEADBEEF);
    int prev_failures = test_failures;
    for (int i = 0; i < 50; i++) {
        random_round(i);
        if (test_failures != prev_failures)
            return; /* stop on first failure so output stays readable */
    }
}

/* Extended chars including the patched 0xC6 and 0xE4 entries */
static void test_asm_equiv_extended_chars(void)
{
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];

    /* String with chars around the patched positions (0xC6 and 0xE4) */
    char str[] = { (char)0xC5, (char)0xC6, (char)0xC7, (char)0xE3,
                   (char)0xE4, (char)0xE5, '\0' };

    setup_screen();
    AffString(20, 20, str);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    asm_AffString(20, 20, str);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "AffString extended 0xC5-0xE5");
}

/* Solid paper (non-transparent) */
static void test_asm_equiv_with_paper(void)
{
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];

    setup_screen();
    TextInk = 0x0F; TextPaper = 0x01;
    asm_TextInk = 0x0F; asm_TextPaper = 0x01;
    AffString(100, 100, (char *)"Paper");
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    TextInk = 0x0F; TextPaper = 0x01;
    asm_TextInk = 0x0F; asm_TextPaper = 0x01;
    asm_AffString(100, 100, (char *)"Paper");
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "AffString with paper color");
}

int main(void)
{
    RUN_TEST(test_cpp_renders_char);
    RUN_TEST(test_empty_string);
    RUN_TEST(test_asm_equiv_hello);
    RUN_TEST(test_asm_equiv_special);
    RUN_TEST(test_asm_equiv_at_origin);
    RUN_TEST(test_asm_equiv_extended_chars);
    RUN_TEST(test_asm_equiv_with_paper);
    RUN_TEST(test_random_batch);
    TEST_SUMMARY();
    return test_failures != 0;
}
