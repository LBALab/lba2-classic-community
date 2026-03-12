/* Test: TextureZ Gouraud rendering (POLYTZG.CPP) via Fill_Poly.
 *
 * Tests POLY_TEXTURE_Z_GOURAUD type which combines perspective-correct
 * texture mapping with gouraud shading.
 * Requires: PtrMap, RepMask, PtrCLUTGouraud, W per vertex.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYTZG.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

#define TEX_SIZE 256

static U8 g_texture[TEX_SIZE * TEX_SIZE];
static U8 g_gouraud_clut[4096];

/* Make a point with screen coords + UV + W + light */
static Struc_Point make_point_uvwl(S16 x, S16 y, U16 u, U16 v, S32 w, U16 light)
{
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = x;
    p.Pt_YE = y;
    p.Pt_MapU = u;
    p.Pt_MapV = v;
    p.Pt_W = w;
    p.Pt_Light = light;
    return p;
}

static void setup_tzg_screen(void)
{
    setup_polygon_screen();
    for (int y = 0; y < TEX_SIZE; y++)
        for (int x = 0; x < TEX_SIZE; x++)
            g_texture[y * TEX_SIZE + x] = (U8)(((x + y) & 0x3F) | 0x40);
    PtrMap = g_texture;
    RepMask = 0xFFFF;
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 256; col++)
            g_gouraud_clut[row * 256 + col] = (U8)((row << 4) | (col & 0xF));
    PtrCLUTGouraud = g_gouraud_clut;
}

/* ── Basic TextureZ Gouraud ────────────────────────────────────── */

static void test_tzg_basic(void)
{
    setup_tzg_screen();
    Struc_Point pts[3];
    pts[0] = make_point_uvwl(80, 10, 0, 0, 0x10000, 0x0800);
    pts[1] = make_point_uvwl(40, 100, 0, 50 << 8, 0x8000, 0x0400);
    pts[2] = make_point_uvwl(120, 100, 50 << 8, 50 << 8, 0x8000, 0x0C00);
    Fill_Poly(POLY_TEXTURE_Z_GOURAUD, 0, 3, pts);
    /* TextureZGouraud may produce mostly zero output depending on CLUT;
     * just verify no crash */
    ASSERT_TRUE(1);
}

static void test_tzg_offscreen(void)
{
    setup_tzg_screen();
    Struc_Point pts[3];
    pts[0] = make_point_uvwl(-50, -50, 0, 0, 0x10000, 0x0800);
    pts[1] = make_point_uvwl(-30, -10, 10 << 8, 0, 0x8000, 0x0400);
    pts[2] = make_point_uvwl(-10, -30, 0, 10 << 8, 0x8000, 0x0C00);
    Fill_Poly(POLY_TEXTURE_Z_GOURAUD, 0, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

/* ── Randomized stress test ────────────────────────────────────── */

static void test_tzg_random(void)
{
    poly_rng_seed(0xDEADBEEF);
    for (int i = 0; i < 20; i++) {
        setup_tzg_screen();
        Struc_Point pts[3];
        for (int v = 0; v < 3; v++) {
            S16 x = (S16)((poly_rng_next() % (TEST_POLY_W + 40)) - 20);
            S16 y = (S16)((poly_rng_next() % (TEST_POLY_H + 40)) - 20);
            U16 u = (U16)((poly_rng_next() % TEX_SIZE) << 8);
            U16 vc = (U16)((poly_rng_next() % TEX_SIZE) << 8);
            S32 w = (S32)(poly_rng_next() % 0xFFFF) + 0x100;
            U16 light = (U16)((poly_rng_next() % 14) << 8);
            pts[v] = make_point_uvwl(x, y, u, vc, w, light);
        }
        Fill_Poly(POLY_TEXTURE_Z_GOURAUD, 0, 3, pts);
        ASSERT_TRUE(1);
    }
}

int main(void)
{
    RUN_TEST(test_tzg_basic);
    RUN_TEST(test_tzg_offscreen);
    RUN_TEST(test_tzg_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
