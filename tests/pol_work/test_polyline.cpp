/* Test: Line — line rendering with clipping.
 *
 * Line() in ASM uses C-adapted calling convention:
 *   Line PROC USES esi edi ebp ebx, x0:DWORD, y0:DWORD, x1:DWORD, y1:DWORD, coul:DWORD
 */
#include "test_harness.h"
#include <SVGA/FIL_LINE.H>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

extern "C" void asm_Line(S32 x0, S32 y0, S32 x1, S32 y1, S32 col);

static U8 cpp_buf[TEST_POLY_SIZE];
static U8 asm_buf[TEST_POLY_SIZE];

/* ── CPP-only sanity checks ────────────────────────────────────── */

static void test_horizontal(void)
{
    setup_polygon_screen();
    Line(10, 50, 60, 50, 0xAA);
    /* Pixels along y=50 from x=10..60 should be set */
    ASSERT_EQ_UINT(0xAA, g_poly_framebuf[50 * TEST_POLY_W + 10]);
    ASSERT_EQ_UINT(0xAA, g_poly_framebuf[50 * TEST_POLY_W + 35]);
}

static void test_vertical(void)
{
    setup_polygon_screen();
    Line(80, 10, 80, 60, 0xBB);
    ASSERT_EQ_UINT(0xBB, g_poly_framebuf[10 * TEST_POLY_W + 80]);
    ASSERT_EQ_UINT(0xBB, g_poly_framebuf[35 * TEST_POLY_W + 80]);
}

static void test_single_pixel(void)
{
    setup_polygon_screen();
    Line(42, 42, 42, 42, 0xCC);
    ASSERT_EQ_UINT(0xCC, g_poly_framebuf[42 * TEST_POLY_W + 42]);
}

static void test_clipped(void)
{
    setup_polygon_screen();
    /* Line extending outside clip region */
    Line(-10, 50, 20, 50, 0xDD);
    /* Pixel at (0,50) should be set (clamped) */
    ASSERT_EQ_UINT(0xDD, g_poly_framebuf[50 * TEST_POLY_W + 0]);
    /* Nothing written at negative coords (no crash) */
}

static void test_fully_outside(void)
{
    setup_polygon_screen();
    Line(-50, -50, -10, -10, 0xEE);
    /* No pixels in visible area */
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

/* ── ASM-vs-CPP equivalence ────────────────────────────────────── */

static void test_asm_equiv_horizontal(void)
{
    setup_polygon_screen();
    Line(10, 50, 60, 50, 0xAA);
    memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_polygon_screen();
    asm_Line(10, 50, 60, 50, 0xAA);
    memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Line horizontal");
}

static void test_asm_equiv_diagonal(void)
{
    setup_polygon_screen();
    Line(10, 10, 80, 90, 0x55);
    memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_polygon_screen();
    asm_Line(10, 10, 80, 90, 0x55);
    memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Line diagonal");
}

static void test_asm_equiv_clipped(void)
{
    setup_polygon_screen();
    Line(-10, 50, 170, 50, 0x77);
    memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_polygon_screen();
    asm_Line(-10, 50, 170, 50, 0x77);
    memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Line clipped");
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
    for (int i = 0; i < 30; i++) {
        S32 x0 = (S32)(rng_next() % (TEST_POLY_W + 40)) - 20;
        S32 y0 = (S32)(rng_next() % (TEST_POLY_H + 40)) - 20;
        S32 x1 = (S32)(rng_next() % (TEST_POLY_W + 40)) - 20;
        S32 y1 = (S32)(rng_next() % (TEST_POLY_H + 40)) - 20;
        S32 col = (S32)(rng_next() & 0xFF);
        if (col == 0) col = 1;  /* avoid 0 which matches background */

        setup_polygon_screen();
        Line(x0, y0, x1, y1, col);
        memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_polygon_screen();
        asm_Line(x0, y0, x1, y1, col);
        memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Line random");
    }
}

int main(void)
{
    RUN_TEST(test_horizontal);
    RUN_TEST(test_vertical);
    RUN_TEST(test_single_pixel);
    RUN_TEST(test_clipped);
    RUN_TEST(test_fully_outside);
    RUN_TEST(test_asm_equiv_horizontal);
    RUN_TEST(test_asm_equiv_diagonal);
    RUN_TEST(test_asm_equiv_clipped);
    RUN_TEST(test_asm_equiv_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
