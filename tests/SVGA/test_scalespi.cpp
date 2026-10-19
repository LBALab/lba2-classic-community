/* Test: ScaleSprite — ASM-vs-CPP equivalence.
 *
 * The original ASM PROC only declared `uses ebp` but clobbers esi/edi/ebx
 * (callee-saved in cdecl). Fixed by adding `uses esi edi ebx` to the proc.
 *
 * Covers the 1:1 fast path and the 16.16 fixed-point scaled path. The CPP
 * implementation mirrors ScaleSpriteTransp's algorithm minus the
 * transparency-table indirection.
 */
#include "test_harness.h"
#include <SVGA/SCALESPI.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREENXY.H>
#include <string.h>
#include <stdio.h>

/* The raw ASM symbol */
extern "C" void asm_ScaleSprite(S32 num, S32 x, S32 y, S32 factorx, S32 factory, void *ptrbank);
#define call_asm_ScaleSprite asm_ScaleSprite

static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

static U8 g_bank[4096];
static U8 framebuf[640 * 480];
static U8 cpp_framebuf[640 * 480];
static U8 asm_framebuf[640 * 480];

typedef struct SpriteRunResult {
    S32 xmin;
    S32 xmax;
    S32 ymin;
    S32 ymax;
} SpriteRunResult;

static U32 build_multi_bank(int n) {
    static const S8 hot_x_values[] = {0, 1, -1, 2, -2, 3, -3, 1};
    static const S8 hot_y_values[] = {0, -1, 1, -2, 2, -3, 3, 0};

    memset(g_bank, 0, sizeof(g_bank));
    U32 *off = (U32 *)g_bank;
    U32 pos = (U32)(n * 4);
    for (int i = 0; i < n; i++) {
        off[i] = pos;
        U8 w = (U8)((i % 8) + 2);
        U8 h = (U8)((i % 6) + 2);
        g_bank[pos] = w;
        g_bank[pos + 1] = h;
        g_bank[pos + 2] = (U8)hot_x_values[i & 7];
        g_bank[pos + 3] = (U8)hot_y_values[i & 7];
        U8 *px = g_bank + pos + 4;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                px[y * w + x] = ((x + y) % 2 == 0) ? (U8)(i + 1 + y * w + x) : 0;
        pos += 4 + (U32)(w * h);
    }
    return pos;
}

static void setup_screen(U8 *buf) {
    memset(buf, 0, sizeof(framebuf));
    Log = buf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = 639;
    ClipYMax = 479;
}

static void setup(void) {
    setup_screen(framebuf);
}

static SpriteRunResult run_cpp_case(S32 num, S32 x, S32 y) {
    SpriteRunResult result;
    setup_screen(cpp_framebuf);
    ScaleSprite(num, x, y, 0x10000, 0x10000, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static SpriteRunResult run_asm_case(S32 num, S32 x, S32 y) {
    SpriteRunResult result;
    setup_screen(asm_framebuf);
    call_asm_ScaleSprite(num, x, y, 0x10000, 0x10000, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static void assert_case_matches(const char *label, S32 num, S32 x, S32 y) {
    SpriteRunResult cpp_result = run_cpp_case(num, x, y);
    SpriteRunResult asm_result = run_asm_case(num, x, y);

    ASSERT_ASM_CPP_EQ_INT(asm_result.xmin, cpp_result.xmin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.xmax, cpp_result.xmax, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymin, cpp_result.ymin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymax, cpp_result.ymax, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_framebuf, cpp_framebuf, sizeof(cpp_framebuf), label);
}

static SpriteRunResult run_cpp_case_scaled(S32 num, S32 x, S32 y, S32 factorx, S32 factory) {
    SpriteRunResult result;
    setup_screen(cpp_framebuf);
    ScaleSprite(num, x, y, factorx, factory, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static SpriteRunResult run_asm_case_scaled(S32 num, S32 x, S32 y, S32 factorx, S32 factory) {
    SpriteRunResult result;
    setup_screen(asm_framebuf);
    call_asm_ScaleSprite(num, x, y, factorx, factory, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static void assert_case_matches_scaled(const char *label, S32 num, S32 x, S32 y, S32 factorx, S32 factory) {
    SpriteRunResult cpp_result = run_cpp_case_scaled(num, x, y, factorx, factory);
    SpriteRunResult asm_result = run_asm_case_scaled(num, x, y, factorx, factory);

    ASSERT_ASM_CPP_EQ_INT(asm_result.xmin, cpp_result.xmin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.xmax, cpp_result.xmax, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymin, cpp_result.ymin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymax, cpp_result.ymax, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_framebuf, cpp_framebuf, sizeof(cpp_framebuf), label);
}

static void test_basic(void) {
    build_multi_bank(1);
    setup();
    ScaleSprite(0, 100, 100, 0x10000, 0x10000, g_bank);
    ASSERT_EQ_UINT(1, framebuf[100 * 640 + 100]);
    ASSERT_EQ_UINT(0, framebuf[100 * 640 + 101]);
}

static void test_asm_minimal(void) {
    build_multi_bank(1);
    assert_case_matches("ScaleSprite minimal 1:1", 0, 100, 100);
}

static void test_transparency(void) {
    build_multi_bank(1);
    setup();
    for (int y = 99; y < 103; y++)
        for (int x = 99; x < 103; x++)
            framebuf[y * 640 + x] = 0xAA;
    ScaleSprite(0, 100, 100, 0x10000, 0x10000, g_bank);
    ASSERT_EQ_UINT(0xAA, framebuf[100 * 640 + 101]);
    ASSERT_TRUE(framebuf[100 * 640 + 100] != 0xAA);
}

static void test_fully_clipped(void) {
    build_multi_bank(1);
    assert_case_matches("ScaleSprite fully clipped", 0, -100, -100);
}

static void test_clip_left(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clip left", 3, -2, 100);
}

static void test_clip_right(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clip right", 3, 636, 100);
}

static void test_clip_top(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clip top", 3, 100, -2);
}

static void test_clip_bottom(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clip bottom", 3, 100, 477);
}

static void test_hotspot(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite hotspot", 2, 200, 200);
}

static void test_negative_hotspot(void) {
    build_multi_bank(8);
    assert_case_matches("ScaleSprite negative hotspot", 4, 1, 1);
}

static void test_multi_sprites(void) {
    build_multi_bank(8);
    setup();
    for (int i = 0; i < 8; i++) {
        ScaleSprite(i, 50 + i * 50, 50, 0x10000, 0x10000, g_bank);
        ASSERT_TRUE(ScreenXMin >= 0);
    }
}

static void test_random_batch(void) {
    build_multi_bank(8);
    rng_seed(0x5A710001);
    int prev = test_failures;
    for (int i = 0; i < 100 && test_failures == prev; i++) {
        S32 num = (S32)(rng_next() % 8);
        S32 sx = (S32)(rng_next() % 680) - 20;
        S32 sy = (S32)(rng_next() % 520) - 20;
        char label[64];
        snprintf(label, sizeof(label), "ScaleSprite random #%d", i);
        assert_case_matches(label, num, sx, sy);
    }
}

static void test_asm_equiv_1to1(void) {
    build_multi_bank(1);
    assert_case_matches("ScaleSprite 1:1", 0, 50, 50);
}

static void test_asm_equiv_clipped(void) {
    build_multi_bank(4);
    assert_case_matches("ScaleSprite clipped", 3, -1, -1);
}

static void test_asm_equiv_random(void) {
    build_multi_bank(8);
    rng_seed(0xA5E01234);
    int prev = test_failures;
    for (int i = 0; i < 20 && test_failures == prev; i++) {
        S32 num = (S32)(rng_next() % 8);
        S32 sx = (S32)(rng_next() % 600) + 10;
        S32 sy = (S32)(rng_next() % 440) + 10;

        char label[64];
        snprintf(label, sizeof(label), "ScaleSprite random #%d spr=%d (%d,%d)", i, num, sx, sy);
        assert_case_matches(label, num, sx, sy);
    }
}

static void test_asm_equiv_scaled_basic(void) {
    build_multi_bank(8);
    assert_case_matches_scaled("ScaleSprite 1:1 baseline", 0, 120, 80, 0x10000, 0x10000);
    assert_case_matches_scaled("ScaleSprite scaled up", 3, 210, 140, 0x18000, 0x14000);
    assert_case_matches_scaled("ScaleSprite scaled down", 5, 320, 200, 0x08000, 0x0C000);
}

static void test_asm_equiv_scaled_edge(void) {
    build_multi_bank(8);
    assert_case_matches_scaled("ScaleSprite scaled clipped top-left", 4, -3, -2, 0x10000, 0x10000);
    assert_case_matches_scaled("ScaleSprite scaled fully clipped right", 2, 900, 50, 0x10000, 0x10000);
    assert_case_matches_scaled("ScaleSprite negative-factor fallback", 6, 180, 150, -3, 0x10000);
    assert_case_matches_scaled("ScaleSprite zero-factor early exit", 1, 70, 60, 0, 0x10000);
}

static void test_asm_equiv_scaled_random(void) {
    build_multi_bank(8);
    rng_seed(0xC0DEC0DE);
    int prev = test_failures;
    for (int round = 0; round < 32 && test_failures == prev; round++) {
        S32 num = (S32)(rng_next() % 8);
        S32 x = (S32)(rng_next() % 700) - 30;
        S32 y = (S32)(rng_next() % 540) - 30;
        S32 factorx_choices[] = {0x10000, 0x08000, 0x18000, 0x14000, -5, 0};
        S32 factory_choices[] = {0x10000, 0x0C000, 0x16000, 0x09000, -7, 0};
        S32 factorx = factorx_choices[rng_next() % 6];
        S32 factory = factory_choices[rng_next() % 6];

        char label[128];
        snprintf(label, sizeof(label),
                 "ScaleSprite scaled random #%d spr=%d pos=(%d,%d) fx=%d fy=%d",
                 round, num, x, y, factorx, factory);
        assert_case_matches_scaled(label, num, x, y, factorx, factory);
    }
}

#define EDGE_CPP_BUF cpp_framebuf
#define EDGE_ASM_BUF asm_framebuf
#define EDGE_CALL_CPP(num, x, y, fx, fy) ScaleSprite((num), (x), (y), (fx), (fy), g_edge_bank)
#define EDGE_CALL_ASM(num, x, y, fx, fy) call_asm_ScaleSprite((num), (x), (y), (fx), (fy), g_edge_bank)

/* ── Edge sweep ───────────────────────────────────────────────────────
 *
 * Realistic sprite sizes walked across every edge of three clip windows
 * (full screen, an inner window, a one-row and a one-column window) at
 * 1:1, downscale, upscale and asymmetric factors. The 1:1 fast path and
 * the scaled path test the top and bottom edges with different
 * comparisons (fast: clip when topY <= ClipYMin, exit when bottomY <
 * ClipYMin; scaled: clip when topY < ClipYMin, exit when bottomY <=
 * ClipYMin), so the walks step one pixel at a time through those exact
 * positions, and test_edge_exact_boundaries names them explicitly.
 */

enum { EDGE_SPR_BRICK,
       EDGE_SPR_LARGE,
       EDGE_SPR_SMALL,
       EDGE_SPR_MAX,
       EDGE_SPR_COUNT };

static U8 g_edge_bank[96 * 1024];
static S32 g_edge_draw_full;    /* cases that drew the whole sprite */
static S32 g_edge_draw_partial; /* cases that drew a clipped part */
static S32 g_edge_draw_none;    /* cases clipped out entirely */
static S32 g_edge_cases;

static const struct {
    U8 w, h;
    S8 hot_x, hot_y;
} g_edge_sprites[EDGE_SPR_COUNT] = {
    {48, 38, -24, -37},  /* brick-sized, hotspot at bottom centre */
    {100, 80, -50, -79}, /* large, high-bit hotspot bytes */
    {9, 7, 3, 2},        /* small, positive hotspot (blown up by upscale) */
    {255, 200, -128, 0}, /* largest width the U8 header allows */
};

static void build_edge_bank(void) {
    memset(g_edge_bank, 0, sizeof(g_edge_bank));
    U32 *off = (U32 *)g_edge_bank;
    U32 pos = EDGE_SPR_COUNT * 4;
    for (int i = 0; i < EDGE_SPR_COUNT; i++) {
        const U8 w = g_edge_sprites[i].w;
        const U8 h = g_edge_sprites[i].h;
        off[i] = pos;
        g_edge_bank[pos + 0] = w;
        g_edge_bank[pos + 1] = h;
        g_edge_bank[pos + 2] = (U8)g_edge_sprites[i].hot_x;
        g_edge_bank[pos + 3] = (U8)g_edge_sprites[i].hot_y;
        U8 *px = g_edge_bank + pos + 4;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                /* Row and column both show in the value; diagonal bands of
                 * colour 0 make transparent runs. */
                U8 v = (U8)((x * 7 + y * 31 + i * 50) & 0xFF);
                if (v == 0)
                    v = 1;
                if (((x / 5) + (y / 3)) % 4 == 0)
                    v = 0;
                px[y * w + x] = v;
            }
        }
        pos += 4 + (U32)(w * h);
    }
    /* Past the last sprite: non-zero filler, so a read past the sprite data
     * (the scaled path's clip arithmetic can step a few texels over) shows
     * up the same on both sides instead of hiding as transparent. */
    for (U32 p = pos; p < sizeof(g_edge_bank); p++)
        g_edge_bank[p] = (U8)(0x80 | (p & 0x3F));
}

typedef struct EdgeWindow {
    const char *name;
    S32 xmin, ymin, xmax, ymax;
} EdgeWindow;

static const EdgeWindow g_edge_windows[] = {
    {"full", 0, 0, 639, 479},
    {"inner", 100, 80, 539, 399},
    {"row", 100, 240, 539, 240},
    {"col", 320, 80, 320, 399},
};

typedef struct EdgeFactor {
    S32 fx, fy;
} EdgeFactor;

static const EdgeFactor g_edge_factors[] = {
    {0x10000, 0x10000}, /* 1:1 fast path */
    {0x08000, 0x08000}, /* half */
    {0x0C000, 0x0A000}, /* downscale, asymmetric */
    {0x20000, 0x20000}, /* double */
    {0x18000, 0x0C000}, /* up in x, down in y */
    {0x10000, 0x20000}, /* scaled path with x at 1:1 */
    {0x30000, 0x30000}, /* triple */
};

static S32 edge_mul16(S32 a, S32 b) {
    return (S32)(((long long)a * (long long)b) >> 16);
}

/* The background is filled once per side and only the rows a case drew
 * are restored after it (from the bbox each side reported), which keeps
 * the sweep fast under emulation. The whole framebuffer is still compared
 * every case, so a write outside the reported bbox is caught. */
static U8 g_edge_background[640 * 480];

static void edge_init_buffers(void) {
    for (U32 y = 0; y < 480; y++)
        for (U32 x = 0; x < 640; x++)
            g_edge_background[y * 640 + x] = (U8)((x * 3 + y * 5 + 0x21) | 1);
    memcpy(EDGE_CPP_BUF, g_edge_background, sizeof(g_edge_background));
    memcpy(EDGE_ASM_BUF, g_edge_background, sizeof(g_edge_background));
}

static void edge_restore_rows(U8 *buf, S32 ymin, S32 ymax) {
    if (ymin < 0)
        ymin = 0;
    if (ymax > 480)
        ymax = 480;
    if (ymax > ymin)
        memcpy(buf + ymin * 640, g_edge_background + ymin * 640, (size_t)(ymax - ymin) * 640);
}

static void edge_setup(U8 *buf, const EdgeWindow *win) {
    Log = buf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    ClipXMin = win->xmin;
    ClipYMin = win->ymin;
    ClipXMax = win->xmax;
    ClipYMax = win->ymax;
    ScreenXMin = 11111;
    ScreenXMax = 22222;
    ScreenYMin = 33333;
    ScreenYMax = 44444;
}

/* Runs one case on both sides; returns 0 when they match. */
static int edge_case(const char *label, const EdgeWindow *win, S32 num, S32 x, S32 y, S32 fx, S32 fy) {
    S32 c[4], a[4];
    int prev = test_failures;

    edge_setup(EDGE_CPP_BUF, win);
    EDGE_CALL_CPP(num, x, y, fx, fy);
    c[0] = ScreenXMin;
    c[1] = ScreenXMax;
    c[2] = ScreenYMin;
    c[3] = ScreenYMax;

    edge_setup(EDGE_ASM_BUF, win);
    EDGE_CALL_ASM(num, x, y, fx, fy);
    a[0] = ScreenXMin;
    a[1] = ScreenXMax;
    a[2] = ScreenYMin;
    a[3] = ScreenYMax;

    ASSERT_ASM_CPP_EQ_INT(a[0], c[0], label);
    ASSERT_ASM_CPP_EQ_INT(a[1], c[1], label);
    ASSERT_ASM_CPP_EQ_INT(a[2], c[2], label);
    ASSERT_ASM_CPP_EQ_INT(a[3], c[3], label);
    ASSERT_ASM_CPP_MEM_EQ(EDGE_ASM_BUF, EDGE_CPP_BUF, 640 * 480, label);
    if (test_failures != prev) {
        /* After a mismatch the writes may be anywhere: start clean. */
        memcpy(EDGE_CPP_BUF, g_edge_background, sizeof(g_edge_background));
        memcpy(EDGE_ASM_BUF, g_edge_background, sizeof(g_edge_background));
    } else {
        edge_restore_rows(EDGE_CPP_BUF, c[2], c[3]);
        edge_restore_rows(EDGE_ASM_BUF, a[2], a[3]);
    }

    /* Classify what the case drew, from the scaled extents the routine
     * computes, to confirm the sweep really exercises partial clipping. */
    const S32 ndx = (fx == 0x10000) ? g_edge_sprites[num].w : edge_mul16(g_edge_sprites[num].w, fx);
    const S32 ndy = (fy == 0x10000) ? g_edge_sprites[num].h : edge_mul16(g_edge_sprites[num].h, fy);
    const S32 l = x + ((fx == 0x10000) ? g_edge_sprites[num].hot_x : edge_mul16(g_edge_sprites[num].hot_x, fx));
    const S32 t = y + ((fy == 0x10000) ? g_edge_sprites[num].hot_y : edge_mul16(g_edge_sprites[num].hot_y, fy));
    g_edge_cases++;
    if (a[1] <= a[0] || a[3] <= a[2])
        g_edge_draw_none++;
    else if (a[0] == l && a[2] == t && a[1] == l + ndx && a[3] == t + ndy)
        g_edge_draw_full++;
    else
        g_edge_draw_partial++;

    return test_failures != prev;
}

/* Walks one sprite at one scale across every edge and corner of one window.
 * Positions are chosen through the sprite's on-screen extents (scaled
 * size and hotspot), from just past the edge to fully inside. */
static int edge_sweep_one(const char *tag, const EdgeWindow *win, S32 num, S32 fx, S32 fy, int max_fail) {
    const S32 ndx = (fx == 0x10000) ? g_edge_sprites[num].w : edge_mul16(g_edge_sprites[num].w, fx);
    const S32 ndy = (fy == 0x10000) ? g_edge_sprites[num].h : edge_mul16(g_edge_sprites[num].h, fy);
    const S32 hx = (fx == 0x10000) ? g_edge_sprites[num].hot_x : edge_mul16(g_edge_sprites[num].hot_x, fx);
    const S32 hy = (fy == 0x10000) ? g_edge_sprites[num].hot_y : edge_mul16(g_edge_sprites[num].hot_y, fy);
    /* Mid position on the other axis: sprite centred on the window. */
    const S32 mid_l = (win->xmin + win->xmax + 1) / 2 - ndx / 2;
    const S32 mid_t = (win->ymin + win->ymax + 1) / 2 - ndy / 2;
    const S32 diag = (ndx > ndy ? ndx : ndy) + 3;
    int fails = 0;
    char label[192];

#define EDGE_RUN(edge, l, t)                                                                    \
    do {                                                                                        \
        snprintf(label, sizeof(label), "%s %s spr=%d fx=0x%X fy=0x%X win=%s %s left=%d top=%d", \
                 current_test_name, tag, (int)num, (unsigned)fx, (unsigned)fy, win->name, edge, \
                 (int)(l), (int)(t));                                                           \
        fails += edge_case(label, win, num, (l) - hx, (t) - hy, fx, fy);                        \
        if (fails >= max_fail)                                                                  \
            return fails;                                                                       \
    } while (0)

    /* Left edge: right side from one px outside to fully inside. */
    for (S32 l = win->xmin - ndx - 2; l <= win->xmin + 2; l++)
        EDGE_RUN("left", l, mid_t);
    /* Right edge: left side from one px outside to fully inside. */
    for (S32 l = win->xmax + 3; l >= win->xmax - ndx - 1; l--)
        EDGE_RUN("right", l, mid_t);
    for (S32 t = win->ymin - ndy - 2; t <= win->ymin + 2; t++)
        EDGE_RUN("top", mid_l, t);
    for (S32 t = win->ymax + 3; t >= win->ymax - ndy - 1; t--)
        EDGE_RUN("bottom", mid_l, t);
    /* Corners, on a diagonal walk from outside to inside. */
    for (S32 i = 0; i <= diag; i++) {
        EDGE_RUN("top-left", win->xmin - ndx - 1 + i, win->ymin - ndy - 1 + i);
        EDGE_RUN("top-right", win->xmax + 2 - i, win->ymin - ndy - 1 + i);
        EDGE_RUN("bottom-left", win->xmin - ndx - 1 + i, win->ymax + 2 - i);
        EDGE_RUN("bottom-right", win->xmax + 2 - i, win->ymax + 2 - i);
    }
#undef EDGE_RUN
    return fails;
}

static void edge_reset_counts(void) {
    g_edge_cases = g_edge_draw_full = g_edge_draw_partial = g_edge_draw_none = 0;
}

static void edge_print_counts(void) {
    printf("    %s: %d cases, %d full, %d partial, %d clipped out\n", current_test_name, (int)g_edge_cases,
           (int)g_edge_draw_full, (int)g_edge_draw_partial, (int)g_edge_draw_none);
}

/* Every sprite, every factor, every window. */
static void test_edge_sweep(void) {
    build_edge_bank();
    edge_init_buffers();
    edge_reset_counts();
    int fails = 0;
    for (S32 num = 0; num < EDGE_SPR_COUNT && fails < 8; num++) {
        for (size_t f = 0; f < sizeof(g_edge_factors) / sizeof(g_edge_factors[0]); f++) {
            const S32 fx = g_edge_factors[f].fx;
            const S32 fy = g_edge_factors[f].fy;
            /* Keep the run short: the widest sprite only at 1:1 and half
             * size, and triple size only for the small sprite (the others
             * already span most of the screen at double size). */
            if (num == EDGE_SPR_MAX && !(fx == fy && fx <= 0x10000))
                continue;
            if (fx == 0x30000 && num != EDGE_SPR_SMALL)
                continue;
            for (size_t w = 0; w < sizeof(g_edge_windows) / sizeof(g_edge_windows[0]); w++)
                fails += edge_sweep_one("sweep", &g_edge_windows[w], num, fx, fy, 8 - fails);
            if (fails >= 8)
                break;
        }
    }
    edge_print_counts();
    ASSERT_TRUE(g_edge_draw_partial > 0);
    ASSERT_TRUE(g_edge_draw_full > 0);
    ASSERT_TRUE(g_edge_draw_none > 0);
}

/* The positions where the fast path and the scaled path compare
 * differently: top exactly at ClipYMin, bottom exactly at ClipYMin, and the
 * same for X (left at ClipXMin, right at ClipXMin), plus the far edges
 * (top/left at ClipMax + 1, bottom/right at ClipMax + 1), each one px either
 * side, for each factor and window. */
static void test_edge_exact_boundaries(void) {
    build_edge_bank();
    edge_init_buffers();
    edge_reset_counts();
    char label[192];
    for (S32 num = 0; num < EDGE_SPR_COUNT; num++) {
        for (size_t f = 0; f < sizeof(g_edge_factors) / sizeof(g_edge_factors[0]); f++) {
            const S32 fx = g_edge_factors[f].fx;
            const S32 fy = g_edge_factors[f].fy;
            const S32 ndx = (fx == 0x10000) ? g_edge_sprites[num].w : edge_mul16(g_edge_sprites[num].w, fx);
            const S32 ndy = (fy == 0x10000) ? g_edge_sprites[num].h : edge_mul16(g_edge_sprites[num].h, fy);
            const S32 hx = (fx == 0x10000) ? g_edge_sprites[num].hot_x : edge_mul16(g_edge_sprites[num].hot_x, fx);
            const S32 hy = (fy == 0x10000) ? g_edge_sprites[num].hot_y : edge_mul16(g_edge_sprites[num].hot_y, fy);
            for (size_t w = 0; w < sizeof(g_edge_windows) / sizeof(g_edge_windows[0]); w++) {
                const EdgeWindow *win = &g_edge_windows[w];
                const S32 mid_l = (win->xmin + win->xmax + 1) / 2 - ndx / 2;
                const S32 mid_t = (win->ymin + win->ymax + 1) / 2 - ndy / 2;
                const S32 tops[] = {win->ymin, win->ymin - ndy, win->ymax + 1, win->ymax + 1 - ndy};
                const S32 lefts[] = {win->xmin, win->xmin - ndx, win->xmax + 1, win->xmax + 1 - ndx};
                static const char *names[] = {"at min", "end at min", "at max+1", "end at max+1"};
                for (int k = 0; k < 4; k++) {
                    for (S32 d = -1; d <= 1; d++) {
                        snprintf(label, sizeof(label), "exact spr=%d fx=0x%X fy=0x%X win=%s top %s%+d",
                                 (int)num, (unsigned)fx, (unsigned)fy, win->name, names[k], (int)d);
                        edge_case(label, win, num, mid_l - hx, tops[k] + d - hy, fx, fy);
                        snprintf(label, sizeof(label), "exact spr=%d fx=0x%X fy=0x%X win=%s left %s%+d",
                                 (int)num, (unsigned)fx, (unsigned)fy, win->name, names[k], (int)d);
                        edge_case(label, win, num, lefts[k] + d - hx, mid_t - hy, fx, fy);
                        /* Both axes on the boundary at once. */
                        snprintf(label, sizeof(label), "exact spr=%d fx=0x%X fy=0x%X win=%s corner %s%+d",
                                 (int)num, (unsigned)fx, (unsigned)fy, win->name, names[k], (int)d);
                        edge_case(label, win, num, lefts[k] + d - hx, tops[k] + d - hy, fx, fy);
                    }
                }
            }
        }
    }
    edge_print_counts();
}

int main(void) {
    RUN_TEST(test_basic);
    RUN_TEST(test_asm_minimal);
    RUN_TEST(test_transparency);
    RUN_TEST(test_fully_clipped);
    RUN_TEST(test_clip_left);
    RUN_TEST(test_clip_right);
    RUN_TEST(test_clip_top);
    RUN_TEST(test_clip_bottom);
    RUN_TEST(test_hotspot);
    RUN_TEST(test_negative_hotspot);
    RUN_TEST(test_multi_sprites);
    RUN_TEST(test_random_batch);
    RUN_TEST(test_asm_equiv_1to1);
    RUN_TEST(test_asm_equiv_clipped);
    RUN_TEST(test_asm_equiv_random);
    RUN_TEST(test_asm_equiv_scaled_basic);
    RUN_TEST(test_asm_equiv_scaled_edge);
    RUN_TEST(test_asm_equiv_scaled_random);
    RUN_TEST(test_edge_sweep);
    RUN_TEST(test_edge_exact_boundaries);
    TEST_SUMMARY();
    return test_failures != 0;
}
