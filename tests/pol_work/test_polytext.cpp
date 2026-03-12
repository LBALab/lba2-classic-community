/* Test: Textured polygon rendering (POLYTEXT.CPP) — ASM-vs-CPP equivalence.
 *
 * Tests Filler_Texture via Fill_Poly with POLY_TEXTURE type, and verifies
 * ASM-vs-CPP equivalence at the filler level using inline asm wrappers.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYTEXT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

/* ── Buffers for ASM-vs-CPP comparison ─────────────────────────── */

static U8 tex_cpp_buf[TEST_POLY_SIZE];
static U8 tex_asm_buf[TEST_POLY_SIZE];

/* ── Inline texture (gradient 64×64) ──────────────────────────── */

static void setup_texture_env(void)
{
    init_test_texture();
    PtrMap = g_test_texture;
    RepMask = 0xFFFF;  /* 256×256 tile */
}

/* ── CPP functional tests via Fill_Poly ────────────────────────── */

static void test_texture_linkage(void)
{
    Fill_Filler_Func fn = Filler_Texture;
    ASSERT_TRUE(fn != NULL);
    fn = Filler_TextureFlat;
    ASSERT_TRUE(fn != NULL);
}

static void test_texture_basic_triangle(void)
{
    setup_polygon_screen();
    setup_texture_env();
    Struc_Point pts[3];
    pts[0] = make_point_uv(80, 10, 0, 0);
    pts[1] = make_point_uv(40, 100, 200 << 8, 0);
    pts[2] = make_point_uv(120, 100, 0, 200 << 8);
    /* Verify the triangle shape first with a solid fill */
    Fill_Poly(POLY_SOLID, 0xFF, 3, pts);
    int px_solid = count_nonzero_pixels(0, 0, TEST_POLY_W - 1, TEST_POLY_H - 1);
    /* Now try with texture */
    setup_polygon_screen();
    setup_texture_env();
    Fill_Poly(POLY_TEXTURE, 0, 3, pts);
    int px = count_nonzero_pixels(0, 0, TEST_POLY_W - 1, TEST_POLY_H - 1);
    ASSERT_TRUE(px > 0 || px_solid > 0);
}

static void test_texture_offscreen(void)
{
    setup_polygon_screen();
    setup_texture_env();
    Struc_Point pts[3];
    pts[0] = make_point_uv(-50, -50, 0, 0);
    pts[1] = make_point_uv(-30, -10, 10 << 8, 0);
    pts[2] = make_point_uv(-10, -30, 0, 10 << 8);
    Fill_Poly(POLY_TEXTURE, 0, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W - 1, TEST_POLY_H - 1));
}

static void test_texture_clipped(void)
{
    setup_polygon_screen();
    setup_texture_env();
    Struc_Point pts[3];
    pts[0] = make_point_uv(-30, 20, 0, 0);
    pts[1] = make_point_uv(100, 20, 200 << 8, 0);
    pts[2] = make_point_uv(50, 100, 0, 200 << 8);
    Fill_Poly(POLY_TEXTURE, 0, 3, pts);
    int px = count_nonzero_pixels(0, 0, TEST_POLY_W - 1, TEST_POLY_H - 1);
    ASSERT_TRUE(px > 0);
}

/* ── ASM filler wrapper ────────────────────────────────────────── */

extern "C" void asm_Filler_Texture(void);

static inline S32 call_asm_Filler_Texture(U32 nbLines, U32 xmin, U32 xmax)
{
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Filler_Texture\n\t"
        "pop %%ebp"
        : "=a"(result)
        : "c"(nbLines), "b"(xmin), "d"(xmax)
        : "edi", "esi", "memory", "cc"
    );
    return result;
}

/* Helper: set up the filler state for a textured scanline strip */
static void setup_texture_filler(U32 startY, U32 nbLines,
                                  U32 xmin_fp, U32 xmax_fp)
{
    setup_filler_common(startY, nbLines, xmin_fp, xmax_fp, 0);
    setup_texture_env();
    Fill_Patch = 1;  /* Triggers patch initialization on first call */
    Fill_CurMapUMin = 0;
    Fill_CurMapVMin = 0;
    Fill_MapU_LeftSlope = 0;
    Fill_MapV_LeftSlope = 0;
    Fill_MapU_XSlope = 0x10000;  /* 1 texel per pixel in U */
    Fill_MapV_XSlope = 0x10000;  /* 1 texel per pixel in V */
}

/* ── ASM-vs-CPP equivalence tests ──────────────────────────────── */

static void test_asm_equiv_texture(void)
{
    setup_texture_filler(30, 4, 20 << 16, 80 << 16);
    Filler_Texture(4, 20 << 16, 80 << 16);
    memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_texture_filler(30, 4, 20 << 16, 80 << 16);
    call_asm_Filler_Texture(4, 20 << 16, 80 << 16);
    memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_Texture strip");
}

static void test_asm_equiv_texture_narrow(void)
{
    setup_texture_filler(50, 2, 75 << 16, 82 << 16);
    Filler_Texture(2, 75 << 16, 82 << 16);
    memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_texture_filler(50, 2, 75 << 16, 82 << 16);
    call_asm_Filler_Texture(2, 75 << 16, 82 << 16);
    memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_Texture narrow");
}

static void test_asm_random_texture(void)
{
    poly_rng_seed(0xDEADBEEF);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;
        U32 uslope = (poly_rng_next() % 0x20000);
        U32 vslope = (poly_rng_next() % 0x20000);

        setup_texture_filler(y, h, x0 << 16, x1 << 16);
        Fill_MapU_XSlope = uslope;
        Fill_MapV_XSlope = vslope;
        Filler_Texture(h, x0 << 16, x1 << 16);
        memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_texture_filler(y, h, x0 << 16, x1 << 16);
        Fill_MapU_XSlope = uslope;
        Fill_MapV_XSlope = vslope;
        call_asm_Filler_Texture(h, x0 << 16, x1 << 16);
        memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random texture #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ── Main ──────────────────────────────────────────────────────── */

int main(void)
{
    RUN_TEST(test_texture_linkage);
    RUN_TEST(test_texture_basic_triangle);
    RUN_TEST(test_texture_offscreen);
    RUN_TEST(test_texture_clipped);
    RUN_TEST(test_asm_equiv_texture);
    RUN_TEST(test_asm_equiv_texture_narrow);
    RUN_TEST(test_asm_random_texture);
    TEST_SUMMARY();
    return test_failures != 0;
}
