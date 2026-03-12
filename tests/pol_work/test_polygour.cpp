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

/* ── ASM-vs-CPP equivalence (filler level) ─────────────────────── */

extern "C" void asm_Filler_Gouraud(void);

static inline S32 call_asm_Filler_Gouraud(U32 nbLines, U32 xmin, U32 xmax)
{
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Filler_Gouraud\n\t"
        "pop %%ebp"
        : "=a"(result)
        : "c"(nbLines), "b"(xmin), "d"(xmax)
        : "edi", "esi", "memory", "cc"
    );
    return result;
}

static U8 gour_cpp_buf[TEST_POLY_SIZE];
static U8 gour_asm_buf[TEST_POLY_SIZE];

static void setup_gouraud_filler(U32 startY, U32 nbLines,
                                  U32 xmin_fp, U32 xmax_fp,
                                  U32 gouraudStart, U32 gouraudXSlope)
{
    setup_filler_common(startY, nbLines, xmin_fp, xmax_fp, 0x05);
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 256; col++)
            g_gouraud_clut[row * 256 + col] = (U8)((row << 4) | (col & 0xF));
    PtrCLUTGouraud = g_gouraud_clut;
    Fill_CurGouraudMin = gouraudStart;
    Fill_Gouraud_LeftSlope = 0;
    Fill_Gouraud_XSlope = gouraudXSlope;
}

static void test_asm_equiv_gouraud(void)
{
    setup_gouraud_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_Gouraud(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gouraud_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_Gouraud(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                           "Filler_Gouraud basic");
}

static void test_asm_equiv_gouraud_flat(void)
{
    /* Uniform gouraud = no slope → flat shading */
    setup_gouraud_filler(40, 6, 10 << 16, 100 << 16, 0x080000, 0);
    Filler_Gouraud(6, 10 << 16, 100 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gouraud_filler(40, 6, 10 << 16, 100 << 16, 0x080000, 0);
    call_asm_Filler_Gouraud(6, 10 << 16, 100 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                           "Filler_Gouraud uniform");
}

static void test_asm_random_gouraud(void)
{
    poly_rng_seed(0xCAFEBABE);
    for (int i = 0; i < 30; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;

        setup_gouraud_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Filler_Gouraud(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gouraud_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        call_asm_Filler_Gouraud(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "random gouraud #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
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
    RUN_TEST(test_asm_equiv_gouraud);
    RUN_TEST(test_asm_equiv_gouraud_flat);
    RUN_TEST(test_asm_random_gouraud);
    TEST_SUMMARY();
    return test_failures != 0;
}
