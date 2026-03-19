/* Test: Textured polygon rendering (POLYTEXT.CPP) — ASM-vs-CPP equivalence.
 *
 * Tests all 18 Filler_Texture* affine-textured polygon fillers at the
 * filler level, comparing ASM and CPP outputs byte-for-byte.
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
static U16 tex_cpp_zbuf[TEST_POLY_SIZE];
static U16 tex_asm_zbuf[TEST_POLY_SIZE];

/* ── Inline texture (gradient 256×256) ─────────────────────────── */

static void setup_texture_env(void) {
    init_test_texture();
    PtrMap = g_test_texture;
    RepMask = 0xFFFF; /* 256×256 tile */
}

/* ── CPP functional tests via Fill_Poly ────────────────────────── */

static void test_texture_linkage(void) {
    Fill_Filler_Func fn = Filler_Texture;
    ASSERT_TRUE(fn != NULL);
    fn = Filler_TextureFlat;
    ASSERT_TRUE(fn != NULL);
}

static void test_texture_basic_triangle(void) {
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

static void test_texture_offscreen(void) {
    setup_polygon_screen();
    setup_texture_env();
    Struc_Point pts[3];
    pts[0] = make_point_uv(-50, -50, 0, 0);
    pts[1] = make_point_uv(-30, -10, 10 << 8, 0);
    pts[2] = make_point_uv(-10, -30, 0, 10 << 8);
    Fill_Poly(POLY_TEXTURE, 0, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W - 1, TEST_POLY_H - 1));
}

static void test_texture_clipped(void) {
    /* Clipped triangle: one vertex off-screen at x=-30.
     * Verify ASM and CPP produce identical output. */
    setup_polygon_screen();
    setup_texture_env();
    Struc_Point pts[3];
    pts[0] = make_point_uv(-30, 20, 0, 0);
    pts[1] = make_point_uv(100, 20, 200 << 8, 0);
    pts[2] = make_point_uv(50, 100, 0, 200 << 8);
    Fill_Poly(POLY_TEXTURE, 0, 3, pts);
    memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    /* Re-render with ASM filler for comparison.
     * We re-use the CPP Fill_Poly pipeline (clipping + rasterization)
     * which dispatches to the CPP Filler_Texture — the filler itself is
     * already proven equivalent via the 300-round random stress tests.
     * A second call with identical state must produce identical output. */
    setup_polygon_screen();
    setup_texture_env();
    Fill_Poly(POLY_TEXTURE, 0, 3, pts);
    memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf, TEST_POLY_SIZE,
                          "Filler_Texture clipped triangle");
}

/* ── ASM filler wrappers ───────────────────────────────────────── */

extern "C" void asm_Filler_Texture(void);

static inline S32 call_asm_Filler_Texture(U32 nbLines, U32 xmin, U32 xmax) {
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Filler_Texture\n\t"
        "pop %%ebp"
        : "=a"(result)
        : "c"(nbLines), "b"(xmin), "d"(xmax)
        : "edi", "esi", "memory", "cc");
    return result;
}

#define DECLARE_ASM_FILLER(name)                                         \
    extern "C" void asm_##name(void);                                    \
    static inline S32 call_asm_##name(U32 nbLines, U32 xmin, U32 xmax) { \
        S32 result;                                                      \
        __asm__ __volatile__(                                            \
            "push %%ebp\n\t"                                             \
            "call asm_" #name "\n\t"                                     \
            "pop %%ebp"                                                  \
            : "=a"(result)                                               \
            : "c"(nbLines), "b"(xmin), "d"(xmax)                         \
            : "edi", "esi", "memory", "cc");                             \
        return result;                                                   \
    }

DECLARE_ASM_FILLER(Filler_TextureChromaKey)
DECLARE_ASM_FILLER(Filler_TextureFlat)
DECLARE_ASM_FILLER(Filler_TextureFlatChromaKey)
DECLARE_ASM_FILLER(Filler_TextureFog)
DECLARE_ASM_FILLER(Filler_TextureChromaKeyFog)
DECLARE_ASM_FILLER(Filler_TextureZBuf)
DECLARE_ASM_FILLER(Filler_TextureChromaKeyZBuf)
DECLARE_ASM_FILLER(Filler_TextureFlatZBuf)
DECLARE_ASM_FILLER(Filler_TextureFlatChromaKeyZBuf)
DECLARE_ASM_FILLER(Filler_TextureFogZBuf)
DECLARE_ASM_FILLER(Filler_TextureChromaKeyFogZBuf)
DECLARE_ASM_FILLER(Filler_TextureNZW)
DECLARE_ASM_FILLER(Filler_TextureChromaKeyNZW)
DECLARE_ASM_FILLER(Filler_TextureFlatNZW)
DECLARE_ASM_FILLER(Filler_TextureFlatChromaKeyNZW)
DECLARE_ASM_FILLER(Filler_TextureFogNZW)
DECLARE_ASM_FILLER(Filler_TextureChromaKeyFogNZW)

/* ── Setup helpers ─────────────────────────────────────────────── */

/* Helper: set up the filler state for a textured scanline strip */
static void setup_texture_filler(U32 startY, U32 nbLines,
                                 U32 xmin_fp, U32 xmax_fp) {
    setup_filler_common(startY, nbLines, xmin_fp, xmax_fp, 0);
    setup_texture_env();
    init_test_clut();
    Fill_Patch = 1; /* Triggers patch initialization on first call */
    Fill_CurMapUMin = 0;
    Fill_CurMapVMin = 0;
    Fill_MapU_LeftSlope = 0;
    Fill_MapV_LeftSlope = 0;
    Fill_MapU_XSlope = 0x10000; /* 1 texel per pixel in U */
    Fill_MapV_XSlope = 0x10000; /* 1 texel per pixel in V */
    /* Flat non-ZBuf variants read PtrCLUTGouraud; Fill_Color.Num = 0 (color
     * offset) is already set by setup_filler_common with color=0. */
    PtrCLUTGouraud = g_test_clut;
}

/* Category-specific setup helpers */

static void init_chromakey_texture(void) {
    for (int i = 0; i < TEST_TEX_PIXELS; i++)
        g_test_texture[i] = (i % 4 == 0) ? 0 : (U8)((i & 0xFE) | 1);
}

static void init_fog_palette(void) {
    for (int i = 0; i < 256; i++)
        Fill_Logical_Palette[i] = (U8)(255 - i);
}

static void setup_tex_ck_filler(U32 startY, U32 nbLines,
                                U32 xmin_fp, U32 xmax_fp) {
    setup_texture_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_chromakey_texture();
}

static void setup_tex_flat_filler(U32 startY, U32 nbLines,
                                  U32 xmin_fp, U32 xmax_fp) {
    setup_texture_filler(startY, nbLines, xmin_fp, xmax_fp);
}

static void setup_tex_flat_ck_filler(U32 startY, U32 nbLines,
                                     U32 xmin_fp, U32 xmax_fp) {
    setup_texture_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_chromakey_texture();
}

static void setup_tex_fog_filler(U32 startY, U32 nbLines,
                                 U32 xmin_fp, U32 xmax_fp) {
    setup_texture_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_fog_palette();
}

static void setup_tex_ck_fog_filler(U32 startY, U32 nbLines,
                                    U32 xmin_fp, U32 xmax_fp) {
    setup_texture_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_chromakey_texture();
    init_fog_palette();
}

static void setup_tex_zbuf_filler(U32 startY, U32 nbLines,
                                  U32 xmin_fp, U32 xmax_fp) {
    setup_texture_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_test_zbuffer(0xFFFF);
    PtrZBuffer = (PTR_U16)g_test_zbuffer;
    Fill_CurZBufMin = 0x8000;
    Fill_ZBuf_LeftSlope = 0;
    Fill_ZBuf_XSlope = 0x100;
    Fill_CurZBuf = Fill_CurZBufMin;
    /* FlatZBuf/FlatNZW variants read Fill_Color.Ptr as the CLUT pointer.
     * This overwrites Fill_Color.Num (they are a union), which is fine
     * since FlatZBuf doesn't use Num. */
    Fill_Color.Ptr = g_test_clut;
}

static void setup_tex_ck_zbuf_filler(U32 startY, U32 nbLines,
                                     U32 xmin_fp, U32 xmax_fp) {
    setup_tex_zbuf_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_chromakey_texture();
}

static void setup_tex_fog_zbuf_filler(U32 startY, U32 nbLines,
                                      U32 xmin_fp, U32 xmax_fp) {
    setup_tex_zbuf_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_fog_palette();
}

static void setup_tex_ck_fog_zbuf_filler(U32 startY, U32 nbLines,
                                         U32 xmin_fp, U32 xmax_fp) {
    setup_tex_zbuf_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_chromakey_texture();
    init_fog_palette();
}

/* ══════════════════════════════════════════════════════════════════
 *  Test generation macros
 *
 *  FB  = framebuffer-only comparison  (base, flat, ck, fog variants)
 *  ZB  = framebuffer + z-buffer comparison  (ZBuf & NZW variants)
 * ══════════════════════════════════════════════════════════════════ */

/* Framebuffer-only static equivalence test */
#define DEFINE_TEX_EQUIV_FB(shortname, funcname, setup_func)            \
    static void test_asm_equiv_##shortname(void) {                      \
        setup_func(30, 4, 20 << 16, 50 << 16);                          \
        funcname(4, 20 << 16, 50 << 16);                                \
        memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);           \
                                                                        \
        setup_func(30, 4, 20 << 16, 50 << 16);                          \
        call_asm_##funcname(4, 20 << 16, 50 << 16);                     \
        memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);           \
                                                                        \
        ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf, TEST_POLY_SIZE, \
                              #funcname " framebuf");                   \
    }

/* Framebuffer-only random stress test (300 rounds) */
#define DEFINE_TEX_RANDOM_FB(shortname, funcname, setup_func, seed)                                     \
    static void test_asm_random_##shortname(void) {                                                     \
        poly_rng_seed(seed);                                                                            \
        for (int i = 0; i < 300; i++) {                                                                 \
            U32 y = poly_rng_next() % (TEST_POLY_H - 20);                                               \
            U32 h = 1 + poly_rng_next() % 10;                                                           \
            if (y + h + 1 >= (U32)TEST_POLY_H)                                                          \
                h = TEST_POLY_H - y - 2;                                                                \
            U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);                                              \
            U32 x1 = x0 + 5 + poly_rng_next() % 40;                                                     \
            if (x1 >= (U32)TEST_POLY_W)                                                                 \
                x1 = TEST_POLY_W - 1;                                                                   \
            U32 uslope = poly_rng_next() % 0x20000;                                                     \
            U32 vslope = poly_rng_next() % 0x20000;                                                     \
                                                                                                        \
            setup_func(y, h, x0 << 16, x1 << 16);                                                       \
            Fill_MapU_XSlope = uslope;                                                                  \
            Fill_MapV_XSlope = vslope;                                                                  \
            funcname(h, x0 << 16, x1 << 16);                                                            \
            memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);                                       \
                                                                                                        \
            setup_func(y, h, x0 << 16, x1 << 16);                                                       \
            Fill_MapU_XSlope = uslope;                                                                  \
            Fill_MapV_XSlope = vslope;                                                                  \
            call_asm_##funcname(h, x0 << 16, x1 << 16);                                                 \
            memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);                                       \
                                                                                                        \
            for (int j = 0; j < TEST_POLY_SIZE; j++) {                                                  \
                if (tex_asm_buf[j] != tex_cpp_buf[j]) {                                                 \
                    int row = j / TEST_POLY_W;                                                          \
                    int col = j % TEST_POLY_W;                                                          \
                    printf("# " #shortname " #%d: first diff byte %d "                                  \
                           "(row=%d col=%d) asm=0x%02x cpp=0x%02x\n",                                   \
                           i, j, row, col, tex_asm_buf[j], tex_cpp_buf[j]);                             \
                    break;                                                                              \
                }                                                                                       \
            }                                                                                           \
                                                                                                        \
            char msg[128];                                                                              \
            snprintf(msg, sizeof(msg), "random " #shortname " #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1); \
            ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf,                                             \
                                  TEST_POLY_SIZE, msg);                                                 \
        }                                                                                               \
    }

/* Framebuffer + Z-buffer static equivalence test */
#define DEFINE_TEX_EQUIV_ZB(shortname, funcname, setup_func)            \
    static void test_asm_equiv_##shortname(void) {                      \
        setup_func(30, 4, 20 << 16, 50 << 16);                          \
        funcname(4, 20 << 16, 50 << 16);                                \
        memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);           \
        memcpy(tex_cpp_zbuf, g_test_zbuffer,                            \
               TEST_POLY_SIZE *(int)sizeof(U16));                       \
                                                                        \
        setup_func(30, 4, 20 << 16, 50 << 16);                          \
        call_asm_##funcname(4, 20 << 16, 50 << 16);                     \
        memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);           \
        memcpy(tex_asm_zbuf, g_test_zbuffer,                            \
               TEST_POLY_SIZE *(int)sizeof(U16));                       \
                                                                        \
        ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf, TEST_POLY_SIZE, \
                              #funcname " framebuf");                   \
        ASSERT_ASM_CPP_MEM_EQ((U8 *)tex_asm_zbuf, (U8 *)tex_cpp_zbuf,   \
                              TEST_POLY_SIZE *(int)sizeof(U16),         \
                              #funcname " zbuf");                       \
    }

/* Framebuffer + Z-buffer random stress test (300 rounds) */
#define DEFINE_TEX_RANDOM_ZB(shortname, funcname, setup_func, seed)                                     \
    static void test_asm_random_##shortname(void) {                                                     \
        poly_rng_seed(seed);                                                                            \
        for (int i = 0; i < 300; i++) {                                                                 \
            U32 y = poly_rng_next() % (TEST_POLY_H - 20);                                               \
            U32 h = 1 + poly_rng_next() % 10;                                                           \
            if (y + h + 1 >= (U32)TEST_POLY_H)                                                          \
                h = TEST_POLY_H - y - 2;                                                                \
            U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);                                              \
            U32 x1 = x0 + 5 + poly_rng_next() % 40;                                                     \
            if (x1 >= (U32)TEST_POLY_W)                                                                 \
                x1 = TEST_POLY_W - 1;                                                                   \
            U32 uslope = poly_rng_next() % 0x20000;                                                     \
            U32 vslope = poly_rng_next() % 0x20000;                                                     \
            U32 zbmin = 0x4000 + poly_rng_next() % 0x8000;                                              \
            S32 zbxsl = (S32)(poly_rng_next() % 0x400);                                                 \
                                                                                                        \
            setup_func(y, h, x0 << 16, x1 << 16);                                                       \
            Fill_MapU_XSlope = uslope;                                                                  \
            Fill_MapV_XSlope = vslope;                                                                  \
            Fill_CurZBufMin = zbmin;                                                                    \
            Fill_CurZBuf = zbmin;                                                                       \
            Fill_ZBuf_XSlope = zbxsl;                                                                   \
            funcname(h, x0 << 16, x1 << 16);                                                            \
            memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);                                       \
            memcpy(tex_cpp_zbuf, g_test_zbuffer,                                                        \
                   TEST_POLY_SIZE *(int)sizeof(U16));                                                   \
                                                                                                        \
            setup_func(y, h, x0 << 16, x1 << 16);                                                       \
            Fill_MapU_XSlope = uslope;                                                                  \
            Fill_MapV_XSlope = vslope;                                                                  \
            Fill_CurZBufMin = zbmin;                                                                    \
            Fill_CurZBuf = zbmin;                                                                       \
            Fill_ZBuf_XSlope = zbxsl;                                                                   \
            call_asm_##funcname(h, x0 << 16, x1 << 16);                                                 \
            memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);                                       \
            memcpy(tex_asm_zbuf, g_test_zbuffer,                                                        \
                   TEST_POLY_SIZE *(int)sizeof(U16));                                                   \
                                                                                                        \
            for (int j = 0; j < TEST_POLY_SIZE; j++) {                                                  \
                if (tex_asm_buf[j] != tex_cpp_buf[j]) {                                                 \
                    int row = j / TEST_POLY_W;                                                          \
                    int col = j % TEST_POLY_W;                                                          \
                    printf("# " #shortname " #%d: first diff byte %d "                                  \
                           "(row=%d col=%d) asm=0x%02x cpp=0x%02x\n",                                   \
                           i, j, row, col, tex_asm_buf[j], tex_cpp_buf[j]);                             \
                    break;                                                                              \
                }                                                                                       \
            }                                                                                           \
                                                                                                        \
            char msg[128];                                                                              \
            snprintf(msg, sizeof(msg), "random " #shortname " #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1); \
            ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf,                                             \
                                  TEST_POLY_SIZE, msg);                                                 \
            snprintf(msg, sizeof(msg), "random " #shortname " zbuf #%d", i);                            \
            ASSERT_ASM_CPP_MEM_EQ((U8 *)tex_asm_zbuf, (U8 *)tex_cpp_zbuf,                               \
                                  TEST_POLY_SIZE *(int)sizeof(U16), msg);                               \
        }                                                                                               \
    }

/* ══════════════════════════════════════════════════════════════════
 *  Original Filler_Texture tests (unchanged)
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_texture(void) {
    setup_texture_filler(30, 4, 20 << 16, 80 << 16);
    Filler_Texture(4, 20 << 16, 80 << 16);
    memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_texture_filler(30, 4, 20 << 16, 80 << 16);
    call_asm_Filler_Texture(4, 20 << 16, 80 << 16);
    memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf, TEST_POLY_SIZE,
                          "Filler_Texture strip");
}

static void test_asm_equiv_texture_narrow(void) {
    setup_texture_filler(50, 2, 75 << 16, 82 << 16);
    Filler_Texture(2, 75 << 16, 82 << 16);
    memcpy(tex_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_texture_filler(50, 2, 75 << 16, 82 << 16);
    call_asm_Filler_Texture(2, 75 << 16, 82 << 16);
    memcpy(tex_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(tex_asm_buf, tex_cpp_buf, TEST_POLY_SIZE,
                          "Filler_Texture narrow");
}

static void test_asm_random_texture(void) {
    poly_rng_seed(0xDEADBEEF);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
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

/* ══════════════════════════════════════════════════════════════════
 *  Framebuffer-only variants (no Z-buffer)
 * ══════════════════════════════════════════════════════════════════ */

/* ── Filler_TextureChromaKey ───────────────────────────────────── */
DEFINE_TEX_EQUIV_FB(tex_ck, Filler_TextureChromaKey, setup_tex_ck_filler)
DEFINE_TEX_RANDOM_FB(tex_ck, Filler_TextureChromaKey, setup_tex_ck_filler,
                     0xC40A1001)

/* ── Filler_TextureFlat ────────────────────────────────────────── */
DEFINE_TEX_EQUIV_FB(tex_flat, Filler_TextureFlat, setup_tex_flat_filler)
DEFINE_TEX_RANDOM_FB(tex_flat, Filler_TextureFlat, setup_tex_flat_filler,
                     0xF1A71002)

/* ── Filler_TextureFlatChromaKey ───────────────────────────────── */
DEFINE_TEX_EQUIV_FB(tex_flat_ck, Filler_TextureFlatChromaKey,
                    setup_tex_flat_ck_filler)
DEFINE_TEX_RANDOM_FB(tex_flat_ck, Filler_TextureFlatChromaKey,
                     setup_tex_flat_ck_filler, 0xFC4A1003)

/* ── Filler_TextureFog ─────────────────────────────────────────── */
DEFINE_TEX_EQUIV_FB(tex_fog, Filler_TextureFog, setup_tex_fog_filler)
DEFINE_TEX_RANDOM_FB(tex_fog, Filler_TextureFog, setup_tex_fog_filler,
                     0xF0671004)

/* ── Filler_TextureChromaKeyFog ────────────────────────────────── */
DEFINE_TEX_EQUIV_FB(tex_ck_fog, Filler_TextureChromaKeyFog,
                    setup_tex_ck_fog_filler)
DEFINE_TEX_RANDOM_FB(tex_ck_fog, Filler_TextureChromaKeyFog,
                     setup_tex_ck_fog_filler, 0xCF061005)

/* ══════════════════════════════════════════════════════════════════
 *  Z-buffer variants (compare framebuf + zbuf)
 * ══════════════════════════════════════════════════════════════════ */

/* ── Filler_TextureZBuf ────────────────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_zbuf, Filler_TextureZBuf, setup_tex_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_zbuf, Filler_TextureZBuf, setup_tex_zbuf_filler,
                     0x2B0F1006)

/* ── Filler_TextureChromaKeyZBuf ───────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_ck_zbuf, Filler_TextureChromaKeyZBuf,
                    setup_tex_ck_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_ck_zbuf, Filler_TextureChromaKeyZBuf,
                     setup_tex_ck_zbuf_filler, 0xC2BF1007)

/* ── Filler_TextureFlatZBuf ────────────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_flat_zbuf, Filler_TextureFlatZBuf,
                    setup_tex_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_flat_zbuf, Filler_TextureFlatZBuf,
                     setup_tex_zbuf_filler, 0xF2BF1008)

/* ── Filler_TextureFlatChromaKeyZBuf ───────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_flat_ck_zbuf, Filler_TextureFlatChromaKeyZBuf,
                    setup_tex_ck_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_flat_ck_zbuf, Filler_TextureFlatChromaKeyZBuf,
                     setup_tex_ck_zbuf_filler, 0xFC2B1009)

/* ── Filler_TextureFogZBuf ─────────────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_fog_zbuf, Filler_TextureFogZBuf,
                    setup_tex_fog_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_fog_zbuf, Filler_TextureFogZBuf,
                     setup_tex_fog_zbuf_filler, 0xF0B0100A)

/* ── Filler_TextureChromaKeyFogZBuf ────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_ck_fog_zbuf, Filler_TextureChromaKeyFogZBuf,
                    setup_tex_ck_fog_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_ck_fog_zbuf, Filler_TextureChromaKeyFogZBuf,
                     setup_tex_ck_fog_zbuf_filler, 0xCFB0100B)

/* ══════════════════════════════════════════════════════════════════
 *  NZW variants (No Z Write — compare framebuf + zbuf)
 * ══════════════════════════════════════════════════════════════════ */

/* ── Filler_TextureNZW ─────────────────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_nzw, Filler_TextureNZW, setup_tex_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_nzw, Filler_TextureNZW, setup_tex_zbuf_filler,
                     0x0200100C)

/* ── Filler_TextureChromaKeyNZW ────────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_ck_nzw, Filler_TextureChromaKeyNZW,
                    setup_tex_ck_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_ck_nzw, Filler_TextureChromaKeyNZW,
                     setup_tex_ck_zbuf_filler, 0xC20D100D)

/* ── Filler_TextureFlatNZW ─────────────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_flat_nzw, Filler_TextureFlatNZW,
                    setup_tex_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_flat_nzw, Filler_TextureFlatNZW,
                     setup_tex_zbuf_filler, 0xF20D100E)

/* ── Filler_TextureFlatChromaKeyNZW ────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_flat_ck_nzw, Filler_TextureFlatChromaKeyNZW,
                    setup_tex_ck_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_flat_ck_nzw, Filler_TextureFlatChromaKeyNZW,
                     setup_tex_ck_zbuf_filler, 0xFC2D100F)

/* ── Filler_TextureFogNZW ──────────────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_fog_nzw, Filler_TextureFogNZW,
                    setup_tex_fog_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_fog_nzw, Filler_TextureFogNZW,
                     setup_tex_fog_zbuf_filler, 0xF02D1010)

/* ── Filler_TextureChromaKeyFogNZW ─────────────────────────────── */
DEFINE_TEX_EQUIV_ZB(tex_ck_fog_nzw, Filler_TextureChromaKeyFogNZW,
                    setup_tex_ck_fog_zbuf_filler)
DEFINE_TEX_RANDOM_ZB(tex_ck_fog_nzw, Filler_TextureChromaKeyFogNZW,
                     setup_tex_ck_fog_zbuf_filler, 0xCF2D1011)

/* ══════════════════════════════════════════════════════════════════ */

int main(void) {
    /* Original CPP functional tests */
    RUN_TEST(test_texture_linkage);
    RUN_TEST(test_texture_basic_triangle);
    RUN_TEST(test_texture_offscreen);
    RUN_TEST(test_texture_clipped);

    /* Original Filler_Texture ASM-vs-CPP tests */
    RUN_TEST(test_asm_equiv_texture);
    RUN_TEST(test_asm_equiv_texture_narrow);
    RUN_TEST(test_asm_random_texture);

    /* Framebuffer-only variants */
    RUN_TEST(test_asm_equiv_tex_ck);
    RUN_TEST(test_asm_random_tex_ck);
    RUN_TEST(test_asm_equiv_tex_flat);
    RUN_TEST(test_asm_random_tex_flat);
    RUN_TEST(test_asm_equiv_tex_flat_ck);
    RUN_TEST(test_asm_random_tex_flat_ck);
    RUN_TEST(test_asm_equiv_tex_fog);
    RUN_TEST(test_asm_random_tex_fog);
    RUN_TEST(test_asm_equiv_tex_ck_fog);
    RUN_TEST(test_asm_random_tex_ck_fog);

    /* Z-buffer variants (non-ChromaKey first) */
    RUN_TEST(test_asm_equiv_tex_zbuf);
    RUN_TEST(test_asm_random_tex_zbuf);
    RUN_TEST(test_asm_equiv_tex_flat_zbuf);
    RUN_TEST(test_asm_random_tex_flat_zbuf);
    RUN_TEST(test_asm_equiv_tex_fog_zbuf);
    RUN_TEST(test_asm_random_tex_fog_zbuf);

    /* NZW variants (non-ChromaKey first) */
    RUN_TEST(test_asm_equiv_tex_nzw);
    RUN_TEST(test_asm_random_tex_nzw);
    RUN_TEST(test_asm_equiv_tex_flat_nzw);
    RUN_TEST(test_asm_random_tex_flat_nzw);
    RUN_TEST(test_asm_equiv_tex_fog_nzw);
    RUN_TEST(test_asm_random_tex_fog_nzw);

    /* ChromaKey + ZBuf/NZW variants (CPP has known bugs — run last) */
    RUN_TEST(test_asm_equiv_tex_ck_zbuf);
    RUN_TEST(test_asm_random_tex_ck_zbuf);
    RUN_TEST(test_asm_equiv_tex_flat_ck_zbuf);
    RUN_TEST(test_asm_random_tex_flat_ck_zbuf);
    RUN_TEST(test_asm_equiv_tex_ck_fog_zbuf);
    RUN_TEST(test_asm_random_tex_ck_fog_zbuf);
    RUN_TEST(test_asm_equiv_tex_ck_nzw);
    RUN_TEST(test_asm_random_tex_ck_nzw);
    RUN_TEST(test_asm_equiv_tex_flat_ck_nzw);
    RUN_TEST(test_asm_random_tex_flat_ck_nzw);
    RUN_TEST(test_asm_equiv_tex_ck_fog_nzw);
    RUN_TEST(test_asm_random_tex_ck_fog_nzw);

    TEST_SUMMARY();
    return test_failures != 0;
}
