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

static void assert_pixels_within_bounds(S16 min_x, S16 min_y, S16 max_x, S16 max_y) {
    for (int y = 0; y < TEST_POLY_H; ++y) {
        for (int x = 0; x < TEST_POLY_W; ++x) {
            U8 pixel = g_poly_framebuf[y * TEST_POLY_W + x];
            if (pixel == 0)
                continue;
            ASSERT_TRUE(x >= min_x - 1);
            ASSERT_TRUE(x <= max_x + 1);
            ASSERT_TRUE(y >= min_y);
            ASSERT_TRUE(y <= max_y);
        }
    }
}

static void setup_gouraud_screen(void) {
    setup_polygon_screen();
    /* Build a simple gouraud CLUT */
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 256; col++)
            g_gouraud_clut[row * 256 + col] = (U8)((row << 4) | (col & 0xF));
    PtrCLUTGouraud = g_gouraud_clut;
}

/* ── Gouraud triangle ──────────────────────────────────────────── */

static void test_gouraud_basic(void) {
    setup_gouraud_screen();
    Struc_Point pts[3];
    /* Light values span 0x0000 to 0x0E00 (full range for 16-row CLUT) */
    pts[0] = make_point_lit(80, 10, 0x0000);
    pts[1] = make_point_lit(40, 100, 0x0800);
    pts[2] = make_point_lit(120, 100, 0x0E00);
    Fill_Poly(POLY_GOURAUD, 0x05, 3, pts);
    /* Some pixels should be written */
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_EQ_INT(3640, n);
}

static void test_gouraud_uniform_light(void) {
    setup_gouraud_screen();
    Struc_Point pts[3];
    /* All vertices at same intensity → uniform output */
    pts[0] = make_point_lit(40, 20, 0x0800);
    pts[1] = make_point_lit(20, 90, 0x0800);
    pts[2] = make_point_lit(80, 90, 0x0800);
    Fill_Poly(POLY_GOURAUD, 0x03, 3, pts);
    /* Centre pixel should be set (non-zero) */
    U8 centre = g_poly_framebuf[60 * TEST_POLY_W + 40];
    ASSERT_EQ_UINT(11, centre);
}

/* ── Dither triangle ───────────────────────────────────────────── */

static void test_dither_basic(void) {
    setup_gouraud_screen();
    Struc_Point pts[3];
    pts[0] = make_point_lit(60, 10, 0x0300);
    pts[1] = make_point_lit(20, 100, 0x0900);
    pts[2] = make_point_lit(100, 100, 0x0600);
    Fill_Poly(POLY_DITHER, 0x07, 3, pts);
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_EQ_INT(3640, n);
}

/* ── Edge cases ────────────────────────────────────────────────── */

static void test_gouraud_offscreen(void) {
    setup_gouraud_screen();
    Struc_Point pts[3];
    pts[0] = make_point_lit(-50, -50, 0x0800);
    pts[1] = make_point_lit(-30, -10, 0x0400);
    pts[2] = make_point_lit(-10, -30, 0x0C00);
    Fill_Poly(POLY_GOURAUD, 0x01, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

static void test_gouraud_clipped(void) {
    static U8 first_pass[TEST_POLY_SIZE];
    setup_gouraud_screen();
    Struc_Point pts[3];
    Struc_Point pts_repeat[3];
    /* Triangle partially off-screen */
    pts[0] = make_point_lit(-10, 50, 0x0800);
    pts[1] = make_point_lit(60, 20, 0x0400);
    pts[2] = make_point_lit(60, 80, 0x0C00);
    memcpy(pts_repeat, pts, sizeof(pts));
    Fill_Poly(POLY_GOURAUD, 0x02, 3, pts);
    memcpy(first_pass, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gouraud_screen();
    Fill_Poly(POLY_GOURAUD, 0x02, 3, pts_repeat);

    ASSERT_ASM_CPP_MEM_EQ(first_pass, g_poly_framebuf, TEST_POLY_SIZE,
                          "Fill_Poly Gouraud clipped repeatable");
    assert_pixels_within_bounds(-10, 20, 60, 80);
}

/* ── Randomized stress test ────────────────────────────────────── */

static void test_gouraud_random(void) {
    static U8 first_pass[TEST_POLY_SIZE];

    poly_rng_seed(0xDEADBEEF);
    for (int i = 0; i < 100; i++) {
        Struc_Point pts[3];
        Struc_Point pts_repeat[3];
        S16 min_x = TEST_POLY_W;
        S16 min_y = TEST_POLY_H;
        S16 max_x = -1;
        S16 max_y = -1;
        for (int v = 0; v < 3; v++) {
            S16 x = (S16)((poly_rng_next() % (TEST_POLY_W + 40)) - 20);
            S16 y = (S16)((poly_rng_next() % (TEST_POLY_H + 40)) - 20);
            U16 light = (U16)((poly_rng_next() % 14) << 8);
            pts[v] = make_point_lit(x, y, light);
            pts_repeat[v] = pts[v];
            if (x < min_x)
                min_x = x;
            if (y < min_y)
                min_y = y;
            if (x > max_x)
                max_x = x;
            if (y > max_y)
                max_y = y;
        }
        U8 color = (U8)(poly_rng_next() & 0x0F);
        S32 type = (poly_rng_next() & 1) ? POLY_GOURAUD : POLY_DITHER;

        setup_gouraud_screen();
        Fill_Poly(type, color, 3, pts);
        memcpy(first_pass, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gouraud_screen();
        Fill_Poly(type, color, 3, pts_repeat);

        char label[96];
        snprintf(label, sizeof(label), "Fill_Poly Gouraud random #%d type=%d", i, type);
        ASSERT_ASM_CPP_MEM_EQ(first_pass, g_poly_framebuf, TEST_POLY_SIZE, label);
        assert_pixels_within_bounds(min_x, min_y, max_x, max_y);
    }
}

/* ── ASM-vs-CPP equivalence (filler level) ─────────────────────── */

extern "C" void asm_Filler_Gouraud(void);

static inline S32 call_asm_Filler_Gouraud(U32 nbLines, U32 xmin, U32 xmax) {
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Filler_Gouraud\n\t"
        "pop %%ebp"
        : "=a"(result)
        : "c"(nbLines), "b"(xmin), "d"(xmax)
        : "edi", "esi", "memory", "cc");
    return result;
}

static U8 gour_cpp_buf[TEST_POLY_SIZE];
static U8 gour_asm_buf[TEST_POLY_SIZE];

static void setup_gouraud_filler(U32 startY, U32 nbLines,
                                 U32 xmin_fp, U32 xmax_fp,
                                 U32 gouraudStart, U32 gouraudXSlope) {
    setup_filler_common(startY, nbLines, xmin_fp, xmax_fp, 0x05);
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 256; col++)
            g_gouraud_clut[row * 256 + col] = (U8)((row << 4) | (col & 0xF));
    PtrCLUTGouraud = g_gouraud_clut;
    Fill_CurGouraudMin = gouraudStart;
    Fill_Gouraud_LeftSlope = 0;
    Fill_Gouraud_XSlope = gouraudXSlope;
}

static void test_asm_equiv_gouraud(void) {
    setup_gouraud_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_Gouraud(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gouraud_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_Gouraud(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_Gouraud basic");
}

static void test_asm_equiv_gouraud_flat(void) {
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

static void test_asm_random_gouraud(void) {
    poly_rng_seed(0xCAFEBABE);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
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
/* ── ASM filler macro for remaining gouraud variants ───────────── */

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

DECLARE_ASM_FILLER(Filler_Dither)
DECLARE_ASM_FILLER(Filler_GouraudTable)
DECLARE_ASM_FILLER(Filler_DitherTable)
DECLARE_ASM_FILLER(Filler_GouraudFog)
DECLARE_ASM_FILLER(Filler_DitherFog)
DECLARE_ASM_FILLER(Filler_GouraudZBuf)
DECLARE_ASM_FILLER(Filler_DitherZBuf)
DECLARE_ASM_FILLER(Filler_GouraudTableZBuf)
DECLARE_ASM_FILLER(Filler_DitherTableZBuf)
DECLARE_ASM_FILLER(Filler_GouraudFogZBuf)
DECLARE_ASM_FILLER(Filler_DitherFogZBuf)
DECLARE_ASM_FILLER(Filler_GouraudNZW)
DECLARE_ASM_FILLER(Filler_DitherNZW)
DECLARE_ASM_FILLER(Filler_GouraudTableNZW)
DECLARE_ASM_FILLER(Filler_DitherTableNZW)
DECLARE_ASM_FILLER(Filler_GouraudFogNZW)
DECLARE_ASM_FILLER(Filler_DitherFogNZW)

static U16 gour_cpp_zbuf[TEST_POLY_SIZE];
static U16 gour_asm_zbuf[TEST_POLY_SIZE];

static void init_fog_palette(void) {
    for (int i = 0; i < 256; i++)
        Fill_Logical_Palette[i] = (U8)(255 - i);
}

static void setup_gouraud_table_filler(U32 startY, U32 nbLines,
                                       U32 xmin_fp, U32 xmax_fp,
                                       U32 gouraudStart, U32 gouraudXSlope) {
    setup_gouraud_filler(startY, nbLines, xmin_fp, xmax_fp, gouraudStart, gouraudXSlope);
    init_test_clut();
    Fill_Color.Ptr = g_test_clut;
}

static void setup_gouraud_fog_filler(U32 startY, U32 nbLines,
                                     U32 xmin_fp, U32 xmax_fp,
                                     U32 gouraudStart, U32 gouraudXSlope) {
    setup_gouraud_filler(startY, nbLines, xmin_fp, xmax_fp, gouraudStart, gouraudXSlope);
    init_fog_palette();
}

static void setup_gouraud_zbuf_filler(U32 startY, U32 nbLines,
                                      U32 xmin_fp, U32 xmax_fp,
                                      U32 gouraudStart, U32 gouraudXSlope) {
    setup_gouraud_filler(startY, nbLines, xmin_fp, xmax_fp, gouraudStart, gouraudXSlope);
    Fill_Patch = 1; /* ZBuf init block requires patch == 0 i.e. Fill_Patch == 1 */
    init_test_zbuffer(0xFFFF);
    PtrZBuffer = (PTR_U16)g_test_zbuffer;
    Fill_CurZBufMin = 0x8000;
    Fill_ZBuf_LeftSlope = 0;
    Fill_ZBuf_XSlope = 0x100;
    Fill_CurZBuf = Fill_CurZBufMin;
}

static void setup_gouraud_table_zbuf_filler(U32 startY, U32 nbLines,
                                            U32 xmin_fp, U32 xmax_fp,
                                            U32 gouraudStart, U32 gouraudXSlope) {
    setup_gouraud_zbuf_filler(startY, nbLines, xmin_fp, xmax_fp, gouraudStart, gouraudXSlope);
    init_test_clut();
    Fill_Color.Ptr = g_test_clut;
}

static void setup_gouraud_fog_zbuf_filler(U32 startY, U32 nbLines,
                                          U32 xmin_fp, U32 xmax_fp,
                                          U32 gouraudStart, U32 gouraudXSlope) {
    setup_gouraud_zbuf_filler(startY, nbLines, xmin_fp, xmax_fp, gouraudStart, gouraudXSlope);
    init_fog_palette();
}

/* ── Filler_Dither ─────────────────────────────────────────────── */

static void test_asm_equiv_dither(void) {
    setup_gouraud_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_Dither(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gouraud_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_Dither(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_Dither basic");
}

static void test_asm_random_dither(void) {
    poly_rng_seed(0xD1740001);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;

        setup_gouraud_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Filler_Dither(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gouraud_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        call_asm_Filler_Dither(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random Dither #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ── Filler_GouraudTable ───────────────────────────────────────── */

static void test_asm_equiv_gouraud_table(void) {
    setup_gouraud_table_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_GouraudTable(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gouraud_table_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_GouraudTable(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_GouraudTable basic");
}

static void test_asm_random_gouraud_table(void) {
    poly_rng_seed(0x67A80002);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;

        setup_gouraud_table_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Filler_GouraudTable(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gouraud_table_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        call_asm_Filler_GouraudTable(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random GouraudTable #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ── Filler_DitherTable ────────────────────────────────────────── */

static void test_asm_equiv_dither_table(void) {
    setup_gouraud_table_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_DitherTable(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gouraud_table_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_DitherTable(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_DitherTable basic");
}

static void test_asm_random_dither_table(void) {
    poly_rng_seed(0xD7A80003);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;

        setup_gouraud_table_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Filler_DitherTable(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gouraud_table_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        call_asm_Filler_DitherTable(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random DitherTable #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* Regression (#47): a scanline width that is a nonzero multiple of 32 makes
 * (ditherCount & 31) == 0 in Filler_DitherTable's dither-pattern rotate, so the
 * original `>> (32 - (ditherCount & 31))` shifted a 32-bit value by 32 — UB.
 * Zero edge slopes keep the width constant, so widths 32 and 64 hit the rotate
 * on every scanline. x86 masks shift counts so buggy/fixed output matches here;
 * the case earns its keep under UBSan and on targets that don't mask. */
static void test_asm_equiv_dither_table_width_mult_32(void) {
    static const U32 widths[2] = {32, 64};
    for (int w = 0; w < 2; w++) {
        U32 xmin = 16 << 16;
        U32 xmax = (16 + widths[w]) << 16;

        setup_gouraud_table_filler(30, 4, xmin, xmax, 0x020000, 0x800);
        Filler_DitherTable(4, xmin, xmax);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gouraud_table_filler(30, 4, xmin, xmax, 0x020000, 0x800);
        call_asm_Filler_DitherTable(4, xmin, xmax);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[96];
        snprintf(msg, sizeof(msg),
                 "Filler_DitherTable width=%u (dither-rotate shift-by-32 UB)",
                 widths[w]);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ── Filler_GouraudFog ─────────────────────────────────────────── */

static void test_asm_equiv_gouraud_fog(void) {
    setup_gouraud_fog_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_GouraudFog(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gouraud_fog_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_GouraudFog(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_GouraudFog basic");
}

static void test_asm_random_gouraud_fog(void) {
    poly_rng_seed(0x6F060004);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;

        setup_gouraud_fog_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Filler_GouraudFog(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gouraud_fog_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        call_asm_Filler_GouraudFog(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random GouraudFog #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ── Filler_DitherFog ──────────────────────────────────────────── */

static void test_asm_equiv_dither_fog(void) {
    setup_gouraud_fog_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_DitherFog(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_gouraud_fog_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_DitherFog(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_DitherFog basic");
}

static void test_asm_random_dither_fog(void) {
    poly_rng_seed(0xDF060005);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;

        setup_gouraud_fog_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Filler_DitherFog(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_gouraud_fog_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        call_asm_Filler_DitherFog(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "random DitherFog #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ── Filler_GouraudZBuf ────────────────────────────────────────── */

static void test_asm_equiv_gouraud_zbuf(void) {
    setup_gouraud_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_GouraudZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_GouraudZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_GouraudZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16), "Filler_GouraudZBuf zbuf");
}

static void test_asm_random_gouraud_zbuf(void) {
    poly_rng_seed(0x62B00006);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_GouraudZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_GouraudZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random GouraudZBuf #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random GouraudZBuf Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_DitherZBuf ─────────────────────────────────────────── */

static void test_asm_equiv_dither_zbuf(void) {
    setup_gouraud_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_DitherZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_DitherZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_DitherZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16), "Filler_DitherZBuf zbuf");
}

static void test_asm_random_dither_zbuf(void) {
    poly_rng_seed(0xD2B00007);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_DitherZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_DitherZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random DitherZBuf #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random DitherZBuf Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_GouraudTableZBuf ───────────────────────────────────── */

static void test_asm_equiv_gouraud_table_zbuf(void) {
    setup_gouraud_table_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_GouraudTableZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_table_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_GouraudTableZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_GouraudTableZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16), "Filler_GouraudTableZBuf zbuf");
}

static void test_asm_random_gouraud_table_zbuf(void) {
    poly_rng_seed(0x67B00008);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_table_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_GouraudTableZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_table_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_GouraudTableZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random GouraudTableZBuf #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random GouraudTableZBuf Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_DitherTableZBuf ────────────────────────────────────── */

static void test_asm_equiv_dither_table_zbuf(void) {
    setup_gouraud_table_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_DitherTableZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_table_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_DitherTableZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_DitherTableZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16), "Filler_DitherTableZBuf zbuf");
}

static void test_asm_random_dither_table_zbuf(void) {
    poly_rng_seed(0xD7B00009);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_table_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_DitherTableZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_table_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_DitherTableZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random DitherTableZBuf #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random DitherTableZBuf Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_GouraudFogZBuf ─────────────────────────────────────── */

static void test_asm_equiv_gouraud_fog_zbuf(void) {
    setup_gouraud_fog_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_GouraudFogZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_fog_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_GouraudFogZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_GouraudFogZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16), "Filler_GouraudFogZBuf zbuf");
}

static void test_asm_random_gouraud_fog_zbuf(void) {
    poly_rng_seed(0x6FB0000A);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_fog_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_GouraudFogZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_fog_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_GouraudFogZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random GouraudFogZBuf #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random GouraudFogZBuf Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_DitherFogZBuf ──────────────────────────────────────── */

static void test_asm_equiv_dither_fog_zbuf(void) {
    setup_gouraud_fog_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_DitherFogZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_fog_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_DitherFogZBuf(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_DitherFogZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16), "Filler_DitherFogZBuf zbuf");
}

static void test_asm_random_dither_fog_zbuf(void) {
    poly_rng_seed(0xDFB0000B);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_fog_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_DitherFogZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_fog_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_DitherFogZBuf(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random DitherFogZBuf #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random DitherFogZBuf Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_GouraudNZW ─────────────────────────────────────────── */

static void test_asm_equiv_gouraud_nzw(void) {
    setup_gouraud_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_GouraudNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_GouraudNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_GouraudNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16),
                          "Filler_GouraudNZW zbuf");
    /* Verify NZW did not modify Z-buffer */
    {
        int zbuf_modified = 0;
        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (gour_asm_zbuf[j] != 0xFFFF) {
                zbuf_modified = 1;
                break;
            }
        }
        ASSERT_EQ_INT(0, zbuf_modified);
    }
}

static void test_asm_random_gouraud_nzw(void) {
    poly_rng_seed(0x620D000C);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_GouraudNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_GouraudNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random GouraudNZW #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random GouraudNZW Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_DitherNZW ──────────────────────────────────────────── */

static void test_asm_equiv_dither_nzw(void) {
    setup_gouraud_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_DitherNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_DitherNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_DitherNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16),
                          "Filler_DitherNZW zbuf");
    /* Verify NZW did not modify Z-buffer */
    {
        int zbuf_modified = 0;
        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (gour_asm_zbuf[j] != 0xFFFF) {
                zbuf_modified = 1;
                break;
            }
        }
        ASSERT_EQ_INT(0, zbuf_modified);
    }
}

static void test_asm_random_dither_nzw(void) {
    poly_rng_seed(0xD20D000D);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_DitherNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_DitherNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random DitherNZW #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random DitherNZW Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_GouraudTableNZW ────────────────────────────────────── */

static void test_asm_equiv_gouraud_table_nzw(void) {
    setup_gouraud_table_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_GouraudTableNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_table_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_GouraudTableNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_GouraudTableNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16),
                          "Filler_GouraudTableNZW zbuf");
    /* Verify NZW did not modify Z-buffer */
    {
        int zbuf_modified = 0;
        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (gour_asm_zbuf[j] != 0xFFFF) {
                zbuf_modified = 1;
                break;
            }
        }
        ASSERT_EQ_INT(0, zbuf_modified);
    }
}

static void test_asm_random_gouraud_table_nzw(void) {
    poly_rng_seed(0x670D000E);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_table_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_GouraudTableNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_table_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_GouraudTableNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random GouraudTableNZW #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random GouraudTableNZW Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_DitherTableNZW ─────────────────────────────────────── */

static void test_asm_equiv_dither_table_nzw(void) {
    setup_gouraud_table_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_DitherTableNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_table_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_DitherTableNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_DitherTableNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16),
                          "Filler_DitherTableNZW zbuf");
    /* Verify NZW did not modify Z-buffer */
    {
        int zbuf_modified = 0;
        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (gour_asm_zbuf[j] != 0xFFFF) {
                zbuf_modified = 1;
                break;
            }
        }
        ASSERT_EQ_INT(0, zbuf_modified);
    }
}

static void test_asm_random_dither_table_nzw(void) {
    poly_rng_seed(0xD70D000F);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_table_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_DitherTableNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_table_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_DitherTableNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random DitherTableNZW #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random DitherTableNZW Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_GouraudFogNZW ──────────────────────────────────────── */

static void test_asm_equiv_gouraud_fog_nzw(void) {
    setup_gouraud_fog_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_GouraudFogNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_fog_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_GouraudFogNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_GouraudFogNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16),
                          "Filler_GouraudFogNZW zbuf");
    /* Verify NZW did not modify Z-buffer */
    {
        int zbuf_modified = 0;
        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (gour_asm_zbuf[j] != 0xFFFF) {
                zbuf_modified = 1;
                break;
            }
        }
        ASSERT_EQ_INT(0, zbuf_modified);
    }
}

static void test_asm_random_gouraud_fog_nzw(void) {
    poly_rng_seed(0x6F0D0010);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_fog_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_GouraudFogNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_fog_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_GouraudFogNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random GouraudFogNZW #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random GouraudFogNZW Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Filler_DitherFogNZW ───────────────────────────────────────── */

static void test_asm_equiv_dither_fog_nzw(void) {
    setup_gouraud_fog_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    Filler_DitherFogNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_gouraud_fog_zbuf_filler(30, 4, 20 << 16, 80 << 16, 0x020000, 0x800);
    call_asm_Filler_DitherFogNZW(4, 20 << 16, 80 << 16);
    memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE,
                          "Filler_DitherFogNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                          TEST_POLY_SIZE * sizeof(U16),
                          "Filler_DitherFogNZW zbuf");
    /* Verify NZW did not modify Z-buffer */
    {
        int zbuf_modified = 0;
        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (gour_asm_zbuf[j] != 0xFFFF) {
                zbuf_modified = 1;
                break;
            }
        }
        ASSERT_EQ_INT(0, zbuf_modified);
    }
}

static void test_asm_random_dither_fog_nzw(void) {
    poly_rng_seed(0xDF0D0011);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 10;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 40;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;
        U32 gStart = (poly_rng_next() % 0x0E) << 16;
        U32 gSlope = poly_rng_next() % 0x2000;
        U32 zBufStart = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() % 0x400);

        setup_gouraud_fog_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_DitherFogNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_gouraud_fog_zbuf_filler(y, h, x0 << 16, x1 << 16, gStart, gSlope);
        Fill_CurZBufMin = zBufStart;
        Fill_CurZBuf = zBufStart;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_DitherFogNZW(h, x0 << 16, x1 << 16);
        memcpy(gour_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(gour_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        char msg[128];
        snprintf(msg, sizeof(msg), "random DitherFogNZW #%d y=%u h=%u x=%u-%u", i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(gour_asm_buf, gour_cpp_buf, TEST_POLY_SIZE, msg);
        snprintf(msg, sizeof(msg), "random DitherFogNZW Z #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)gour_asm_zbuf, (U8 *)gour_cpp_zbuf,
                              TEST_POLY_SIZE * sizeof(U16), msg);
    }
}

/* ── Test runner ───────────────────────────────────────────────── */

int main(void) {
    RUN_TEST(test_gouraud_basic);
    RUN_TEST(test_gouraud_uniform_light);
    RUN_TEST(test_dither_basic);
    RUN_TEST(test_gouraud_offscreen);
    RUN_TEST(test_gouraud_clipped);
    RUN_TEST(test_gouraud_random);
    RUN_TEST(test_asm_equiv_gouraud);
    RUN_TEST(test_asm_equiv_gouraud_flat);
    RUN_TEST(test_asm_random_gouraud);
    /* Filler_Dither */
    RUN_TEST(test_asm_equiv_dither);
    RUN_TEST(test_asm_random_dither);
    /* Filler_GouraudTable / DitherTable */
    RUN_TEST(test_asm_equiv_gouraud_table);
    RUN_TEST(test_asm_random_gouraud_table);
    RUN_TEST(test_asm_equiv_dither_table);
    RUN_TEST(test_asm_random_dither_table);
    RUN_TEST(test_asm_equiv_dither_table_width_mult_32);
    /* Filler_GouraudFog / DitherFog */
    RUN_TEST(test_asm_equiv_gouraud_fog);
    RUN_TEST(test_asm_random_gouraud_fog);
    RUN_TEST(test_asm_equiv_dither_fog);
    RUN_TEST(test_asm_random_dither_fog);
    /* Filler_GouraudZBuf / DitherZBuf */
    RUN_TEST(test_asm_equiv_gouraud_zbuf);
    RUN_TEST(test_asm_random_gouraud_zbuf);
    RUN_TEST(test_asm_equiv_dither_zbuf);
    RUN_TEST(test_asm_random_dither_zbuf);
    /* Filler_GouraudTableZBuf / DitherTableZBuf */
    RUN_TEST(test_asm_equiv_gouraud_table_zbuf);
    RUN_TEST(test_asm_random_gouraud_table_zbuf);
    RUN_TEST(test_asm_equiv_dither_table_zbuf);
    RUN_TEST(test_asm_random_dither_table_zbuf);
    /* Filler_GouraudFogZBuf / DitherFogZBuf */
    RUN_TEST(test_asm_equiv_gouraud_fog_zbuf);
    RUN_TEST(test_asm_random_gouraud_fog_zbuf);
    RUN_TEST(test_asm_equiv_dither_fog_zbuf);
    RUN_TEST(test_asm_random_dither_fog_zbuf);
    /* Filler_GouraudNZW / DitherNZW */
    RUN_TEST(test_asm_equiv_gouraud_nzw);
    RUN_TEST(test_asm_random_gouraud_nzw);
    RUN_TEST(test_asm_equiv_dither_nzw);
    RUN_TEST(test_asm_random_dither_nzw);
    /* Filler_GouraudTableNZW / DitherTableNZW */
    RUN_TEST(test_asm_equiv_gouraud_table_nzw);
    RUN_TEST(test_asm_random_gouraud_table_nzw);
    RUN_TEST(test_asm_equiv_dither_table_nzw);
    RUN_TEST(test_asm_random_dither_table_nzw);
    /* Filler_GouraudFogNZW / DitherFogNZW */
    RUN_TEST(test_asm_equiv_gouraud_fog_nzw);
    RUN_TEST(test_asm_random_gouraud_fog_nzw);
    RUN_TEST(test_asm_equiv_dither_fog_nzw);
    RUN_TEST(test_asm_random_dither_fog_nzw);
    TEST_SUMMARY();
    return test_failures != 0;
}
