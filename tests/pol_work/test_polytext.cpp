/* Test: Textured polygon rendering (POLYTEXT.CPP) via Fill_Poly.
 *
 * NOTE: The affine texture fillers (Filler_Texture, etc.) depend on
 * internal "patch" variables (POLYTEXT_PtrMapPatch, POLYTEXT_RepMaskPatch,
 * etc.) that are set on the first scanline via Fill_Patch. They also
 * rely on specific texture sizes matching RepMask. Full integration
 * testing requires more elaborate fixture setup. For now, we test
 * that the dispatch tables are wired up and basic linkage works.
 *
 * TODO: Add full rendering tests once the fixture supports proper
 * texture pipeline initialization.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYTEXT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

static void test_texture_declares_exist(void)
{
    /* Verify that the filler function pointers exist (linkage test) */
    Fill_Filler_Func fn = Filler_Texture;
    ASSERT_TRUE(fn != NULL);
    fn = Filler_TextureFlat;
    ASSERT_TRUE(fn != NULL);
    fn = Filler_TextureChromaKey;
    ASSERT_TRUE(fn != NULL);
}

static void test_texture_offscreen(void)
{
    setup_polygon_screen();
    PtrMap = NULL;
    RepMask = 0;
    /* Fully off-screen triangle should never reach the filler */
    Struc_Point pts[3];
    pts[0] = make_point_uv(-50, -50, 0, 0);
    pts[1] = make_point_uv(-30, -10, 0, 0);
    pts[2] = make_point_uv(-10, -30, 0, 0);
    Fill_Poly(POLY_TEXTURE, 0, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

int main(void)
{
    RUN_TEST(test_texture_declares_exist);
    RUN_TEST(test_texture_offscreen);
    TEST_SUMMARY();
    return test_failures != 0;
}
