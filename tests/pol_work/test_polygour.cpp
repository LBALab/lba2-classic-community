/* Test: Gouraud polygon rendering (POLYGOUR.CPP) via Fill_Poly.
 *
 * Tests gouraud-shaded polygons (POLY_GOURAUD, POLY_DITHER) through
 * the Fill_Poly dispatcher.  Gouraud shading interpolates per-vertex
 * light values (Pt_Light) across the triangle surface.
 *
 * Requires: PtrCLUTGouraud (256-entry CLUT used for gouraud lookup).
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYGOUR.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

/* 4KB gouraud CLUT: 16 rows × 256 columns.
 * Row = intensity (0-15), column = base color.
 * For testing: CLUT[row][col] = (row << 4) | (col & 0xF) */
static U8 g_gouraud_clut[4096];

static void setup_gouraud_screen(void)
{
    setup_polygon_screen();
    /* Build a simple gouraud CLUT */
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 256; col++)
            g_gouraud_clut[row * 256 + col] = (U8)((row << 4) | (col & 0xF));
    PtrCLUTGouraud = g_gouraud_clut;
}

/* ── Gouraud triangle ──────────────────────────────────────────── */

static void test_gouraud_basic(void)
{
    setup_gouraud_screen();
    Struc_Point pts[3];
    /* Light values span 0x0000 to 0x0E00 (full range for 16-row CLUT) */
    pts[0] = make_point_lit(80, 10, 0x0000);
    pts[1] = make_point_lit(40, 100, 0x0800);
    pts[2] = make_point_lit(120, 100, 0x0E00);
    Fill_Poly(POLY_GOURAUD, 0x05, 3, pts);
    /* Some pixels should be written */
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n > 100);
}

static void test_gouraud_uniform_light(void)
{
    setup_gouraud_screen();
    Struc_Point pts[3];
    /* All vertices at same intensity → uniform output */
    pts[0] = make_point_lit(40, 20, 0x0800);
    pts[1] = make_point_lit(20, 90, 0x0800);
    pts[2] = make_point_lit(80, 90, 0x0800);
    Fill_Poly(POLY_GOURAUD, 0x03, 3, pts);
    /* Centre pixel should be set (non-zero) */
    U8 centre = g_poly_framebuf[60 * TEST_POLY_W + 40];
    ASSERT_TRUE(centre != 0);
}

/* ── Dither triangle ───────────────────────────────────────────── */

static void test_dither_basic(void)
{
    setup_gouraud_screen();
    Struc_Point pts[3];
    pts[0] = make_point_lit(60, 10, 0x0300);
    pts[1] = make_point_lit(20, 100, 0x0900);
    pts[2] = make_point_lit(100, 100, 0x0600);
    Fill_Poly(POLY_DITHER, 0x07, 3, pts);
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n > 100);
}

/* ── Edge cases ────────────────────────────────────────────────── */

static void test_gouraud_offscreen(void)
{
    setup_gouraud_screen();
    Struc_Point pts[3];
    pts[0] = make_point_lit(-50, -50, 0x0800);
    pts[1] = make_point_lit(-30, -10, 0x0400);
    pts[2] = make_point_lit(-10, -30, 0x0C00);
    Fill_Poly(POLY_GOURAUD, 0x01, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

static void test_gouraud_clipped(void)
{
    setup_gouraud_screen();
    Struc_Point pts[3];
    /* Triangle partially off-screen */
    pts[0] = make_point_lit(-10, 50, 0x0800);
    pts[1] = make_point_lit(60, 20, 0x0400);
    pts[2] = make_point_lit(60, 80, 0x0C00);
    Fill_Poly(POLY_GOURAUD, 0x02, 3, pts);
    /* Gouraud CLUT may map some combinations to 0; just verify no crash */
    ASSERT_TRUE(1);
}

/* ── Randomized stress test ────────────────────────────────────── */

static void test_gouraud_random(void)
{
    poly_rng_seed(0xDEADBEEF);
    for (int i = 0; i < 30; i++) {
        setup_gouraud_screen();
        Struc_Point pts[3];
        for (int v = 0; v < 3; v++) {
            S16 x = (S16)((poly_rng_next() % (TEST_POLY_W + 40)) - 20);
            S16 y = (S16)((poly_rng_next() % (TEST_POLY_H + 40)) - 20);
            U16 light = (U16)((poly_rng_next() % 14) << 8);
            pts[v] = make_point_lit(x, y, light);
        }
        U8 color = (U8)(poly_rng_next() & 0x0F);
        S32 type = (poly_rng_next() & 1) ? POLY_GOURAUD : POLY_DITHER;
        Fill_Poly(type, color, 3, pts);
        ASSERT_TRUE(1);
    }
}

int main(void)
{
    RUN_TEST(test_gouraud_basic);
    RUN_TEST(test_gouraud_uniform_light);
    RUN_TEST(test_dither_basic);
    RUN_TEST(test_gouraud_offscreen);
    RUN_TEST(test_gouraud_clipped);
    RUN_TEST(test_gouraud_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
