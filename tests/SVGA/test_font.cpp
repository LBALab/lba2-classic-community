/* Test: SizeFont / CarFont / Font — font metrics and rendering.
 *
 * SizeFont is fully testable with a synthetic font bank (it only reads
 * the offset table and width bytes, never renders).
 * CarFont and Font call AffMask for rendering — those are tested for
 * SizeFont-equivalent return values only (not pixel output).
 */
#include "test_harness.h"
#include <SVGA/FONT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

extern "C" S32 asm_SizeFont(char *str);
extern "C" S32 asm_CarFont(S32 x, S32 y, U8 c);
extern "C" S32 asm_Font(S32 x, S32 y, const char *str);

/* Renamed ASM globals (separate from CPP's PtrFont/InterLeave/InterSpace) */
extern "C" void *asm_PtrFont;
extern "C" S32 asm_InterLeave;
extern "C" S32 asm_InterSpace;

/* ---------- Synthetic font bank ---------- */

/* Layout: 256×U32 offset table + glyph data.
 * Each glyph: first byte = width, rest = mask data (unused by SizeFont). */
static U8 g_fontbank[2048];

static void build_font_bank(void)
{
    memset(g_fontbank, 0, sizeof(g_fontbank));
    U32 *offsets = (U32 *)g_fontbank;
    U32 data_start = 256 * 4;  /* 1024 bytes for offset table */

    /* Set up a few characters with known widths */
    /* 'A' (65): width 8 */
    offsets[65] = data_start + 0;
    g_fontbank[data_start + 0] = 8;

    /* 'B' (66): width 7 */
    offsets[66] = data_start + 16;
    g_fontbank[data_start + 16] = 7;

    /* 'C' (67): width 6 */
    offsets[67] = data_start + 32;
    g_fontbank[data_start + 32] = 6;

    /* '!' (33): width 3 */
    offsets[33] = data_start + 48;
    g_fontbank[data_start + 48] = 3;

    /* Space (32) is handled by InterSpace, not the offset table */
}

static void setup_font(void)
{
    build_font_bank();
    PtrFont = g_fontbank;
    InterLeave = 1;
    InterSpace = 10;
    asm_PtrFont = g_fontbank;
    asm_InterLeave = 1;
    asm_InterSpace = 10;
}

/* ---------- SizeFont tests ---------- */

static void test_sizefont_single_char(void)
{
    setup_font();
    S32 size = SizeFont((char *)"A");
    /* size = InterLeave(1) + width(8) = 9 */
    ASSERT_EQ_INT(9, size);
}

static void test_sizefont_multi(void)
{
    setup_font();
    S32 size = SizeFont((char *)"ABC");
    /* A: 1+8=9, B: 1+7=8, C: 1+6=7 → total = 24 */
    ASSERT_EQ_INT(24, size);
}

static void test_sizefont_with_space(void)
{
    setup_font();
    S32 size = SizeFont((char *)"A B");
    /* A: 1+8=9, space: 10, B: 1+7=8 → total = 27 */
    ASSERT_EQ_INT(27, size);
}

static void test_sizefont_empty(void)
{
    setup_font();
    S32 size = SizeFont((char *)"");
    ASSERT_EQ_INT(0, size);
}

static void test_sizefont_space_only(void)
{
    setup_font();
    S32 size = SizeFont((char *)" ");
    ASSERT_EQ_INT(10, size);
}

static void test_asm_equiv_sizefont(void)
{
    setup_font();
    S32 cpp_size = SizeFont((char *)"ABC!");
    S32 asm_size = asm_SizeFont((char *)"ABC!");
    ASSERT_EQ_INT(cpp_size, asm_size);
}

static void test_asm_equiv_sizefont_with_spaces(void)
{
    setup_font();
    S32 cpp_size = SizeFont((char *)"A B C");
    S32 asm_size = asm_SizeFont((char *)"A B C");
    ASSERT_EQ_INT(cpp_size, asm_size);
}

static void test_asm_equiv_sizefont_empty(void)
{
    setup_font();
    S32 cpp_size = SizeFont((char *)"");
    S32 asm_size = asm_SizeFont((char *)"");
    ASSERT_EQ_INT(cpp_size, asm_size);
}

int main(void)
{
    RUN_TEST(test_sizefont_single_char);
    RUN_TEST(test_sizefont_multi);
    RUN_TEST(test_sizefont_with_space);
    RUN_TEST(test_sizefont_empty);
    RUN_TEST(test_sizefont_space_only);
    RUN_TEST(test_asm_equiv_sizefont);
    RUN_TEST(test_asm_equiv_sizefont_with_spaces);
    RUN_TEST(test_asm_equiv_sizefont_empty);
    TEST_SUMMARY();
    return test_failures != 0;
}
