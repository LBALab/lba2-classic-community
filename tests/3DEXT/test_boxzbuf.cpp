#include "test_harness.h"

#include "BOXZBUF.H"

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/LIMITS.H>

#include <string.h>

extern "C" S32 asm_ZBufBoxOverWrite2(S32 z0, S32 z1,
                                      S32 xmin, S32 ymin,
                                      S32 xmax, S32 ymax);
extern S32 IncZ;

void *Log = NULL;
void *Screen = NULL;
U32 ModeDesiredX = 0;
U32 ModeDesiredY = 0;
U32 TabOffLine[ADELINE_MAX_Y_RES];
U32 ScreenPitch = 0;
PTR_U16 PtrZBuffer = NULL;
U32 Fill_ZBuffer_Factor = 0;

static const U32 kWidth = 64;
static const U32 kHeight = 48;
static const U32 kPixelCount = kWidth * kHeight;

static U8 initial_screen[kPixelCount];
static U8 initial_log[kPixelCount];
static U16 initial_zbuf[kPixelCount];

static U8 cpp_screen[kPixelCount];
static U8 cpp_log[kPixelCount];
static U16 cpp_zbuf[kPixelCount];

static U8 asm_screen[kPixelCount];
static U8 asm_log[kPixelCount];
static U16 asm_zbuf[kPixelCount];

static U32 rng_state;

static void rng_seed(U32 s) { rng_state = s; }

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

struct RunResult {
    S32 ret;
};

static void setup_globals(U8 *screen, U8 *log, U16 *zbuf, U32 z_factor)
{
    Screen = screen;
    Log = log;
    PtrZBuffer = zbuf;
    ScreenPitch = kWidth;
    ModeDesiredX = kWidth;
    ModeDesiredY = kHeight;
    Fill_ZBuffer_Factor = z_factor;
    for (U32 y = 0; y < kHeight; ++y) {
        TabOffLine[y] = y * kWidth;
    }
}

static void init_default_buffers(void)
{
    for (U32 i = 0; i < kPixelCount; ++i) {
        initial_screen[i] = (U8)((i * 37u + 13u) & 0xFFu);
        initial_log[i] = (U8)((i * 19u + 91u) & 0xFFu);
        initial_zbuf[i] = (U16)((i * 29u + 7u) & 0xFFFFu);
    }
}

static void clone_initial_state(void)
{
    memcpy(cpp_screen, initial_screen, sizeof(initial_screen));
    memcpy(cpp_log, initial_log, sizeof(initial_log));
    memcpy(cpp_zbuf, initial_zbuf, sizeof(initial_zbuf));

    memcpy(asm_screen, initial_screen, sizeof(initial_screen));
    memcpy(asm_log, initial_log, sizeof(initial_log));
    memcpy(asm_zbuf, initial_zbuf, sizeof(initial_zbuf));
}

static U32 pixel_index(U32 x, U32 y)
{
    return y * kWidth + x;
}

static RunResult run_cpp(S32 z0, S32 z1,
                         S32 xmin, S32 ymin,
                         S32 xmax, S32 ymax,
                         U32 z_factor)
{
    setup_globals(cpp_screen, cpp_log, cpp_zbuf, z_factor);
    IncZ = 0x13579BDF;

    RunResult result;
    result.ret = ZBufBoxOverWrite2(z0, z1, xmin, ymin, xmax, ymax);
    return result;
}

static RunResult run_asm(S32 z0, S32 z1,
                         S32 xmin, S32 ymin,
                         S32 xmax, S32 ymax,
                         U32 z_factor)
{
    setup_globals(asm_screen, asm_log, asm_zbuf, z_factor);

    RunResult result;
    result.ret = asm_ZBufBoxOverWrite2(z0, z1, xmin, ymin, xmax, ymax);
    return result;
}

static void assert_case_equivalence(const char *label,
                                    S32 z0, S32 z1,
                                    S32 xmin, S32 ymin,
                                    S32 xmax, S32 ymax,
                                    U32 z_factor)
{
    clone_initial_state();

    RunResult cpp_result = run_cpp(z0, z1, xmin, ymin, xmax, ymax, z_factor);
    RunResult asm_result = run_asm(z0, z1, xmin, ymin, xmax, ymax, z_factor);

    ASSERT_ASM_CPP_EQ_INT(asm_result.ret, cpp_result.ret, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_log, cpp_log, sizeof(cpp_log), label);
    ASSERT_ASM_CPP_MEM_EQ((U8 *)asm_zbuf, (U8 *)cpp_zbuf, sizeof(cpp_zbuf), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_screen, cpp_screen, sizeof(cpp_screen), label);
}

static void fill_rect_z(U16 value, U32 xmin, U32 ymin, U32 xmax, U32 ymax)
{
    for (U32 y = ymin; y <= ymax; ++y) {
        for (U32 x = xmin; x <= xmax; ++x) {
            initial_zbuf[pixel_index(x, y)] = value;
        }
    }
}

static void test_boxzbuf_all_visible(void)
{
    init_default_buffers();
    fill_rect_z(4, 10, 8, 13, 10);

    assert_case_equivalence("ZBufBoxOverWrite2 all visible",
                            16, 12, 10, 8, 13, 10, 0x10000u);
}

static void test_boxzbuf_all_hidden(void)
{
    init_default_buffers();
    fill_rect_z(255, 20, 6, 24, 9);

    assert_case_equivalence("ZBufBoxOverWrite2 all hidden",
                            18, 12, 20, 6, 24, 9, 0x10000u);
}

static void test_boxzbuf_mixed_occlusion(void)
{
    init_default_buffers();
    fill_rect_z(5, 30, 12, 33, 14);
    initial_zbuf[pixel_index(31, 12)] = 20;
    initial_zbuf[pixel_index(33, 12)] = 21;
    initial_zbuf[pixel_index(32, 13)] = 15;
    initial_zbuf[pixel_index(30, 14)] = 19;

    assert_case_equivalence("ZBufBoxOverWrite2 mixed occlusion",
                            18, 12, 30, 12, 33, 14, 0x10000u);
}

static void test_boxzbuf_single_pixel(void)
{
    init_default_buffers();
    initial_zbuf[pixel_index(7, 11)] = 3;

    assert_case_equivalence("ZBufBoxOverWrite2 single pixel",
                            9, 9, 7, 11, 7, 11, 0x10000u);
}

static void test_boxzbuf_negative_head_depth(void)
{
    init_default_buffers();
    fill_rect_z(65000u, 4, 4, 6, 6);

    assert_case_equivalence("ZBufBoxOverWrite2 negative head depth",
                            4, -2, 4, 4, 6, 6, 0x10000u);
}

static void randomize_buffers(void)
{
    for (U32 i = 0; i < kPixelCount; ++i) {
        initial_screen[i] = (U8)rng_next();
        initial_log[i] = (U8)rng_next();
        initial_zbuf[i] = (U16)((rng_next() << 1) ^ rng_next());
    }
}

static void test_boxzbuf_random_stress(void)
{
    rng_seed(0xDEADBEEFu);

    for (int round = 0; round < 300; ++round) {
        randomize_buffers();

        U32 xmin = (U32)(rng_next() % (kWidth - 1));
        U32 xmax = xmin + (U32)(rng_next() % (kWidth - xmin));
        U32 ymin = (U32)(rng_next() % (kHeight - 1));
        U32 ymax = ymin + (U32)(rng_next() % (kHeight - ymin));
        S32 z0 = (S32)(rng_next() % 2001) - 1000;
        S32 z1 = (S32)(rng_next() % 2001) - 1000;
        U32 z_factor = 0x10000u;
        char msg[96];

        snprintf(msg, sizeof(msg),
                 "ZBufBoxOverWrite2 random round %d", round);

        assert_case_equivalence(msg, z0, z1,
                                (S32)xmin, (S32)ymin,
                                (S32)xmax, (S32)ymax,
                                z_factor);
    }
}

int main(void)
{
    RUN_TEST(test_boxzbuf_all_visible);
    RUN_TEST(test_boxzbuf_all_hidden);
    RUN_TEST(test_boxzbuf_mixed_occlusion);
    RUN_TEST(test_boxzbuf_single_pixel);
    RUN_TEST(test_boxzbuf_negative_head_depth);
    RUN_TEST(test_boxzbuf_random_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}