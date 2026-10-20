/* Test: AffString — display string at screen position using embedded Font8x8 */
#include "test_harness.h"
#include <SVGA/AFFSTR.H>
#include <SVGA/SCREEN.H>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern "C" void asm_AffString(S32 x, S32 y, char *str);
extern "C" S32 asm_SizeChar;
extern "C" U8 asm_TextInk;
extern "C" U8 asm_TextPaper;

static U8 framebuf[640 * 480];

static int count_ink_pixels(S32 x0, S32 y0, S32 width, S32 height, U8 ink) {
    int count = 0;
    for (S32 y = y0; y < y0 + height; ++y) {
        for (S32 x = x0; x < x0 + width; ++x) {
            if (framebuf[y * 640 + x] == ink)
                count++;
        }
    }
    return count;
}

static void setup_screen(void) {
    memset(framebuf, 0, sizeof(framebuf));
    Log = framebuf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    SizeChar = 8;
    TextInk = 0xFF;
    TextPaper = 0xFF; /* 0xFF = transparent paper (no background fill) */
    asm_SizeChar = 8;
    asm_TextInk = 0xFF;
    asm_TextPaper = 0xFF;
}

static void test_asm_equiv_single_char_exact(void) {
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];

    setup_screen();
    AffString(0, 0, (char *)"A");
    ASSERT_EQ_INT(28, count_ink_pixels(0, 0, 8, 8, 0xFF));
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    asm_AffString(0, 0, (char *)"A");
    ASSERT_EQ_INT(28, count_ink_pixels(0, 0, 8, 8, 0xFF));
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "AffString single char exact");
}

static void test_empty_string(void) {
    setup_screen();
    AffString(10, 10, (char *)"");
    int nonzero = 0;
    for (int y = 10; y < 18; y++)
        for (int x = 10; x < 26; x++)
            if (framebuf[y * 640 + x] != 0)
                nonzero++;
    ASSERT_EQ_INT(0, nonzero);
}

static void test_asm_equiv_hello(void) {
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

static void test_asm_equiv_special(void) {
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

static void test_asm_equiv_at_origin(void) {
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
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

/* Build a random printable string of length 1..maxlen into buf (NUL-terminated).
   Characters are in the ASCII range 0x20..0x7E. */
static void rng_string(char *buf, int maxlen) {
    int len = (int)(rng_next() % (U32)maxlen) + 1;
    for (int i = 0; i < len; i++)
        buf[i] = (char)(0x20 + rng_next() % 95); /* 0x20 .. 0x7E */
    buf[len] = '\0';
}

/* Single randomized round: pick random (x,y), random string, compare ASM vs CPP */
static int random_round(int idx) {
    char str[32];
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];
    char label[128];

    /* Random position — keep inside safe screen area */
    S32 x = (S32)(rng_next() % 500); /* 0 .. 499 */
    S32 y = (S32)(rng_next() % 460); /* 0 .. 459 */

    /* Random ink/paper */
    U8 ink = (U8)(rng_next() & 0xFF);
    U8 paper = (rng_next() % 4 == 0) ? 0xFF : (U8)(rng_next() & 0xFF);

    rng_string(str, 16);

    /* CPP */
    setup_screen();
    TextInk = ink;
    TextPaper = paper;
    asm_TextInk = ink;
    asm_TextPaper = paper;
    AffString(x, y, str);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    /* ASM */
    setup_screen();
    TextInk = ink;
    TextPaper = paper;
    asm_TextInk = ink;
    asm_TextPaper = paper;
    asm_AffString(x, y, str);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    snprintf(label, sizeof(label), "random #%d (%d,%d) ink=0x%02X paper=0x%02X \"%s\"",
             idx, (int)x, (int)y, ink, paper, str);
    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), label);
    return test_failures;
}

static void test_random_batch(void) {
    rng_seed(0xDEADBEEF);
    int prev_failures = test_failures;
    for (int i = 0; i < 100; i++) {
        random_round(i);
        if (test_failures != prev_failures)
            return; /* stop on first failure so output stays readable */
    }
}

/* Extended chars including the patched 0xC6 and 0xE4 entries */
static void test_asm_equiv_extended_chars(void) {
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];

    /* String with chars around the patched positions (0xC6 and 0xE4) */
    char str[] = {(char)0xC5, (char)0xC6, (char)0xC7, (char)0xE3,
                  (char)0xE4, (char)0xE5, '\0'};

    setup_screen();
    AffString(20, 20, str);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    asm_AffString(20, 20, str);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "AffString extended 0xC5-0xE5");
}

/* Solid paper (non-transparent) */
static void test_asm_equiv_with_paper(void) {
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];

    setup_screen();
    TextInk = 0x0F;
    TextPaper = 0x01;
    asm_TextInk = 0x0F;
    asm_TextPaper = 0x01;
    AffString(100, 100, (char *)"Paper");
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    TextInk = 0x0F;
    TextPaper = 0x01;
    asm_TextInk = 0x0F;
    asm_TextPaper = 0x01;
    asm_AffString(100, 100, (char *)"Paper");
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "AffString with paper color");
}

/* ---------- on-screen edge sweep ----------
 * AffString does not clip, in AFFSTR.ASM or AFFSTR.CPP: it writes 8x8 cells
 * from Log + TabOffLine[y] + x on, so off-screen geometry is out of contract
 * (negative y even indexes TabOffLine[-1]). Its one caller, GraphPrintf
 * (GPRINTF.CPP), rejects y < ClipYMin, y > ClipYMax - 7 and x > ClipXMax and
 * cuts the string at ClipXMax before calling it. So this sweep stays on
 * screen, right up to the last row and column, with the framebuffer between
 * guard rows so a write past either end of it shows. */

#define AFF_GUARD_ROWS 2
#define AFF_GUARD_BYTE 0xEE
#define AFF_GUARDED_SIZE (640 * (480 + 2 * AFF_GUARD_ROWS))

static U8 aff_guarded[AFF_GUARDED_SIZE];
static U8 aff_guarded_cpp[AFF_GUARDED_SIZE];

static void setup_guarded(U8 ink, U8 paper) {
    setup_screen();
    memset(aff_guarded, AFF_GUARD_BYTE, sizeof(aff_guarded));
    memset(aff_guarded + 640 * AFF_GUARD_ROWS, 0, 640 * 480);
    Log = aff_guarded + 640 * AFF_GUARD_ROWS;
    TextInk = asm_TextInk = ink;
    TextPaper = asm_TextPaper = paper;
}

static int aff_guards_intact(const U8 *buf) {
    for (int i = 0; i < 640 * AFF_GUARD_ROWS; i++)
        if (buf[i] != AFF_GUARD_BYTE || buf[AFF_GUARDED_SIZE - 1 - i] != AFF_GUARD_BYTE)
            return 0;
    return 1;
}

/* Returns 1 on match; with expect_guards, also requires both sides to leave
 * the guard rows alone. */
static int aff_guarded_case(S32 x, S32 y, const char *str, U8 ink, U8 paper, int expect_guards) {
    char label[160];
    int prev = test_failures;
    snprintf(label, sizeof(label), "AffString (%d,%d) paper=0x%02X \"%s\"", (int)x, (int)y, paper, str);

    setup_guarded(ink, paper);
    AffString(x, y, (char *)str);
    memcpy(aff_guarded_cpp, aff_guarded, sizeof(aff_guarded_cpp));

    setup_guarded(ink, paper);
    asm_AffString(x, y, (char *)str);

    ASSERT_ASM_CPP_MEM_EQ(aff_guarded, aff_guarded_cpp, sizeof(aff_guarded), label);
    if (expect_guards) {
        ASSERT_TRUE(aff_guards_intact(aff_guarded));
        ASSERT_TRUE(aff_guards_intact(aff_guarded_cpp));
    }
    return test_failures == prev;
}

/* 0xDB is a solid cell, so the first and last column and row of every cell
 * are inked; '_' inks only the bottom row; ' ' inks nothing (paper only). */
static const char *const aff_strings[] = {
    "\xDB",
    "\xDB_ \xDB",
    "Hello, World! \xDB",
    "\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB",
};

static void test_onscreen_edge_sweep(void) {
    static const U8 papers[2] = {0xFF, 0x11};
    int cases = 0;
    for (size_t si = 0; si < sizeof(aff_strings) / sizeof(aff_strings[0]); si++) {
        const char *str = aff_strings[si];
        S32 w = (S32)strlen(str) * 8;
        for (int p = 0; p < 2; p++) {
            U8 ink = (U8)(0xA0 + si), paper = papers[p];
            /* left and right edges at mid height: first/last column 0..16 px in */
            for (S32 o = 0; o <= 16; o++) {
                if (!aff_guarded_case(o, 236, str, ink, paper, 1))
                    return;
                if (!aff_guarded_case(640 - w - o, 236, str, ink, paper, 1))
                    return;
                cases += 2;
            }
            /* top and bottom edges at mid width: the cell's last row lands on 479 */
            for (S32 o = 0; o <= 16; o++) {
                if (!aff_guarded_case(320 - w / 2, o, str, ink, paper, 1))
                    return;
                if (!aff_guarded_case(320 - w / 2, 472 - o, str, ink, paper, 1))
                    return;
                cases += 2;
            }
            /* corner diagonals */
            for (S32 o = 0; o <= 16; o++) {
                if (!aff_guarded_case(o, o, str, ink, paper, 1))
                    return;
                if (!aff_guarded_case(640 - w - o, o, str, ink, paper, 1))
                    return;
                if (!aff_guarded_case(o, 472 - o, str, ink, paper, 1))
                    return;
                if (!aff_guarded_case(640 - w - o, 472 - o, str, ink, paper, 1))
                    return;
                cases += 4;
            }
        }
    }
    /* the bottom-right cell is the very last 8x8 of the framebuffer */
    if (!aff_guarded_case(632, 472, "\xDB", 0x42, 0x11, 1))
        return;
    int inked = 0;
    for (int i = 0; i < 640 * 480; i++)
        inked += aff_guarded[640 * AFF_GUARD_ROWS + i] == 0x42;
    ASSERT_EQ_INT(64, inked);
    ASSERT_EQ_INT(0x42, aff_guarded[640 * AFF_GUARD_ROWS + 640 * 480 - 1]);
    printf("  on-screen edge sweep: %d cases\n", cases + 1);
}

/* GraphPrintf's own clip can still hand AffString a string that runs past
 * ClipXMax: with x < ClipXMin it drops (ClipXMin - x) / 8 characters and moves
 * x up to ClipXMin, so the end moves right by (ClipXMin - x) % 8 (up to 7 px)
 * after the length was cut to fit. With ClipXMax = 639 and y = ClipYMax - 7 =
 * 472 the last row runs past the end of a 640x480 framebuffer. Both sides
 * write the same bytes; this checks they agree, guard rows included, and
 * reports how far they went. Example: GraphPrintf(.., 0, 472, <80 chars>)
 * with ClipXMin = 5 -> AffString(5, 472, <80 chars>). */
static void test_graphprintf_overrun_shape(void) {
    char str[81];
    memset(str, (char)0xDB, 80);
    str[80] = 0;
    if (!aff_guarded_case(5, 472, str, 0x42, 0xFF, 0))
        return;
    int over = 0;
    for (int i = 0; i < 640 * AFF_GUARD_ROWS; i++)
        over += aff_guarded[AFF_GUARDED_SIZE - 640 * AFF_GUARD_ROWS + i] != AFF_GUARD_BYTE;
    printf("  GraphPrintf-shaped (5,472) x80: %d bytes past the framebuffer, ASM and C++ alike\n", over);
    ASSERT_EQ_INT(5, over);
}

int main(void) {
    RUN_TEST(test_asm_equiv_single_char_exact);
    RUN_TEST(test_empty_string);
    RUN_TEST(test_asm_equiv_hello);
    RUN_TEST(test_asm_equiv_special);
    RUN_TEST(test_asm_equiv_at_origin);
    RUN_TEST(test_asm_equiv_extended_chars);
    RUN_TEST(test_asm_equiv_with_paper);
    RUN_TEST(test_random_batch);
    RUN_TEST(test_onscreen_edge_sweep);
    RUN_TEST(test_graphprintf_overrun_shape);
    TEST_SUMMARY();
    return test_failures != 0;
}
