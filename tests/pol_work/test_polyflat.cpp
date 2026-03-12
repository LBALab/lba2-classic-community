/* Test: Filler_Flat — flat polygon scanline filler.
 *
 * ASM Filler_Flat uses register calling convention:
 *   ECX = nbLines, EBX = fillCurXMin (16.16 fixed), EDX = fillCurXMax (16.16 fixed)
 * Reads globals: Fill_CurY, Fill_CurOffLine, Fill_Patch, Fill_LeftSlope,
 *   Fill_RightSlope, ScreenPitch, Fill_Color
 * Writes: Fill_CurXMin, Fill_CurXMax, Fill_CurOffLine, Fill_CurY
 * Then calls Triangle_ReadNextEdge (linked to CPP version).
 *
 * We test by setting up all globals, calling both versions on identical
 * state, and comparing the framebuffer output.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYFLAT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

/* ASM filler — register calling convention */
extern "C" S32 asm_Filler_Flat(void);

static inline S32 call_asm_Filler_Flat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    S32 result;
    __asm__ __volatile__(
        "call asm_Filler_Flat"
        : "=a"(result), "+b"(fillCurXMin), "+c"(nbLines), "+d"(fillCurXMax)
        :
        : "edi", "esi", "ebp", "memory", "cc"
    );
    return result;
}

static U8 cpp_buf[TEST_POLY_SIZE];
static U8 asm_buf[TEST_POLY_SIZE];

/* Set up minimal state for a single Filler_Flat call covering a horizontal strip.
 * startY = first scanline, nbLines = number of scanlines to fill.
 * xmin/xmax are in 16.16 fixed-point. */
static void setup_filler(U32 startY, U32 xmin_fp, U32 xmax_fp)
{
    setup_polygon_screen();
    Fill_CurY = startY;
    Fill_CurOffLine = (PTR_U8)((U8 *)Log + TabOffLine[startY]);
    Fill_CurXMin = xmin_fp;
    Fill_CurXMax = xmax_fp;
    Fill_LeftSlope = 0;
    Fill_RightSlope = 0;
    Fill_Patch = 0;
    Fill_Color.Num = 0xFF;
    /* Fill_Filler is set but not used directly here */
    Fill_Filler = Filler_Flat;
    Fill_ClipFlag = 0;
    /* Prevent Triangle_ReadNextEdge from reading garbage.
     * Set up minimal point list so it returns gracefully. */
    Fill_ReadFlag = 0;
}

/* ── CPP-only sanity checks ────────────────────────────────────── */

static void test_cpp_horizontal_strip(void)
{
    setup_filler(50, 10 << 16, 60 << 16);
    Filler_Flat(4, 10 << 16, 60 << 16);
    /* 5 scanlines (50..54) from x=10..59 should be filled with 0xFF */
    for (int y = 50; y <= 54; y++)
        ASSERT_EQ_UINT(0xFF, g_poly_framebuf[y * TEST_POLY_W + 30]);
    /* Pixel outside should be 0 */
    ASSERT_EQ_UINT(0, g_poly_framebuf[50 * TEST_POLY_W + 5]);
}

static void test_cpp_single_scanline(void)
{
    setup_filler(20, 5 << 16, 15 << 16);
    Filler_Flat(0, 5 << 16, 15 << 16);
    /* 1 scanline at y=20 from x=5..14 */
    ASSERT_EQ_UINT(0xFF, g_poly_framebuf[20 * TEST_POLY_W + 5]);
    ASSERT_EQ_UINT(0xFF, g_poly_framebuf[20 * TEST_POLY_W + 14]);
}

static void test_cpp_with_slope(void)
{
    setup_filler(40, 20 << 16, 40 << 16);
    /* Left slope: +1 pixel/line, right slope: -1 pixel/line → narrows */
    Fill_LeftSlope = 1 << 16;
    Fill_RightSlope = -(1 << 16);
    Filler_Flat(9, 20 << 16, 40 << 16);
    /* First line: x=20..39, last line (y=49): x=29..30 (or thereabouts) */
    ASSERT_EQ_UINT(0xFF, g_poly_framebuf[40 * TEST_POLY_W + 30]);
}

/* ── ASM-vs-CPP equivalence ────────────────────────────────────── */

static void test_asm_equiv_strip(void)
{
    setup_filler(50, 10 << 16, 60 << 16);
    Filler_Flat(4, 10 << 16, 60 << 16);
    memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_filler(50, 10 << 16, 60 << 16);
    call_asm_Filler_Flat(4, 10 << 16, 60 << 16);
    memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Filler_Flat strip");
}

static void test_asm_equiv_single(void)
{
    setup_filler(20, 5 << 16, 30 << 16);
    Filler_Flat(0, 5 << 16, 30 << 16);
    memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_filler(20, 5 << 16, 30 << 16);
    call_asm_Filler_Flat(0, 5 << 16, 30 << 16);
    memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Filler_Flat single");
}

static void test_asm_equiv_slope(void)
{
    setup_filler(30, 20 << 16, 80 << 16);
    Fill_LeftSlope = (1 << 16) / 2;
    Fill_RightSlope = -(1 << 16) / 2;
    Filler_Flat(19, 20 << 16, 80 << 16);
    memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_filler(30, 20 << 16, 80 << 16);
    Fill_LeftSlope = (1 << 16) / 2;
    Fill_RightSlope = -(1 << 16) / 2;
    call_asm_Filler_Flat(19, 20 << 16, 80 << 16);
    memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Filler_Flat slope");
}

/* ── Randomized stress test ────────────────────────────────────── */

static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

static void test_asm_equiv_random(void)
{
    rng_seed(0xDEADBEEF);
    for (int i = 0; i < 20; i++) {
        U32 startY = rng_next() % (TEST_POLY_H - 20);
        U32 nbLines = rng_next() % 15;
        if (startY + nbLines >= TEST_POLY_H) nbLines = TEST_POLY_H - startY - 2;
        U32 xmin = (rng_next() % (TEST_POLY_W / 2)) << 16;
        U32 xmax = xmin + ((rng_next() % (TEST_POLY_W / 2) + 1) << 16);
        U8 color = (U8)(rng_next() | 1);

        setup_filler(startY, xmin, xmax);
        Fill_Color.Num = color;
        Filler_Flat(nbLines, xmin, xmax);
        memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_filler(startY, xmin, xmax);
        Fill_Color.Num = color;
        call_asm_Filler_Flat(nbLines, xmin, xmax);
        memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Filler_Flat random");
    }
}

int main(void)
{
    RUN_TEST(test_cpp_horizontal_strip);
    RUN_TEST(test_cpp_single_scanline);
    RUN_TEST(test_cpp_with_slope);
    RUN_TEST(test_asm_equiv_strip);
    RUN_TEST(test_asm_equiv_single);
    RUN_TEST(test_asm_equiv_slope);
    RUN_TEST(test_asm_equiv_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
