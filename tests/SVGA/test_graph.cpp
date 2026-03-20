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
    S32 ret;
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
    result.ret = AffGraph(0, x, y, g_bank);
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
    result.ret = asm_AffGraph(0, x, y, g_bank);
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
    TEST_SUMMARY();
    return test_failures != 0;
}
