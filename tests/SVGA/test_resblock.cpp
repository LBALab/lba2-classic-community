/* Test: RestoreBlock — restore saved screen region */
#include "test_harness.h"
#include <SVGA/RESBLOCK.H>
#include <SVGA/SAVBLOCK.H>
#include <SVGA/SCREEN.H>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

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
 * about 4 G and the copy runs past both buffers. With U32 deltas the C++
 * would do the same, so RestoreBlock returns early for any inverted
 * rectangle. Each side runs in a child process so a crash or hang fails
 * this test instead of the binary. */
static int restoreblock_run_in_child(int use_asm, S32 x0, S32 y0, S32 x1, S32 y1, int *sig) {
    setup();
    for (U32 i = 0; i < sizeof(screen_init); i++) {
        screen_init[i] = (U8)((i * 23u + (i >> 1)) & 0xFFu);
        save_init[i] = (U8)((i * 7u + 0x3Cu) & 0xFFu);
    }
    memcpy(screen_buf, screen_init, sizeof(screen_buf));
    memcpy(save_buf, save_init, sizeof(save_buf));

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) {
        *sig = 0;
        return -2; /* no child ran: never reads as "copied nothing" */
    }
    if (pid == 0) {
        alarm(3);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
            dup2(devnull, 1);
        if (use_asm)
            asm_RestoreBlock(screen_buf, save_buf, x0, y0, x1, y1);
        else
            RestoreBlock(screen_buf, save_buf, x0, y0, x1, y1);
        int same = memcmp(screen_init, screen_buf, sizeof(screen_buf)) == 0 &&
                   memcmp(save_init, save_buf, sizeof(save_buf)) == 0;
        _exit(same ? 0 : 1);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) != pid) {
        *sig = 0;
        return -2;
    }
    *sig = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void test_inverted_rect_copies_nothing(void) {
    /* Inverted by one column: the ASM copies nothing, the C++ must match. */
    static const S32 by_one[][4] = {
        {201, 300, 200, 315},
        {0, 0, -1, 479},
    };
    for (size_t i = 0; i < sizeof(by_one) / sizeof(by_one[0]); i++) {
        const S32 *c = by_one[i];
        char label[96];
        int asm_sig, cpp_sig;
        int asm_rc = restoreblock_run_in_child(1, c[0], c[1], c[2], c[3], &asm_sig);
        int cpp_rc = restoreblock_run_in_child(0, c[0], c[1], c[2], c[3], &cpp_sig);
        snprintf(label, sizeof(label), "RestoreBlock (%d,%d)-(%d,%d) (asm sig %d, cpp sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], asm_sig, cpp_sig);
        ASSERT_ASM_CPP_EQ_INT(asm_rc, cpp_rc, label);
        ASSERT_ASM_CPP_EQ_INT(0, asm_rc, label);
    }

    /* Inverted further, or by one row: the ASM runs a wrapped count and
       crashes, so there is nothing to match. The C++ copies nothing. */
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
        int cpp_sig;
        int cpp_rc = restoreblock_run_in_child(0, c[0], c[1], c[2], c[3], &cpp_sig);
        snprintf(label, sizeof(label), "RestoreBlock (%d,%d)-(%d,%d) copies nothing (cpp sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], cpp_sig);
        ASSERT_ASM_CPP_EQ_INT(0, cpp_rc, label);
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
    TEST_SUMMARY();
    return test_failures != 0;
}
