/* Test: AffMask — draw mask/sprite with single color.
 * Uses the same brick bank format as AffGraph and compares full framebuffer
 * plus touched screen-bounds globals against the ASM implementation. */
#include "test_harness.h"
#include <SVGA/MASK.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREENXY.H>
#include <string.h>
#include <stdio.h>

/* ASM AffMask uses Watcom register convention:
 *   parm [eax]=nummask [ebx]=x [ecx]=y [esi]=bankmask
 * We need a wrapper that takes C stack params and forwards to ASM regs. */
extern "C" void asm_AffMask(void);
extern "C" U8 asm_ColMask;

static S32 call_asm_AffMask(S32 num, S32 x, S32 y, void *bank) {
    S32 result;
    __asm__ __volatile__(
        "call asm_AffMask"
        : "=a"(result)
        : "a"(num), "b"(x), "c"(y), "S"(bank)
        : "edx", "edi", "memory", "cc");
    return result;
}

static U8 g_cpp_framebuf[640 * 480];
static U8 g_asm_framebuf[640 * 480];
static U8 g_bank[1024];

typedef struct MaskRunResult {
    S32 ret;
    S32 xmin;
    S32 xmax;
    S32 ymin;
    S32 ymax;
} MaskRunResult;

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
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    ClipXMin = clip_xmin;
    ClipYMin = clip_ymin;
    ClipXMax = clip_xmax;
    ClipYMax = clip_ymax;
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
}

static void build_mask_bank_from_lines(U8 delta_x, U8 delta_y,
                                       U8 hot_x, U8 hot_y,
                                       const U8 *line_bytes, size_t line_size) {
    memset(g_bank, 0, sizeof(g_bank));
    U32 *offsets = (U32 *)g_bank;
    offsets[0] = 4;
    U8 *b = g_bank + 4;
    b[0] = delta_x;
    b[1] = delta_y;
    b[2] = hot_x;
    b[3] = hot_y;
    memcpy(b + 4, line_bytes, line_size);
}

static void build_solid_rect_bank(U8 width, U8 height, U8 hot_x, U8 hot_y) {
    U8 line_bytes[256];
    size_t pos = 0;
    for (U8 row = 0; row < height; row++) {
        line_bytes[pos++] = 2;
        line_bytes[pos++] = 0;
        line_bytes[pos++] = width;
    }
    build_mask_bank_from_lines(width, height, hot_x, hot_y, line_bytes, pos);
}

static void build_pattern_bank(void) {
    static const U8 line_bytes[] = {
        5,
        1,
        2,
        1,
        3,
        1,
        4,
        0,
        1,
        2,
        5,
        2,
        3,
        5,
        5,
        0,
        2,
        2,
        2,
        2,
    };
    build_mask_bank_from_lines(8, 4, 1, 2, line_bytes, sizeof(line_bytes));
}

static void build_sparse_bank(void) {
    static const U8 line_bytes[] = {
        1,
        6,
        6,
        0,
        1,
        1,
        2,
        2,
        0,
        5,
        2,
        2,
        1,
        1,
        0,
        3,
        4,
        1,
        1,
        2,
        0,
        6,
    };
    build_mask_bank_from_lines(6, 5, 0, 0, line_bytes, sizeof(line_bytes));
}

static void build_random_bank(void) {
    U8 width = (U8)(1 + (rng_next() % 12));
    U8 height = (U8)(1 + (rng_next() % 8));
    U8 hot_x = (U8)(rng_next() % 4);
    U8 hot_y = (U8)(rng_next() % 4);
    U8 line_bytes[768];
    size_t pos = 0;

    for (U8 row = 0; row < height; row++) {
        size_t count_pos = pos++;
        U8 block_count = 0;
        U8 remaining = width;

        while (remaining > 0) {
            U8 jump = (U8)(rng_next() % (remaining + 1));
            line_bytes[pos++] = jump;
            block_count++;
            remaining = (U8)(remaining - jump);
            if (remaining == 0) {
                break;
            }

            U8 fill = (U8)(1 + (rng_next() % remaining));
            line_bytes[pos++] = fill;
            block_count++;
            remaining = (U8)(remaining - fill);
        }

        line_bytes[count_pos] = block_count;
    }

    build_mask_bank_from_lines(width, height, hot_x, hot_y, line_bytes, pos);
}

static MaskRunResult run_cpp_case(S32 x, S32 y, U8 color,
                                  S32 clip_xmin, S32 clip_ymin,
                                  S32 clip_xmax, S32 clip_ymax) {
    MaskRunResult result;
    setup_screen(g_cpp_framebuf, clip_xmin, clip_ymin, clip_xmax, clip_ymax);
    ColMask = color;
    result.ret = AffMask(0, x, y, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static MaskRunResult run_asm_case(S32 x, S32 y, U8 color,
                                  S32 clip_xmin, S32 clip_ymin,
                                  S32 clip_xmax, S32 clip_ymax) {
    MaskRunResult result;
    setup_screen(g_asm_framebuf, clip_xmin, clip_ymin, clip_xmax, clip_ymax);
    asm_ColMask = color;
    result.ret = call_asm_AffMask(0, x, y, g_bank);
    result.xmin = ScreenXMin;
    result.xmax = ScreenXMax;
    result.ymin = ScreenYMin;
    result.ymax = ScreenYMax;
    return result;
}

static void assert_case_matches(const char *label,
                                S32 x, S32 y, U8 color,
                                S32 clip_xmin, S32 clip_ymin,
                                S32 clip_xmax, S32 clip_ymax) {
    MaskRunResult cpp_result = run_cpp_case(x, y, color, clip_xmin, clip_ymin, clip_xmax, clip_ymax);
    MaskRunResult asm_result = run_asm_case(x, y, color, clip_xmin, clip_ymin, clip_xmax, clip_ymax);

    ASSERT_ASM_CPP_EQ_INT(asm_result.xmin, cpp_result.xmin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.xmax, cpp_result.xmax, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymin, cpp_result.ymin, label);
    ASSERT_ASM_CPP_EQ_INT(asm_result.ymax, cpp_result.ymax, label);
    ASSERT_ASM_CPP_MEM_EQ(g_asm_framebuf, g_cpp_framebuf, sizeof(g_cpp_framebuf), label);
}

static void test_cpp_basic(void) {
    build_solid_rect_bank(4, 2, 0, 0);
    setup_screen(g_cpp_framebuf, 0, 0, 639, 479);
    ColMask = 0xFF;
    AffMask(0, 50, 50, g_bank);
    /* All 8 pixels should be filled with ColMask=0xFF */
    ASSERT_EQ_UINT(0xFF, g_cpp_framebuf[50 * 640 + 50]);
    ASSERT_EQ_UINT(0xFF, g_cpp_framebuf[50 * 640 + 53]);
    ASSERT_EQ_UINT(0xFF, g_cpp_framebuf[51 * 640 + 50]);
    /* Outside the mask area should be 0 */
    ASSERT_EQ_UINT(0, g_cpp_framebuf[49 * 640 + 50]);
    ASSERT_EQ_INT(50, ScreenXMin);
    ASSERT_EQ_INT(53, ScreenXMax);
    ASSERT_EQ_INT(50, ScreenYMin);
    ASSERT_EQ_INT(51, ScreenYMax);
}

static void test_solid_and_pattern_fixed_cases(void) {
    build_solid_rect_bank(4, 2, 0, 0);
    assert_case_matches("AffMask solid interior", 100, 100, 0xAB, 0, 0, 639, 479);
    assert_case_matches("AffMask solid offset", 17, 203, 0x61, 0, 0, 639, 479);

    build_pattern_bank();
    assert_case_matches("AffMask pattern interior", 80, 60, 0x44, 0, 0, 639, 479);
    assert_case_matches("AffMask pattern hotspot", 11, 14, 0x7E, 0, 0, 639, 479);

    build_sparse_bank();
    assert_case_matches("AffMask sparse interior", 200, 33, 0x99, 0, 0, 639, 479);
}

static void test_screen_edge_clipping_cases(void) {
    build_solid_rect_bank(4, 2, 0, 0);
    assert_case_matches("AffMask left edge", -2, 120, 0x50, 0, 0, 639, 479);
    assert_case_matches("AffMask right edge", 638, 122, 0x51, 0, 0, 639, 479);
    assert_case_matches("AffMask top edge", 240, -1, 0x52, 0, 0, 639, 479);
    assert_case_matches("AffMask bottom edge", 241, 479, 0x53, 0, 0, 639, 479);
    assert_case_matches("AffMask top-left corner", -2, -1, 0x54, 0, 0, 639, 479);
    assert_case_matches("AffMask bottom-right corner", 638, 479, 0x55, 0, 0, 639, 479);

    build_pattern_bank();
    assert_case_matches("AffMask pattern crosses left edge", -4, 90, 0x56, 0, 0, 639, 479);
    assert_case_matches("AffMask pattern crosses right edge", 634, 90, 0x57, 0, 0, 639, 479);

    build_solid_rect_bank(4, 2, 0, 0);
    run_cpp_case(-2, 120, 0x58, 0, 0, 639, 479);
    ASSERT_EQ_UINT(0x58, g_cpp_framebuf[120 * 640 + 0]);
    ASSERT_EQ_UINT(0x58, g_cpp_framebuf[120 * 640 + 1]);
    ASSERT_EQ_UINT(0, g_cpp_framebuf[120 * 640 + 2]);

    run_cpp_case(638, 122, 0x59, 0, 0, 639, 479);
    ASSERT_EQ_UINT(0x59, g_cpp_framebuf[122 * 640 + 638]);
    ASSERT_EQ_UINT(0x59, g_cpp_framebuf[122 * 640 + 639]);
    ASSERT_EQ_UINT(0, g_cpp_framebuf[122 * 640 + 637]);
}

static void test_clip_window_cases(void) {
    build_solid_rect_bank(6, 3, 0, 0);
    assert_case_matches("AffMask clip window left", 118, 100, 0x33, 120, 90, 130, 110);
    assert_case_matches("AffMask clip window right", 127, 101, 0x34, 120, 90, 130, 110);
    assert_case_matches("AffMask clip window top", 123, 88, 0x35, 120, 90, 130, 110);
    assert_case_matches("AffMask clip window bottom", 123, 109, 0x36, 120, 90, 130, 110);

    build_pattern_bank();
    assert_case_matches("AffMask pattern clip window", 118, 97, 0x37, 120, 90, 126, 100);

    build_sparse_bank();
    assert_case_matches("AffMask sparse narrow clip", 50, 52, 0x38, 52, 54, 54, 57);
}

static void test_fully_clipped_noop_cases(void) {
    build_solid_rect_bank(4, 2, 0, 0);
    assert_case_matches("AffMask fully off left", -5, 140, 0x20, 0, 0, 639, 479);
    assert_case_matches("AffMask fully off right", 640, 140, 0x21, 0, 0, 639, 479);
    assert_case_matches("AffMask fully off top", 120, -3, 0x22, 0, 0, 639, 479);
    assert_case_matches("AffMask fully off bottom", 120, 480, 0x23, 0, 0, 639, 479);
    assert_case_matches("AffMask fully outside clip window", 50, 50, 0x24, 100, 100, 110, 110);

    run_cpp_case(-5, 140, 0x25, 0, 0, 639, 479);
    ASSERT_EQ_INT(32000, ScreenXMin);
    ASSERT_EQ_INT(-32000, ScreenXMax);
    ASSERT_EQ_INT(32000, ScreenYMin);
    ASSERT_EQ_INT(-32000, ScreenYMax);
}

static void test_randomized_stress(void) {
    int prev = test_failures;
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 100 && test_failures == prev; i++) {
        S32 x;
        S32 y;
        U8 color;
        S32 clip_xmin;
        S32 clip_ymin;
        S32 clip_xmax;
        S32 clip_ymax;
        char label[128];

        switch (rng_next() % 4) {
        case 0:
            build_solid_rect_bank((U8)(1 + (rng_next() % 10)), (U8)(1 + (rng_next() % 6)),
                                  (U8)(rng_next() % 3), (U8)(rng_next() % 3));
            break;
        case 1:
            build_pattern_bank();
            break;
        case 2:
            build_sparse_bank();
            break;
        default:
            build_random_bank();
            break;
        }

        x = (S32)(rng_next() % 700) - 40;
        y = (S32)(rng_next() % 540) - 40;
        color = (U8)rng_next();
        clip_xmin = (S32)(rng_next() % 640);
        clip_ymin = (S32)(rng_next() % 480);
        clip_xmax = clip_xmin + (S32)(rng_next() % (640 - clip_xmin));
        clip_ymax = clip_ymin + (S32)(rng_next() % (480 - clip_ymin));

        snprintf(label, sizeof(label),
                 "AffMask random #%d x=%d y=%d clip=(%d,%d)-(%d,%d) col=0x%02X",
                 i, x, y, clip_xmin, clip_ymin, clip_xmax, clip_ymax, color);
        assert_case_matches(label, x, y, color, clip_xmin, clip_ymin, clip_xmax, clip_ymax);
    }
}

int main(void) {
    RUN_TEST(test_cpp_basic);
    RUN_TEST(test_solid_and_pattern_fixed_cases);
    RUN_TEST(test_screen_edge_clipping_cases);
    RUN_TEST(test_clip_window_cases);
    RUN_TEST(test_fully_clipped_noop_cases);
    RUN_TEST(test_randomized_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}
