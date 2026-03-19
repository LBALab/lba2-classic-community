/* Test: Box — draw rectangle outline with clipping */
#include "test_harness.h"

#include <SVGA/BOX.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <string.h>

extern "C" void asm_Box(S32 x0, S32 y0, S32 x1, S32 y1, S32 col);

static U8 framebuf[640 * 480];
static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void setup_screen(void) {
    memset(framebuf, 0, sizeof(framebuf));
    Log = framebuf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = 639;
    ClipYMax = 479;
}

static void assert_box_case(const char *label,
                            S32 x0, S32 y0, S32 x1, S32 y1, S32 col,
                            S32 clip_x_min, S32 clip_y_min,
                            S32 clip_x_max, S32 clip_y_max) {
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];

    setup_screen();
    ClipXMin = clip_x_min;
    ClipYMin = clip_y_min;
    ClipXMax = clip_x_max;
    ClipYMax = clip_y_max;
    Box(x0, y0, x1, y1, col);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    ClipXMin = clip_x_min;
    ClipYMin = clip_y_min;
    ClipXMax = clip_x_max;
    ClipYMax = clip_y_max;
    asm_Box(x0, y0, x1, y1, col);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), label);
}

static void test_equivalence(void) {
    assert_box_case("Box fixed simple", 10, 10, 20, 20, 0xFF, 0, 0, 639, 479);
    assert_box_case("Box fixed clipped top-left", -5, -5, 5, 5, 0xAA, 0, 0, 639, 479);
    assert_box_case("Box fixed single pixel", 100, 100, 100, 100, 0x42, 0, 0, 639, 479);
    assert_box_case("Box fixed custom clip", 10, 10, 50, 50, 0xCC, 20, 20, 40, 40);
    assert_box_case("Box fixed outside clip", -10, 30, -1, 60, 0x33, 0, 0, 639, 479);
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; ++i) {
        S32 x0 = (S32)(rng_next() % 800u) - 80;
        S32 y0 = (S32)(rng_next() % 600u) - 60;
        S32 x1 = (S32)(rng_next() % 800u) - 80;
        S32 y1 = (S32)(rng_next() % 600u) - 60;
        S32 clip_x_min = (S32)(rng_next() % 320u);
        S32 clip_y_min = (S32)(rng_next() % 240u);
        S32 clip_x_max = clip_x_min + (S32)(rng_next() % (640u - (U32)clip_x_min));
        S32 clip_y_max = clip_y_min + (S32)(rng_next() % (480u - (U32)clip_y_min));
        char label[64];

        if (x0 > x1) {
            S32 tmp = x0;
            x0 = x1;
            x1 = tmp;
        }
        if (y0 > y1) {
            S32 tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        snprintf(label, sizeof(label), "Box rand %d", i);
        assert_box_case(label, x0, y0, x1, y1, (S32)(rng_next() & 0xFFu),
                        clip_x_min, clip_y_min, clip_x_max, clip_y_max);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
