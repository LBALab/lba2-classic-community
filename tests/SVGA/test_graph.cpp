/* Test: GetBoxGraph / AffGraph — graph bounds and rendering equivalence. */
#include "test_harness.h"
#include <SVGA/GRAPH.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREENXY.H>
#include <string.h>

extern "C" void *Log;
extern "C" U32 ModeDesiredX;
extern "C" U32 ModeDesiredY;
extern "C" U32 TabOffLine[];
extern "C" S32 ClipXMin;
extern "C" S32 ClipYMin;
extern "C" S32 ClipXMax;
extern "C" S32 ClipYMax;
extern "C" S32 ScreenXMin;
extern "C" S32 ScreenYMin;
extern "C" S32 ScreenXMax;
extern "C" S32 ScreenYMax;

extern "C" S32 asm_AffGraph(S32 numgraph, S32 x, S32 y, const void *bankgraph);
extern "C" S32 asm_GetBoxGraph(S32 numgraph, S32 *x0, S32 *y0, S32 *x1, S32 *y1, const void *bankgraph);

/* Synthetic graph bank: offset table + brick headers.
 * Each brick: DeltaX, DeltaY, HotX(signed), HotY(signed), then line data. */
static U8 g_bank[256];
static U8 g_cpp_framebuf[640 * 480];
static U8 g_asm_framebuf[640 * 480];

typedef struct GraphRunResult {
    S32 xmin;
    S32 ymin;
    S32 xmax;
    S32 ymax;
} GraphRunResult;

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void setup_screen(U8 *framebuf,
                         S32 clip_xmin, S32 clip_ymin,
                         S32 clip_xmax, S32 clip_ymax) {
    memset(framebuf, 0, 640 * 480);
    Log = framebuf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; ++i)
        TabOffLine[i] = i * 640;
    ClipXMin = clip_xmin;
    ClipYMin = clip_ymin;
    ClipXMax = clip_xmax;
    ClipYMax = clip_ymax;
    ScreenXMin = 32000;
    ScreenYMin = 32000;
    ScreenXMax = -32000;
    ScreenYMax = -32000;
}

static void build_graph_bank_from_lines(U8 delta_x, U8 delta_y,
                                        U8 hot_x, U8 hot_y,
                                        const U8 *line_bytes, size_t line_size) {
    memset(g_bank, 0, sizeof(g_bank));
    ((U32 *)g_bank)[0] = 4;
    U8 *brick = g_bank + 4;
    brick[0] = delta_x;
    brick[1] = delta_y;
    brick[2] = hot_x;
    brick[3] = hot_y;
    memcpy(brick + 4, line_bytes, line_size);
}

static void build_solid_rect_bank(U8 width, U8 height, U8 hot_x, U8 hot_y, U8 color) {
    U8 line_bytes[256];
    size_t pos = 0;

    for (U8 row = 0; row < height; ++row) {
        line_bytes[pos++] = 1;
        line_bytes[pos++] = (U8)(0x80 | (width - 1));
        line_bytes[pos++] = color;
    }

    build_graph_bank_from_lines(width, height, hot_x, hot_y, line_bytes, pos);
}

static void build_pattern_bank(void) {
    static const U8 line_bytes[] = {
        3,
        0x00, /* jump 1 */
        0x41,
        0x11,
        0x22,
        0x81,
        0x33,
        3,
        0x80,
        0x44,
        0x01,
        0x42,
        0x55,
        0x66,
        0x77,
        2,
        0x82,
        0x88,
        0x40,
        0x99,
    };
    build_graph_bank_from_lines(6, 3, 1, 2, line_bytes, sizeof(line_bytes));
}

static void build_random_bank(void) {
    U8 width = (U8)(1 + (rng_next() % 12));
    U8 height = (U8)(1 + (rng_next() % 8));
    U8 hot_x = (U8)(rng_next() % 8);
    U8 hot_y = (U8)(rng_next() % 8);
    U8 line_bytes[768];
    size_t pos = 0;

    for (U8 row = 0; row < height; ++row) {
        size_t count_pos = pos++;
        U8 block_count = 0;
        U8 remaining = width;

        while (remaining > 0) {
            U8 block_type = (U8)(rng_next() % 3u);
            U8 amount = (U8)(1 + (rng_next() % remaining));

            if (block_type == 0) {
                line_bytes[pos++] = (U8)(amount - 1);
            } else if (block_type == 1) {
                line_bytes[pos++] = (U8)(0x40 | (amount - 1));
                for (U8 i = 0; i < amount; ++i)
                    line_bytes[pos++] = (U8)rng_next();
            } else {
                line_bytes[pos++] = (U8)(0x80 | (amount - 1));
                line_bytes[pos++] = (U8)rng_next();
            }

            block_count++;
            remaining = (U8)(remaining - amount);
        }

        line_bytes[count_pos] = block_count;
    }

    build_graph_bank_from_lines(width, height, hot_x, hot_y, line_bytes, pos);
}

static GraphRunResult run_cpp_affgraph(S32 x, S32 y,
                                       S32 clip_xmin, S32 clip_ymin,
                                       S32 clip_xmax, S32 clip_ymax) {
    GraphRunResult result;

    setup_screen(g_cpp_framebuf, clip_xmin, clip_ymin, clip_xmax, clip_ymax);
    AffGraph(0, x, y, g_bank);
    result.xmin = ScreenXMin;
    result.ymin = ScreenYMin;
    result.xmax = ScreenXMax;
    result.ymax = ScreenYMax;
    return result;
}

static GraphRunResult run_asm_affgraph(S32 x, S32 y,
                                       S32 clip_xmin, S32 clip_ymin,
                                       S32 clip_xmax, S32 clip_ymax) {
    GraphRunResult result;

    setup_screen(g_asm_framebuf, clip_xmin, clip_ymin, clip_xmax, clip_ymax);
    asm_AffGraph(0, x, y, g_bank);
    result.xmin = ScreenXMin;
    result.ymin = ScreenYMin;
    result.xmax = ScreenXMax;
    result.ymax = ScreenYMax;
    return result;
}

static void assert_affgraph_case(const char *label,
                                 S32 x, S32 y,
                                 S32 clip_xmin, S32 clip_ymin,
                                 S32 clip_xmax, S32 clip_ymax) {
    GraphRunResult cpp_result = run_cpp_affgraph(x, y, clip_xmin, clip_ymin, clip_xmax, clip_ymax);
    GraphRunResult asm_result = run_asm_affgraph(x, y, clip_xmin, clip_ymin, clip_xmax, clip_ymax);

    /* AffGraph has an S32 signature, but the ASM exits via raw ret paths
       without defining EAX, so only the framebuffer and ScreenX/Y globals
       are stable equivalence outputs here. */
    ASSERT_ASM_CPP_EQ_INT(asm_result.xmin, cpp_result.xmin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymin, cpp_result.ymin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.xmax, cpp_result.xmax, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymax, cpp_result.ymax, label);
    ASSERT_ASM_CPP_MEM_EQ(g_asm_framebuf, g_cpp_framebuf, sizeof(g_cpp_framebuf), label);
}

static void build_bank(void) {
    memset(g_bank, 0, sizeof(g_bank));
    U32 *offsets = (U32 *)g_bank;
    /* Brick 0 at offset 8 (after 2-entry offset table) */
    offsets[0] = 8;
    offsets[1] = 16;

    /* Brick 0: 10x20, hot=(0,0) */
    g_bank[8] = 10; /* DeltaX */
    g_bank[9] = 20; /* DeltaY */
    g_bank[10] = 0; /* HotX */
    g_bank[11] = 0; /* HotY */

    /* Brick 1: 8x6, hot=(-3, -2) (signed bytes) */
    g_bank[16] = 8;
    g_bank[17] = 6;
    g_bank[18] = (U8)(-3); /* HotX = -3 */
    g_bank[19] = (U8)(-2); /* HotY = -2 */
}

static void test_cpp_getbox_basic(void) {
    S32 x0, y0, x1, y1;
    build_bank();
    GetBoxGraph(0, &x0, &y0, &x1, &y1, g_bank);
    ASSERT_EQ_INT(0, x0);
    ASSERT_EQ_INT(0, y0);
    ASSERT_EQ_INT(10, x1);
    ASSERT_EQ_INT(20, y1);
}

static void test_cpp_getbox_hotspot(void) {
    S32 x0, y0, x1, y1;
    build_bank();
    GetBoxGraph(1, &x0, &y0, &x1, &y1, g_bank);
    ASSERT_EQ_INT(-3, x0);
    ASSERT_EQ_INT(-2, y0);
    ASSERT_EQ_INT(5, x1);
    ASSERT_EQ_INT(4, y1);
}

static void test_asm_equiv_getbox(void) {
    S32 cpp_x0, cpp_y0, cpp_x1, cpp_y1;
    S32 asm_x0, asm_y0, asm_x1, asm_y1;
    build_bank();

    GetBoxGraph(0, &cpp_x0, &cpp_y0, &cpp_x1, &cpp_y1, g_bank);
    asm_GetBoxGraph(0, &asm_x0, &asm_y0, &asm_x1, &asm_y1, g_bank);

    ASSERT_EQ_INT(cpp_x0, asm_x0);
    ASSERT_EQ_INT(cpp_y0, asm_y0);
    ASSERT_EQ_INT(cpp_x1, asm_x1);
    ASSERT_EQ_INT(cpp_y1, asm_y1);
}

static void test_asm_equiv_getbox_hotspot(void) {
    S32 cpp_x0, cpp_y0, cpp_x1, cpp_y1;
    S32 asm_x0, asm_y0, asm_x1, asm_y1;

    build_bank();

    GetBoxGraph(1, &cpp_x0, &cpp_y0, &cpp_x1, &cpp_y1, g_bank);
    asm_GetBoxGraph(1, &asm_x0, &asm_y0, &asm_x1, &asm_y1, g_bank);

    ASSERT_EQ_INT(cpp_x0, asm_x0);
    ASSERT_EQ_INT(cpp_y0, asm_y0);
    ASSERT_EQ_INT(cpp_x1, asm_x1);
    ASSERT_EQ_INT(cpp_y1, asm_y1);
}

static void test_asm_equiv_getbox_positive_hotspot(void) {
    S32 cpp_x0, cpp_y0, cpp_x1, cpp_y1;
    S32 asm_x0, asm_y0, asm_x1, asm_y1;

    memset(g_bank, 0, sizeof(g_bank));
    U32 *offsets = (U32 *)g_bank;
    offsets[0] = 4;
    g_bank[4] = 12;
    g_bank[5] = 8;
    g_bank[6] = 3;
    g_bank[7] = 2;

    GetBoxGraph(0, &cpp_x0, &cpp_y0, &cpp_x1, &cpp_y1, g_bank);
    asm_GetBoxGraph(0, &asm_x0, &asm_y0, &asm_x1, &asm_y1, g_bank);

    ASSERT_EQ_INT(cpp_x0, asm_x0);
    ASSERT_EQ_INT(cpp_y0, asm_y0);
    ASSERT_EQ_INT(cpp_x1, asm_x1);
    ASSERT_EQ_INT(cpp_y1, asm_y1);
}

static void test_cpp_getbox_highbit_hotspot(void) {
    S32 x0, y0, x1, y1;

    memset(g_bank, 0, sizeof(g_bank));
    ((U32 *)g_bank)[0] = 4;
    g_bank[4] = 5;
    g_bank[5] = 4;
    g_bank[6] = 0x80; /* HotX = -128 */
    g_bank[7] = 0xC8; /* HotY = -56 */

    GetBoxGraph(0, &x0, &y0, &x1, &y1, g_bank);
    ASSERT_EQ_INT(-128, x0);
    ASSERT_EQ_INT(-56, y0);
    ASSERT_EQ_INT(-123, x1);
    ASSERT_EQ_INT(-52, y1);
}

static void test_asm_equiv_getbox_highbit_hotspot(void) {
    S32 cpp_x0, cpp_y0, cpp_x1, cpp_y1;
    S32 asm_x0, asm_y0, asm_x1, asm_y1;

    memset(g_bank, 0, sizeof(g_bank));
    ((U32 *)g_bank)[0] = 4;
    g_bank[4] = 5;
    g_bank[5] = 4;
    g_bank[6] = 0x80;
    g_bank[7] = 0xC8;

    GetBoxGraph(0, &cpp_x0, &cpp_y0, &cpp_x1, &cpp_y1, g_bank);
    asm_GetBoxGraph(0, &asm_x0, &asm_y0, &asm_x1, &asm_y1, g_bank);

    ASSERT_EQ_INT(cpp_x0, asm_x0);
    ASSERT_EQ_INT(cpp_y0, asm_y0);
    ASSERT_EQ_INT(cpp_x1, asm_x1);
    ASSERT_EQ_INT(cpp_y1, asm_y1);
}

static void test_cpp_affgraph_basic(void) {
    build_solid_rect_bank(4, 2, 0, 0, 0x5A);
    setup_screen(g_cpp_framebuf, 0, 0, 639, 479);
    AffGraph(0, 50, 60, g_bank);

    ASSERT_EQ_INT(0x5A, g_cpp_framebuf[60 * 640 + 50]);
    ASSERT_EQ_INT(0x5A, g_cpp_framebuf[60 * 640 + 53]);
    ASSERT_EQ_INT(0x5A, g_cpp_framebuf[61 * 640 + 50]);
    ASSERT_EQ_INT(50, ScreenXMin);
    ASSERT_EQ_INT(60, ScreenYMin);
    ASSERT_EQ_INT(53, ScreenXMax);
    ASSERT_EQ_INT(61, ScreenYMax);
}

static void test_affgraph_fixed_equivalence(void) {
    build_solid_rect_bank(4, 2, 0, 0, 0xA1);
    assert_affgraph_case("AffGraph solid interior", 100, 120, 0, 0, 639, 479);

    build_pattern_bank();
    assert_affgraph_case("AffGraph pattern interior", 80, 60, 0, 0, 639, 479);

    build_solid_rect_bank(6, 3, (U8)-3, (U8)-2, 0x4C);
    assert_affgraph_case("AffGraph negative hotspot interior", 30, 40, 0, 0, 639, 479);

    build_solid_rect_bank(5, 3, 0x80, 0xC8, 0x6D);
    assert_affgraph_case("AffGraph highbit hotspot interior", 200, 100, 0, 0, 639, 479);

    setup_screen(g_cpp_framebuf, 0, 0, 639, 479);
    build_solid_rect_bank(5, 3, 0x80, 0xC8, 0x6E);
    AffGraph(0, 200, 100, g_bank);
    ASSERT_EQ_INT(72, ScreenXMin);
    ASSERT_EQ_INT(44, ScreenYMin);
    ASSERT_EQ_INT(76, ScreenXMax);
    ASSERT_EQ_INT(46, ScreenYMax);
    ASSERT_EQ_UINT(0x6E, g_cpp_framebuf[44 * 640 + 72]);
    ASSERT_EQ_UINT(0x6E, g_cpp_framebuf[46 * 640 + 76]);
}

static void test_affgraph_clipping_equivalence(void) {
    build_solid_rect_bank(6, 3, 0, 0, 0x77);
    assert_affgraph_case("AffGraph left clip", -2, 100, 0, 0, 639, 479);
    assert_affgraph_case("AffGraph top clip", 200, -1, 0, 0, 639, 479);

    build_pattern_bank();
    assert_affgraph_case("AffGraph clip window", 12, 14, 10, 12, 20, 20);
    assert_affgraph_case("AffGraph offscreen no-op", -40, -30, 0, 0, 639, 479);
}

static void test_affgraph_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int round = 0; round < 100; ++round) {
        S32 x = (S32)(rng_next() % 680u) - 20;
        S32 y = (S32)(rng_next() % 520u) - 20;
        S32 clip_xmin = (S32)(rng_next() % 320u);
        S32 clip_ymin = (S32)(rng_next() % 240u);
        S32 clip_xmax = clip_xmin + (S32)(rng_next() % (640u - (U32)clip_xmin));
        S32 clip_ymax = clip_ymin + (S32)(rng_next() % (480u - (U32)clip_ymin));
        char label[80];

        build_random_bank();
        snprintf(label, sizeof(label), "AffGraph random %d", round);
        assert_affgraph_case(label, x, y, clip_xmin, clip_ymin, clip_xmax, clip_ymax);
    }
}

/* ── Edge sweep ────────────────────────────────────────────────────────────
 * Real-size shapes walked in 1-px steps across each edge and corner of the
 * screen and of smaller clip windows, ASM vs C++. AffGraph clips in the
 * original (ClippingGraph), so the shapes start fully outside the window.
 *
 * The background is 0xFF and no block colour is 0xFF, so every written
 * pixel shows, including the zeros a copy block writes on the unclipped
 * path (the clipped path treats them as transparent). */

#define SWEEP_BG 0xFF

typedef struct SweepShape {
    const char *name;
    U8 width;
    U8 height;
    U8 hot_x; /* signed byte */
    U8 hot_y; /* signed byte */
    U32 seed;
} SweepShape;

typedef struct SweepWindow {
    const char *name;
    S32 xmin;
    S32 ymin;
    S32 xmax;
    S32 ymax;
} SweepWindow;

typedef struct SweepStats {
    int cases;
    int empty;
    int partial;
    int full;
} SweepStats;

/* A 255x255 shape is at most 255 * (1 + 2 * 255) bytes of line data. */
static U32 g_sweep_bank_words[(1u << 18) / 4];
#define g_sweep_bank ((U8 *)g_sweep_bank_words)
static S32 g_sweep_full_count;

static const SweepShape g_sweep_shapes[] = {
    {"brick 48x38", 48, 38, (U8)-24, (U8)-38, 0x356u},
    {"sprite 100x80", 100, 80, 0x9C /* -100 */, 0xB0 /* -80 */, 0x1997u},
    {"max 255x255", 255, 255, 0x80 /* -128 */, 0x7F /* +127 */, 0xB1Cu},
};

static U8 sweep_color(void) {
    return (U8)(1 + (rng_next() % 0xFEu)); /* 1..0xFE */
}

/* Appends blocks covering exactly `len` pixels; `kind` -1 picks at random,
 * otherwise 0 jump, 1 copy, 2 repeat for every block. */
static void sweep_emit_span(U8 *line, size_t *pos, U8 *nb_block, S32 len, int kind) {
    while (len > 0) {
        S32 max = len < 64 ? len : 64;
        S32 amount = 1 + (S32)(rng_next() % (U32)max);
        int type = kind;

        if (type < 0) {
            U32 r = rng_next() % 20u;
            type = r < 5 ? 0 : (r < 13 ? 1 : 2);
        }
        if (type == 0) {
            line[(*pos)++] = (U8)(amount - 1);
        } else if (type == 1) {
            line[(*pos)++] = (U8)(0x40 | (amount - 1));
            for (S32 i = 0; i < amount; ++i)
                line[(*pos)++] = (rng_next() % 8u) == 0 ? 0 : sweep_color();
        } else {
            line[(*pos)++] = (U8)(0x80 | (amount - 1));
            line[(*pos)++] = sweep_color();
        }
        (*nb_block)++;
        len -= amount;
    }
}

/* Rows 0 and h-1 are solid, every 11th row (from 5) is fully transparent,
 * a 3-px column hole runs down the middle, the outer columns are opaque,
 * and the rest is a random mix of jump/copy/repeat runs of 1..64 px. */
static void build_sweep_shape(const SweepShape *shape) {
    U8 *bank = g_sweep_bank;
    S32 w = shape->width;
    S32 h = shape->height;
    S32 hole = w / 2 - 1;
    size_t pos = 8;

    rng_seed(shape->seed);
    memset(bank, 0, sizeof(g_sweep_bank_words));
    ((U32 *)bank)[0] = 4;
    bank[4] = shape->width;
    bank[5] = shape->height;
    bank[6] = shape->hot_x;
    bank[7] = shape->hot_y;

    for (S32 row = 0; row < h; ++row) {
        size_t count_pos = pos++;
        U8 nb_block = 0;

        if (row == 0 || row == h - 1) {
            sweep_emit_span(bank, &pos, &nb_block, w, 2);
        } else if (row % 11 == 5) {
            sweep_emit_span(bank, &pos, &nb_block, w, 0);
        } else {
            sweep_emit_span(bank, &pos, &nb_block, 1, 1);
            sweep_emit_span(bank, &pos, &nb_block, hole - 1, -1);
            sweep_emit_span(bank, &pos, &nb_block, 3, 0);
            sweep_emit_span(bank, &pos, &nb_block, w - hole - 4, -1);
            sweep_emit_span(bank, &pos, &nb_block, 1, 1);
        }
        bank[count_pos] = nb_block;
    }
}

static void setup_sweep_screen(U8 *framebuf, const SweepWindow *win) {
    setup_screen(framebuf, win->xmin, win->ymin, win->xmax, win->ymax);
    memset(framebuf, SWEEP_BG, 640 * 480);
}

static void report_first_diff(const char *label) {
    S32 n = 0;
    S32 first = -1;
    for (S32 i = 0; i < 640 * 480; ++i) {
        if (g_asm_framebuf[i] != g_cpp_framebuf[i]) {
            if (first < 0)
                first = i;
            n++;
        }
    }
    if (first >= 0)
        fprintf(stderr, "    [%s] %d px differ, first at (%d,%d): ASM=%02X CPP=%02X\n",
                label, n, first % 640, first / 640,
                g_asm_framebuf[first], g_cpp_framebuf[first]);
}

/* Both framebuffers start a walk all background. Each case compares,
 * counts and then clears only the shape's box inside the clip window, so
 * a stray write outside that box is never cleared and the full-buffer
 * compare at the end of the walk still sees it. */
static void sweep_begin(const SweepWindow *win) {
    setup_sweep_screen(g_asm_framebuf, win);
    setup_sweep_screen(g_cpp_framebuf, win);
}

static void sweep_end(const char *label) {
    if (memcmp(g_asm_framebuf, g_cpp_framebuf, sizeof(g_cpp_framebuf)) != 0)
        report_first_diff(label);
    ASSERT_ASM_CPP_MEM_EQ(g_asm_framebuf, g_cpp_framebuf, sizeof(g_cpp_framebuf), label);
}

static void sweep_draw(U8 *framebuf, S32 x, S32 y, int use_asm) {
    Log = framebuf;
    ScreenXMin = 32000;
    ScreenYMin = 32000;
    ScreenXMax = -32000;
    ScreenYMax = -32000;
    if (use_asm)
        asm_AffGraph(0, x, y, g_sweep_bank);
    else
        AffGraph(0, x, y, g_sweep_bank);
}

/* Box position (bx0, by0) is the shape's top-left on screen. Returns the
 * number of pixels the C++ drew. */
static S32 sweep_case(const SweepShape *shape, const SweepWindow *win,
                      const char *edge, S32 bx0, S32 by0, SweepStats *stats) {
    S32 x = bx0 - (S8)shape->hot_x;
    S32 y = by0 - (S8)shape->hot_y;
    S32 rx0 = bx0 > win->xmin ? bx0 : win->xmin;
    S32 ry0 = by0 > win->ymin ? by0 : win->ymin;
    S32 rx1 = bx0 + shape->width - 1;
    S32 ry1 = by0 + shape->height - 1;
    S32 gx0, gy0, gx1, gy1;
    S32 asm_box[4];
    S32 drawn = 0;
    int rect_differs = 0;
    char label[160];

    if (rx1 > win->xmax)
        rx1 = win->xmax;
    if (ry1 > win->ymax)
        ry1 = win->ymax;
    snprintf(label, sizeof(label), "%s / %s / %s box=(%d,%d)",
             shape->name, win->name, edge, bx0, by0);

    sweep_draw(g_asm_framebuf, x, y, 1);
    asm_box[0] = ScreenXMin;
    asm_box[1] = ScreenYMin;
    asm_box[2] = ScreenXMax;
    asm_box[3] = ScreenYMax;
    sweep_draw(g_cpp_framebuf, x, y, 0);

    ASSERT_ASM_CPP_EQ_INT(asm_box[0], ScreenXMin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_box[1], ScreenYMin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_box[2], ScreenXMax, label);
    ASSERT_ASM_CPP_EQ_INT(asm_box[3], ScreenYMax, label);

    /* AffGraph's bbox is GetBoxGraph's box (exclusive max) moved to (x, y). */
    GetBoxGraph(0, &gx0, &gy0, &gx1, &gy1, g_sweep_bank);
    ASSERT_EQ_INT(x + gx0, ScreenXMin);
    ASSERT_EQ_INT(y + gy0, ScreenYMin);
    ASSERT_EQ_INT(x + gx1 - 1, ScreenXMax);
    ASSERT_EQ_INT(y + gy1 - 1, ScreenYMax);

    if (rx0 <= rx1 && ry0 <= ry1) {
        size_t span = (size_t)(rx1 - rx0 + 1);
        for (S32 row = ry0; row <= ry1; ++row) {
            const U8 *c = g_cpp_framebuf + row * 640 + rx0;
            if (memcmp(g_asm_framebuf + row * 640 + rx0, c, span) != 0)
                rect_differs = 1;
            for (size_t i = 0; i < span; ++i)
                drawn += c[i] != SWEEP_BG;
        }
    }
    assert_count++;
    if (rect_differs) {
        report_first_diff(label);
        FAIL_MSG("[%s] ASM vs CPP framebuffer differs inside the box", label);
    }
    if (rx0 <= rx1 && ry0 <= ry1) {
        size_t span = (size_t)(rx1 - rx0 + 1);
        for (S32 row = ry0; row <= ry1; ++row) {
            memset(g_asm_framebuf + row * 640 + rx0, SWEEP_BG, span);
            memset(g_cpp_framebuf + row * 640 + rx0, SWEEP_BG, span);
        }
    }

    stats->cases++;
    if (drawn == 0)
        stats->empty++;
    else if (drawn == g_sweep_full_count)
        stats->full++;
    else
        stats->partial++;
    return drawn;
}

static void sweep_shape_window(const SweepShape *shape, const SweepWindow *win,
                               int expect_full) {
    static const char *const edges[] = {"left", "right", "top", "bottom",
                                        "top-left", "top-right",
                                        "bottom-left", "bottom-right"};
    S32 w = shape->width;
    S32 h = shape->height;
    S32 mid_x = (win->xmin + win->xmax) / 2 - w / 2;
    S32 mid_y = (win->ymin + win->ymax) / 2 - h / 2;
    S32 diag = (w > h ? w : h) + 1;

    for (int e = 0; e < 8; ++e) {
        SweepStats stats = {0, 0, 0, 0};
        char label[160];

        snprintf(label, sizeof(label), "%s / %s / %s walk", shape->name, win->name, edges[e]);
        sweep_begin(win);
        /* t = 0: just past the edge; last step: one pixel inside it. */
        if (e == 0) {
            for (S32 t = 0; t <= w + 1; ++t)
                sweep_case(shape, win, edges[e], win->xmin - w + t, mid_y, &stats);
        } else if (e == 1) {
            for (S32 t = 0; t <= w + 1; ++t)
                sweep_case(shape, win, edges[e], win->xmax + 1 - t, mid_y, &stats);
        } else if (e == 2) {
            for (S32 t = 0; t <= h + 1; ++t)
                sweep_case(shape, win, edges[e], mid_x, win->ymin - h + t, &stats);
        } else if (e == 3) {
            for (S32 t = 0; t <= h + 1; ++t)
                sweep_case(shape, win, edges[e], mid_x, win->ymax + 1 - t, &stats);
        } else {
            for (S32 t = 0; t <= diag; ++t) {
                S32 ox = t < w + 1 ? t : w + 1;
                S32 oy = t < h + 1 ? t : h + 1;
                S32 bx = (e == 4 || e == 6) ? win->xmin - w + ox : win->xmax + 1 - ox;
                S32 by = (e == 4 || e == 5) ? win->ymin - h + oy : win->ymax + 1 - oy;
                sweep_case(shape, win, edges[e], bx, by, &stats);
            }
        }

        sweep_end(label);

        /* The walk must cover nothing drawn, partial draws and, where the
         * window can hold the shape, the whole shape. */
        snprintf(label, sizeof(label), "%s / %s / %s coverage", shape->name, win->name, edges[e]);
        ASSERT_TRUE(stats.empty > 0);
        ASSERT_TRUE(stats.partial > 0);
        if (expect_full)
            ASSERT_TRUE(stats.full > 0);
        printf("    %s: %d cases, %d empty, %d partial, %d full\n",
               label, stats.cases, stats.empty, stats.partial, stats.full);
    }
}

static void sweep_windows(const SweepWindow *wins, int nwin, int expect_full) {
    static const SweepWindow full_screen = {"full screen", 0, 0, 639, 479};

    for (size_t s = 0; s < sizeof(g_sweep_shapes) / sizeof(g_sweep_shapes[0]); ++s) {
        const SweepShape *shape = &g_sweep_shapes[s];
        SweepStats ref = {0, 0, 0, 0};

        build_sweep_shape(shape);
        /* Reference pixel count: shape whole and centred on a full screen. */
        g_sweep_full_count = -1;
        sweep_begin(&full_screen);
        g_sweep_full_count = sweep_case(shape, &full_screen, "centre", 320 - shape->width / 2,
                                        240 - shape->height / 2, &ref);
        sweep_end("centre");
        ASSERT_TRUE(g_sweep_full_count > 0);

        for (int w = 0; w < nwin; ++w)
            sweep_shape_window(shape, &wins[w], expect_full);
    }
}

static void test_getbox_sweep_shapes(void) {
    for (size_t s = 0; s < sizeof(g_sweep_shapes) / sizeof(g_sweep_shapes[0]); ++s) {
        const SweepShape *shape = &g_sweep_shapes[s];
        S32 c[4], a[4];

        build_sweep_shape(shape);
        GetBoxGraph(0, &c[0], &c[1], &c[2], &c[3], g_sweep_bank);
        asm_GetBoxGraph(0, &a[0], &a[1], &a[2], &a[3], g_sweep_bank);
        for (int i = 0; i < 4; ++i)
            ASSERT_ASM_CPP_EQ_INT(a[i], c[i], shape->name);
        ASSERT_EQ_INT((S8)shape->hot_x, c[0]);
        ASSERT_EQ_INT((S8)shape->hot_y, c[1]);
        ASSERT_EQ_INT((S8)shape->hot_x + shape->width, c[2]);
        ASSERT_EQ_INT((S8)shape->hot_y + shape->height, c[3]);
    }
}

static void test_affgraph_edge_sweep_full_screen(void) {
    static const SweepWindow wins[] = {{"full screen", 0, 0, 639, 479}};
    sweep_windows(wins, 1, 1);
}

static void test_affgraph_edge_sweep_inner_window(void) {
    static const SweepWindow wins[] = {{"inner 100,80-539,399", 100, 80, 539, 399}};
    sweep_windows(wins, 1, 1);
}

static void test_affgraph_edge_sweep_thin_windows(void) {
    static const SweepWindow wins[] = {
        {"1-row 0,240-639,240", 0, 240, 639, 240},
        {"1-column 320,0-320,479", 320, 0, 320, 479},
        {"1x1 320,240", 320, 240, 320, 240},
    };
    sweep_windows(wins, 3, 0);
}

int main(void) {
    RUN_TEST(test_cpp_getbox_basic);
    RUN_TEST(test_cpp_getbox_hotspot);
    RUN_TEST(test_cpp_getbox_highbit_hotspot);
    RUN_TEST(test_asm_equiv_getbox);
    RUN_TEST(test_asm_equiv_getbox_hotspot);
    RUN_TEST(test_asm_equiv_getbox_positive_hotspot);
    RUN_TEST(test_asm_equiv_getbox_highbit_hotspot);
    RUN_TEST(test_cpp_affgraph_basic);
    RUN_TEST(test_affgraph_fixed_equivalence);
    RUN_TEST(test_affgraph_clipping_equivalence);
    RUN_TEST(test_affgraph_random_equivalence);
    RUN_TEST(test_getbox_sweep_shapes);
    RUN_TEST(test_affgraph_edge_sweep_full_screen);
    RUN_TEST(test_affgraph_edge_sweep_inner_window);
    RUN_TEST(test_affgraph_edge_sweep_thin_windows);
    TEST_SUMMARY();
    return test_failures != 0;
}
