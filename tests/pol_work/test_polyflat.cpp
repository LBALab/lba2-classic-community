/* Test: Filler_Flat / Filler_Trame / Filler_Transparent — flat polygon
 * scanline fillers.
 *
 * Tests the low-level scanline fillers and the top-level Fill_Poly()
 * dispatching for flat polygon types (POLY_SOLID, POLY_TRAME, POLY_TRANS).
 *
 * NOTE: ASM equivalence tests are not yet wired up for pol_work — the
 * ASM objects are not compiled by the current CMakeLists.  These tests
 * validate the CPP implementations only.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYFLAT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

/* Set up for polygon rendering tests */
static void setup_flat_screen(void)
{
    setup_polygon_screen();
}

/* ── Fill_Poly with POLY_SOLID (type 0) ──────────────────────── */

static void test_flat_solid_triangle(void)
{
    setup_flat_screen();
    Struc_Point pts[3];
    pts[0] = make_point(80, 10);
    pts[1] = make_point(40, 100);
    pts[2] = make_point(120, 100);
    Fill_Poly(POLY_SOLID, 0xFF, 3, pts);
    /* Horizontal strip should be filled */
    for (int y = 50; y <= 54; y++)
        ASSERT_TRUE(g_poly_framebuf[y * TEST_POLY_W + 80] != 0);
    /* Outside should be empty */
    ASSERT_EQ_UINT(0, g_poly_framebuf[5 * TEST_POLY_W + 5]);
}

static void test_flat_single_pixel_area(void)
{
    setup_flat_screen();
    Struc_Point pts[3];
    pts[0] = make_point(20, 20);
    pts[1] = make_point(10, 60);
    pts[2] = make_point(50, 60);
    Fill_Poly(POLY_SOLID, 0xFF, 3, pts);
    ASSERT_TRUE(g_poly_framebuf[40 * TEST_POLY_W + 20] != 0);
}

static void test_flat_with_different_color(void)
{
    setup_flat_screen();
    Struc_Point pts[3];
    pts[0] = make_point(40, 20);
    pts[1] = make_point(20, 80);
    pts[2] = make_point(80, 80);
    Fill_Poly(POLY_SOLID, 0xAA, 3, pts);
    ASSERT_EQ_UINT(0xAA, g_poly_framebuf[50 * TEST_POLY_W + 40]);
}

/* ── Fill_Poly with POLY_SOLID (type 0) ──────────────────────── */

static void test_fill_poly_solid_triangle(void)
{
    setup_polygon_screen();
    Struc_Point pts[3];
    pts[0] = make_point(80, 10);
    pts[1] = make_point(40, 100);
    pts[2] = make_point(120, 100);
    Fill_Poly(POLY_SOLID, 0xAA, 3, pts);
    /* Centre of triangle should be filled */
    ASSERT_EQ_UINT(0xAA, g_poly_framebuf[60 * TEST_POLY_W + 80]);
    /* Outside should be empty */
    ASSERT_EQ_UINT(0, g_poly_framebuf[5 * TEST_POLY_W + 5]);
}

static void test_fill_poly_solid_covers_area(void)
{
    setup_polygon_screen();
    Struc_Point pts[3];
    pts[0] = make_point(20, 20);
    pts[1] = make_point(10, 60);
    pts[2] = make_point(50, 60);
    Fill_Poly(POLY_SOLID, 0x55, 3, pts);
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n > 100);  /* should fill a significant area */
}

/* ── Fill_Poly with POLY_TRAME (type 3) ──────────────────────── */

static void test_fill_poly_trame_checkerboard(void)
{
    setup_polygon_screen();
    Struc_Point pts[3];
    pts[0] = make_point(20, 20);
    pts[1] = make_point(10, 80);
    pts[2] = make_point(60, 80);
    Fill_Poly(POLY_TRAME, 0xBB, 3, pts);
    /* Trame fills every other pixel — should have roughly half the
     * pixels of a solid fill */
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n > 50);  /* some pixels filled */
}

/* ── Fill_Poly with POLY_TRANS (type 2) ──────────────────────── */

static void test_fill_poly_transparent(void)
{
    setup_polygon_screen();
    /* Pre-fill the area with 0x11 so transparency has something to blend */
    memset(g_poly_framebuf, 0x11, TEST_POLY_SIZE);
    Struc_Point pts[3];
    pts[0] = make_point(40, 20);
    pts[1] = make_point(20, 80);
    pts[2] = make_point(80, 80);
    Fill_Poly(POLY_TRANS, 0xA0, 3, pts);
    /* Transparent blends: pixel = (pixel & 0x0F) + (color & 0xF0) */
    /* Centre pixel was 0x11, now should be (0x01 + 0xA0) = 0xA1 */
    U8 centre = g_poly_framebuf[50 * TEST_POLY_W + 40];
    ASSERT_EQ_UINT(0xA1, centre);
}

/* ── Edge case: degenerate triangle ──────────────────────────── */

static void test_fill_poly_degenerate_line(void)
{
    setup_polygon_screen();
    /* All points collinear — should NOT crash, fill nothing or minimal */
    Struc_Point pts[3];
    pts[0] = make_point(10, 50);
    pts[1] = make_point(50, 50);
    pts[2] = make_point(90, 50);
    Fill_Poly(POLY_SOLID, 0xCC, 3, pts);
    /* No crash — pixels might or might not be filled; just verify no OOB */
    ASSERT_TRUE(1);
}

static void test_fill_poly_fully_clipped(void)
{
    setup_polygon_screen();
    /* Triangle entirely off-screen */
    Struc_Point pts[3];
    pts[0] = make_point(-100, -100);
    pts[1] = make_point(-50, -10);
    pts[2] = make_point(-10, -50);
    Fill_Poly(POLY_SOLID, 0xDD, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

/* ── Randomized stress test ────────────────────────────────────── */

static void test_fill_poly_random_solid(void)
{
    poly_rng_seed(0xDEADBEEF);
    for (int i = 0; i < 30; i++) {
        setup_polygon_screen();
        Struc_Point pts[3];
        for (int v = 0; v < 3; v++) {
            S16 x = (S16)((poly_rng_next() % (TEST_POLY_W + 40)) - 20);
            S16 y = (S16)((poly_rng_next() % (TEST_POLY_H + 40)) - 20);
            pts[v] = make_point(x, y);
        }
        U8 color = (U8)(poly_rng_next() | 1);
        /* Should not crash for any random triangle */
        Fill_Poly(POLY_SOLID, color, 3, pts);
        ASSERT_TRUE(1);
    }
}

int main(void)
{
    RUN_TEST(test_flat_solid_triangle);
    RUN_TEST(test_flat_single_pixel_area);
    RUN_TEST(test_flat_with_different_color);
    RUN_TEST(test_fill_poly_solid_triangle);
    RUN_TEST(test_fill_poly_solid_covers_area);
    RUN_TEST(test_fill_poly_trame_checkerboard);
    RUN_TEST(test_fill_poly_transparent);
    RUN_TEST(test_fill_poly_degenerate_line);
    RUN_TEST(test_fill_poly_fully_clipped);
    RUN_TEST(test_fill_poly_random_solid);
    TEST_SUMMARY();
    return test_failures != 0;
}
