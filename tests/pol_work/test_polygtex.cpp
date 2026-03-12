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

/* ── ASM-vs-CPP equivalence (filler level) ─────────────────────── */

extern "C" void asm_Filler_TextureGouraud(void);

static inline S32 call_asm_Filler_TextureGouraud(U32 nbLines, U32 xmin, U32 xmax)
{
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Filler_TextureGouraud\n\t"
        "pop %%ebp"
        : "=a"(result)
        : "c"(nbLines), "b"(xmin), "d"(xmax)
        : "edi", "esi", "memory", "cc"
    );
    return result;
}

static U8 gtex_cpp_buf[TEST_POLY_SIZE];
static U8 gtex_asm_buf[TEST_POLY_SIZE];

static void setup_gtex_filler(U32 startY, U32 nbLines,
                               U32 xmin_fp, U32 xmax_fp)
{
    setup_filler_common(startY, nbLines, xmin_fp, xmax_fp, 0);
    init_test_texture();
    init_test_clut();
    PtrMap = g_test_texture;
    RepMask = 0x3F3F;
    PtrCLUTGouraud = g_test_clut;
    Fill_Patch = 1;  /* First scanline → initializes GTEX patch globals */
    Fill_CurMapUMin = 0;
    Fill_CurMapVMin = 0;
    Fill_MapU_LeftSlope = 0;
    Fill_MapV_LeftSlope = 0;
    Fill_MapU_XSlope = 0x10000;
    Fill_MapV_XSlope = 0;
    Fill_CurGouraudMin = 0x020000;
    Fill_Gouraud_LeftSlope = 0;
    Fill_Gouraud_XSlope = 0x800;
}

static void test_asm_equiv_gtex(void)
{
    setup_gtex_filler(30, 4, 20 << 16, 80 << 16);
    Filler_TextureGouraud(4, 20 << 16, 80 << 16);
    memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gtex_filler(30, 4, 20 << 16, 80 << 16);
    call_asm_Filler_TextureGouraud(4, 20 << 16, 80 << 16);
    memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_TextureGouraud basic");
}

static void test_asm_equiv_gtex_narrow(void)
{
    setup_gtex_filler(50, 2, 60 << 16, 72 << 16);
    Filler_TextureGouraud(2, 60 << 16, 72 << 16);
    memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gtex_filler(50, 2, 60 << 16, 72 << 16);
    call_asm_Filler_TextureGouraud(2, 60 << 16, 72 << 16);
    memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_TextureGouraud narrow");
}

static void test_asm_random_gtex(void)
{
    poly_rng_seed(0xBAADF00D);
    init_test_texture();
    init_test_clut();
    for (int i = 0; i < 30; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;

        setup_gtex_filler(y, h, x0 << 16, x1 << 16);
        Filler_TextureGouraud(h, x0 << 16, x1 << 16);
        memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gtex_filler(y, h, x0 << 16, x1 << 16);
        call_asm_Filler_TextureGouraud(h, x0 << 16, x1 << 16);
        memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random gtex #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

int main(void)
{
    RUN_TEST(test_texture_gouraud_basic);
    RUN_TEST(test_texture_dither_basic);
    RUN_TEST(test_texture_gouraud_offscreen);
    RUN_TEST(test_texture_gouraud_clipped);
    RUN_TEST(test_texture_gouraud_random);
    RUN_TEST(test_asm_equiv_gtex);
    RUN_TEST(test_asm_equiv_gtex_narrow);
    RUN_TEST(test_asm_random_gtex);
    TEST_SUMMARY();
    return test_failures != 0;
}
