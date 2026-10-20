/* Test: SaveBlock — save screen region to buffer */
#include "test_harness.h"

#include <SVGA/SAVBLOCK.H>
#include <SVGA/SCREEN.H>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
 * about 4 G and the copy runs past both buffers. With U32 deltas the C++
 * would do the same, so SaveBlock returns early for any inverted
 * rectangle. Each side runs in a child process so a crash or hang fails
 * this test instead of the binary. */
static int saveblock_run_in_child(int use_asm, S32 x0, S32 y0, S32 x1, S32 y1, int *sig) {
    setup_screen();
    fill_screen_pattern(7u);
    fill_buffer_pattern(buffer_init, 8u);
    memcpy(cpp_screen, screen_init, sizeof(cpp_screen));
    memcpy(cpp_buffer, buffer_init, sizeof(cpp_buffer));

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
            asm_SaveBlock(cpp_screen, cpp_buffer, x0, y0, x1, y1);
        else
            SaveBlock(cpp_screen, cpp_buffer, x0, y0, x1, y1);
        int same = memcmp(screen_init, cpp_screen, sizeof(cpp_screen)) == 0 &&
                   memcmp(buffer_init, cpp_buffer, sizeof(cpp_buffer)) == 0;
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
        int asm_rc = saveblock_run_in_child(1, c[0], c[1], c[2], c[3], &asm_sig);
        int cpp_rc = saveblock_run_in_child(0, c[0], c[1], c[2], c[3], &cpp_sig);
        snprintf(label, sizeof(label), "SaveBlock (%d,%d)-(%d,%d) (asm sig %d, cpp sig %d)",
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
        int cpp_rc = saveblock_run_in_child(0, c[0], c[1], c[2], c[3], &cpp_sig);
        snprintf(label, sizeof(label), "SaveBlock (%d,%d)-(%d,%d) copies nothing (cpp sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], cpp_sig);
        ASSERT_ASM_CPP_EQ_INT(0, cpp_rc, label);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_inverted_rect_copies_nothing);
    TEST_SUMMARY();
    return test_failures != 0;
}
