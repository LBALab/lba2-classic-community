/* Test: TextureZ Fog Smooth rendering (POLYTZF.CPP) via Fill_Poly.
 *
 * Tests POLY_TEXTURE_Z_FOG type which combines perspective-correct
 * texture mapping with smooth fog blending.
 * Requires: PtrMap, RepMask, PtrCLUTFog, fog parameters.
 */
#include "test_harness.h"
#include <fenv.h>
#include <POLYGON/POLY.H>
#include <POLYGON/POLYTZF.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

#define TEX_SIZE 256

static U8 g_texture[TEX_SIZE * TEX_SIZE];
static U8 g_fog_clut[65536]; /* Fog CLUT: base + 12*256 + fogRow(up to 0xF000) + texel(0-255) */

static void assert_tzf_pixels_within_bounds(S16 min_x, S16 min_y, S16 max_x, S16 max_y) {
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

static Struc_Point make_point_uvw(S16 x, S16 y, U16 u, U16 v, S32 w) {
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = x;
    p.Pt_YE = y;
    p.Pt_MapU = u;
    p.Pt_MapV = v;
    p.Pt_W = w;
    return p;
}

static void setup_tzf_screen(void) {
    setup_polygon_screen();
    for (int y = 0; y < TEX_SIZE; y++)
        for (int x = 0; x < TEX_SIZE; x++)
            g_texture[y * TEX_SIZE + x] = (U8)(((x + y) & 0x3F) | 0x40);
    PtrMap = g_texture;
    RepMask = 0xFFFF;
    for (int i = 0; i < 65536; i++)
        g_fog_clut[i] = (U8)(i & 0xFF);
    PtrCLUTFog = g_fog_clut;
    PtrTruePal = g_fog_clut;
    SetFog(100, 10000);
    /* Activate fog mode */
    Switch_Fillers(FILL_POLY_FOG);
}

/* ── Basic TextureZ fog ────────────────────────────────────────── */

static void test_tzf_basic(void) {
    static U8 first_pass[TEST_POLY_SIZE];
    setup_tzf_screen();
    Struc_Point pts[3];
    Struc_Point pts_repeat[3];
    pts[0] = make_point_uvw(80, 10, 0, 0, 0x10000);
    pts[1] = make_point_uvw(40, 100, 0, 50 << 8, 0x8000);
    pts[2] = make_point_uvw(120, 100, 50 << 8, 50 << 8, 0x8000);
    memcpy(pts_repeat, pts, sizeof(pts));
    Fill_Poly(POLY_TEXTURE_Z_FOG, 0, 3, pts);
    memcpy(first_pass, g_poly_framebuf, TEST_POLY_SIZE);

    setup_tzf_screen();
    Fill_Poly(POLY_TEXTURE_Z_FOG, 0, 3, pts_repeat);

    ASSERT_ASM_CPP_MEM_EQ(first_pass, g_poly_framebuf, TEST_POLY_SIZE,
                          "Fill_Poly TextureZFog basic repeatable");
    assert_tzf_pixels_within_bounds(40, 10, 120, 100);
    Switch_Fillers(FILL_POLY_NO_TEXTURES);
}

static void test_tzf_offscreen(void) {
    setup_tzf_screen();
    Struc_Point pts[3];
    pts[0] = make_point_uvw(-50, -50, 0, 0, 0x10000);
    pts[1] = make_point_uvw(-30, -10, 10 << 8, 0, 0x8000);
    pts[2] = make_point_uvw(-10, -30, 0, 10 << 8, 0x8000);
    Fill_Poly(POLY_TEXTURE_Z_FOG, 0, 3, pts);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

/* ── Randomized stress test ────────────────────────────────────── */

static void test_tzf_random(void) {
    static U8 first_pass[TEST_POLY_SIZE];

    poly_rng_seed(0xDEADBEEF);
    for (int i = 0; i < 20; i++) {
        Struc_Point pts[3];
        Struc_Point pts_repeat[3];
        S16 min_x = TEST_POLY_W;
        S16 min_y = TEST_POLY_H;
        S16 max_x = -1;
        S16 max_y = -1;
        for (int v = 0; v < 3; v++) {
            S16 x = (S16)((poly_rng_next() % (TEST_POLY_W + 40)) - 20);
            S16 y = (S16)((poly_rng_next() % (TEST_POLY_H + 40)) - 20);
            U16 u = (U16)((poly_rng_next() % TEX_SIZE) << 8);
            U16 vc = (U16)((poly_rng_next() % TEX_SIZE) << 8);
            S32 w = (S32)(poly_rng_next() % 0xFFFF) + 0x100;
            pts[v] = make_point_uvw(x, y, u, vc, w);
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

        setup_tzf_screen();
        Fill_Poly(POLY_TEXTURE_Z_FOG, 0, 3, pts);
        memcpy(first_pass, g_poly_framebuf, TEST_POLY_SIZE);

        setup_tzf_screen();
        Fill_Poly(POLY_TEXTURE_Z_FOG, 0, 3, pts_repeat);

        char label[96];
        snprintf(label, sizeof(label), "Fill_Poly TextureZFog random #%d", i);
        ASSERT_ASM_CPP_MEM_EQ(first_pass, g_poly_framebuf, TEST_POLY_SIZE, label);
        assert_tzf_pixels_within_bounds(min_x, min_y, max_x, max_y);
    }
    /* Restore normal mode */
    Switch_Fillers(FILL_POLY_NO_TEXTURES);
}

/* ── ASM-vs-CPP equivalence (filler level) ─────────────────────── */

/* Provide F_256 constant needed by ASM perspective code */
extern "C" {
float F_256 = 256.0f;
}

extern "C" void asm_Filler_TextureZFogSmooth(void);
extern "C" void asm_Filler_TextureZFogSmoothZBuf(void);
extern "C" void asm_Filler_TextureZFogSmoothNZW(void);

static inline S32 call_asm_Filler_TextureZFogSmooth(U32 nbLines, U32 xmin, U32 xmax) {
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Filler_TextureZFogSmooth\n\t"
        "pop %%ebp"
        : "=a"(result)
        : "c"(nbLines), "b"(xmin), "d"(xmax)
        : "edi", "esi", "memory", "cc");
    return result;
}

static inline S32 call_asm_Filler_TextureZFogSmoothZBuf(U32 nbLines, U32 xmin, U32 xmax) {
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Filler_TextureZFogSmoothZBuf\n\t"
        "pop %%ebp"
        : "=a"(result)
        : "c"(nbLines), "b"(xmin), "d"(xmax)
        : "edi", "esi", "memory", "cc");
    return result;
}

static inline S32 call_asm_Filler_TextureZFogSmoothNZW(U32 nbLines, U32 xmin, U32 xmax) {
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Filler_TextureZFogSmoothNZW\n\t"
        "pop %%ebp"
        : "=a"(result)
        : "c"(nbLines), "b"(xmin), "d"(xmax)
        : "edi", "esi", "memory", "cc");
    return result;
}

static U8 tzf_cpp_buf[TEST_POLY_SIZE];
static U8 tzf_asm_buf[TEST_POLY_SIZE];
static U16 tzf_cpp_zbuf[TEST_POLY_SIZE];
static U16 tzf_asm_zbuf[TEST_POLY_SIZE];

static void setup_tzf_filler(U32 startY, U32 nbLines,
                             U32 xmin_fp, U32 xmax_fp) {
    setup_filler_common(startY, nbLines, xmin_fp, xmax_fp, 0);
    for (int y = 0; y < TEX_SIZE; y++)
        for (int x = 0; x < TEX_SIZE; x++)
            g_texture[y * TEX_SIZE + x] = (U8)(((x + y) & 0x3F) | 0x40);
    PtrMap = g_texture;
    RepMask = 0xFFFF;
    Fill_Patch = 1;
    Fill_Color.Ptr = g_fog_clut; /* Used as CLUT by perspective fillers */
    PtrCLUTFog = g_fog_clut;     /* Used by CPP fog filler */
    for (int i = 0; i < 65536; i++)
        g_fog_clut[i] = (U8)(i & 0xFF);
    memcpy(Fill_Logical_Palette, g_fog_clut, 256);

    Fill_CurMapUMin = 0;
    Fill_CurMapVMin = 0;
    Fill_MapU_LeftSlope = 0;
    Fill_MapV_LeftSlope = 0;
    Fill_MapU_XSlope = 0x10000;
    Fill_MapV_XSlope = 0;
    Fill_Cur_W = 0x10000;
    Fill_W_XSlope = 0;
    Fill_CurWMin = 0x10000;
    Fill_W_LeftSlope = 0;
    Fill_Cur_MapUOverW = 0;
    Fill_Cur_MapVOverW = 0;
    Fill_Next_W = 0x10000;
    Fill_Next_MapUOverW = 0;
    Fill_Next_MapVOverW = 0;
    Fill_Next_MapU = 0;
    Fill_Next_MapV = 0;
    Fill_CurZBufMin = 0x10000;
    Fill_ZBuf_LeftSlope = 0;
    Fill_ZBuf_XSlope = 0;
    Fill_CurZBuf = 0;
}

static void setup_tzf_zbuf_filler(U32 startY, U32 nbLines,
                                  U32 xmin_fp, U32 xmax_fp) {
    setup_tzf_filler(startY, nbLines, xmin_fp, xmax_fp);
    init_test_zbuffer(0xFFFF);
    PtrZBuffer = (PTR_U16)g_test_zbuffer;
}

static void test_asm_equiv_tzf(void) {
    setup_tzf_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureZFogSmooth(4, 20 << 16, 50 << 16);
    memcpy(tzf_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_tzf_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureZFogSmooth(4, 20 << 16, 50 << 16);
    memcpy(tzf_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(tzf_asm_buf, tzf_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZFogSmooth strip");
}

static void test_asm_random_tzf(void) {
    poly_rng_seed(0xFEEDFACE);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;

        setup_tzf_filler(y, h, x0 << 16, x1 << 16);
        Filler_TextureZFogSmooth(h, x0 << 16, x1 << 16);
        memcpy(tzf_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_tzf_filler(y, h, x0 << 16, x1 << 16);
        call_asm_Filler_TextureZFogSmooth(h, x0 << 16, x1 << 16);
        memcpy(tzf_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        /* Check for first mismatch */
        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzf_asm_buf[j] != tzf_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzf #%d w=%u: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, x1 - x0, j, row, col, tzf_asm_buf[j], tzf_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tzf_asm_buf, tzf_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

/* ── ASM-vs-CPP equivalence: ZBuf variant ──────────────────────── */

static void test_asm_equiv_tzf_zbuf(void) {
    fesetround(FE_TOWARDZERO); /* Match full-pipeline FPU mode */
    setup_tzf_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureZFogSmoothZBuf(4, 20 << 16, 50 << 16);
    memcpy(tzf_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzf_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_tzf_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureZFogSmoothZBuf(4, 20 << 16, 50 << 16);
    memcpy(tzf_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzf_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));
    fesetround(FE_TONEAREST);

    ASSERT_ASM_CPP_MEM_EQ(tzf_asm_buf, tzf_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZFogSmoothZBuf framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)tzf_asm_zbuf, (U8 *)tzf_cpp_zbuf,
                          TEST_POLY_SIZE * (int)sizeof(U16),
                          "Filler_TextureZFogSmoothZBuf zbuf");
}

static void test_asm_random_tzf_zbuf(void) {
    poly_rng_seed(0xCAFEBABE);
    fesetround(FE_TOWARDZERO); /* Match full-pipeline FPU mode */
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;

        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_tzf_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureZFogSmoothZBuf(h, x0 << 16, x1 << 16);
        memcpy(tzf_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzf_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_tzf_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureZFogSmoothZBuf(h, x0 << 16, x1 << 16);
        memcpy(tzf_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzf_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzf_asm_buf[j] != tzf_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzf_zbuf #%d w=%u: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, x1 - x0, j, row, col, tzf_asm_buf[j], tzf_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzf_zbuf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tzf_asm_buf, tzf_cpp_buf, TEST_POLY_SIZE, msg);

        snprintf(msg, sizeof(msg), "random tzf_zbuf zbuf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)tzf_asm_zbuf, (U8 *)tzf_cpp_zbuf,
                              TEST_POLY_SIZE * (int)sizeof(U16), msg);
    }
}

/* ── ASM-vs-CPP equivalence: ZBuf variant with non-zero W slope ──── */
/* This tests the filler with perspective correction VARIATION across the
 * scanline — the code path exercised by the full pipeline but missed by
 * the constant-W tests above. */
static void test_asm_random_tzf_zbuf_wslope(void) {
    poly_rng_seed(0xF00DCAFE);
    fesetround(FE_TOWARDZERO); /* Match full-pipeline FPU mode */
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 60);
        U32 x1 = x0 + 32 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;

        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;
        S32 curWMin = (S32)(0x10000 + (poly_rng_next() % 0x40000));
        S32 wXSlope = (S32)(poly_rng_next() % 4096) - 2048;
        S32 wLeftSlope = (S32)(poly_rng_next() % 4096) - 2048;
        S32 mapUXSlope = (S32)(poly_rng_next() % 0x20000) - 0x10000;
        S32 mapVXSlope = (S32)(poly_rng_next() % 0x20000) - 0x10000;
        S32 curMapU = (S32)(poly_rng_next() % 0x400000);
        S32 curMapV = (S32)(poly_rng_next() % 0x400000);

        /* CPP path */
        setup_tzf_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Fill_CurWMin = curWMin;
        Fill_Cur_W = curWMin; /* CPP filler resets Fill_Cur_W = Fill_CurWMin */
        Fill_W_XSlope = wXSlope;
        Fill_W_LeftSlope = wLeftSlope;
        Fill_MapU_XSlope = mapUXSlope;
        Fill_MapV_XSlope = mapVXSlope;
        Fill_CurMapUMin = curMapU;
        Fill_CurMapVMin = curMapV;
        Filler_TextureZFogSmoothZBuf(h, x0 << 16, x1 << 16);
        memcpy(tzf_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzf_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        /* ASM path — same params */
        setup_tzf_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Fill_CurWMin = curWMin;
        Fill_Cur_W = curWMin; /* Match CPP: the ASM would read Fill_Cur_W
                                  which in the real pipeline equals CurWMin
                                  at the first filler call after slope setup */
        Fill_W_XSlope = wXSlope;
        Fill_W_LeftSlope = wLeftSlope;
        Fill_MapU_XSlope = mapUXSlope;
        Fill_MapV_XSlope = mapVXSlope;
        Fill_CurMapUMin = curMapU;
        Fill_CurMapVMin = curMapV;
        call_asm_Filler_TextureZFogSmoothZBuf(h, x0 << 16, x1 << 16);
        memcpy(tzf_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzf_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzf_asm_buf[j] != tzf_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzf_zbuf_wslope #%d w=%u: first diff byte %d "
                       "(row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, x1 - x0, j, row, col, tzf_asm_buf[j], tzf_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzf_zbuf_wslope #%d", i);
        ASSERT_ASM_CPP_MEM_EQ(tzf_asm_buf, tzf_cpp_buf, TEST_POLY_SIZE, msg);
    }
    fesetround(FE_TONEAREST);
}

static void test_asm_equiv_tzf_nzw(void) {
    setup_tzf_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    Filler_TextureZFogSmoothNZW(4, 20 << 16, 50 << 16);
    memcpy(tzf_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzf_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    setup_tzf_zbuf_filler(30, 4, 20 << 16, 50 << 16);
    call_asm_Filler_TextureZFogSmoothNZW(4, 20 << 16, 50 << 16);
    memcpy(tzf_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(tzf_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    ASSERT_ASM_CPP_MEM_EQ(tzf_asm_buf, tzf_cpp_buf, TEST_POLY_SIZE,
                          "Filler_TextureZFogSmoothNZW framebuf");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)tzf_asm_zbuf, (U8 *)tzf_cpp_zbuf,
                          TEST_POLY_SIZE * (int)sizeof(U16),
                          "Filler_TextureZFogSmoothNZW zbuf");
}

static void test_asm_random_tzf_nzw(void) {
    poly_rng_seed(0xBAADF00D);
    for (int i = 0; i < 300; i++) {
        U32 y = poly_rng_next() % (TEST_POLY_H - 20);
        U32 h = 1 + poly_rng_next() % 8;
        if (y + h + 1 >= (U32)TEST_POLY_H)
            h = TEST_POLY_H - y - 2;
        U32 x0 = poly_rng_next() % (TEST_POLY_W - 30);
        U32 x1 = x0 + 5 + poly_rng_next() % 30;
        if (x1 >= (U32)TEST_POLY_W)
            x1 = TEST_POLY_W - 1;

        U32 zBufMin = (poly_rng_next() % 0x7FFF) << 8;
        S32 zBufXSlope = (S32)(poly_rng_next() - 0x4000) << 4;

        setup_tzf_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        Filler_TextureZFogSmoothNZW(h, x0 << 16, x1 << 16);
        memcpy(tzf_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzf_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        setup_tzf_zbuf_filler(y, h, x0 << 16, x1 << 16);
        Fill_CurZBufMin = zBufMin;
        Fill_ZBuf_XSlope = zBufXSlope;
        call_asm_Filler_TextureZFogSmoothNZW(h, x0 << 16, x1 << 16);
        memcpy(tzf_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(tzf_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (tzf_asm_buf[j] != tzf_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# tzf_nzw #%d w=%u: first diff byte %d (row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, x1 - x0, j, row, col, tzf_asm_buf[j], tzf_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "random tzf_nzw #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ(tzf_asm_buf, tzf_cpp_buf, TEST_POLY_SIZE, msg);

        snprintf(msg, sizeof(msg), "random tzf_nzw zbuf #%d y=%u h=%u x=%u-%u",
                 i, y, h, x0, x1);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)tzf_asm_zbuf, (U8 *)tzf_cpp_zbuf,
                              TEST_POLY_SIZE * (int)sizeof(U16), msg);
    }
}

int main(void) {
    RUN_TEST(test_tzf_basic);
    RUN_TEST(test_tzf_offscreen);
    RUN_TEST(test_tzf_random);
    RUN_TEST(test_asm_equiv_tzf);
    RUN_TEST(test_asm_random_tzf);
    RUN_TEST(test_asm_equiv_tzf_zbuf);
    RUN_TEST(test_asm_random_tzf_zbuf);
    RUN_TEST(test_asm_random_tzf_zbuf_wslope);
    RUN_TEST(test_asm_equiv_tzf_nzw);
    RUN_TEST(test_asm_random_tzf_nzw);
    TEST_SUMMARY();
    return test_failures != 0;
}
