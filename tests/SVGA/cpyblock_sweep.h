/* Shared ASM-vs-CPP harness for the two block copies, CopyBlock and
 * CopyBlockIncrust: fixed cases, random cases and the clip-window edge sweep.
 * Both have the same signature and the same clipping, so the harness takes the
 * two calls under test as parameters.
 *
 * Included by exactly one test file, which first defines:
 *
 *   static void fill_source_pattern(U32 salt);  // fills src_init
 *   static void fill_dest_pattern(U32 salt);    // fills dst_init
 *
 * and then calls blit_sweep_use() with its two calls and a name for labels.
 *
 * The two patterns must never hold the same value at the same position where
 * the source is drawn: a sweep case classifies a draw as full, partial or empty
 * from the bytes that changed, and a drawn byte equal to the one beneath it does
 * not change, which would make a whole draw count as partial. Keeping source
 * bytes below 0x80 and destination bytes at or above it does that; a transparent
 * zero in CopyBlockIncrust is not drawn at all, so it changes nothing either way. */
#ifndef TESTS_SVGA_CPYBLOCK_SWEEP_H
#define TESTS_SVGA_CPYBLOCK_SWEEP_H

#include "test_harness.h"

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>
#include <string.h>

typedef void (*BlitCall)(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd, void *dst);

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

static BlitCall g_cpp_blit;
static BlitCall g_asm_blit;
static const char *g_blit_name = "blit";

static void fill_source_pattern(U32 salt);
static void fill_dest_pattern(U32 salt);

static void blit_sweep_use(BlitCall cpp, BlitCall asm_call, const char *name) {
    g_cpp_blit = cpp;
    g_asm_blit = asm_call;
    g_blit_name = name;
}

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

static void set_clip(S32 x_min, S32 y_min, S32 x_max, S32 y_max) {
    setup_screen();
    ClipXMin = x_min;
    ClipYMin = y_min;
    ClipXMax = x_max;
    ClipYMax = y_max;
}

/* One call on both sides from fresh buffers; the source must come back
 * untouched and the two destinations must match everywhere. */
static void assert_blit_case(const char *label,
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

    set_clip(clip_x_min, clip_y_min, clip_x_max, clip_y_max);
    g_cpp_blit(x0, y0, x1, y1, cpp_src, xd, yd, cpp_dst);
    set_clip(clip_x_min, clip_y_min, clip_x_max, clip_y_max);
    g_asm_blit(x0, y0, x1, y1, asm_src, xd, yd, asm_dst);

    ASSERT_ASM_CPP_MEM_EQ(src_init, cpp_src, sizeof(cpp_src), label);
    ASSERT_ASM_CPP_MEM_EQ(src_init, asm_src, sizeof(asm_src), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), label);
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

        snprintf(label, sizeof(label), "%s rand %d", g_blit_name, i);
        assert_blit_case(label, x0, y0, x1, y1, xd, yd,
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

/* The pixels just outside the clip window that exist on screen: the row above
 * and below it and the column left and right of it. A write there is a clip
 * off by one, the likeliest way to write outside the window, so each case looks
 * there and names itself; sweep_finish still checks the whole buffer once. */
static U32 sweep_ring_writes(const SweepClip &clip, const U8 *buf) {
    U32 n = 0;
    const S32 w = (S32)kScreenWidth;
    for (S32 y = clip.y_min - 1; y <= clip.y_max + 1; y += clip.y_max - clip.y_min + 2) {
        if (y < 0 || y >= (S32)kScreenHeight)
            continue;
        const S32 x0 = clip.x_min > 0 ? clip.x_min - 1 : 0;
        const S32 x1 = clip.x_max < w - 1 ? clip.x_max + 1 : w - 1;
        if (memcmp(buf + y * w + x0, dst_init + y * w + x0, (size_t)(x1 - x0 + 1)) != 0)
            n++;
    }
    for (S32 y = clip.y_min; y <= clip.y_max; ++y) {
        if (clip.x_min > 0 && buf[y * w + clip.x_min - 1] != dst_init[y * w + clip.x_min - 1])
            n++;
        if (clip.x_max < w - 1 && buf[y * w + clip.x_max + 1] != dst_init[y * w + clip.x_max + 1])
            n++;
    }
    return n;
}

/* Runs one call on both sides, compares the destinations inside the clip
 * window, classifies the drawn area by the bounding box of changed bytes,
 * then restores the rows it changed on both. */
static void sweep_case(const char *what, const SweepClip &clip,
                       S32 x0, S32 y0, S32 x1, S32 y1, S32 xd, S32 yd,
                       SweepStats *stats) {
    set_clip(clip.x_min, clip.y_min, clip.x_max, clip.y_max);
    g_cpp_blit(x0, y0, x1, y1, cpp_src, xd, yd, cpp_dst);
    set_clip(clip.x_min, clip.y_min, clip.x_max, clip.y_max);
    g_asm_blit(x0, y0, x1, y1, asm_src, xd, yd, asm_dst);
    setup_screen();

    const U32 ring_asm = sweep_ring_writes(clip, asm_dst);
    const U32 ring_cpp = sweep_ring_writes(clip, cpp_dst);
    if (ring_asm != 0 || ring_cpp != 0) {
        stats->mismatches++;
        if (stats->mismatches <= 8) {
            FAIL_MSG("[%s clip=%s src=(%d,%d)-(%d,%d) dst=(%d,%d)] wrote outside the clip "
                     "window (asm %u, cpp %u places)",
                     what, clip.name, (int)x0, (int)y0, (int)x1, (int)y1, (int)xd, (int)yd,
                     ring_asm, ring_cpp);
        } else {
            test_failures++;
        }
        /* Put the whole buffers back, so later cases are not blamed for it. */
        memcpy(asm_dst, dst_init, sizeof(asm_dst));
        memcpy(cpp_dst, dst_init, sizeof(cpp_dst));
    }

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
        const bool asm_drew = memcmp(a, d, span) != 0;
        const bool sides_differ = memcmp(a, c, span) != 0;
        /* Most rows of a case are not drawn on: both sides equal the initial
         * pattern, and there is nothing to classify or restore. */
        if (!asm_drew && !sides_differ)
            continue;
        if (asm_drew) {
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
        if (sides_differ) {
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
    snprintf(what, sizeof(what), "%s sweep %dx%d %s", g_blit_name, (int)w, (int)h,
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

#endif /* TESTS_SVGA_CPYBLOCK_SWEEP_H */
