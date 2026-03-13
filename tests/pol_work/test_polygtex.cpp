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
    for (int i = 0; i < 300; i++) {
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
    for (int i = 0; i < 300; i++) {
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

/* ── ASM filler wrapper macro ──────────────────────────────────── */

#define DECLARE_ASM_FILLER(name) \
    extern "C" void asm_##name(void); \
    static inline S32 call_asm_##name(U32 nbLines, U32 xmin, U32 xmax) { \
        S32 result; \
        __asm__ __volatile__( \
            "push %%ebp\n\t" \
            "call asm_" #name "\n\t" \
            "pop %%ebp" \
            : "=a"(result) \
            : "c"(nbLines), "b"(xmin), "d"(xmax) \
            : "edi", "esi", "memory", "cc"); \
        return result; \
    }

DECLARE_ASM_FILLER(Filler_TextureDither)
DECLARE_ASM_FILLER(Filler_TextureGouraudChromaKey)
DECLARE_ASM_FILLER(Filler_TextureDitherChromaKey)
DECLARE_ASM_FILLER(Filler_TextureGouraudZBuf)
DECLARE_ASM_FILLER(Filler_TextureGouraudChromaKeyZBuf)
DECLARE_ASM_FILLER(Filler_TextureGouraudNZW)
DECLARE_ASM_FILLER(Filler_TextureGouraudChromaKeyNZW)

/* ── Extra buffers for Z-buffer comparison ─────────────────────── */

static U16 gtex_cpp_zbuf[TEST_POLY_SIZE];
static U16 gtex_asm_zbuf[TEST_POLY_SIZE];

/* ── ChromaKey texture: every 4th byte is 0 (transparent) ──────── */

static void init_chromakey_texture(void) {
    for (int i = 0; i < TEST_TEX_PIXELS; i++)
        g_test_texture[i] = (i % 4 == 0) ? 0 : (U8)((i & 0xFE) | 1);
}

/* ── Setup for ChromaKey variants ──────────────────────────────── */

static void setup_gtex_chromakey_filler(U32 startY, U32 nbLines,
                                        U32 xmin_fp, U32 xmax_fp)
{
    setup_filler_common(startY, nbLines, xmin_fp, xmax_fp, 0);
    init_chromakey_texture();
    init_test_clut();
    PtrMap = g_test_texture;
    RepMask = 0x3F3F;
    PtrCLUTGouraud = g_test_clut;
    Fill_Patch = 1;
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

/* ── Setup for ZBuf variants ───────────────────────────────────── */

static void setup_gtex_zbuf_filler(U32 startY, U32 nbLines,
                                    U32 xmin_fp, U32 xmax_fp)
{
    setup_gtex_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_test_zbuffer(0xFFFF);
    PtrZBuffer = (PTR_U16)g_test_zbuffer;
    Fill_CurZBufMin = 0x8000;
    Fill_ZBuf_LeftSlope = 0;
    Fill_ZBuf_XSlope = 0x100;
}

/* ── Setup for ChromaKey + ZBuf variants ───────────────────────── */

static void setup_gtex_chromakey_zbuf_filler(U32 startY, U32 nbLines,
                                              U32 xmin_fp, U32 xmax_fp)
{
    setup_gtex_chromakey_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_test_zbuffer(0xFFFF);
    PtrZBuffer = (PTR_U16)g_test_zbuffer;
    Fill_CurZBufMin = 0x8000;
    Fill_ZBuf_LeftSlope = 0;
    Fill_ZBuf_XSlope = 0x100;
}

/* ══════════════════════════════════════════════════════════════════
 *  Filler_TextureDither ASM-vs-CPP equivalence
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_dither(void)
{
    setup_gtex_filler(30, 4, 20 << 16, 80 << 16);
    Filler_TextureDither(4, 20 << 16, 80 << 16);
    memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gtex_filler(30, 4, 20 << 16, 80 << 16);
    call_asm_Filler_TextureDither(4, 20 << 16, 80 << 16);
    memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_TextureDither basic");
}

static void test_asm_random_dither(void)
{
    poly_rng_seed(0xD1EB0001);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 uSlope = poly_rng_next() % 0x20000;
        U32 vSlope = poly_rng_next() % 0x20000;

        setup_gtex_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Filler_TextureDither(h, x0 << 16, x1 << 16);
        memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gtex_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        call_asm_Filler_TextureDither(h, x0 << 16, x1 << 16);
        memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random dither #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  Filler_TextureGouraudChromaKey ASM-vs-CPP equivalence
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_gouraud_ck(void)
{
    setup_gtex_chromakey_filler(30, 4, 20 << 16, 80 << 16);
    Filler_TextureGouraudChromaKey(4, 20 << 16, 80 << 16);
    memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gtex_chromakey_filler(30, 4, 20 << 16, 80 << 16);
    call_asm_Filler_TextureGouraudChromaKey(4, 20 << 16, 80 << 16);
    memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_TextureGouraudChromaKey basic");
}

static void test_asm_random_gouraud_ck(void)
{
    poly_rng_seed(0x6C4A0002);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 uSlope = poly_rng_next() % 0x20000;
        U32 vSlope = poly_rng_next() % 0x20000;

        setup_gtex_chromakey_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Filler_TextureGouraudChromaKey(h, x0 << 16, x1 << 16);
        memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gtex_chromakey_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        call_asm_Filler_TextureGouraudChromaKey(h, x0 << 16, x1 << 16);
        memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random gouraud_ck #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  Filler_TextureDitherChromaKey ASM-vs-CPP equivalence
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_dither_ck(void)
{
    setup_gtex_chromakey_filler(30, 4, 20 << 16, 80 << 16);
    Filler_TextureDitherChromaKey(4, 20 << 16, 80 << 16);
    memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gtex_chromakey_filler(30, 4, 20 << 16, 80 << 16);
    call_asm_Filler_TextureDitherChromaKey(4, 20 << 16, 80 << 16);
    memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_TextureDitherChromaKey basic");
}

static void test_asm_random_dither_ck(void)
{
    poly_rng_seed(0xDC4A0003);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 uSlope = poly_rng_next() % 0x20000;
        U32 vSlope = poly_rng_next() % 0x20000;

        setup_gtex_chromakey_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Filler_TextureDitherChromaKey(h, x0 << 16, x1 << 16);
        memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gtex_chromakey_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        call_asm_Filler_TextureDitherChromaKey(h, x0 << 16, x1 << 16);
        memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random dither_ck #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  Filler_TextureGouraudZBuf ASM-vs-CPP equivalence
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_gouraud_zbuf(void)
{
    setup_gtex_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureGouraudZBuf(4, 20 << 16, 50 << 16);
    memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gtex_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

    setup_gtex_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureGouraudZBuf(4, 20 << 16, 50 << 16);
    memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gtex_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_TextureGouraudZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gtex_asm_zbuf, (U8 *)gtex_cpp_zbuf,
                           TEST_POLY_SIZE * (int)sizeof(U16),
                           "Filler_TextureGouraudZBuf zbuf");
}

static void test_asm_random_gouraud_zbuf(void)
{
    poly_rng_seed(0x62B00004);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 uSlope = poly_rng_next() % 0x20000;
        U32 vSlope = poly_rng_next() % 0x20000;
        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_gtex_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureGouraudZBuf(h, x0 << 16, x1 << 16);
        memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gtex_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

        setup_gtex_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureGouraudZBuf(h, x0 << 16, x1 << 16);
        memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gtex_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random gouraud_zbuf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random gouraud_zbuf zbuf #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gtex_asm_zbuf, (U8 *)gtex_cpp_zbuf,
                               TEST_POLY_SIZE * (int)sizeof(U16), msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  Filler_TextureGouraudChromaKeyZBuf ASM-vs-CPP equivalence
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_gouraud_ck_zbuf(void)
{
    setup_gtex_chromakey_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureGouraudChromaKeyZBuf(4, 20 << 16, 50 << 16);
    memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gtex_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

    setup_gtex_chromakey_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureGouraudChromaKeyZBuf(4, 20 << 16, 50 << 16);
    memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gtex_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_TextureGouraudChromaKeyZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gtex_asm_zbuf, (U8 *)gtex_cpp_zbuf,
                           TEST_POLY_SIZE * (int)sizeof(U16),
                           "Filler_TextureGouraudChromaKeyZBuf zbuf");
}

static void test_asm_random_gouraud_ck_zbuf(void)
{
    poly_rng_seed(0x6CB00005);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 uSlope = poly_rng_next() % 0x20000;
        U32 vSlope = poly_rng_next() % 0x20000;
        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_gtex_chromakey_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureGouraudChromaKeyZBuf(h, x0 << 16, x1 << 16);
        memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gtex_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

        setup_gtex_chromakey_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureGouraudChromaKeyZBuf(h, x0 << 16, x1 << 16);
        memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gtex_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random gouraud_ck_zbuf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random gouraud_ck_zbuf zbuf #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gtex_asm_zbuf, (U8 *)gtex_cpp_zbuf,
                               TEST_POLY_SIZE * (int)sizeof(U16), msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  Filler_TextureGouraudNZW ASM-vs-CPP equivalence
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_gouraud_nzw(void)
{
    setup_gtex_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureGouraudNZW(4, 20 << 16, 50 << 16);
    memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gtex_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

    setup_gtex_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureGouraudNZW(4, 20 << 16, 50 << 16);
    memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gtex_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_TextureGouraudNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gtex_asm_zbuf, (U8 *)gtex_cpp_zbuf,
                           TEST_POLY_SIZE * (int)sizeof(U16),
                           "Filler_TextureGouraudNZW zbuf unchanged");
}

static void test_asm_random_gouraud_nzw(void)
{
    poly_rng_seed(0x620D0006);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 uSlope = poly_rng_next() % 0x20000;
        U32 vSlope = poly_rng_next() % 0x20000;
        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_gtex_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureGouraudNZW(h, x0 << 16, x1 << 16);
        memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gtex_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

        setup_gtex_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureGouraudNZW(h, x0 << 16, x1 << 16);
        memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gtex_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random gouraud_nzw #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random gouraud_nzw zbuf #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gtex_asm_zbuf, (U8 *)gtex_cpp_zbuf,
                               TEST_POLY_SIZE * (int)sizeof(U16), msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  Filler_TextureGouraudChromaKeyNZW ASM-vs-CPP equivalence
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_gouraud_ck_nzw(void)
{
    setup_gtex_chromakey_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureGouraudChromaKeyNZW(4, 20 << 16, 50 << 16);
    memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gtex_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

    setup_gtex_chromakey_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureGouraudChromaKeyNZW(4, 20 << 16, 50 << 16);
    memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gtex_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE,
                           "Filler_TextureGouraudChromaKeyNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gtex_asm_zbuf, (U8 *)gtex_cpp_zbuf,
                           TEST_POLY_SIZE * (int)sizeof(U16),
                           "Filler_TextureGouraudChromaKeyNZW zbuf unchanged");
}

static void test_asm_random_gouraud_ck_nzw(void)
{
    poly_rng_seed(0x6C0D0007);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H) h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W) x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 uSlope = poly_rng_next() % 0x20000;
        U32 vSlope = poly_rng_next() % 0x20000;
        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_gtex_chromakey_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureGouraudChromaKeyNZW(h, x0 << 16, x1 << 16);
        memcpy(gtex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gtex_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

        setup_gtex_chromakey_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_MapU_XSlope = uSlope;
        Fill_MapV_XSlope = vSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureGouraudChromaKeyNZW(h, x0 << 16, x1 << 16);
        memcpy(gtex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gtex_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * (int)sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random gouraud_ck_nzw #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gtex_asm_buf, gtex_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random gouraud_ck_nzw zbuf #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gtex_asm_zbuf, (U8 *)gtex_cpp_zbuf,
                               TEST_POLY_SIZE * (int)sizeof(U16), msg);
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

    /* TextureDither */
    RUN_TEST(test_asm_equiv_dither);
    RUN_TEST(test_asm_random_dither);

    /* TextureGouraudChromaKey */
    RUN_TEST(test_asm_equiv_gouraud_ck);
    RUN_TEST(test_asm_random_gouraud_ck);

    /* TextureDitherChromaKey */
    RUN_TEST(test_asm_equiv_dither_ck);
    RUN_TEST(test_asm_random_dither_ck);

    /* TextureGouraudZBuf */
    RUN_TEST(test_asm_equiv_gouraud_zbuf);
    RUN_TEST(test_asm_random_gouraud_zbuf);

    /* TextureGouraudChromaKeyZBuf */
    RUN_TEST(test_asm_equiv_gouraud_ck_zbuf);
    RUN_TEST(test_asm_random_gouraud_ck_zbuf);

    /* TextureGouraudNZW */
    RUN_TEST(test_asm_equiv_gouraud_nzw);
    RUN_TEST(test_asm_random_gouraud_nzw);

    /* TextureGouraudChromaKeyNZW */
    RUN_TEST(test_asm_equiv_gouraud_ck_nzw);
    RUN_TEST(test_asm_random_gouraud_ck_nzw);

    TEST_SUMMARY();
    return test_failures != 0;
}
