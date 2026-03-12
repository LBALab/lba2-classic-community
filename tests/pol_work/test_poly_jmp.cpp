/* Test: Polygon jump/dispatch tables (POLY_JMP.CPP).
 *
 * Verifies that the Jmp_* dispatch functions correctly set up state
 * and produce output via Fill_Poly.  Tests the flat types (Solid,
 * Transparent, Trame) and gouraud types through the dispatch layer.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLY_JMP.H>
#include <POLYGON/POLYFLAT.H>
#include <POLYGON/POLYGOUR.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

static U8 g_gouraud_clut[4096];

static void setup_jmp_screen(void)
{
    setup_polygon_screen();
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 256; col++)
            g_gouraud_clut[row * 256 + col] = (U8)((row << 4) | (col & 0xF));
    PtrCLUTGouraud = g_gouraud_clut;
}

/* ── Test that each fill mode table is populated ────────────────── */

static void test_normal_table_populated(void)
{
    Switch_Fillers(FILL_POLY_NO_TEXTURES);
    /* Table entries for basic types should not be NULL */
    ASSERT_TRUE(Fill_N_Table_Jumps[POLY_SOLID] != NULL);
    ASSERT_TRUE(Fill_N_Table_Jumps[POLY_TRANS] != NULL);
    ASSERT_TRUE(Fill_N_Table_Jumps[POLY_TRAME] != NULL);
    ASSERT_TRUE(Fill_N_Table_Jumps[POLY_GOURAUD] != NULL);
}

/* ── Dispatch through Fill_Poly for each flat type ─────────────── */

static void test_dispatch_solid(void)
{
    setup_jmp_screen();
    Struc_Point pts[3];
    pts[0] = make_point(60, 10);
    pts[1] = make_point(20, 90);
    pts[2] = make_point(100, 90);
    Fill_Poly(POLY_SOLID, 0x77, 3, pts);
    ASSERT_EQ_UINT(0x77, g_poly_framebuf[50 * TEST_POLY_W + 60]);
}

static void test_dispatch_trame(void)
{
    setup_jmp_screen();
    Struc_Point pts[3];
    pts[0] = make_point(60, 10);
    pts[1] = make_point(20, 90);
    pts[2] = make_point(100, 90);
    Fill_Poly(POLY_TRAME, 0x88, 3, pts);
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n > 50);
}

static void test_dispatch_transparent(void)
{
    setup_jmp_screen();
    memset(g_poly_framebuf, 0x11, TEST_POLY_SIZE);
    Struc_Point pts[3];
    pts[0] = make_point(60, 10);
    pts[1] = make_point(20, 90);
    pts[2] = make_point(100, 90);
    Fill_Poly(POLY_TRANS, 0xA0, 3, pts);
    U8 centre = g_poly_framebuf[50 * TEST_POLY_W + 60];
    ASSERT_EQ_UINT(0xA1, centre);
}

static void test_dispatch_gouraud(void)
{
    setup_jmp_screen();
    Struc_Point pts[3];
    pts[0] = make_point_lit(60, 10, 0x0800);
    pts[1] = make_point_lit(20, 90, 0x0400);
    pts[2] = make_point_lit(100, 90, 0x0C00);
    Fill_Poly(POLY_GOURAUD, 0x05, 3, pts);
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n > 100);
}

/* ── Quad dispatch ─────────────────────────────────────────────── */

static void test_dispatch_solid_quad(void)
{
    setup_jmp_screen();
    Struc_Point pts[4];
    pts[0] = make_point(30, 30);
    pts[1] = make_point(30, 90);
    pts[2] = make_point(120, 90);
    pts[3] = make_point(120, 30);
    Fill_Poly(POLY_SOLID, 0x33, 4, pts);
    ASSERT_EQ_UINT(0x33, g_poly_framebuf[60 * TEST_POLY_W + 80]);
}

/* ── Randomized dispatch stress ────────────────────────────────── */

static void test_dispatch_random(void)
{
    poly_rng_seed(0xBEEFCAFE);
    for (int i = 0; i < 30; i++) {
        setup_jmp_screen();
        memset(g_poly_framebuf, 0x11, TEST_POLY_SIZE);
        Struc_Point pts[3];
        for (int v = 0; v < 3; v++) {
            S16 x = (S16)((poly_rng_next() % (TEST_POLY_W + 40)) - 20);
            S16 y = (S16)((poly_rng_next() % (TEST_POLY_H + 40)) - 20);
            U16 light = (U16)((poly_rng_next() % 14) << 8);
            pts[v] = make_point_lit(x, y, light);
        }
        U8 color = (U8)(poly_rng_next() | 1);
        /* Types 0-7 cover flat + gouraud variants */
        S32 type = (S32)(poly_rng_next() % 8);
        Fill_Poly(type, color, 3, pts);
        ASSERT_TRUE(1);
    }
}

int main(void)
{
    RUN_TEST(test_normal_table_populated);
    RUN_TEST(test_dispatch_solid);
    RUN_TEST(test_dispatch_trame);
    RUN_TEST(test_dispatch_transparent);
    RUN_TEST(test_dispatch_gouraud);
    RUN_TEST(test_dispatch_solid_quad);
    RUN_TEST(test_dispatch_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
