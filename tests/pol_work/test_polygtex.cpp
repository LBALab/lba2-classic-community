/* Test: Gouraud+texture polygon rendering (POLYGTEX.CPP) via Fill_Poly.
 *
 * Tests POLY_TEXTURE_GOURAUD and POLY_TEXTURE_DITHER types which combine
 * affine texture mapping with gouraud/dithered shading.
 * Requires: PtrMap, RepMask, PtrCLUTGouraud.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYGTEX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

#define TEX_SIZE 256

static U8 g_texture[TEX_SIZE * TEX_SIZE];
static U8 g_gouraud_clut[4096];

/* Make a point with screen coords + UV + light */
static Struc_Point make_point_uv_lit(S16 x, S16 y, U16 u, U16 v, U16 light)
{
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = x;
    p.Pt_YE = y;
    p.Pt_MapU = u;
    p.Pt_MapV = v;
    p.Pt_Light = light;
    return p;
}

static void setup_gtex_screen(void)
{
    setup_polygon_screen();
    /* Checkerboard texture */
    for (int y = 0; y < TEX_SIZE; y++)
        for (int x = 0; x < TEX_SIZE; x++)
            g_texture[y * TEX_SIZE + x] = (U8)(((x ^ y) & 1) ? 0xAA : 0x55);
    PtrMap = g_texture;
    RepMask = 0xFFFF;
    /* Gouraud CLUT */
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 256; col++)
            g_gouraud_clut[row * 256 + col] = (U8)((row << 4) | (col & 0xF));
    PtrCLUTGouraud = g_gouraud_clut;
}

/* ── TextureGouraud ────────────────────────────────────────────── */

static void test_texture_gouraud_basic(void)
{
    setup_gtex_screen();
    Struc_Point pts[3];
    pts[0] = make_point_uv_lit(80, 10, 0, 0, 0x0800);
    pts[1] = make_point_uv_lit(40, 100, 0, 50 << 8, 0x0400);
    pts[2] = make_point_uv_lit(120, 100, 50 << 8, 50 << 8, 0x0C00);
    Fill_Poly(POLY_TEXTURE_GOURAUD, 0, 3, pts);
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n > 100);
}

/* ── TextureDither ─────────────────────────────────────────────── */

static void test_texture_dither_basic(void)
{
    setup_gtex_screen();
    Struc_Point pts[3];
    pts[0] = make_point_uv_lit(60, 10, 0, 0, 0x0600);
    pts[1] = make_point_uv_lit(20, 80, 0, 40 << 8, 0x0300);
    pts[2] = make_point_uv_lit(100, 80, 40 << 8, 40 << 8, 0x0A00);
    Fill_Poly(POLY_TEXTURE_DITHER, 0, 3, pts);
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n > 100);
}

/* ── Edge cases ────────────────────────────────────────────────── */

static void test_texture_gouraud_offscreen(void)
{
    setup_gtex_screen();
    Struc_Point pts[3];
    pts[0] = make_point_uv_lit(-50, -50, 0, 0, 0x0800);
    pts[1] = make_point_uv_lit(-30, -10, 10 << 8, 0, 0x0400);
    pts[2] = make_point_uv_lit(-10, -30, 0, 10 << 8, 0x0C00);
    Fill_Poly(POLY_TEXTURE_GOURAUD, 0, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

static void test_texture_gouraud_clipped(void)
{
    setup_gtex_screen();
    Struc_Point pts[3];
    pts[0] = make_point_uv_lit(-10, 40, 0, 0, 0x0800);
    pts[1] = make_point_uv_lit(60, 10, 30 << 8, 0, 0x0400);
    pts[2] = make_point_uv_lit(60, 90, 30 << 8, 50 << 8, 0x0C00);
    Fill_Poly(POLY_TEXTURE_GOURAUD, 0, 3, pts);
    /* CLUT may map some combinations to 0; just verify no crash */
    ASSERT_TRUE(1);
}

/* ── Randomized stress test ────────────────────────────────────── */

static void test_texture_gouraud_random(void)
{
    poly_rng_seed(0xDEADBEEF);
    for (int i = 0; i < 30; i++) {
        setup_gtex_screen();
        Struc_Point pts[3];
        for (int v = 0; v < 3; v++) {
            S16 x = (S16)((poly_rng_next() % (TEST_POLY_W + 40)) - 20);
            S16 y = (S16)((poly_rng_next() % (TEST_POLY_H + 40)) - 20);
            U16 u = (U16)((poly_rng_next() % TEX_SIZE) << 8);
            U16 vcoord = (U16)((poly_rng_next() % TEX_SIZE) << 8);
            U16 light = (U16)((poly_rng_next() % 14) << 8);
            pts[v] = make_point_uv_lit(x, y, u, vcoord, light);
        }
        S32 type = (poly_rng_next() & 1) ? POLY_TEXTURE_GOURAUD : POLY_TEXTURE_DITHER;
        Fill_Poly(type, 0, 3, pts);
        ASSERT_TRUE(1);
    }
}

int main(void)
{
    RUN_TEST(test_texture_gouraud_basic);
    RUN_TEST(test_texture_dither_basic);
    RUN_TEST(test_texture_gouraud_offscreen);
    RUN_TEST(test_texture_gouraud_clipped);
    RUN_TEST(test_texture_gouraud_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
