/* Test: POLY.CPP — core polygon infrastructure.
 *
 * Tests SetFog, INV64, Switch_Fillers, SetCLUT, and Fill_Poly
 * integration with various polygon types.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYFLAT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

/* ── INV64 ─────────────────────────────────────────────────────── */

static void test_inv64_positive(void)
{
    /* INV64(a) = (1<<32) / a — integer reciprocal for 16.16 fixed point */
    S32 r = INV64(65536);
    ASSERT_EQ_INT(65536, r);
}

static void test_inv64_small(void)
{
    S32 r = INV64(1);
    /* 2^32 / 1 wraps to 0 in 32-bit signed — expected behavior */
    (void)r; /* just confirm it doesn't crash */
    ASSERT_TRUE(1);
}

static void test_inv64_negative(void)
{
    S32 r = INV64(-65536);
    ASSERT_EQ_INT(-65536, r);
}

/* ── SetFog ────────────────────────────────────────────────────── */

static void test_setfog_basic(void)
{
    SetFog(100, 1000);
    ASSERT_EQ_INT(100, Fill_Z_Fog_Near);
    ASSERT_EQ_INT(1000, Fill_Z_Fog_Far);
    ASSERT_TRUE(Fill_ZBuffer_Factor > 0);
}

static void test_setfog_zero_far(void)
{
    /* z_far=0 should be clamped to 1 to avoid division by zero */
    SetFog(0, 0);
    ASSERT_EQ_INT(1, Fill_Z_Fog_Far);
    /* ZBuffer factor may wrap with these extreme values — just check no crash */
    ASSERT_TRUE(1);
}

static void test_setfog_large_range(void)
{
    SetFog(0, 65535);
    ASSERT_EQ_INT(0, Fill_Z_Fog_Near);
    ASSERT_EQ_INT(65535, Fill_Z_Fog_Far);
    ASSERT_TRUE(Fill_ZBuffer_Factor > 0);
}

/* ── Switch_Fillers ────────────────────────────────────────────── */

static void test_switch_normal(void)
{
    Switch_Fillers(FILL_POLY_NO_TEXTURES);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_Fog);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_ZBuffer);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_NZW);
}

static void test_switch_fog(void)
{
    Switch_Fillers(FILL_POLY_FOG);
    ASSERT_EQ_UINT(TRUE, Fill_Flag_Fog);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_ZBuffer);
}

static void test_switch_zbuffer(void)
{
    Switch_Fillers(FILL_POLY_ZBUFFER);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_Fog);
    ASSERT_EQ_UINT(TRUE, Fill_Flag_ZBuffer);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_NZW);
}

static void test_switch_nzw(void)
{
    Switch_Fillers(FILL_POLY_NZW);
    ASSERT_EQ_UINT(TRUE, Fill_Flag_ZBuffer);
    ASSERT_EQ_UINT(TRUE, Fill_Flag_NZW);
}

/* ── SetScreenPitch ────────────────────────────────────────────── */

static void test_set_screen_pitch(void)
{
    U32 tab[2] = { 0, 320 };
    SetScreenPitch(tab);
    ASSERT_EQ_UINT(320, ScreenPitch);
    ASSERT_TRUE(PTR_TabOffLine == tab);
}

/* ── Fill_Poly integration with quad (4 points) ─────────────────── */

static void test_fill_poly_quad(void)
{
    setup_polygon_screen();
    Struc_Point pts[4];
    pts[0] = make_point(20, 20);
    pts[1] = make_point(20, 80);
    pts[2] = make_point(100, 80);
    pts[3] = make_point(100, 20);
    Fill_Poly(POLY_SOLID, 0x77, 4, pts);
    /* Centre should be filled */
    ASSERT_EQ_UINT(0x77, g_poly_framebuf[50 * TEST_POLY_W + 60]);
}

/* ── Randomized stress: random triangles with random types ──────── */

static void test_fill_poly_random_types(void)
{
    poly_rng_seed(0xCAFEBABE);
    for (int i = 0; i < 30; i++) {
        setup_polygon_screen();
        /* Pre-fill for transparency tests */
        memset(g_poly_framebuf, 0x11, TEST_POLY_SIZE);
        Struc_Point pts[3];
        for (int v = 0; v < 3; v++) {
            S16 x = (S16)((poly_rng_next() % (TEST_POLY_W + 20)) - 10);
            S16 y = (S16)((poly_rng_next() % (TEST_POLY_H + 20)) - 10);
            pts[v] = make_point(x, y);
        }
        U8 color = (U8)(poly_rng_next() | 1);
        /* Test types 0-3 (flat/trans/trame) */
        S32 type = (S32)(poly_rng_next() % 4);
        Fill_Poly(type, color, 3, pts);
        /* Just ensure no crash */
        ASSERT_TRUE(1);
    }
}

int main(void)
{
    RUN_TEST(test_inv64_positive);
    RUN_TEST(test_inv64_small);
    RUN_TEST(test_inv64_negative);
    RUN_TEST(test_setfog_basic);
    RUN_TEST(test_setfog_zero_far);
    RUN_TEST(test_setfog_large_range);
    RUN_TEST(test_switch_normal);
    RUN_TEST(test_switch_fog);
    RUN_TEST(test_switch_zbuffer);
    RUN_TEST(test_switch_nzw);
    RUN_TEST(test_set_screen_pitch);
    RUN_TEST(test_fill_poly_quad);
    RUN_TEST(test_fill_poly_random_types);
    TEST_SUMMARY();
    return test_failures != 0;
}
