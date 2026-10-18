/* Test: Box — draw rectangle outline with clipping */
#include "test_harness.h"

#include <SVGA/BOX.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

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

/* ---------- exact clip-boundary sweep ----------
 * Box clips in the assembly (BOX.ASM TestLeft..TestDown clamp each edge to
 * ClipXMin..ClipYMax after rejecting boxes wholly outside), so the sweep puts
 * each rect edge exactly on, one before and one past every clip edge, and
 * walks a brick-sized (48x38) and a large (100x80) rect across each edge of
 * the full screen, an inner window and the thin windows in 1-px steps.
 * The framebuffer sits between guard rows so a write above row 0 or below
 * row 479 shows up as a guard byte change. */

#define BOX_GUARD_ROWS 4
#define BOX_GUARD_BYTE 0xEE
#define BOX_GUARDED_SIZE (640 * (480 + 2 * BOX_GUARD_ROWS))

static U8 guarded_buf[BOX_GUARDED_SIZE];
static U8 guarded_cpp[BOX_GUARDED_SIZE];

struct BoxClip {
    const char *name;
    S32 x0, y0, x1, y1;
};

static const BoxClip box_clips[] = {
    {"full", 0, 0, 639, 479},
    {"inner", 100, 80, 539, 399},
    {"thin-row", 0, 240, 639, 240},
    {"thin-col", 320, 0, 320, 479},
    {"one-pixel", 320, 240, 320, 240},
};

static int box_partial_cases;
static int box_full_cases;
static int box_empty_cases;

static void setup_guarded(const BoxClip *clip) {
    memset(guarded_buf, BOX_GUARD_BYTE, sizeof(guarded_buf));
    memset(guarded_buf + 640 * BOX_GUARD_ROWS, 0, 640 * 480);
    Log = guarded_buf + 640 * BOX_GUARD_ROWS;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    ClipXMin = clip->x0;
    ClipYMin = clip->y0;
    ClipXMax = clip->x1;
    ClipYMax = clip->y1;
}

static int guards_intact(const U8 *buf) {
    for (int i = 0; i < 640 * BOX_GUARD_ROWS; i++)
        if (buf[i] != BOX_GUARD_BYTE || buf[BOX_GUARDED_SIZE - 1 - i] != BOX_GUARD_BYTE)
            return 0;
    return 1;
}

/* Expected pixel count of the rect intersected with the clip window. */
static S32 clipped_area(const BoxClip *c, S32 x0, S32 y0, S32 x1, S32 y1) {
    S32 cx0 = x0 > c->x0 ? x0 : c->x0;
    S32 cy0 = y0 > c->y0 ? y0 : c->y0;
    S32 cx1 = x1 < c->x1 ? x1 : c->x1;
    S32 cy1 = y1 < c->y1 ? y1 : c->y1;
    if (cx0 > cx1 || cy0 > cy1)
        return 0;
    return (cx1 - cx0 + 1) * (cy1 - cy0 + 1);
}

/* Returns 1 on match. Compares the whole guarded buffer ASM vs C++, checks the
 * guard rows, and checks the drawn pixel count equals the clipped area. */
static int box_guarded_case(const BoxClip *clip, S32 x0, S32 y0, S32 x1, S32 y1, U8 col) {
    char label[128];
    snprintf(label, sizeof(label), "Box %s clip (%d,%d)-(%d,%d)", clip->name,
             (int)x0, (int)y0, (int)x1, (int)y1);
    int prev = test_failures;

    setup_guarded(clip);
    Box(x0, y0, x1, y1, col);
    memcpy(guarded_cpp, guarded_buf, sizeof(guarded_cpp));
    int cpp_guards = guards_intact(guarded_cpp);

    setup_guarded(clip);
    asm_Box(x0, y0, x1, y1, col);
    int asm_guards = guards_intact(guarded_buf);

    ASSERT_ASM_CPP_MEM_EQ(guarded_buf, guarded_cpp, sizeof(guarded_cpp), label);
    ASSERT_ASM_CPP_EQ_INT(asm_guards, cpp_guards, label);
    ASSERT_TRUE(asm_guards);

    S32 drawn = 0;
    const U8 *fb = guarded_buf + 640 * BOX_GUARD_ROWS;
    for (int i = 0; i < 640 * 480; i++)
        drawn += fb[i] == col;
    S32 expect = clipped_area(clip, x0, y0, x1, y1);
    ASSERT_ASM_CPP_EQ_INT(drawn, expect, label);

    S32 full = (x1 - x0 + 1) * (y1 - y0 + 1);
    if (expect == 0)
        box_empty_cases++;
    else if (expect == full)
        box_full_cases++;
    else
        box_partial_cases++;
    return test_failures == prev;
}

/* Rect edge exactly on, one before and one past each clip edge, on both sides
 * of the edge (the rect's near edge and its far edge). */
static void test_exact_clip_boundaries(void) {
    const U8 col = 0x5A;
    for (size_t c = 0; c < sizeof(box_clips) / sizeof(box_clips[0]); c++) {
        const BoxClip *k = &box_clips[c];
        S32 midx = (k->x0 + k->x1) / 2, midy = (k->y0 + k->y1) / 2;
        for (S32 d = -1; d <= 1; d++) {
            /* far edge of the rect on the clip edge (rect mostly outside) */
            if (!box_guarded_case(k, k->x0 - 47, midy - 19, k->x0 + d, midy + 18, col))
                return;
            if (!box_guarded_case(k, k->x1 + d, midy - 19, k->x1 + 47, midy + 18, col))
                return;
            if (!box_guarded_case(k, midx - 24, k->y0 - 37, midx + 23, k->y0 + d, col))
                return;
            if (!box_guarded_case(k, midx - 24, k->y1 + d, midx + 23, k->y1 + 37, col))
                return;
            /* near edge of the rect on the clip edge (rect mostly inside) */
            if (!box_guarded_case(k, k->x0 + d, midy - 19, k->x0 + d + 47, midy + 18, col))
                return;
            if (!box_guarded_case(k, k->x1 + d - 47, midy - 19, k->x1 + d, midy + 18, col))
                return;
            if (!box_guarded_case(k, midx - 24, k->y0 + d, midx + 23, k->y0 + d + 37, col))
                return;
            if (!box_guarded_case(k, midx - 24, k->y1 + d - 37, midx + 23, k->y1 + d, col))
                return;
            /* rect exactly the clip window, grown or shrunk by one */
            if (!box_guarded_case(k, k->x0 - d, k->y0 - d, k->x1 + d, k->y1 + d, col))
                return;
        }
        /* 1-px rects straddling each corner */
        for (S32 dy = -1; dy <= 1; dy++)
            for (S32 dx = -1; dx <= 1; dx++) {
                if (!box_guarded_case(k, k->x0 + dx, k->y0 + dy, k->x0 + dx, k->y0 + dy, col))
                    return;
                if (!box_guarded_case(k, k->x1 + dx, k->y1 + dy, k->x1 + dx, k->y1 + dy, col))
                    return;
            }
    }
}

/* Walk 48x38 and 100x80 rects across every clip edge in 1-px steps, from just
 * outside to just inside, at the mid position of the other axis; then walk the
 * diagonal through each corner. */
static void test_edge_sweep(void) {
    static const S32 sizes[2][2] = {{48, 38}, {100, 80}};
    box_partial_cases = box_full_cases = box_empty_cases = 0;
    for (size_t c = 0; c < sizeof(box_clips) / sizeof(box_clips[0]); c++) {
        const BoxClip *k = &box_clips[c];
        S32 midx = (k->x0 + k->x1) / 2, midy = (k->y0 + k->y1) / 2;
        for (int s = 0; s < 2; s++) {
            S32 w = sizes[s][0], h = sizes[s][1];
            U8 col = (U8)(0x30 + c * 2 + s);
            for (S32 o = -w - 1; o <= 1; o++) {
                /* left edge: rect x0 from k->x0-w-1 (outside) to k->x0+1 */
                if (!box_guarded_case(k, k->x0 + o, midy - h / 2, k->x0 + o + w - 1, midy - h / 2 + h - 1, col))
                    return;
                /* right edge */
                if (!box_guarded_case(k, k->x1 - o - w + 1, midy - h / 2, k->x1 - o, midy - h / 2 + h - 1, col))
                    return;
            }
            for (S32 o = -h - 1; o <= 1; o++) {
                if (!box_guarded_case(k, midx - w / 2, k->y0 + o, midx - w / 2 + w - 1, k->y0 + o + h - 1, col))
                    return;
                if (!box_guarded_case(k, midx - w / 2, k->y1 - o - h + 1, midx - w / 2 + w - 1, k->y1 - o, col))
                    return;
            }
            S32 n = (w > h ? w : h) + 1;
            for (S32 o = -n; o <= 1; o++) {
                /* top-left, top-right, bottom-left, bottom-right */
                if (!box_guarded_case(k, k->x0 + o, k->y0 + o, k->x0 + o + w - 1, k->y0 + o + h - 1, col))
                    return;
                if (!box_guarded_case(k, k->x1 - o - w + 1, k->y0 + o, k->x1 - o, k->y0 + o + h - 1, col))
                    return;
                if (!box_guarded_case(k, k->x0 + o, k->y1 - o - h + 1, k->x0 + o + w - 1, k->y1 - o, col))
                    return;
                if (!box_guarded_case(k, k->x1 - o - w + 1, k->y1 - o - h + 1, k->x1 - o, k->y1 - o, col))
                    return;
            }
        }
    }
    printf("  edge sweep: %d partial, %d whole, %d rejected\n",
           box_partial_cases, box_full_cases, box_empty_cases);
    ASSERT_TRUE(box_partial_cases > 1000);
}

/* ---------- inverted boxes ----------
 * Callers build bars as Box(start + 1, ..., fill_end, ...) where fill_end can
 * equal start: INVENT.CPP AffValueBar draws the conch's value bar that way, so
 * a conch at 0 or 1 (the fill rounds down) passes x0 == x1 + 1. In BOX.ASM
 * that gives ECX = x1 - x0 + 1 = 0, both rep counts are 0 and nothing is
 * written. With U32 deltas the C++ loops would never end, so Box returns
 * early for an inverted box. Each side runs in a child process so a crash or
 * hang fails this test instead of the binary. */
static int box_run_in_child(int use_asm, S32 x0, S32 y0, S32 x1, S32 y1, int *sig) {
    static const BoxClip full = {"full", 0, 0, 639, 479};
    U8 expect[BOX_GUARDED_SIZE];
    setup_guarded(&full);
    memcpy(expect, guarded_buf, sizeof(expect));

    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        alarm(3);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
            dup2(devnull, 1);
        if (use_asm)
            asm_Box(x0, y0, x1, y1, 0x77);
        else
            Box(x0, y0, x1, y1, 0x77);
        _exit(memcmp(expect, guarded_buf, sizeof(expect)) == 0 ? 0 : 1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    *sig = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void test_inverted_box_draws_nothing(void) {
    /* Inverted by one column: the ASM draws nothing, the C++ must match. */
    static const S32 by_one[][4] = {
        {201, 300, 200, 315}, /* AffValueBar shape, empty bar */
        {0, 0, -1, 10},       /* rejected by the x1 < ClipXMin test anyway */
    };
    for (size_t i = 0; i < sizeof(by_one) / sizeof(by_one[0]); i++) {
        const S32 *c = by_one[i];
        char label[96];
        int asm_sig, cpp_sig;
        int asm_rc = box_run_in_child(1, c[0], c[1], c[2], c[3], &asm_sig);
        int cpp_rc = box_run_in_child(0, c[0], c[1], c[2], c[3], &cpp_sig);
        snprintf(label, sizeof(label), "Box (%d,%d)-(%d,%d) exit code (asm sig %d, cpp sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], asm_sig, cpp_sig);
        ASSERT_ASM_CPP_EQ_INT(asm_rc, cpp_rc, label);
        ASSERT_ASM_CPP_EQ_INT(0, asm_rc, label);
    }

    /* Inverted further, or by one row: the ASM runs a negative rep count and
       crashes, so there is nothing to match. The C++ draws nothing. */
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
        int cpp_rc = box_run_in_child(0, c[0], c[1], c[2], c[3], &cpp_sig);
        snprintf(label, sizeof(label), "Box (%d,%d)-(%d,%d) draws nothing (cpp sig %d)",
                 (int)c[0], (int)c[1], (int)c[2], (int)c[3], cpp_sig);
        ASSERT_ASM_CPP_EQ_INT(0, cpp_rc, label);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_exact_clip_boundaries);
    RUN_TEST(test_edge_sweep);
    RUN_TEST(test_inverted_box_draws_nothing);
    TEST_SUMMARY();
    return test_failures != 0;
}
