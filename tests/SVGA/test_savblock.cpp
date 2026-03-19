/* Test: SaveBlock — save screen region to buffer */
#include "test_harness.h"

#include <SVGA/SAVBLOCK.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>
#include <string.h>

extern "C" void asm_SaveBlock(void *screen, void *buffer, S32 x0, S32 y0, S32 x1, S32 y1);

static const U32 kScreenWidth = 640;
static const U32 kScreenHeight = 480;
static const U32 kBufferSize = kScreenWidth * kScreenHeight;

static U8 screen_init[kBufferSize];
static U8 cpp_screen[kBufferSize];
static U8 asm_screen[kBufferSize];
static U8 cpp_buffer[kBufferSize];
static U8 asm_buffer[kBufferSize];
static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void setup_screen(void) {
    ModeDesiredX = (S32)kScreenWidth;
    ModeDesiredY = (S32)kScreenHeight;
    for (U32 i = 0; i < kScreenHeight; ++i) {
        TabOffLine[i] = i * kScreenWidth;
    }
}

static void fill_screen_pattern(U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        screen_init[i] = (U8)(((i * 23u) + (salt * 41u) + (i >> 1)) & 0xFFu);
    }
}

static void fill_buffer_pattern(U8 *buffer, U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        buffer[i] = (U8)(((i * 7u) + (salt * 19u) + 0x3Cu) & 0xFFu);
    }
}

static void assert_saveblock_case(const char *label,
                                  S32 x0, S32 y0, S32 x1, S32 y1,
                                  U32 salt) {
    setup_screen();
    fill_screen_pattern(salt);
    memcpy(cpp_screen, screen_init, sizeof(cpp_screen));
    memcpy(asm_screen, screen_init, sizeof(asm_screen));
    fill_buffer_pattern(cpp_buffer, salt + 1u);
    fill_buffer_pattern(asm_buffer, salt + 1u);

    SaveBlock(cpp_screen, cpp_buffer, x0, y0, x1, y1);
    asm_SaveBlock(asm_screen, asm_buffer, x0, y0, x1, y1);

    ASSERT_ASM_CPP_MEM_EQ(screen_init, cpp_screen, sizeof(cpp_screen), label);
    ASSERT_ASM_CPP_MEM_EQ(screen_init, asm_screen, sizeof(asm_screen), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_buffer, cpp_buffer, sizeof(cpp_buffer), label);
}

static void test_equivalence(void) {
    assert_saveblock_case("SaveBlock fixed square", 10, 10, 20, 20, 1u);
    assert_saveblock_case("SaveBlock fixed single pixel", 50, 50, 50, 50, 2u);
    assert_saveblock_case("SaveBlock fixed full row", 0, 100, 639, 100, 3u);
    assert_saveblock_case("SaveBlock fixed tall rect", 200, 5, 240, 300, 4u);
    assert_saveblock_case("SaveBlock fixed full screen", 0, 0, 639, 479, 5u);
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 80; ++i) {
        S32 x0 = (S32)(rng_next() % 640u);
        S32 y0 = (S32)(rng_next() % 480u);
        S32 x1 = x0 + (S32)(rng_next() % (640u - (U32)x0));
        S32 y1 = y0 + (S32)(rng_next() % (480u - (U32)y0));
        char label[64];

        snprintf(label, sizeof(label), "SaveBlock rand %d", i);
        assert_saveblock_case(label, x0, y0, x1, y1, (U32)i + 100u);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
