/* Test: RestoreBlock — restore saved screen region */
#include "test_harness.h"
#include "run_in_child.h"
#include <SVGA/RESBLOCK.H>
#include <SVGA/SAVBLOCK.H>
#include <SVGA/SCREEN.H>
#include <limits.h>
#include <string.h>
#include <stdio.h>

extern "C" void asm_RestoreBlock(void *screen, void *buffer, S32 x0, S32 y0, S32 x1, S32 y1);

static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

static U8 screen_buf[640 * 480];
static U8 save_buf[640 * 480];
static U8 screen_init[640 * 480];
static U8 save_init[640 * 480];

static void setup(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    memset(screen_buf, 0, sizeof(screen_buf));
    memset(save_buf, 0, sizeof(save_buf));
}

static void test_restore_simple(void) {
    setup();
    /* Paint a pattern on screen, save it, clear screen, restore */
    for (int y = 10; y <= 30; y++)
        for (int x = 10; x <= 30; x++)
            screen_buf[y * 640 + x] = (U8)((x + y) & 0xFF);
    SaveBlock(screen_buf, save_buf, 10, 10, 30, 30);
    memset(screen_buf, 0, sizeof(screen_buf));
    RestoreBlock(screen_buf, save_buf, 10, 10, 30, 30);
    /* Verify a few pixels */
    ASSERT_EQ_UINT((10 + 10) & 0xFF, screen_buf[10 * 640 + 10]);
    ASSERT_EQ_UINT((20 + 20) & 0xFF, screen_buf[20 * 640 + 20]);
    ASSERT_EQ_UINT((30 + 30) & 0xFF, screen_buf[30 * 640 + 30]);
}

static void test_restore_single_pixel(void) {
    setup();
    screen_buf[50 * 640 + 50] = 0xAB;
    SaveBlock(screen_buf, save_buf, 50, 50, 50, 50);
    screen_buf[50 * 640 + 50] = 0;
    RestoreBlock(screen_buf, save_buf, 50, 50, 50, 50);
    ASSERT_EQ_UINT(0xAB, screen_buf[50 * 640 + 50]);
}

static void test_restore_full_width(void) {
    setup();
    for (int x = 0; x < 640; x++)
        screen_buf[0 * 640 + x] = (U8)(x & 0xFF);
    SaveBlock(screen_buf, save_buf, 0, 0, 639, 0);
    memset(screen_buf, 0, 640);
    RestoreBlock(screen_buf, save_buf, 0, 0, 639, 0);
    ASSERT_EQ_UINT(0, screen_buf[0]);
    ASSERT_EQ_UINT(100 & 0xFF, screen_buf[100]);
    ASSERT_EQ_UINT(639 & 0xFF, screen_buf[639]);
}

static void test_asm_equiv(void) {
    U8 cpp_screen[640 * 480];
    U8 asm_screen[640 * 480];
    U8 buf[640 * 480];

    setup();
    /* Paint a pattern */
    for (int y = 5; y <= 25; y++)
        for (int x = 5; x <= 25; x++)
            screen_buf[y * 640 + x] = (U8)(x * y);
    SaveBlock(screen_buf, buf, 5, 5, 25, 25);

    /* CPP RestoreBlock */
    memset(cpp_screen, 0, sizeof(cpp_screen));
    RestoreBlock(cpp_screen, buf, 5, 5, 25, 25);

    /* ASM RestoreBlock */
    memset(asm_screen, 0, sizeof(asm_screen));
    asm_RestoreBlock(asm_screen, buf, 5, 5, 25, 25);

    ASSERT_ASM_CPP_MEM_EQ(asm_screen, cpp_screen, sizeof(cpp_screen), "RestoreBlock");
}

static void test_asm_equiv_large(void) {
    U8 cpp_screen[640 * 480];
    U8 asm_screen[640 * 480];
    U8 buf[640 * 480];

    setup();
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++)
            screen_buf[y * 640 + x] = (U8)((x ^ y) & 0xFF);
    SaveBlock(screen_buf, buf, 0, 0, 639, 479);

    memset(cpp_screen, 0, sizeof(cpp_screen));
    RestoreBlock(cpp_screen, buf, 0, 0, 639, 479);

    memset(asm_screen, 0, sizeof(asm_screen));
    asm_RestoreBlock(asm_screen, buf, 0, 0, 639, 479);

    ASSERT_ASM_CPP_MEM_EQ(asm_screen, cpp_screen, sizeof(cpp_screen), "RestoreBlock full-screen");
}

static void test_random_batch(void) {
    U8 cpp_screen[640 * 480];
    U8 asm_screen[640 * 480];
    U8 buf[640 * 480];

    rng_seed(0xDEADBEEF);
    int prev = test_failures;
    for (int i = 0; i < 100 && test_failures == prev; i++) {
        S32 x0 = (S32)(rng_next() % 600);
        S32 y0 = (S32)(rng_next() % 440);
        S32 x1 = x0 + (S32)(rng_next() % (639 - x0));
        S32 y1 = y0 + (S32)(rng_next() % (479 - y0));
        if (x1 > 639)
            x1 = 639;
        if (y1 > 479)
            y1 = 479;

        setup();
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                screen_buf[y * 640 + x] = (U8)(rng_next() & 0xFF);
        SaveBlock(screen_buf, buf, x0, y0, x1, y1);

        memset(cpp_screen, 0, sizeof(cpp_screen));
        RestoreBlock(cpp_screen, buf, x0, y0, x1, y1);

        memset(asm_screen, 0, sizeof(asm_screen));
        asm_RestoreBlock(asm_screen, buf, x0, y0, x1, y1);

        char label[64];
        snprintf(label, sizeof(label), "RestoreBlock random #%d (%d,%d)-(%d,%d)", i, x0, y0, x1, y1);
        ASSERT_ASM_CPP_MEM_EQ(asm_screen, cpp_screen, sizeof(cpp_screen), label);
    }
}

/* ---------- inverted rectangles ----------
 * Inverted by one column (x1 == x0 - 1), RESBLOCK.ASM gets zero rep counts
 * and copies nothing. Inverted further, or by one row, its counts wrap to
 * about 4 G and the copy runs past both buffers; RestoreBlock returns early
 * for any inverted rectangle. Each side runs in a child process so a crash
 * or hang fails this test instead of the binary. */
struct RestoreCall {
    int use_asm;
    S32 x0, y0, x1, y1;
};

/* Child: 0 when neither the screen nor the buffer changed. */
static int restore_copies_nothing(void *ctx) {
    const RestoreCall *c = (const RestoreCall *)ctx;
    if (c->use_asm)
        asm_RestoreBlock(screen_buf, save_buf, c->x0, c->y0, c->x1, c->y1);
    else
        RestoreBlock(screen_buf, save_buf, c->x0, c->y0, c->x1, c->y1);
    return memcmp(screen_init, screen_buf, sizeof(screen_buf)) == 0 &&
                   memcmp(save_init, save_buf, sizeof(save_buf)) == 0
               ? 0
               : 1;
}

static void test_inverted_rect_copies_nothing(void) {
    setup();
    for (U32 i = 0; i < sizeof(screen_init); i++) {
        screen_init[i] = (U8)((i * 23u + (i >> 1)) & 0xFFu);
        save_init[i] = (U8)((i * 7u + 0x3Cu) & 0xFFu);
    }
    memcpy(screen_buf, screen_init, sizeof(screen_buf));
    memcpy(save_buf, save_init, sizeof(save_buf));

    /* Inverted by one column: the ASM copies nothing, the C++ must match. */
    static const S32 by_one[][4] = {
        {201, 300, 200, 315},
        {0, 0, -1, 479},
    };
    for (size_t i = 0; i < sizeof(by_one) / sizeof(by_one[0]); i++) {
        const S32 *c = by_one[i];
        char label[96];
        int asm_sig, cpp_sig;
        RestoreCall asm_call = {1, c[0], c[1], c[2], c[3]};
        RestoreCall cpp_call = {0, c[0], c[1], c[2], c[3]};
        int asm_rc = run_in_child(restore_copies_nothing, &asm_call, &asm_sig);
        int cpp_rc = run_in_child(restore_copies_nothing, &cpp_call, &cpp_sig);
        snprintf(label, sizeof(label), "RestoreBlock (%d,%d)-(%d,%d) (asm sig %d, cpp sig %d)",
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
        RestoreCall asm_call = {1, c[0], c[1], c[2], c[3]};
        RestoreCall cpp_call = {0, c[0], c[1], c[2], c[3]};
        int asm_rc = run_in_child(restore_copies_nothing, &asm_call, &asm_sig);
        int cpp_rc = run_in_child(restore_copies_nothing, &cpp_call, &cpp_sig);
        snprintf(label, sizeof(label), "RestoreBlock (%d,%d)-(%d,%d) ASM dies (sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], asm_sig);
        ASSERT_EQ_INT_MSG(-1, asm_rc, label);
        snprintf(label, sizeof(label), "RestoreBlock (%d,%d)-(%d,%d) copies nothing (cpp sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], cpp_sig);
        ASSERT_EQ_INT_MSG(0, cpp_rc, label);
    }
}

/* ---------- rectangles off the screen ----------
 * The ASM does not clip, so these are C++ only. The buffer holds the whole
 * rectangle; RestoreBlock draws only the part on screen. The screen sits
 * between guard rows, so a write that strays is seen. */
enum { kGuardRows = 16,
       kOffBufferSize = 720 * 520 };
static U8 guarded_screen[640 * (480 + 2 * kGuardRows)];
static U8 *const off_screen = guarded_screen + 640 * kGuardRows;
static U8 screen_expect[sizeof(guarded_screen)];
static U8 off_buffer[kOffBufferSize];
static U8 off_buffer_init[kOffBufferSize];

/* Child: 0 when exactly the on-screen pixels were drawn, each from its place
   in the whole rectangle's layout, and the buffer is unchanged. */
static int restore_clips(void *ctx) {
    const RestoreCall *c = (const RestoreCall *)ctx;
    for (U32 i = 0; i < sizeof(guarded_screen); i++)
        guarded_screen[i] = screen_expect[i] = (U8)((i * 29u + (i >> 3)) & 0xFFu);
    for (U32 i = 0; i < kOffBufferSize; i++)
        off_buffer[i] = off_buffer_init[i] = (U8)((i * 13u + 0x5Au) & 0xFFu);

    const S64 w = (S64)c->x1 - c->x0 + 1;
    U8 *expect = screen_expect + 640 * kGuardRows;
    for (S64 y = c->y0 < 0 ? 0 : c->y0; y <= c->y1 && y < 480; y++)
        for (S64 x = c->x0 < 0 ? 0 : c->x0; x <= c->x1 && x < 640; x++)
            expect[y * 640 + x] = off_buffer[(y - c->y0) * w + (x - c->x0)];

    RestoreBlock(off_screen, off_buffer, c->x0, c->y0, c->x1, c->y1);
    return memcmp(screen_expect, guarded_screen, sizeof(guarded_screen)) == 0 &&
                   memcmp(off_buffer_init, off_buffer, kOffBufferSize) == 0
               ? 0
               : 1;
}

static void test_offscreen_rect_clips(void) {
    setup();
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
        RestoreCall call = {0, c[0], c[1], c[2], c[3]};
        int rc = run_in_child(restore_clips, &call, &sig);
        snprintf(label, sizeof(label), "RestoreBlock (%d,%d)-(%d,%d) clips (sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], sig);
        ASSERT_EQ_INT_MSG(0, rc, label);
    }
}

/* A rectangle partly off screen, saved and restored with the same
   coordinates, puts back what was on screen. */
static void test_offscreen_round_trip(void) {
    setup();
    static U8 original[640 * 480];
    static U8 buf[700 * 120];
    static const S32 cases[][4] = {
        {-30, -20, 99, 59},
        {580, 420, 679, 519},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const S32 *c = cases[i];
        for (U32 p = 0; p < sizeof(original); p++)
            original[p] = screen_buf[p] = (U8)((p * 31u + i) & 0xFFu);
        SaveBlock(screen_buf, buf, c[0], c[1], c[2], c[3]);
        memset(screen_buf, 0, sizeof(screen_buf));
        RestoreBlock(screen_buf, buf, c[0], c[1], c[2], c[3]);

        int ok = 1;
        for (S32 y = 0; y < 480; y++)
            for (S32 x = 0; x < 640; x++) {
                int inside = x >= c[0] && x <= c[2] && y >= c[1] && y <= c[3];
                U8 want = inside ? original[y * 640 + x] : 0;
                if (screen_buf[y * 640 + x] != want)
                    ok = 0;
            }
        char label[96];
        snprintf(label, sizeof(label), "SaveBlock then RestoreBlock (%d,%d)-(%d,%d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3]);
        ASSERT_EQ_INT_MSG(1, ok, label);
    }
}

int main(void) {
    RUN_TEST(test_restore_simple);
    RUN_TEST(test_restore_single_pixel);
    RUN_TEST(test_restore_full_width);
    RUN_TEST(test_asm_equiv);
    RUN_TEST(test_asm_equiv_large);
    RUN_TEST(test_random_batch);
    RUN_TEST(test_inverted_rect_copies_nothing);
    RUN_TEST(test_offscreen_rect_clips);
    RUN_TEST(test_offscreen_round_trip);
    TEST_SUMMARY();
    return test_failures != 0;
}
