/* Test: Perspective-correct texture polygon rendering (POLYTEXZ.CPP).
 *
 * NOTE: The perspective-correct texture fillers depend on internal
 * "patch" variables and the FPU stack for perspective division. Full
 * rendering tests require elaborate fixture setup to initialize the
 * perspective W stack and patch variables. For now, test linkage and
 * off-screen rejection.
 *
 * TODO: Add full rendering tests with proper perspective pipeline setup.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYTEXZ.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

static void test_texz_declares_exist(void)
{
    Fill_Filler_Func fn = Filler_TextureZ;
    ASSERT_TRUE(fn != NULL);
    fn = Filler_TextureZFlat;
    ASSERT_TRUE(fn != NULL);
}

static Struc_Point make_point_uvw(S16 x, S16 y, U16 u, U16 v, S32 w)
{
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = x;
    p.Pt_YE = y;
    p.Pt_MapU = u;
    p.Pt_MapV = v;
    p.Pt_W = w;
    return p;
}

static void test_texz_offscreen(void)
{
    setup_polygon_screen();
    PtrMap = NULL;
    RepMask = 0;
    Struc_Point pts[3];
    pts[0] = make_point_uvw(-50, -50, 0, 0, 0x10000);
    pts[1] = make_point_uvw(-30, -10, 0, 0, 0x8000);
    pts[2] = make_point_uvw(-10, -30, 0, 0, 0x8000);
    Fill_Poly(POLY_TEXTURE_Z, 0, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

int main(void)
{
    RUN_TEST(test_texz_declares_exist);
    RUN_TEST(test_texz_offscreen);
    TEST_SUMMARY();
    return test_failures != 0;
}
