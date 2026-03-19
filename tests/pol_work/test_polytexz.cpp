/* Test: Perspective-correct texture polygon rendering (POLYTEXZ.CPP).
 *
 * Tests all 18 Filler_TextureZ* functions at the filler level with
 * ASM-vs-CPP equivalence.  The perspective correction uses FPU/double-
 * precision math internally.  Fill_Color.Ptr must point to a valid CLUT
 * (used by the filler for palette lookup).
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYTEXZ.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

/* Provide F_256 constant needed by ASM perspective code */
extern "C" {
float F_256 = 256.0f;
}

static void test_texz_declares_exist(void) {
    Fill_Filler_Func fn = Filler_TextureZ;
    ASSERT_TRUE(fn != NULL);
    fn = Filler_TextureZFlat;
    ASSERT_TRUE(fn != NULL);
}

/* ── ASM filler wrappers ───────────────────────────────────────── */

extern "C" void asm_Filler_TextureZ(void);

static inline S32 call_asm_Filler_TextureZ(U32 nbLines, U32 xmin, U32 xmax) {
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Filler_TextureZ\n\t"
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

DECLARE_ASM_FILLER(Filler_TextureZFlat)
DECLARE_ASM_FILLER(Filler_TextureZChromaKey)
DECLARE_ASM_FILLER(Filler_TextureZFlatChromaKey)
DECLARE_ASM_FILLER(Filler_TextureZFog)
DECLARE_ASM_FILLER(Filler_TextureZChromaKeyFog)
DECLARE_ASM_FILLER(Filler_TextureZZBuf)
DECLARE_ASM_FILLER(Filler_TextureZChromaKeyZBuf)
DECLARE_ASM_FILLER(Filler_TextureZFlatZBuf)
DECLARE_ASM_FILLER(Filler_TextureZFlatChromaKeyZBuf)
DECLARE_ASM_FILLER(Filler_TextureZFogZBuf)
DECLARE_ASM_FILLER(Filler_TextureZChromaKeyFogZBuf)
DECLARE_ASM_FILLER(Filler_TextureZNZW)
DECLARE_ASM_FILLER(Filler_TextureZChromaKeyNZW)
DECLARE_ASM_FILLER(Filler_TextureZFlatNZW)
DECLARE_ASM_FILLER(Filler_TextureZFlatChromaKeyNZW)
DECLARE_ASM_FILLER(Filler_TextureZFogNZW)
DECLARE_ASM_FILLER(Filler_TextureZChromaKeyFogNZW)

/* ── Test buffers ──────────────────────────────────────────────── */

static U8 texz_cpp_buf[TEST_POLY_SIZE];
static U8 texz_asm_buf[TEST_POLY_SIZE];
static U16 texz_cpp_zbuf[TEST_POLY_SIZE];
static U16 texz_asm_zbuf[TEST_POLY_SIZE];

/* ── Base setup (unchanged from original) ──────────────────────── */

static void setup_texz_filler(U32 startY, U32 nbLines,
                              U32 xmin_fp, U32 xmax_fp) {
    setup_filler_common(startY, nbLines, xmin_fp, xmax_fp, 0);
    init_test_texture();
    init_test_clut();
    PtrMap = g_test_texture;
    RepMask = 0xFFFF;
    Fill_Patch = 1;

    /* The TextureZ filler uses Fill_Color.Ptr as a CLUT */
    Fill_Color.Ptr = g_test_clut;

    Fill_CurMapUMin = 0;
    Fill_CurMapVMin = 0;
    Fill_MapU_LeftSlope = 0;
    Fill_MapV_LeftSlope = 0;
    Fill_MapU_XSlope = 0x10000;
    Fill_MapV_XSlope = 0;

    Fill_CurWMin = 0x10000;
    Fill_W_LeftSlope = 0;
    Fill_Cur_W = 0x10000;
    Fill_W_XSlope = 0;
    Fill_Cur_MapUOverW = 0;
    Fill_Cur_MapVOverW = 0;
    Fill_Next_W = 0x10000;
    Fill_Next_MapUOverW = 0;
    Fill_Next_MapVOverW = 0;
    Fill_Next_MapU = 0;
    Fill_Next_MapV = 0;
}

/* ── Category-specific setup helpers ───────────────────────────── */

static void init_chromakey_texture(void) {
    for (int i = 0; i < TEST_TEX_PIXELS; i++)
        g_test_texture[i] = (i % 4 == 0) ? 0 : (U8)((i & 0xFE) | 1);
}

static void init_fog_palette(void) {
    for (int i = 0; i < 256; i++)
        Fill_Logical_Palette[i] = (U8)(255 - i);
}

static void setup_texz_ck_filler(U32 startY, U32 nbLines,
                                 U32 xmin_fp, U32 xmax_fp) {
    setup_texz_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_chromakey_texture();
}

static void setup_texz_fog_filler(U32 startY, U32 nbLines,
                                  U32 xmin_fp, U32 xmax_fp) {
    setup_texz_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_fog_palette();
}

static void setup_texz_ck_fog_filler(U32 startY, U32 nbLines,
                                     U32 xmin_fp, U32 xmax_fp) {
    setup_texz_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_chromakey_texture();
    init_fog_palette();
}

static void setup_texz_zbuf_filler(U32 startY, U32 nbLines,
                                   U32 xmin_fp, U32 xmax_fp) {
    setup_texz_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_test_zbuffer(0xFFFF);
    PtrZBuffer = (PTR_U16)g_test_zbuffer;
    Fill_CurZBufMin = 0x8000;
    Fill_ZBuf_LeftSlope = 0;
    Fill_ZBuf_XSlope = 0x100;
    Fill_CurZBuf = Fill_CurZBufMin;
}

static void setup_texz_ck_zbuf_filler(U32 startY, U32 nbLines,
                                      U32 xmin_fp, U32 xmax_fp) {
    setup_texz_zbuf_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_chromakey_texture();
}

static void setup_texz_fog_zbuf_filler(U32 startY, U32 nbLines,
                                       U32 xmin_fp, U32 xmax_fp) {
    setup_texz_zbuf_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_fog_palette();
}

static void setup_texz_ck_fog_zbuf_filler(U32 startY, U32 nbLines,
                                          U32 xmin_fp, U32 xmax_fp) {
    setup_texz_zbuf_filler(startY, nbLines, xmin_fp, xmax_fp);
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
#define DEFINE_TEXZ_EQUIV_FB(shortname, funcname, setup_func)             \
    static void test_asm_equiv_##shortname(void) {                        \
        setup_func(30, 4, 20 << 16, 50 << 16);                            \
        funcname(4, 20 << 16, 50 << 16);                                  \
        memcpy(texz_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);            \
                                                                          \
        setup_func(30, 4, 20 << 16, 50 << 16);                            \
        call_asm_##funcname(4, 20 << 16, 50 << 16);                       \
        memcpy(texz_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);            \
                                                                          \
        ASSERT_ASM_CPP_MEM_EQ(texz_asm_buf, texz_cpp_buf, TEST_POLY_SIZE, \
                              #funcname " framebuf");                     \
    }

/* Framebuffer-only random stress test (300 rounds) */
#define DEFINE_TEXZ_RANDOM_FB(shortname, funcname, setup_func, seed)                                    \
    static void test_asm_random_##shortname(void) {                                                     \
        poly_rng_seed(seed);                                                                            \
        for (int i = 0; i < 300; i++) {                                                                 \
            U32 y = poly_rng_next() % (TEST_POLY_H - 20);                                               \
            U32 h = 1 + poly_rng_next() % 8;                                                            \
            if (y + h + 1 >= (U32)TEST_POLY_H)                                                          \
                h = TEST_POLY_H - y - 2;                                                                \
            U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);                                              \
            U32 x1 = x0 + 5 + poly_rng_next() % 30;                                                     \
            if (x1 >= (U32)TEST_POLY_W)                                                                 \
                x1 = TEST_POLY_W - 1;                                                                   \
            U32 uslope = poly_rng_next() % 0x20000;                                                     \
            S32 w = 0x8000 + (S32)(poly_rng_next() % 0x18000);                                          \
                                                                                                        \
            setup_func(y, h, x0 << 16, x1 << 16);                                                       \
            Fill_MapU_XSlope = uslope;                                                                  \
            Fill_Cur_W = w;                                                                             \
            funcname(h, x0 << 16, x1 << 16);                                                            \
            memcpy(texz_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);                                      \
                                                                                                        \
            setup_func(y, h, x0 << 16, x1 << 16);                                                       \
            Fill_MapU_XSlope = uslope;                                                                  \
            Fill_Cur_W = w;                                                                             \
            call_asm_##funcname(h, x0 << 16, x1 << 16);                                                 \
            memcpy(texz_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);                                      \
                                                                                                        \
            for (int j = 0; j < TEST_POLY_SIZE; j++) {                                                  \
                if (texz_asm_buf[j] != texz_cpp_buf[j]) {                                               \
                    int row = j / TEST_POLY_W;                                                          \
                    int col = j % TEST_POLY_W;                                                          \
                    printf("# " #shortname " #%d: first diff byte %d "                                  \
                           "(row=%d col=%d) asm=0x%02x cpp=0x%02x\n",                                   \
                           i, j, row, col, texz_asm_buf[j], texz_cpp_buf[j]);                           \
                    break;                                                                              \
                }                                                                                       \
            }                                                                                           \
                                                                                                        \
            char msg[128];                                                                              \
            snprintf(msg, sizeof(msg), "random " #shortname " #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1); \
            ASSERT_ASM_CPP_MEM_EQ(texz_asm_buf, texz_cpp_buf,                                           \
                                  TEST_POLY_SIZE, msg);                                                 \
        }                                                                                               \
    }

/* Framebuffer + Z-buffer static equivalence test */
#define DEFINE_TEXZ_EQUIV_ZB(shortname, funcname, setup_func)             \
    static void test_asm_equiv_##shortname(void) {                        \
        setup_func(30, 4, 20 << 16, 50 << 16);                            \
        funcname(4, 20 << 16, 50 << 16);                                  \
        memcpy(texz_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);            \
        memcpy(texz_cpp_zbuf, g_test_zbuffer,                             \
               TEST_POLY_SIZE *(int)sizeof(U16));                         \
                                                                          \
        setup_func(30, 4, 20 << 16, 50 << 16);                            \
        call_asm_##funcname(4, 20 << 16, 50 << 16);                       \
        memcpy(texz_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);            \
        memcpy(texz_asm_zbuf, g_test_zbuffer,                             \
               TEST_POLY_SIZE *(int)sizeof(U16));                         \
                                                                          \
        ASSERT_ASM_CPP_MEM_EQ(texz_asm_buf, texz_cpp_buf, TEST_POLY_SIZE, \
                              #funcname " framebuf");                     \
        ASSERT_ASM_CPP_MEM_EQ((U8 *)texz_asm_zbuf, (U8 *)texz_cpp_zbuf,   \
                              TEST_POLY_SIZE *(int)sizeof(U16),           \
                              #funcname " zbuf");                         \
    }

/* Framebuffer + Z-buffer random stress test (300 rounds) */
#define DEFINE_TEXZ_RANDOM_ZB(shortname, funcname, setup_func, seed)                                    \
    static void test_asm_random_##shortname(void) {                                                     \
        poly_rng_seed(seed);                                                                            \
        for (int i = 0; i < 300; i++) {                                                                 \
            U32 y = poly_rng_next() % (TEST_POLY_H - 20);                                               \
            U32 h = 1 + poly_rng_next() % 8;                                                            \
            if (y + h + 1 >= (U32)TEST_POLY_H)                                                          \
                h = TEST_POLY_H - y - 2;                                                                \
            U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);                                              \
            U32 x1 = x0 + 5 + poly_rng_next() % 30;                                                     \
            if (x1 >= (U32)TEST_POLY_W)                                                                 \
                x1 = TEST_POLY_W - 1;                                                                   \
            U32 uslope = poly_rng_next() % 0x20000;                                                     \
            S32 w = 0x8000 + (S32)(poly_rng_next() % 0x18000);                                          \
            U32 zbmin = 0x4000 + poly_rng_next() % 0x8000;                                              \
            S32 zbxsl = (S32)(poly_rng_next() % 0x400);                                                 \
                                                                                                        \
            setup_func(y, h, x0 << 16, x1 << 16);                                                       \
            Fill_MapU_XSlope = uslope;                                                                  \
            Fill_Cur_W = w;                                                                             \
            Fill_CurZBufMin = zbmin;                                                                    \
            Fill_CurZBuf = zbmin;                                                                       \
            Fill_ZBuf_XSlope = zbxsl;                                                                   \
            funcname(h, x0 << 16, x1 << 16);                                                            \
            memcpy(texz_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);                                      \
            memcpy(texz_cpp_zbuf, g_test_zbuffer,                                                       \
                   TEST_POLY_SIZE *(int)sizeof(U16));                                                   \
                                                                                                        \
            setup_func(y, h, x0 << 16, x1 << 16);                                                       \
            Fill_MapU_XSlope = uslope;                                                                  \
            Fill_Cur_W = w;                                                                             \
            Fill_CurZBufMin = zbmin;                                                                    \
            Fill_CurZBuf = zbmin;                                                                       \
            Fill_ZBuf_XSlope = zbxsl;                                                                   \
            call_asm_##funcname(h, x0 << 16, x1 << 16);                                                 \
            memcpy(texz_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);                                      \
            memcpy(texz_asm_zbuf, g_test_zbuffer,                                                       \
                   TEST_POLY_SIZE *(int)sizeof(U16));                                                   \
                                                                                                        \
            for (int j = 0; j < TEST_POLY_SIZE; j++) {                                                  \
                if (texz_asm_buf[j] != texz_cpp_buf[j]) {                                               \
                    int row = j / TEST_POLY_W;                                                          \
                    int col = j % TEST_POLY_W;                                                          \
                    printf("# " #shortname " #%d: first diff byte %d "                                  \
                           "(row=%d col=%d) asm=0x%02x cpp=0x%02x\n",                                   \
                           i, j, row, col, texz_asm_buf[j], texz_cpp_buf[j]);                           \
                    break;                                                                              \
                }                                                                                       \
            }                                                                                           \
                                                                                                        \
            char msg[128];                                                                              \
            snprintf(msg, sizeof(msg), "random " #shortname " #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1); \
            ASSERT_ASM_CPP_MEM_EQ(texz_asm_buf, texz_cpp_buf,                                           \
                                  TEST_POLY_SIZE, msg);                                                 \
            snprintf(msg, sizeof(msg), "random " #shortname " zbuf #%d", i);                            \
            ASSERT_ASM_CPP_MEM_EQ((U8 *)texz_asm_zbuf, (U8 *)texz_cpp_zbuf,                             \
                                  TEST_POLY_SIZE *(int)sizeof(U16), msg);                               \
        }                                                                                               \
    }

/* ══════════════════════════════════════════════════════════════════
 *  Original Filler_TextureZ tests (unchanged)
 * ══════════════════════════════════════════════════════════════════ */

static void test_texz_cpp_narrow(void) {
    setup_texz_filler(50, 2, 60 << 16, 68 << 16);
    Filler_TextureZ(2, 60 << 16, 68 << 16);
    ASSERT_TRUE(1);
}

static void test_texz_cpp_wide(void) {
    setup_texz_filler(30, 4, 20 << 16, 80 << 16);
    Filler_TextureZ(4, 20 << 16, 80 << 16);
    ASSERT_TRUE(1);
}

static void test_asm_equiv_texz(void) {
    setup_texz_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureZ(4, 20 << 16, 50 << 16);
    memcpy(texz_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_texz_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureZ(4, 20 << 16, 50 << 16);
    memcpy(texz_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(texz_asm_buf, texz_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZ strip");
}

static void test_asm_random_texz(void) {
    poly_rng_seed(0xBEEFCAFE);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 uslope = poly_rng_next() % 0x20000;
        S32 w = 0x8000 + (S32)(poly_rng_next() % 0x18000);

        setup_texz_filler(y, h, x0 << 16, x1 << 16);
        Fill_MapU_XSlope = uslope;
        Fill_Cur_W = w;
        Filler_TextureZ(h, x0 << 16, x1 << 16);
        memcpy(texz_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_texz_filler(y, h, x0 << 16, x1 << 16);
        Fill_MapU_XSlope = uslope;
        Fill_Cur_W = w;
        call_asm_Filler_TextureZ(h, x0 << 16, x1 << 16);
        memcpy(texz_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        /* Check for first mismatch */
        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (texz_asm_buf[j] != texz_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# texz #%d w=%u: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, x1 - x0, j, row, col, texz_asm_buf[j], texz_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random texz #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(texz_asm_buf, texz_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  Framebuffer-only variants (no Z-buffer)
 * ══════════════════════════════════════════════════════════════════ */

/* ── Filler_TextureZFlat ───────────────────────────────────────── */
DEFINE_TEXZ_EQUIV_FB(texz_flat, Filler_TextureZFlat, setup_texz_filler)
DEFINE_TEXZ_RANDOM_FB(texz_flat, Filler_TextureZFlat, setup_texz_filler,
                      0xF1A70001)

/* ── Filler_TextureZChromaKey ──────────────────────────────────── */
DEFINE_TEXZ_EQUIV_FB(texz_ck, Filler_TextureZChromaKey, setup_texz_ck_filler)
DEFINE_TEXZ_RANDOM_FB(texz_ck, Filler_TextureZChromaKey, setup_texz_ck_filler,
                      0xC40A0002)

/* ── Filler_TextureZFlatChromaKey ──────────────────────────────── */
DEFINE_TEXZ_EQUIV_FB(texz_flat_ck, Filler_TextureZFlatChromaKey,
                     setup_texz_ck_filler)
DEFINE_TEXZ_RANDOM_FB(texz_flat_ck, Filler_TextureZFlatChromaKey,
                      setup_texz_ck_filler, 0xFC4A0003)

/* ── Filler_TextureZFog ────────────────────────────────────────── */
DEFINE_TEXZ_EQUIV_FB(texz_fog, Filler_TextureZFog, setup_texz_fog_filler)
DEFINE_TEXZ_RANDOM_FB(texz_fog, Filler_TextureZFog, setup_texz_fog_filler,
                      0xF0670004)

/* ── Filler_TextureZChromaKeyFog ───────────────────────────────── */
DEFINE_TEXZ_EQUIV_FB(texz_ck_fog, Filler_TextureZChromaKeyFog,
                     setup_texz_ck_fog_filler)
DEFINE_TEXZ_RANDOM_FB(texz_ck_fog, Filler_TextureZChromaKeyFog,
                      setup_texz_ck_fog_filler, 0xCF060005)

/* ══════════════════════════════════════════════════════════════════
 *  Z-buffer variants (compare framebuf + zbuf)
 * ══════════════════════════════════════════════════════════════════ */

/* ── Filler_TextureZZBuf ───────────────────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_zbuf, Filler_TextureZZBuf, setup_texz_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_zbuf, Filler_TextureZZBuf, setup_texz_zbuf_filler,
                      0x2B0F0006)

/* ── Filler_TextureZChromaKeyZBuf ──────────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_ck_zbuf, Filler_TextureZChromaKeyZBuf,
                     setup_texz_ck_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_ck_zbuf, Filler_TextureZChromaKeyZBuf,
                      setup_texz_ck_zbuf_filler, 0xC2BF0007)

/* ── Filler_TextureZFlatZBuf ───────────────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_flat_zbuf, Filler_TextureZFlatZBuf,
                     setup_texz_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_flat_zbuf, Filler_TextureZFlatZBuf,
                      setup_texz_zbuf_filler, 0xF2BF0008)

/* ── Filler_TextureZFlatChromaKeyZBuf ──────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_flat_ck_zbuf, Filler_TextureZFlatChromaKeyZBuf,
                     setup_texz_ck_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_flat_ck_zbuf, Filler_TextureZFlatChromaKeyZBuf,
                      setup_texz_ck_zbuf_filler, 0xFC2B0009)

/* ── Filler_TextureZFogZBuf ────────────────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_fog_zbuf, Filler_TextureZFogZBuf,
                     setup_texz_fog_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_fog_zbuf, Filler_TextureZFogZBuf,
                      setup_texz_fog_zbuf_filler, 0xF0B0000A)

/* ── Filler_TextureZChromaKeyFogZBuf ───────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_ck_fog_zbuf, Filler_TextureZChromaKeyFogZBuf,
                     setup_texz_ck_fog_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_ck_fog_zbuf, Filler_TextureZChromaKeyFogZBuf,
                      setup_texz_ck_fog_zbuf_filler, 0xCFB0000B)

/* ══════════════════════════════════════════════════════════════════
 *  NZW variants (No Z Write — compare framebuf + zbuf)
 * ══════════════════════════════════════════════════════════════════ */

/* ── Filler_TextureZNZW ────────────────────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_nzw, Filler_TextureZNZW, setup_texz_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_nzw, Filler_TextureZNZW, setup_texz_zbuf_filler,
                      0x0200000C)

/* ── Filler_TextureZChromaKeyNZW ───────────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_ck_nzw, Filler_TextureZChromaKeyNZW,
                     setup_texz_ck_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_ck_nzw, Filler_TextureZChromaKeyNZW,
                      setup_texz_ck_zbuf_filler, 0xC20D000D)

/* ── Filler_TextureZFlatNZW ────────────────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_flat_nzw, Filler_TextureZFlatNZW,
                     setup_texz_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_flat_nzw, Filler_TextureZFlatNZW,
                      setup_texz_zbuf_filler, 0xF20D000E)

/* ── Filler_TextureZFlatChromaKeyNZW ───────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_flat_ck_nzw, Filler_TextureZFlatChromaKeyNZW,
                     setup_texz_ck_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_flat_ck_nzw, Filler_TextureZFlatChromaKeyNZW,
                      setup_texz_ck_zbuf_filler, 0xFC2D000F)

/* ── Filler_TextureZFogNZW ─────────────────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_fog_nzw, Filler_TextureZFogNZW,
                     setup_texz_fog_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_fog_nzw, Filler_TextureZFogNZW,
                      setup_texz_fog_zbuf_filler, 0xF02D0010)

/* ── Filler_TextureZChromaKeyFogNZW ────────────────────────────── */
DEFINE_TEXZ_EQUIV_ZB(texz_ck_fog_nzw, Filler_TextureZChromaKeyFogNZW,
                     setup_texz_ck_fog_zbuf_filler)
DEFINE_TEXZ_RANDOM_ZB(texz_ck_fog_nzw, Filler_TextureZChromaKeyFogNZW,
                      setup_texz_ck_fog_zbuf_filler, 0xCF2D0011)

/* ══════════════════════════════════════════════════════════════════ */

int main(void) {
    /* Original Filler_TextureZ tests */
    RUN_TEST(test_texz_declares_exist);
    RUN_TEST(test_texz_cpp_narrow);
    RUN_TEST(test_texz_cpp_wide);
    RUN_TEST(test_asm_equiv_texz);
    RUN_TEST(test_asm_random_texz);

    /* Framebuffer-only variants */
    RUN_TEST(test_asm_equiv_texz_flat);
    RUN_TEST(test_asm_random_texz_flat);
    RUN_TEST(test_asm_equiv_texz_ck);
    RUN_TEST(test_asm_random_texz_ck);
    RUN_TEST(test_asm_equiv_texz_flat_ck);
    RUN_TEST(test_asm_random_texz_flat_ck);
    RUN_TEST(test_asm_equiv_texz_fog);
    RUN_TEST(test_asm_random_texz_fog);
    RUN_TEST(test_asm_equiv_texz_ck_fog);
    RUN_TEST(test_asm_random_texz_ck_fog);

    /* Z-buffer variants */
    RUN_TEST(test_asm_equiv_texz_zbuf);
    RUN_TEST(test_asm_random_texz_zbuf);
    RUN_TEST(test_asm_equiv_texz_ck_zbuf);
    RUN_TEST(test_asm_random_texz_ck_zbuf);
    RUN_TEST(test_asm_equiv_texz_flat_zbuf);
    RUN_TEST(test_asm_random_texz_flat_zbuf);
    RUN_TEST(test_asm_equiv_texz_flat_ck_zbuf);
    RUN_TEST(test_asm_random_texz_flat_ck_zbuf);
    RUN_TEST(test_asm_equiv_texz_fog_zbuf);
    RUN_TEST(test_asm_random_texz_fog_zbuf);
    RUN_TEST(test_asm_equiv_texz_ck_fog_zbuf);
    RUN_TEST(test_asm_random_texz_ck_fog_zbuf);

    /* NZW variants */
    RUN_TEST(test_asm_equiv_texz_nzw);
    RUN_TEST(test_asm_random_texz_nzw);
    RUN_TEST(test_asm_equiv_texz_ck_nzw);
    RUN_TEST(test_asm_random_texz_ck_nzw);
    RUN_TEST(test_asm_equiv_texz_flat_nzw);
    RUN_TEST(test_asm_random_texz_flat_nzw);
    RUN_TEST(test_asm_equiv_texz_flat_ck_nzw);
    RUN_TEST(test_asm_random_texz_flat_ck_nzw);
    RUN_TEST(test_asm_equiv_texz_fog_nzw);
    RUN_TEST(test_asm_random_texz_fog_nzw);
    RUN_TEST(test_asm_equiv_texz_ck_fog_nzw);
    RUN_TEST(test_asm_random_texz_ck_fog_nzw);

    TEST_SUMMARY();
    return test_failures != 0;
}
