/*
 * Regression test for issue #78 — game crash in the Magic School scene
 * when the grand wizard appears on screen.
 *
 * Root cause: CopyMask used U32 for screen-space geometry. DrawOverBrick3
 * (the painter's-pass that re-stamps foreground bricks over an actor so
 * e.g. a candle on a pole stays in front of Twinsen) calls CopyMask with
 * x = -24 for column 0. On the original 32-bit Watcom build, U32(-24)
 * combined with 32-bit pointer wraparound made `Log + (U32)-24` resolve
 * to `Log - 24` and the existing margin loop then skipped the
 * out-of-buffer columns. On 64-bit the U32 zero-extends to a +4 GiB
 * offset and the inner pixel-copy loop walks into unmapped memory and
 * SIGSEGVs. Intermittent in the wild because whether `Log + 4 GiB` lands
 * in a mapped page is allocator/ASLR luck.
 *
 * The fix changes the geometry locals to S32 so the clip test detects
 * negative xMin and the pointer arithmetic does the right thing.
 *
 * This test pins the fix by calling CopyMask with x = -24 (the actual
 * crash-site argument seen in gdb) against a fixed-size framebuffer with
 * sentinel guard regions on either side. Without the fix the run
 * either segfaults outright or scribbles into the guard region.
 */

#include <SVGA/COPYMASK.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <SYSTEM/ADELINE_TYPES.H>

#include <stdio.h>
#include <string.h>

/* Globals normally provided by libsvg/libsys. Re-declared here so the
 * host test links without pulling in SDL3. */
void *Log = 0;
U32 ModeDesiredX = 640;
U32 ModeDesiredY = 480;
U32 TabOffLine[ADELINE_MAX_Y_RES];
S32 ClipXMin = 0;
S32 ClipYMin = 0;
S32 ClipXMax = 639;
S32 ClipYMax = 479;
S32 MemoClipXMin = 0;
S32 MemoClipYMin = 0;
S32 MemoClipXMax = 639;
S32 MemoClipYMax = 479;

#define SCREEN_W 640
#define SCREEN_H 480
#define GUARD    256
#define SENTINEL 0xA5

/* Framebuffer with sentinel guard regions. Anything CopyMask writes
 * outside [0, SCREEN_W*SCREEN_H) trips an assertion. */
static U8 g_buffer[GUARD + SCREEN_W * SCREEN_H + GUARD];
static U8 g_source[GUARD + SCREEN_W * SCREEN_H + GUARD];
static U8 *g_framebuf = g_buffer + GUARD;
static U8 *g_srcbuf = g_source + GUARD;

/* Mask: 48 wide x 1 tall, fully opaque single fill block.
 * Layout matches CopyMask's expected bank format. */
static U8 g_bank[64];

static int g_failures = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__);   \
            ++g_failures;                                             \
        }                                                             \
    } while (0)

static void setup_screen(void) {
    memset(g_buffer, SENTINEL, sizeof(g_buffer));
    memset(g_source, 0, sizeof(g_source));
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i)
        g_srcbuf[i] = (U8)((i * 7 + 1) & 0xFF);
    Log = g_framebuf;
    for (U32 i = 0; i < SCREEN_H; ++i)
        TabOffLine[i] = i * SCREEN_W;
}

static void build_48x1_opaque_mask(void) {
    memset(g_bank, 0, sizeof(g_bank));
    U32 *offsets = (U32 *)g_bank;
    offsets[0] = 4; /* one mask, header at offset 4 */
    U8 *b = g_bank + 4;
    /* Header: DeltaX, DeltaY, HotX, HotY */
    b[0] = 48;
    b[1] = 1;
    b[2] = 0;
    b[3] = 0;
    /* Line 0: NbBlock=2 (one skip/fill pair), skip 0, fill 48 */
    b[4] = 2;
    b[5] = 0;
    b[6] = 48;
}

static void check_guards(const char *label) {
    for (int i = 0; i < GUARD; ++i) {
        if (g_buffer[i] != SENTINEL) {
            fprintf(stderr, "FAIL: %s — pre-buffer guard clobbered at -%d\n",
                    label, GUARD - i);
            ++g_failures;
            return;
        }
        if (g_buffer[GUARD + SCREEN_W * SCREEN_H + i] != SENTINEL) {
            fprintf(stderr, "FAIL: %s — post-buffer guard clobbered at +%d\n",
                    label, i);
            ++g_failures;
            return;
        }
    }
}

/* The crash scenario: x = -24, y = 0 (col 0 in DrawOverBrick3).
 * Mask is 48 wide; left 24 columns are off-screen and must be clipped,
 * right 24 columns must land at framebuf columns 0..23 of row 0. */
static void test_negative_x_school_crash(void) {
    setup_screen();
    build_48x1_opaque_mask();

    CopyMask(0, -24, 0, g_bank, g_srcbuf);

    check_guards("negative_x_school_crash");

    /* Off-screen columns must NOT have been written. With the bug they
     * land somewhere in the +4 GiB no-mans-land or wrap into adjacent
     * memory; with the fix they're skipped by the margin clip. The
     * framebuffer's row 0 columns 0..23 are the surviving right half. */
    for (int x = 0; x < 24; ++x) {
        U8 expected = g_srcbuf[x];
        U8 actual = g_framebuf[x];
        if (actual != expected) {
            fprintf(stderr,
                    "FAIL: row 0 col %d: expected 0x%02X got 0x%02X\n",
                    x, expected, actual);
            ++g_failures;
            return;
        }
    }
    /* Columns 24..SCREEN_W-1 of row 0 were outside the mask footprint
     * and should be untouched (still SENTINEL from setup). */
    for (int x = 24; x < SCREEN_W; ++x) {
        if (g_framebuf[x] != SENTINEL) {
            fprintf(stderr,
                    "FAIL: row 0 col %d clobbered: 0x%02X\n",
                    x, g_framebuf[x]);
            ++g_failures;
            return;
        }
    }
    /* Other rows must be untouched. */
    for (int y = 1; y < SCREEN_H; ++y) {
        for (int x = 0; x < SCREEN_W; ++x) {
            if (g_framebuf[y * SCREEN_W + x] != SENTINEL) {
                fprintf(stderr,
                        "FAIL: spillover at row %d col %d: 0x%02X\n",
                        y, x, g_framebuf[y * SCREEN_W + x]);
                ++g_failures;
                return;
            }
        }
    }
}

/* Sanity: in-bounds call still works correctly (no regression to
 * the normal path). */
static void test_in_bounds_unchanged(void) {
    setup_screen();
    build_48x1_opaque_mask();

    CopyMask(0, 100, 50, g_bank, g_srcbuf);

    check_guards("in_bounds_unchanged");

    for (int x = 0; x < 48; ++x) {
        S32 sx = 100 + x;
        S32 sy = 50;
        U8 expected = g_srcbuf[sy * SCREEN_W + sx];
        U8 actual = g_framebuf[sy * SCREEN_W + sx];
        if (actual != expected) {
            fprintf(stderr,
                    "FAIL: in-bounds row %d col %d: expected 0x%02X got 0x%02X\n",
                    sy, sx, expected, actual);
            ++g_failures;
            return;
        }
    }
}

/* Right-edge clipping mirror: place the mask so it overhangs ClipXMax.
 * Confirms clip math also works for the symmetric case and that the
 * S32 conversion didn't break the existing right-clip path. */
static void test_right_edge_clip(void) {
    setup_screen();
    build_48x1_opaque_mask();

    CopyMask(0, SCREEN_W - 24, 100, g_bank, g_srcbuf);

    check_guards("right_edge_clip");

    for (int x = 0; x < 24; ++x) {
        S32 sx = SCREEN_W - 24 + x;
        S32 sy = 100;
        U8 expected = g_srcbuf[sy * SCREEN_W + sx];
        U8 actual = g_framebuf[sy * SCREEN_W + sx];
        if (actual != expected) {
            fprintf(stderr,
                    "FAIL: right-clip row %d col %d: expected 0x%02X got 0x%02X\n",
                    sy, sx, expected, actual);
            ++g_failures;
            return;
        }
    }
}

int main(void) {
    test_negative_x_school_crash();
    test_in_bounds_unchanged();
    test_right_edge_clip();

    if (g_failures) {
        fprintf(stderr, "test_copymask_negx: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("test_copymask_negx: ok\n");
    return 0;
}
