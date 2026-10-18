/* Test: ScaleBox — scale rectangular region */
#include "test_harness.h"
#include <SVGA/SCALEBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" void asm_ScaleBox(S32 xs0, S32 ys0, S32 xs1, S32 ys1, void *ptrs, S32 xd0, S32 yd0, S32 xd1, S32 yd1, void *ptrd);

static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

/* ScaleBox reads TabOffLine[] for both source and destination Y indexing.
   We use a 640-wide virtual screen so TabOffLine[y] = y * 640. */
static U8 srcbuf[640 * 480];
static U8 dstbuf[640 * 480];

static int count_nonzero_pixels(const U8 *buffer,
                                S32 x0, S32 y0,
                                S32 width, S32 height) {
    int count = 0;
    for (S32 y = y0; y < y0 + height; ++y) {
        for (S32 x = x0; x < x0 + width; ++x) {
            if (buffer[y * 640 + x] != 0)
                count++;
        }
    }
    return count;
}

static void setup(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    memset(srcbuf, 0, sizeof(srcbuf));
    memset(dstbuf, 0, sizeof(dstbuf));
}

static void assert_scalebox_equiv(const char *label,
                                  S32 xs0, S32 ys0, S32 xs1, S32 ys1,
                                  S32 xd0, S32 yd0, S32 xd1, S32 yd1) {
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];

    memset(cpp_dst, 0, sizeof(cpp_dst));
    ScaleBox(xs0, ys0, xs1, ys1, srcbuf, xd0, yd0, xd1, yd1, cpp_dst);

    memset(asm_dst, 0, sizeof(asm_dst));
    asm_ScaleBox(xs0, ys0, xs1, ys1, srcbuf, xd0, yd0, xd1, yd1, asm_dst);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), label);
}

static void test_same_size_copy(void) {
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];

    setup();
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++)
            srcbuf[y * 640 + x] = (U8)((x + y * 10) & 0xFF);

    memset(cpp_dst, 0, sizeof(cpp_dst));
    ScaleBox(0, 0, 9, 9, srcbuf, 100, 100, 109, 109, cpp_dst);
    ASSERT_EQ_INT(98, count_nonzero_pixels(cpp_dst, 100, 100, 10, 10));

    memset(asm_dst, 0, sizeof(asm_dst));
    asm_ScaleBox(0, 0, 9, 9, srcbuf, 100, 100, 109, 109, asm_dst);
    ASSERT_EQ_INT(98, count_nonzero_pixels(asm_dst, 100, 100, 10, 10));

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "ScaleBox same-size exact 10x10");
}

static void test_asm_equiv_downscale(void) {
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];

    setup();
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            srcbuf[y * 640 + x] = (U8)(y * 4 + x);

    memset(cpp_dst, 0, sizeof(cpp_dst));
    ScaleBox(0, 0, 3, 3, srcbuf, 0, 300, 1, 301, cpp_dst);

    memset(asm_dst, 0, sizeof(asm_dst));
    asm_ScaleBox(0, 0, 3, 3, srcbuf, 0, 300, 1, 301, asm_dst);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "ScaleBox downscale 4x4→2x2");
}

static void test_asm_equiv_same_size(void) {
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];

    setup();
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++)
            srcbuf[y * 640 + x] = (U8)((x * 7 + y * 3) & 0xFF);

    memset(cpp_dst, 0, sizeof(cpp_dst));
    ScaleBox(0, 0, 19, 19, srcbuf, 50, 50, 69, 69, cpp_dst);

    memset(asm_dst, 0, sizeof(asm_dst));
    asm_ScaleBox(0, 0, 19, 19, srcbuf, 50, 50, 69, 69, asm_dst);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "ScaleBox same-size");
}

static void test_asm_equiv_upscale(void) {
    setup();
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++)
            srcbuf[y * 640 + x] = (U8)((x + y) & 0xFF);

    assert_scalebox_equiv("ScaleBox upscale 10x10→30x30", 0, 0, 9, 9, 0, 100, 29, 129);
}

static void test_asm_equiv_single_pixel_upscale(void) {
    setup();
    srcbuf[7 * 640 + 5] = 0xA5;
    assert_scalebox_equiv("ScaleBox single pixel 1x1→4x4", 5, 7, 5, 7, 200, 210, 203, 213);
}

static void test_asm_equiv_vertical_strip(void) {
    setup();
    for (int y = 0; y < 8; y++)
        srcbuf[(20 + y) * 640 + 3] = (U8)(0x20 + y * 7);
    assert_scalebox_equiv("ScaleBox vertical strip 1x8→6x12", 3, 20, 3, 27, 300, 120, 305, 131);
}

static void test_asm_equiv_horizontal_strip(void) {
    setup();
    for (int x = 0; x < 9; x++)
        srcbuf[15 * 640 + 40 + x] = (U8)(0x80 + x * 3);
    assert_scalebox_equiv("ScaleBox horizontal strip 9x1→4x5", 40, 15, 48, 15, 150, 260, 153, 264);
}

static void test_asm_equiv_bottom_right_edge(void) {
    setup();
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            srcbuf[(30 + y) * 640 + 60 + x] = (U8)(0x40 + y * 4 + x);
    assert_scalebox_equiv("ScaleBox bottom-right edge 4x4→4x4", 60, 30, 63, 33, 636, 476, 639, 479);
}

static void test_random_batch(void) {
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];

    rng_seed(0xCAFEBABE);
    int prev = test_failures;
    for (int i = 0; i < 100 && test_failures == prev; i++) {
        setup();
        S32 sw = (S32)(rng_next() % 60) + 2;
        S32 sh = (S32)(rng_next() % 60) + 2;
        S32 dw = (S32)(rng_next() % 80) + 2;
        S32 dh = (S32)(rng_next() % 80) + 2;
        S32 dx = (S32)(rng_next() % 400);
        S32 dy = (S32)(rng_next() % 300) + 100;

        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
                srcbuf[y * 640 + x] = (U8)(rng_next() & 0xFF);

        memset(cpp_dst, 0, sizeof(cpp_dst));
        ScaleBox(0, 0, sw - 1, sh - 1, srcbuf, dx, dy, dx + dw - 1, dy + dh - 1, cpp_dst);

        memset(asm_dst, 0, sizeof(asm_dst));
        asm_ScaleBox(0, 0, sw - 1, sh - 1, srcbuf, dx, dy, dx + dw - 1, dy + dh - 1, asm_dst);

        char label[80];
        snprintf(label, sizeof(label), "ScaleBox random #%d %dx%d->%dx%d @(%d,%d)", i, sw, sh, dw, dh, dx, dy);
        ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), label);
    }
}
/* ---------- on-screen edge sweep ----------
 * ScaleBox does not clip, in SCALEBOX.ASM or SCALEBOX.CPP: it writes every
 * pixel of (xd0,yd0)-(xd1,yd1) and reads the source through TabOffLine. Its
 * callers keep the destination on screen: SAVEGAME.CPP scales the whole
 * screen into (0,0)-(159,119), GAMEMENU.CPP ZoomSavedGame stops before x0 <= 0
 * or x1 >= ModeDesiredX - 1, HOLOPLAN.CPP zooms inside the centred image rect.
 * So the sweep stays on screen, right up to the last row and column, with the
 * destination between guard rows and the source ending against an
 * inaccessible page, so a write past the destination or a read past the
 * source shows (the read as a crash). */

#define SB_GUARD_ROWS 2
#define SB_GUARD_BYTE 0xEE
#define SB_FRAME (640 * 480)
#define SB_GUARDED_SIZE (640 * (480 + 2 * SB_GUARD_ROWS))

static U8 sb_dst_cpp[SB_GUARDED_SIZE];
static U8 sb_dst_asm[SB_GUARDED_SIZE];
static U8 *sb_src; /* 640x480, with an inaccessible page before and after */

static int sb_setup_source(void) {
    long page = sysconf(_SC_PAGESIZE);
    size_t body = ((size_t)SB_FRAME + (size_t)page - 1) / (size_t)page * (size_t)page;
    U8 *base = (U8 *)mmap(NULL, body + 2 * (size_t)page, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED)
        return 0;
    mprotect(base, (size_t)page, PROT_NONE);
    mprotect(base + (size_t)page + body, (size_t)page, PROT_NONE);
    /* end the frame exactly at the trailing guard page */
    sb_src = base + (size_t)page + body - SB_FRAME;
    return 1;
}

/* The guard page is the oracle for reads; prove it traps before trusting it. */
static int sb_read_traps(const U8 *p) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        volatile U8 v = *(volatile const U8 *)p;
        (void)v;
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status);
}

static void sb_fill_source(U32 seed) {
    rng_seed(seed);
    for (int i = 0; i < SB_FRAME; i++)
        sb_src[i] = (U8)((rng_next() & 0xFF) | 1); /* never 0: unwritten dst stays 0 */
}

static int sb_case(const char *what, S32 xs0, S32 ys0, S32 xs1, S32 ys1,
                   S32 xd0, S32 yd0, S32 xd1, S32 yd1) {
    char label[160];
    int prev = test_failures;
    snprintf(label, sizeof(label), "ScaleBox %s (%d,%d)-(%d,%d) -> (%d,%d)-(%d,%d)", what,
             (int)xs0, (int)ys0, (int)xs1, (int)ys1, (int)xd0, (int)yd0, (int)xd1, (int)yd1);
    U8 *bufs[2] = {sb_dst_cpp, sb_dst_asm};
    for (int b = 0; b < 2; b++) {
        memset(bufs[b], SB_GUARD_BYTE, SB_GUARDED_SIZE);
        memset(bufs[b] + 640 * SB_GUARD_ROWS, 0, SB_FRAME);
    }
    ScaleBox(xs0, ys0, xs1, ys1, sb_src, xd0, yd0, xd1, yd1, sb_dst_cpp + 640 * SB_GUARD_ROWS);
    asm_ScaleBox(xs0, ys0, xs1, ys1, sb_src, xd0, yd0, xd1, yd1, sb_dst_asm + 640 * SB_GUARD_ROWS);
    ASSERT_ASM_CPP_MEM_EQ(sb_dst_asm, sb_dst_cpp, SB_GUARDED_SIZE, label);

    /* exactly the destination rect is written, nothing around it */
    int outside = 0, inside_missing = 0;
    for (int i = 0; i < SB_GUARDED_SIZE; i++) {
        int row = i / 640 - SB_GUARD_ROWS, col = i % 640;
        int in = row >= yd0 && row <= yd1 && col >= xd0 && col <= xd1;
        U8 untouched = (row < 0 || row >= 480) ? SB_GUARD_BYTE : 0;
        if (in)
            inside_missing += sb_dst_asm[i] == 0;
        else
            outside += sb_dst_asm[i] != untouched;
    }
    ASSERT_ASM_CPP_EQ_INT(0, outside, label);
    ASSERT_ASM_CPP_EQ_INT(0, inside_missing, label);
    return test_failures == prev;
}

static void test_onscreen_edge_sweep(void) {
    if (!sb_setup_source()) {
        FAIL_MSG("%s", "mmap failed");
        return;
    }
    ASSERT_TRUE(sb_read_traps(sb_src + SB_FRAME));
    ASSERT_TRUE(sb_read_traps(sb_src - 1));
    ASSERT_TRUE(!sb_read_traps(sb_src + SB_FRAME - 1));
    sb_fill_source(0x5CA1EB0Du);

    /* source rects: a brick-sized 48x38 and a large 100x80, placed at the
     * source's bottom-right corner so their last row and column are the
     * frame's; destinations: same size, about 2x up, about 2x down */
    static const S32 src_sizes[2][2] = {{48, 38}, {100, 80}};
    static const S32 dst_mul[3][2] = {{1, 1}, {2, 1}, {1, 2}}; /* num, den */
    int cases = 0;
    for (int s = 0; s < 2; s++) {
        S32 sw = src_sizes[s][0], sh = src_sizes[s][1];
        S32 xs1 = 639, ys1 = 479, xs0 = xs1 - sw + 1, ys0 = ys1 - sh + 1;
        for (int m = 0; m < 3; m++) {
            S32 dw = sw * dst_mul[m][0] / dst_mul[m][1] + m; /* +m: uneven ratios */
            S32 dh = sh * dst_mul[m][0] / dst_mul[m][1] + m;
            S32 mx = 320 - dw / 2, my = 240 - dh / 2;
            for (S32 o = 0; o <= 16; o++) {
                /* left, right, top, bottom edge at the other axis' middle */
                if (!sb_case("left", xs0, ys0, xs1, ys1, o, my, o + dw - 1, my + dh - 1))
                    return;
                if (!sb_case("right", xs0, ys0, xs1, ys1, 640 - dw - o, my, 639 - o, my + dh - 1))
                    return;
                if (!sb_case("top", xs0, ys0, xs1, ys1, mx, o, mx + dw - 1, o + dh - 1))
                    return;
                if (!sb_case("bottom", xs0, ys0, xs1, ys1, mx, 480 - dh - o, mx + dw - 1, 479 - o))
                    return;
                /* corner diagonals */
                if (!sb_case("tl", xs0, ys0, xs1, ys1, o, o, o + dw - 1, o + dh - 1))
                    return;
                if (!sb_case("br", xs0, ys0, xs1, ys1, 640 - dw - o, 480 - dh - o, 639 - o, 479 - o))
                    return;
                cases += 6;
            }
            /* source rect at the top-left corner of the frame too */
            if (!sb_case("src-tl", 0, 0, sw - 1, sh - 1, 640 - dw, 480 - dh, 639, 479))
                return;
            cases++;
        }
    }

    /* the callers' own shapes */
    if (!sb_case("savegame thumbnail", 0, 0, 639, 479, 0, 0, 159, 119))
        return;
    if (!sb_case("full identity", 0, 0, 639, 479, 0, 0, 639, 479))
        return;
    if (!sb_case("full 1px inset", 0, 0, 639, 479, 1, 1, 638, 478))
        return;
    if (!sb_case("full to 1x1", 0, 0, 639, 479, 639, 479, 639, 479))
        return;
    if (!sb_case("1x1 to full", 639, 479, 639, 479, 0, 0, 639, 479))
        return;
    /* ZoomSavedGame: SCREEN_SAVE (160x120) at (x,y) growing to the screen */
    for (S32 t = 0; t <= 20; t++) {
        S32 x = 240, y = 180;
        S32 x0 = x + (0 - x) * t / 21, y0 = y + (0 - y) * t / 21;
        S32 x1 = x + 159 + (639 - (x + 159)) * t / 21, y1 = y + 119 + (479 - (y + 119)) * t / 21;
        if (x0 <= 0 || y0 <= 0 || x1 >= 639 || y1 >= 479)
            break;
        if (!sb_case("zoom saved game", 0, 0, 639, 479, x0, y0, x1, y1))
            return;
        cases++;
    }
    printf("  on-screen edge sweep: %d cases\n", cases + 5);
}

int main(void) {
    RUN_TEST(test_same_size_copy);
    RUN_TEST(test_asm_equiv_downscale);
    RUN_TEST(test_asm_equiv_same_size);
    RUN_TEST(test_asm_equiv_upscale);
    RUN_TEST(test_asm_equiv_single_pixel_upscale);
    RUN_TEST(test_asm_equiv_vertical_strip);
    RUN_TEST(test_asm_equiv_horizontal_strip);
    RUN_TEST(test_asm_equiv_bottom_right_edge);
    RUN_TEST(test_random_batch);
    RUN_TEST(test_onscreen_edge_sweep);
    TEST_SUMMARY();
    return test_failures != 0;
}
