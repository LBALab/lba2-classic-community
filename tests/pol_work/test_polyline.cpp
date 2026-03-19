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

void Line_ZBuffer_NZW(S32 x0, S32 y0, S32 x1, S32 y1, S32 col, S32 z1, S32 z2);
extern "C" void asm_Line(S32 x0, S32 y0, S32 x1, S32 y1, S32 col);
extern "C" void asm_LineZBufNZW(S32 x0, S32 y0, S32 z0,
                                S32 x1, S32 y1, S32 z1,
                                S32 col);
static U8 asm_buf[TEST_POLY_SIZE];

static U8 cpp_buf[TEST_POLY_SIZE];

#define LINEZBUFNZW_W 640
#define LINEZBUFNZW_H 480
#define LINEZBUFNZW_SIZE (LINEZBUFNZW_W * LINEZBUFNZW_H)

static U8 asm_linezbufnzw_buf[LINEZBUFNZW_SIZE];
static U8 cpp_linezbufnzw_buf[LINEZBUFNZW_SIZE];
static U8 linezbufnzw_framebuf[LINEZBUFNZW_SIZE + 256];
static U16 linezbufnzw_zbuffer[LINEZBUFNZW_SIZE];

static U8 *setup_linezbufnzw_screen(U8 color) {
    U8 *base = NULL;

    memset(linezbufnzw_framebuf, 0, sizeof(linezbufnzw_framebuf));
    for (U32 i = 0; i < LINEZBUFNZW_SIZE; i++)
        linezbufnzw_zbuffer[i] = 0xFFFF;

    for (int shift = 0; shift < 256; shift++) {
        U8 *candidate = linezbufnzw_framebuf + shift;
        if ((U8)(unsigned long)candidate != color) {
            base = candidate;
            break;
        }
    }

    ASSERT_TRUE(base != NULL);

    Log = base;
    PtrZBuffer = linezbufnzw_zbuffer;
    ModeDesiredX = LINEZBUFNZW_W;
    ModeDesiredY = LINEZBUFNZW_H;
    for (U32 i = 0; i < LINEZBUFNZW_H; i++)
        TabOffLine[i] = i * LINEZBUFNZW_W;
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = LINEZBUFNZW_W - 1;
    ClipYMax = LINEZBUFNZW_H - 1;
    ScreenPitch = LINEZBUFNZW_W;
    PTR_TabOffLine = TabOffLine;

    memset(Fill_Logical_Palette, 0, sizeof(Fill_Logical_Palette));
    Fill_Logical_Palette[302 & 0xFF] = color;
    Fill_Flag_Fog = TRUE;
    Fill_Flag_ZBuffer = TRUE;
    Fill_Flag_NZW = TRUE;

    return base;
}

/* ── CPP-only sanity checks ────────────────────────────────────── */

static void test_horizontal(void) {
    setup_polygon_screen();
    Line(10, 50, 60, 50, 0xAA);
    /* Pixels along y=50 from x=10..60 should be set */
    ASSERT_EQ_UINT(0xAA, g_poly_framebuf[50 * TEST_POLY_W + 10]);
    ASSERT_EQ_UINT(0xAA, g_poly_framebuf[50 * TEST_POLY_W + 35]);
}

static void test_vertical(void) {
    setup_polygon_screen();
    Line(80, 10, 80, 60, 0xBB);
    ASSERT_EQ_UINT(0xBB, g_poly_framebuf[10 * TEST_POLY_W + 80]);
    ASSERT_EQ_UINT(0xBB, g_poly_framebuf[35 * TEST_POLY_W + 80]);
}

static void test_single_pixel(void) {
    setup_polygon_screen();
    Line(42, 42, 42, 42, 0xCC);
    ASSERT_EQ_UINT(0xCC, g_poly_framebuf[42 * TEST_POLY_W + 42]);
}

static void test_clipped(void) {
    setup_polygon_screen();
    /* Line extending outside clip region */
    Line(-10, 50, 20, 50, 0xDD);
    /* Pixel at (0,50) should be set (clamped) */
    ASSERT_EQ_UINT(0xDD, g_poly_framebuf[50 * TEST_POLY_W + 0]);
    /* Nothing written at negative coords (no crash) */
}

static void test_fully_outside(void) {
    setup_polygon_screen();
    Line(-50, -50, -10, -10, 0xEE);
    /* No pixels in visible area */
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

/* ── ASM-vs-CPP equivalence ────────────────────────────────────── */

static void test_asm_equiv_horizontal(void) {
    setup_polygon_screen();
    Line(10, 50, 60, 50, 0xAA);
    memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_polygon_screen();
    asm_Line(10, 50, 60, 50, 0xAA);
    memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Line horizontal");
}

static void test_asm_equiv_diagonal(void) {
    setup_polygon_screen();
    Line(10, 10, 80, 90, 0x55);
    memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_polygon_screen();
    asm_Line(10, 10, 80, 90, 0x55);
    memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Line diagonal");
}

static void test_asm_equiv_clipped(void) {
    setup_polygon_screen();
    Line(-10, 50, 170, 50, 0x77);
    memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_polygon_screen();
    asm_Line(-10, 50, 170, 50, 0x77);
    memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Line clipped");
}

static void test_asm_equiv_linezbufnzw_vertical_polyrec_0013_dc4333(void) {
    const U8 fogged_color = 0xA7;
    U8 *cpp_base = setup_linezbufnzw_screen(fogged_color);
    const U32 first_pixel = 87 * LINEZBUFNZW_W + 326;
    const U32 last_pixel = 91 * LINEZBUFNZW_W + 326;

    Line_ZBuffer_NZW(326, 87, 326, 91, 302, 0, 0);
    memcpy(cpp_linezbufnzw_buf, cpp_base, LINEZBUFNZW_SIZE);
    ASSERT_EQ_UINT(fogged_color, cpp_linezbufnzw_buf[first_pixel]);
    ASSERT_EQ_UINT(fogged_color, cpp_linezbufnzw_buf[last_pixel]);

    U8 *asm_base = setup_linezbufnzw_screen(fogged_color);
    asm_LineZBufNZW(326, 87, 0, 326, 91, 0, 302);
    memcpy(asm_linezbufnzw_buf, asm_base, LINEZBUFNZW_SIZE);
    ASSERT_EQ_UINT(fogged_color, asm_linezbufnzw_buf[first_pixel]);
    ASSERT_EQ_UINT(fogged_color, asm_linezbufnzw_buf[last_pixel]);

    ASSERT_ASM_CPP_MEM_EQ(asm_linezbufnzw_buf, cpp_linezbufnzw_buf, LINEZBUFNZW_SIZE,
                          "Line_ZBuffer_NZW polyrec_0013 dc4333");
}

/* ── Randomized stress test ────────────────────────────────────── */

static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

static void test_asm_equiv_random(void) {
    rng_seed(0xDEADBEEF);
    for (int i = 0; i < 300; i++) {
        S32 x0 = (S32)(rng_next() % (TEST_POLY_W + 40)) - 20;
        S32 y0 = (S32)(rng_next() % (TEST_POLY_H + 40)) - 20;
        S32 x1 = (S32)(rng_next() % (TEST_POLY_W + 40)) - 20;
        S32 y1 = (S32)(rng_next() % (TEST_POLY_H + 40)) - 20;
        S32 col = (S32)(rng_next() & 0xFF);
        if (col == 0)
            col = 1; /* avoid 0 which matches background */

        setup_polygon_screen();
        Line(x0, y0, x1, y1, col);
        memcpy(cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_polygon_screen();
        asm_Line(x0, y0, x1, y1, col);
        memcpy(asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, TEST_POLY_SIZE, "Line random");
    }
}

int main(void) {
    RUN_TEST(test_horizontal);
    RUN_TEST(test_vertical);
    RUN_TEST(test_single_pixel);
    RUN_TEST(test_clipped);
    RUN_TEST(test_fully_outside);
    RUN_TEST(test_asm_equiv_horizontal);
    RUN_TEST(test_asm_equiv_diagonal);
    RUN_TEST(test_asm_equiv_clipped);
    RUN_TEST(test_asm_equiv_linezbufnzw_vertical_polyrec_0013_dc4333);
    RUN_TEST(test_asm_equiv_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
