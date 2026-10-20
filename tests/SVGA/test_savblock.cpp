/* Test: SaveBlock — save screen region to buffer */
#include "test_harness.h"
#include "run_in_child.h"

#include <SVGA/SAVBLOCK.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>
#include <string.h>
#include <limits.h>

extern "C" void asm_SaveBlock(void *screen, void *buffer, S32 x0, S32 y0, S32 x1, S32 y1);

static const U32 kScreenWidth = 640;
static const U32 kScreenHeight = 480;
static const U32 kBufferSize = kScreenWidth * kScreenHeight;

static U8 screen_init[kBufferSize];
static U8 cpp_screen[kBufferSize];
static U8 asm_screen[kBufferSize];
static U8 cpp_buffer[kBufferSize];
static U8 asm_buffer[kBufferSize];
static U8 buffer_init[kBufferSize];
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

/* ---------- inverted rectangles ----------
 * Inverted by one column (x1 == x0 - 1), SAVBLOCK.ASM gets zero rep counts
 * and copies nothing. Inverted further, or by one row, its counts wrap to
 * about 4 G and the copy runs past both buffers; SaveBlock returns early for
 * any inverted rectangle. Each side runs in a child process so a crash or
 * hang fails this test instead of the binary. */
struct SaveCall {
    int use_asm;
    S32 x0, y0, x1, y1;
};

/* Child: 0 when neither the screen nor the buffer changed. */
static int save_copies_nothing(void *ctx) {
    const SaveCall *c = (const SaveCall *)ctx;
    if (c->use_asm)
        asm_SaveBlock(asm_screen, asm_buffer, c->x0, c->y0, c->x1, c->y1);
    else
        SaveBlock(cpp_screen, cpp_buffer, c->x0, c->y0, c->x1, c->y1);
    const U8 *scr = c->use_asm ? asm_screen : cpp_screen;
    const U8 *buf = c->use_asm ? asm_buffer : cpp_buffer;
    return memcmp(screen_init, scr, kBufferSize) == 0 && memcmp(buffer_init, buf, kBufferSize) == 0 ? 0 : 1;
}

static void test_inverted_rect_copies_nothing(void) {
    setup_screen();
    fill_screen_pattern(7u);
    fill_buffer_pattern(buffer_init, 8u);
    memcpy(cpp_screen, screen_init, sizeof(cpp_screen));
    memcpy(asm_screen, screen_init, sizeof(asm_screen));
    memcpy(cpp_buffer, buffer_init, sizeof(cpp_buffer));
    memcpy(asm_buffer, buffer_init, sizeof(asm_buffer));

    /* Inverted by one column: the ASM copies nothing, the C++ must match. */
    static const S32 by_one[][4] = {
        {201, 300, 200, 315},
        {0, 0, -1, 479},
    };
    for (size_t i = 0; i < sizeof(by_one) / sizeof(by_one[0]); i++) {
        const S32 *c = by_one[i];
        char label[96];
        int asm_sig, cpp_sig;
        SaveCall asm_call = {1, c[0], c[1], c[2], c[3]};
        SaveCall cpp_call = {0, c[0], c[1], c[2], c[3]};
        int asm_rc = run_in_child(save_copies_nothing, &asm_call, &asm_sig);
        int cpp_rc = run_in_child(save_copies_nothing, &cpp_call, &cpp_sig);
        snprintf(label, sizeof(label), "SaveBlock (%d,%d)-(%d,%d) (asm sig %d, cpp sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], asm_sig, cpp_sig);
        ASSERT_ASM_CPP_EQ_INT(asm_rc, cpp_rc, label);
        ASSERT_EQ_INT_MSG(0, asm_rc, label);
    }

    /* Inverted further, or by one row: the ASM runs a wrapped count and
       dies, so there is nothing to match. The C++ copies nothing. */
    static const S32 further[][4] = {
        {202, 300, 200, 315},
        {300, 300, 101, 315},
        {200, 301, 260, 300},
        {200, 320, 260, 300},
        {260, 320, 200, 300},
    };
    for (size_t i = 0; i < sizeof(further) / sizeof(further[0]); i++) {
        const S32 *c = further[i];
        char label[96];
        int asm_sig, cpp_sig;
        SaveCall asm_call = {1, c[0], c[1], c[2], c[3]};
        SaveCall cpp_call = {0, c[0], c[1], c[2], c[3]};
        int asm_rc = run_in_child(save_copies_nothing, &asm_call, &asm_sig);
        int cpp_rc = run_in_child(save_copies_nothing, &cpp_call, &cpp_sig);
        snprintf(label, sizeof(label), "SaveBlock (%d,%d)-(%d,%d) ASM dies (sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], asm_sig);
        ASSERT_EQ_INT_MSG(-1, asm_rc, label);
        snprintf(label, sizeof(label), "SaveBlock (%d,%d)-(%d,%d) copies nothing (cpp sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], cpp_sig);
        ASSERT_EQ_INT_MSG(0, cpp_rc, label);
    }
}

/* ---------- rectangles off the screen ----------
 * The ASM does not clip, so these are C++ only. The buffer holds the whole
 * rectangle; SaveBlock fills the bytes for the part on screen and leaves the
 * rest. The screen sits between guard rows, and the buffer has a guard
 * tail, so a copy that strays is seen. */
enum { kGuardRows = 16,
       kOffBufferSize = 720 * 520 };
static U8 guarded_screen[kScreenWidth * (kScreenHeight + 2 * kGuardRows)];
static U8 *const off_screen = guarded_screen + kScreenWidth * kGuardRows;
static U8 off_buffer[kOffBufferSize];
static U8 off_expect[kOffBufferSize];

/* Child: 0 when the buffer holds exactly the on-screen pixels, in the whole
   rectangle's layout, and nothing else changed. */
static int save_clips(void *ctx) {
    const SaveCall *c = (const SaveCall *)ctx;
    for (U32 i = 0; i < sizeof(guarded_screen); i++)
        guarded_screen[i] = (U8)((i * 29u + (i >> 3)) & 0xFFu);
    for (U32 i = 0; i < kOffBufferSize; i++)
        off_buffer[i] = off_expect[i] = (U8)((i * 13u + 0x5Au) & 0xFFu);

    const S64 w = (S64)c->x1 - c->x0 + 1;
    for (S64 y = c->y0 < 0 ? 0 : c->y0; y <= c->y1 && y < (S64)kScreenHeight; y++)
        for (S64 x = c->x0 < 0 ? 0 : c->x0; x <= c->x1 && x < (S64)kScreenWidth; x++)
            off_expect[(y - c->y0) * w + (x - c->x0)] = off_screen[y * kScreenWidth + x];

    static U8 screen_before[sizeof(guarded_screen)];
    memcpy(screen_before, guarded_screen, sizeof(guarded_screen));
    SaveBlock(off_screen, off_buffer, c->x0, c->y0, c->x1, c->y1);
    return memcmp(off_expect, off_buffer, kOffBufferSize) == 0 &&
                   memcmp(screen_before, guarded_screen, sizeof(guarded_screen)) == 0
               ? 0
               : 1;
}

static void test_offscreen_rect_clips(void) {
    setup_screen();
    static const S32 cases[][4] = {
        /* partly off screen */
        {-5, 10, 50, 20},
        {10, -5, 50, 20},
        {600, 470, 700, 500},
        {630, 0, 644, 3},
        {-10, -10, 650, 490},
        /* wholly off screen */
        {640, 0, 700, 10},
        {-20, 0, -1, 10},
        {0, 480, 10, 490},
        {0, -10, 10, -1},
        /* x1 - x0 does not fit an S32; one row, so the buffer stays small */
        {-5, 5, INT_MAX, 5},
        /* y1 - y0 large, with only the on-screen rows touched */
        {0, -5, 10, INT_MAX},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const S32 *c = cases[i];
        char label[96];
        int sig;
        SaveCall call = {0, c[0], c[1], c[2], c[3]};
        int rc = run_in_child(save_clips, &call, &sig);
        snprintf(label, sizeof(label), "SaveBlock (%d,%d)-(%d,%d) clips (sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], sig);
        ASSERT_EQ_INT_MSG(0, rc, label);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_inverted_rect_copies_nothing);
    RUN_TEST(test_offscreen_rect_clips);
    TEST_SUMMARY();
    return test_failures != 0;
}
