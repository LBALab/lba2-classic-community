/* Test: TextureZ Gouraud rendering (POLYTZG.CPP) — filler-level ASM-vs-CPP.
 *
 * Tests all 6 Filler_TextureZGouraud* functions with perspective-correct
 * texture mapping and gouraud shading, comparing ASM vs CPP byte-for-byte.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYTZG.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

/* F_256 constant needed by ASM perspective code */
extern "C" {
float F_256 = 256.0f;
}

/* ── ASM wrappers ───────────────────────────────────────────────── */

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

DECLARE_ASM_FILLER(Filler_TextureZGouraud)
DECLARE_ASM_FILLER(Filler_TextureZGouraudChromaKey)
DECLARE_ASM_FILLER(Filler_TextureZGouraudZBuf)
DECLARE_ASM_FILLER(Filler_TextureZGouraudChromaKeyZBuf)
DECLARE_ASM_FILLER(Filler_TextureZGouraudNZW)
DECLARE_ASM_FILLER(Filler_TextureZGouraudChromaKeyNZW)

/* ── Buffers ────────────────────────────────────────────────────── */

static U8 tzg_cpp_buf[TEST_POLY_SIZE];
static U8 tzg_asm_buf[TEST_POLY_SIZE];
static U16 tzg_cpp_zbuf[TEST_POLY_SIZE];
static U16 tzg_asm_zbuf[TEST_POLY_SIZE];

/* ── Setup helpers ──────────────────────────────────────────────── */

static void setup_tzg_filler(U32 startY, U32 nbLines,
                             U32 xmin_fp, U32 xmax_fp) {
    setup_filler_common(startY, nbLines, xmin_fp, xmax_fp, 0);
    init_test_texture();
    init_test_clut();
    PtrMap = g_test_texture;
    RepMask = 0xFFFF;
    PtrCLUTGouraud = g_test_clut;
    Fill_Patch = 1;

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

    Fill_CurGouraudMin = 0x020000;
    Fill_Gouraud_LeftSlope = 0;
    Fill_Gouraud_XSlope = 0x800;
}

static void init_chromakey_texture(void) {
    for (int i = 0; i < TEST_TEX_PIXELS; i++)
        g_test_texture[i] = (i % 4 == 0) ? 0 : (U8)((i & 0xFE) | 1);
}

static void setup_tzg_zbuf_filler(U32 startY, U32 nbLines,
                                  U32 xmin_fp, U32 xmax_fp) {
    setup_tzg_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_test_zbuffer(0xFFFF);
    PtrZBuffer = (PTR_U16)g_test_zbuffer;
    Fill_CurZBufMin = 0x10000;
    Fill_ZBuf_LeftSlope = 0;
    Fill_ZBuf_XSlope = 0;
    Fill_CurZBuf = 0;
}

/* ══════════════════════════════════════════════════════════════════
 *  1. Filler_TextureZGouraud
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_tzg(void) {
    setup_tzg_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureZGouraud(4, 20 << 16, 50 << 16);
    memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_tzg_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureZGouraud(4, 20 << 16, 50 << 16);
    memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZGouraud strip");
}

static void test_asm_random_tzg(void) {
    poly_rng_seed(0xA1B20001);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;

        setup_tzg_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Filler_TextureZGouraud(h, x0 << 16, x1 << 16);
        memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_tzg_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        call_asm_Filler_TextureZGouraud(h, x0 << 16, x1 << 16);
        memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzg_asm_buf[j] != tzg_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzg #%d: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, j, row, col, tzg_asm_buf[j], tzg_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzg #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  2. Filler_TextureZGouraudChromaKey
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_tzg_chromakey(void) {
    setup_tzg_filler(30, 4, 20 << 16, 50 << 16);
    init_chromakey_texture();
    Filler_TextureZGouraudChromaKey(4, 20 << 16, 50 << 16);
    memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_tzg_filler(30, 4, 20 << 16, 50 << 16);
    init_chromakey_texture();
    call_asm_Filler_TextureZGouraudChromaKey(4, 20 << 16, 50 << 16);
    memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZGouraudChromaKey strip");
}

static void test_asm_random_tzg_chromakey(void) {
    poly_rng_seed(0xA1C40002);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;

        setup_tzg_filler(y, h, x0 << 16, x1 << 16);
        init_chromakey_texture();
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Filler_TextureZGouraudChromaKey(h, x0 << 16, x1 << 16);
        memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_tzg_filler(y, h, x0 << 16, x1 << 16);
        init_chromakey_texture();
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        call_asm_Filler_TextureZGouraudChromaKey(h, x0 << 16, x1 << 16);
        memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzg_asm_buf[j] != tzg_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzg_ck #%d: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, j, row, col, tzg_asm_buf[j], tzg_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzg_ck #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  3. Filler_TextureZGouraudZBuf
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_tzg_zbuf(void) {
    setup_tzg_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureZGouraudZBuf(4, 20 << 16, 50 << 16);
    memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzg_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_tzg_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureZGouraudZBuf(4, 20 << 16, 50 << 16);
    memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzg_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZGouraudZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)tzg_asm_zbuf, (U8 *)tzg_cpp_zbuf,
                          TEST_POLY_SIZE * (int)sizeof(U16),
                          "Filler_TextureZGouraudZBuf zbuf");
}

static void test_asm_random_tzg_zbuf(void) {
    poly_rng_seed(0xA12B0003);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_tzg_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureZGouraudZBuf(h, x0 << 16, x1 << 16);
        memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzg_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_tzg_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureZGouraudZBuf(h, x0 << 16, x1 << 16);
        memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzg_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzg_asm_buf[j] != tzg_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzg_zbuf #%d: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, j, row, col, tzg_asm_buf[j], tzg_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzg_zbuf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random tzg_zbuf zbuf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)tzg_asm_zbuf, (U8 *)tzg_cpp_zbuf,
                              TEST_POLY_SIZE * (int)sizeof(U16), msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  4. Filler_TextureZGouraudChromaKeyZBuf
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_tzg_chromakey_zbuf(void) {
    setup_tzg_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    init_chromakey_texture();
    Filler_TextureZGouraudChromaKeyZBuf(4, 20 << 16, 50 << 16);
    memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzg_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_tzg_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    init_chromakey_texture();
    call_asm_Filler_TextureZGouraudChromaKeyZBuf(4, 20 << 16, 50 << 16);
    memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzg_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZGouraudChromaKeyZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)tzg_asm_zbuf, (U8 *)tzg_cpp_zbuf,
                          TEST_POLY_SIZE * (int)sizeof(U16),
                          "Filler_TextureZGouraudChromaKeyZBuf zbuf");
}

static void test_asm_random_tzg_chromakey_zbuf(void) {
    poly_rng_seed(0xA1CB0004);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_tzg_zbuf_filler(y, h, x0 << 16, x1 << 16);
        init_chromakey_texture();
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureZGouraudChromaKeyZBuf(h, x0 << 16, x1 << 16);
        memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzg_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_tzg_zbuf_filler(y, h, x0 << 16, x1 << 16);
        init_chromakey_texture();
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureZGouraudChromaKeyZBuf(h, x0 << 16, x1 << 16);
        memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzg_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzg_asm_buf[j] != tzg_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzg_ck_zbuf #%d: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, j, row, col, tzg_asm_buf[j], tzg_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzg_ck_zbuf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random tzg_ck_zbuf zb #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)tzg_asm_zbuf, (U8 *)tzg_cpp_zbuf,
                              TEST_POLY_SIZE * (int)sizeof(U16), msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  5. Filler_TextureZGouraudNZW
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_tzg_nzw(void) {
    setup_tzg_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureZGouraudNZW(4, 20 << 16, 50 << 16);
    memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzg_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_tzg_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureZGouraudNZW(4, 20 << 16, 50 << 16);
    memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzg_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZGouraudNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)tzg_asm_zbuf, (U8 *)tzg_cpp_zbuf,
                          TEST_POLY_SIZE * (int)sizeof(U16),
                          "Filler_TextureZGouraudNZW zbuf unchanged");
}

static void test_asm_random_tzg_nzw(void) {
    poly_rng_seed(0xA1020005);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_tzg_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureZGouraudNZW(h, x0 << 16, x1 << 16);
        memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzg_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_tzg_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureZGouraudNZW(h, x0 << 16, x1 << 16);
        memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzg_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzg_asm_buf[j] != tzg_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzg_nzw #%d: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, j, row, col, tzg_asm_buf[j], tzg_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzg_nzw #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random tzg_nzw zbuf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)tzg_asm_zbuf, (U8 *)tzg_cpp_zbuf,
                              TEST_POLY_SIZE * (int)sizeof(U16), msg);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  6. Filler_TextureZGouraudChromaKeyNZW
 * ══════════════════════════════════════════════════════════════════ */

static void test_asm_equiv_tzg_chromakey_nzw(void) {
    setup_tzg_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    init_chromakey_texture();
    Filler_TextureZGouraudChromaKeyNZW(4, 20 << 16, 50 << 16);
    memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzg_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_tzg_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    init_chromakey_texture();
    call_asm_Filler_TextureZGouraudChromaKeyNZW(4, 20 << 16, 50 << 16);
    memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzg_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZGouraudChromaKeyNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)tzg_asm_zbuf, (U8 *)tzg_cpp_zbuf,
                          TEST_POLY_SIZE * (int)sizeof(U16),
                          "Filler_TextureZGouraudChromaKeyNZW zbuf unchanged");
}

static void test_asm_random_tzg_chromakey_nzw(void) {
    poly_rng_seed(0xA1C20006);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_tzg_zbuf_filler(y, h, x0 << 16, x1 << 16);
        init_chromakey_texture();
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureZGouraudChromaKeyNZW(h, x0 << 16, x1 << 16);
        memcpy(tzg_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzg_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_tzg_zbuf_filler(y, h, x0 << 16, x1 << 16);
        init_chromakey_texture();
        Fill_CurGouraudMin = gStart;
        Fill_Gouraud_XSlope = gSlope;
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureZGouraudChromaKeyNZW(h, x0 << 16, x1 << 16);
        memcpy(tzg_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzg_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzg_asm_buf[j] != tzg_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzg_ck_nzw #%d: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, j, row, col, tzg_asm_buf[j], tzg_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzg_ck_nzw #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tzg_asm_buf, tzg_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random tzg_ck_nzw zb #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)tzg_asm_zbuf, (U8 *)tzg_cpp_zbuf,
                              TEST_POLY_SIZE * (int)sizeof(U16), msg);
    }
}

int main(void) {
    RUN_TEST(test_asm_equiv_tzg);
    RUN_TEST(test_asm_random_tzg);
    RUN_TEST(test_asm_equiv_tzg_chromakey);
    RUN_TEST(test_asm_random_tzg_chromakey);
    RUN_TEST(test_asm_equiv_tzg_zbuf);
    RUN_TEST(test_asm_random_tzg_zbuf);
    RUN_TEST(test_asm_equiv_tzg_chromakey_zbuf);
    RUN_TEST(test_asm_random_tzg_chromakey_zbuf);
    RUN_TEST(test_asm_equiv_tzg_nzw);
    RUN_TEST(test_asm_random_tzg_nzw);
    RUN_TEST(test_asm_equiv_tzg_chromakey_nzw);
    RUN_TEST(test_asm_random_tzg_chromakey_nzw);
    TEST_SUMMARY();
    return test_failures != 0;
}
