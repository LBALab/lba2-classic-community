/* Test: CopyBlock — fast memory block copy between screen regions */
#include "test_harness.h"

#include <SVGA/CPYBLOCK.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>
#include <string.h>

extern "C" void asm_CopyBlock(S32 x0, S32 y0, S32 x1, S32 y1, void *src,
                              S32 xd, S32 yd, void *dst);

static const U32 kScreenWidth = 640;
static const U32 kScreenHeight = 480;
static const U32 kBufferSize = kScreenWidth * kScreenHeight;

static U8 src_init[kBufferSize];
static U8 cpp_src[kBufferSize];
static U8 asm_src[kBufferSize];
static U8 dst_init[kBufferSize];
static U8 cpp_dst[kBufferSize];
static U8 asm_dst[kBufferSize];
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
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = (S32)kScreenWidth - 1;
    ClipYMax = (S32)kScreenHeight - 1;
}

static void fill_source_pattern(U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        src_init[i] = (U8)(((i * 29u) + (salt * 17u) + (i >> 2)) & 0xFFu);
    }
}

static void fill_dest_pattern(U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        dst_init[i] = (U8)(((i * 13u) + (salt * 31u) + 0x66u) & 0xFFu);
    }
}

static void assert_copyblock_case(const char *label,
                                  S32 x0, S32 y0, S32 x1, S32 y1,
                                  S32 xd, S32 yd,
                                  S32 clip_x_min, S32 clip_y_min,
                                  S32 clip_x_max, S32 clip_y_max,
                                  U32 salt) {
    setup_screen();
    fill_source_pattern(salt);
    fill_dest_pattern(salt + 1u);

    memcpy(cpp_src, src_init, sizeof(cpp_src));
    memcpy(asm_src, src_init, sizeof(asm_src));
    memcpy(cpp_dst, dst_init, sizeof(cpp_dst));
    memcpy(asm_dst, dst_init, sizeof(asm_dst));

    ClipXMin = clip_x_min;
    ClipYMin = clip_y_min;
    ClipXMax = clip_x_max;
    ClipYMax = clip_y_max;
    CopyBlock(x0, y0, x1, y1, cpp_src, xd, yd, cpp_dst);

    setup_screen();
    ClipXMin = clip_x_min;
    ClipYMin = clip_y_min;
    ClipXMax = clip_x_max;
    ClipYMax = clip_y_max;
    asm_CopyBlock(x0, y0, x1, y1, asm_src, xd, yd, asm_dst);

    ASSERT_ASM_CPP_MEM_EQ(src_init, cpp_src, sizeof(cpp_src), label);
    ASSERT_ASM_CPP_MEM_EQ(src_init, asm_src, sizeof(asm_src), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), label);
}

static void test_equivalence(void) {
    assert_copyblock_case("CopyBlock fixed simple",
                          10, 10, 20, 20, 50, 50,
                          0, 0, 639, 479, 1u);
    assert_copyblock_case("CopyBlock fixed clipped source",
                          -8, -3, 12, 6, 40, 20,
                          0, 0, 639, 479, 2u);
    assert_copyblock_case("CopyBlock fixed clipped destination",
                          20, 20, 80, 45, 610, 460,
                          0, 0, 639, 479, 3u);
    assert_copyblock_case("CopyBlock fixed custom clip",
                          50, 30, 120, 70, 180, 100,
                          64, 32, 255, 191, 4u);
    assert_copyblock_case("CopyBlock fixed outside clip",
                          100, 100, 140, 120, 20, 20,
                          200, 200, 300, 260, 5u);
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 120; ++i) {
        S32 x0 = (S32)(rng_next() % 760u) - 60;
        S32 y0 = (S32)(rng_next() % 560u) - 40;
        S32 x1 = (S32)(rng_next() % 760u) - 60;
        S32 y1 = (S32)(rng_next() % 560u) - 40;
        S32 xd = (S32)(rng_next() % 760u) - 60;
        S32 yd = (S32)(rng_next() % 560u) - 40;
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

        snprintf(label, sizeof(label), "CopyBlock rand %d", i);
        assert_copyblock_case(label, x0, y0, x1, y1, xd, yd,
                              clip_x_min, clip_y_min, clip_x_max, clip_y_max,
                              (U32)i + 100u);
    }
}

/* ── Edge sweep ───────────────────────────────────────────────────────────
 * The assembly clips the source rectangle against the clip window (shifting
 * the destination by the amount cut), rejects the whole call when the
 * destination's top-left corner is outside the clip window, and trims the
 * destination's right and bottom against ClipXMax/ClipYMax. The sweep walks
 * real-size blocks across each clip-window edge in 1-px steps, on both the
 * source and the destination side, and diagonally across each corner. */

struct SweepClip {
    const char *name;
    S32 x_min, y_min, x_max, y_max;
};

static const SweepClip kSweepClips[] = {
    {"full", 0, 0, 639, 479},
    {"inner", 100, 80, 539, 399},
    {"row", 0, 240, 639, 240},
    {"column", 320, 0, 320, 479},
};

struct SweepStats {
    int cases;
    int empty;
    int partial;
    int full;
    int mismatches;
};

static void sweep_prepare(U32 salt) {
    setup_screen();
    fill_source_pattern(salt);
    fill_dest_pattern(salt + 1u);
    memcpy(cpp_src, src_init, sizeof(cpp_src));
    memcpy(asm_src, src_init, sizeof(asm_src));
    memcpy(cpp_dst, dst_init, sizeof(cpp_dst));
    memcpy(asm_dst, dst_init, sizeof(asm_dst));
}

static void sweep_set_clip(const SweepClip &clip) {
    setup_screen();
    ClipXMin = clip.x_min;
    ClipYMin = clip.y_min;
    ClipXMax = clip.x_max;
    ClipYMax = clip.y_max;
}

/* Runs one call on both sides, compares the destinations inside the clip
 * window, classifies the drawn area by the bounding box of changed bytes,
 * then restores the window on both. Writes outside the window are caught
 * once per sweep in sweep_finish. */
static void sweep_case(const char *what, const SweepClip &clip,
                       S32 x0, S32 y0, S32 x1, S32 y1, S32 xd, S32 yd,
                       SweepStats *stats) {
    sweep_set_clip(clip);
    CopyBlock(x0, y0, x1, y1, cpp_src, xd, yd, cpp_dst);
    sweep_set_clip(clip);
    asm_CopyBlock(x0, y0, x1, y1, asm_src, xd, yd, asm_dst);
    setup_screen();

    const size_t span = (size_t)(clip.x_max - clip.x_min + 1);
    S32 bx0 = (S32)kScreenWidth, by0 = (S32)kScreenHeight, bx1 = -1, by1 = -1;
    S32 diff_x = -1, diff_y = -1;
    U32 diff_count = 0;
    stats->cases++;
    assert_count++;
    for (S32 y = clip.y_min; y <= clip.y_max; ++y) {
        U8 *a = asm_dst + y * (S32)kScreenWidth + clip.x_min;
        U8 *c = cpp_dst + y * (S32)kScreenWidth + clip.x_min;
        const U8 *d = dst_init + y * (S32)kScreenWidth + clip.x_min;
        if (memcmp(a, d, span) != 0) {
            for (S32 i = 0; i < (S32)span; ++i) {
                if (a[i] != d[i]) {
                    if (clip.x_min + i < bx0)
                        bx0 = clip.x_min + i;
                    if (clip.x_min + i > bx1)
                        bx1 = clip.x_min + i;
                }
            }
            if (y < by0)
                by0 = y;
            by1 = y;
        }
        if (memcmp(a, c, span) != 0) {
            for (S32 i = 0; i < (S32)span; ++i) {
                if (a[i] != c[i]) {
                    if (diff_count == 0) {
                        diff_x = clip.x_min + i;
                        diff_y = y;
                    }
                    diff_count++;
                }
            }
        }
        memcpy(a, d, span);
        memcpy(c, d, span);
    }
    if (diff_count != 0) {
        stats->mismatches++;
        if (stats->mismatches <= 8) {
            FAIL_MSG("[%s clip=%s src=(%d,%d)-(%d,%d) dst=(%d,%d)] %u bytes "
                     "differ, first at (%d,%d)",
                     what, clip.name, (int)x0, (int)y0, (int)x1, (int)y1,
                     (int)xd, (int)yd, diff_count, (int)diff_x, (int)diff_y);
        } else {
            test_failures++;
        }
    }
    if (bx1 < 0) {
        stats->empty++;
    } else if (bx1 - bx0 == x1 - x0 && by1 - by0 == y1 - y0) {
        stats->full++;
    } else {
        stats->partial++;
    }
}

static void sweep_finish(const char *what, const SweepStats &stats) {
    ASSERT_ASM_CPP_MEM_EQ(src_init, cpp_src, sizeof(cpp_src), what);
    ASSERT_ASM_CPP_MEM_EQ(src_init, asm_src, sizeof(asm_src), what);
    /* After per-case restores of the clip window both destinations must be
     * back to the initial pattern: nothing was written outside the window. */
    ASSERT_ASM_CPP_MEM_EQ(dst_init, asm_dst, sizeof(asm_dst), what);
    ASSERT_ASM_CPP_MEM_EQ(dst_init, cpp_dst, sizeof(cpp_dst), what);
    printf("  %s: %d cases, %d empty, %d partial, %d full, %d mismatches\n",
           what, stats.cases, stats.empty, stats.partial, stats.full,
           stats.mismatches);
}

/* Walk step: 1 px within 12 px of either end of a walk; in the middle of a
 * long walk (full-screen block only) every 5th position, to keep the run
 * short under emulation. */
static S32 sweep_step(S32 pos, S32 lo, S32 hi, bool coarse) {
    if (!coarse || pos - lo < 12 || hi - pos < 12)
        return 1;
    return 5;
}

/* Left edge of the window at a mid position, centred when the block fits. */
static S32 sweep_mid(S32 lo, S32 hi, S32 size) {
    S32 span = hi - lo + 1;
    return span > size ? lo + (span - size) / 2 : lo;
}

/* One block size against one clip window: the destination and the source
 * each walk every edge from just outside to inside, then every corner. */
static void sweep_block(S32 w, S32 h, const SweepClip &clip, U32 salt,
                        bool coarse) {
    char what[64];
    SweepStats stats = {0, 0, 0, 0, 0};
    snprintf(what, sizeof(what), "CopyBlock sweep %dx%d %s", (int)w, (int)h,
             clip.name);
    sweep_prepare(salt);

    /* Source at a mid position inside the window (clipped when the block is
     * larger than the window); destination at a mid position. */
    const S32 sx = sweep_mid(clip.x_min, clip.x_max, w);
    const S32 sy = sweep_mid(clip.y_min, clip.y_max, h);
    const S32 dx = sweep_mid(clip.x_min, clip.x_max, w);
    const S32 dy = sweep_mid(clip.y_min, clip.y_max, h);
    const S32 m = (w > h ? w : h) + 1;

    /* Destination walks. */
    for (S32 x = clip.x_min - w - 1; x <= clip.x_min + 2; x += sweep_step(x, clip.x_min - w - 1, clip.x_min + 2, coarse))
        sweep_case("dst left", clip, sx, sy, sx + w - 1, sy + h - 1, x, dy, &stats);
    for (S32 x = clip.x_max - w - 1; x <= clip.x_max + 2; x += sweep_step(x, clip.x_max - w - 1, clip.x_max + 2, coarse))
        sweep_case("dst right", clip, sx, sy, sx + w - 1, sy + h - 1, x, dy, &stats);
    for (S32 y = clip.y_min - h - 1; y <= clip.y_min + 2; y += sweep_step(y, clip.y_min - h - 1, clip.y_min + 2, coarse))
        sweep_case("dst top", clip, sx, sy, sx + w - 1, sy + h - 1, dx, y, &stats);
    for (S32 y = clip.y_max - h - 1; y <= clip.y_max + 2; y += sweep_step(y, clip.y_max - h - 1, clip.y_max + 2, coarse))
        sweep_case("dst bottom", clip, sx, sy, sx + w - 1, sy + h - 1, dx, y, &stats);

    /* Source walks. */
    for (S32 x = clip.x_min - w - 1; x <= clip.x_min + 2; x += sweep_step(x, clip.x_min - w - 1, clip.x_min + 2, coarse))
        sweep_case("src left", clip, x, sy, x + w - 1, sy + h - 1, dx, dy, &stats);
    for (S32 x = clip.x_max - w - 1; x <= clip.x_max + 2; x += sweep_step(x, clip.x_max - w - 1, clip.x_max + 2, coarse))
        sweep_case("src right", clip, x, sy, x + w - 1, sy + h - 1, dx, dy, &stats);
    for (S32 y = clip.y_min - h - 1; y <= clip.y_min + 2; y += sweep_step(y, clip.y_min - h - 1, clip.y_min + 2, coarse))
        sweep_case("src top", clip, sx, y, sx + w - 1, y + h - 1, dx, dy, &stats);
    for (S32 y = clip.y_max - h - 1; y <= clip.y_max + 2; y += sweep_step(y, clip.y_max - h - 1, clip.y_max + 2, coarse))
        sweep_case("src bottom", clip, sx, y, sx + w - 1, y + h - 1, dx, dy, &stats);

    /* Corner diagonals: t < 0 is outside the corner, t >= 0 inside. */
    for (S32 t = -m; t <= 2; t += sweep_step(t, -m, 2, coarse)) {
        const S32 l = clip.x_min + t;
        const S32 r = clip.x_max - w + 1 - t;
        const S32 u = clip.y_min + t;
        const S32 b = clip.y_max - h + 1 - t;
        sweep_case("dst top-left", clip, sx, sy, sx + w - 1, sy + h - 1, l, u, &stats);
        sweep_case("dst top-right", clip, sx, sy, sx + w - 1, sy + h - 1, r, u, &stats);
        sweep_case("dst bottom-left", clip, sx, sy, sx + w - 1, sy + h - 1, l, b, &stats);
        sweep_case("dst bottom-right", clip, sx, sy, sx + w - 1, sy + h - 1, r, b, &stats);
        sweep_case("src top-left", clip, l, u, l + w - 1, u + h - 1, dx, dy, &stats);
        sweep_case("src top-right", clip, r, u, r + w - 1, u + h - 1, dx, dy, &stats);
        sweep_case("src bottom-left", clip, l, b, l + w - 1, b + h - 1, dx, dy, &stats);
        sweep_case("src bottom-right", clip, r, b, r + w - 1, b + h - 1, dx, dy, &stats);
    }

    /* Inverted rectangles (x1 < x0, y1 < y0, both), at mid and across the
     * window's edges: the assembly rejects them after clipping. */
    static const S32 kFlip[] = {1, 2, 47, 199};
    for (size_t i = 0; i < sizeof(kFlip) / sizeof(kFlip[0]); ++i) {
        const S32 f = kFlip[i];
        const S32 xs[] = {sx, clip.x_min - 5, clip.x_max - 5, clip.x_min + f / 2};
        const S32 ys[] = {sy, clip.y_min - 5, clip.y_max - 5, clip.y_min + f / 2};
        for (size_t j = 0; j < 4; ++j) {
            sweep_case("inverted x", clip, xs[j], sy, xs[j] - f, sy + h - 1, dx, dy, &stats);
            sweep_case("inverted y", clip, sx, ys[j], sx + w - 1, ys[j] - f, dx, dy, &stats);
            sweep_case("inverted xy", clip, xs[j], ys[j], xs[j] - f, ys[j] - f, dx, dy, &stats);
        }
    }

    sweep_finish(what, stats);
    /* Full and inner windows must see partial draws, or the sweep never
     * reached the clipping code. */
    if (clip.x_max - clip.x_min >= w - 1 && clip.y_max - clip.y_min >= h - 1) {
        ASSERT_TRUE(stats.partial > 0);
        ASSERT_TRUE(stats.full > 0);
    }
    ASSERT_TRUE(stats.empty > 0);
}

static void sweep_size(S32 w, S32 h, U32 salt, bool coarse) {
    for (size_t c = 0; c < sizeof(kSweepClips) / sizeof(kSweepClips[0]); ++c)
        sweep_block(w, h, kSweepClips[c], salt + (U32)c, coarse);
}

static void test_edge_sweep_brick(void) {
    sweep_size(48, 38, 700u, false);
}

static void test_edge_sweep_large(void) {
    sweep_size(200, 150, 710u, false);
}

static void test_edge_sweep_fullscreen(void) {
    sweep_size(640, 480, 720u, true);
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_edge_sweep_brick);
    RUN_TEST(test_edge_sweep_large);
    RUN_TEST(test_edge_sweep_fullscreen);
    TEST_SUMMARY();
    return test_failures != 0;
}
