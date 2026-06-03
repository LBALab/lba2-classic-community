/*
 * Regression test for the save-game screenshot thumbnail capture
 * (SOURCES/SAVEGAME.CPP). The 160x120 save preview is produced by two calls:
 *
 *     ScaleBox(full frame -> 160x120, into a scratch buffer)
 *     SaveBlock(scratch -> packed 160x120)
 *
 * The subtlety: ScaleBox writes its 160-wide downscaled output at a
 * *ModeDesiredX* row stride (offsetLine = ModeDesiredX - deltaX, dst indexed
 * through TabOffLine), so the scratch must hold 120 * ModeDesiredX bytes, not
 * 160 * 120. The historical code scaled into BufSpeak+50000 and packed into
 * BufSpeak+150000 -- a fixed 100 KB gap. That gap is only large enough while
 * 120 * ModeDesiredX <= 100000, i.e. ModeDesiredX <= ~833. At 640x480 (76 800)
 * and 768x480 (92 160) it fits; on a 16:9 widescreen (~848-856 wide) the
 * ScaleBox output overruns into the packed region, SaveBlock then reads back
 * rows it has already clobbered, and the bottom of the saved thumbnail is
 * garbage.
 *
 * The fix scales into ImageStaging (IMAGE_STAGING_BYTES = 640*480 = 307200),
 * which holds 120 * ModeDesiredX for every supported width (<= 1920 ->
 * 230400 bytes). This test pins that an ImageStaging-sized scratch produces the
 * same thumbnail as an oversized one at 640/768/856/1920, and reproduces the
 * overrun corruption of the cramped 100 KB layout at 856.
 */
#include <SVGA/SCALEBOX.H>
#include <SVGA/SAVBLOCK.H>
#include <SVGA/SCREEN.H>

#include <SYSTEM/ADELINE_TYPES.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Engine globals ScaleBox/SaveBlock read (defined in SCREEN.CPP in the real
   build; provided here so the host test links without the rest of the engine). */
U32 ModeDesiredX = 0;
U32 ModeDesiredY = 0;
U32 TabOffLine[ADELINE_MAX_Y_RES];

#define THUMB_W 160
#define THUMB_H 120
#define THUMB_BYTES (THUMB_W * THUMB_H)

/* Mirrors SOURCES/IMAGE_LOAD.H IMAGE_STAGING_BYTES (the scratch the fix uses). */
#define IMAGE_STAGING_BYTES_LOCAL (640u * 480u)

/* Historical BufSpeak layout: ScaleBox scratch at +50000, packed at +150000. */
#define CRAMPED_SCRATCH_OFF 50000u
#define CRAMPED_PACKED_OFF 150000u

static int g_failures = 0;
#define CHECK(cond, ...)                               \
    do {                                               \
        if (!(cond)) {                                 \
            fprintf(stderr, "FAIL: " __VA_ARGS__);     \
            fprintf(stderr, " (line %d)\n", __LINE__); \
            ++g_failures;                              \
        }                                              \
    } while (0)

static void setRes(U32 w, U32 h) {
    ModeDesiredX = w;
    ModeDesiredY = h;
    U32 off = 0;
    for (U32 i = 0; i < ADELINE_MAX_Y_RES; i++) {
        TabOffLine[i] = off;
        off += w;
    }
}

/* Distinctive per-pixel source pattern: any stride/overrun corruption shows up
   as bytes that do not match a clean downscale of the source. */
static U8 srcPixel(U32 x, U32 y) {
    return (U8)((x * 7u + y * 131u + 17u) & 0xFFu);
}

static void fillSource(U8 *src, U32 w, U32 h) {
    for (U32 y = 0; y < h; y++)
        for (U32 x = 0; x < w; x++)
            src[y * w + x] = srcPixel(x, y);
}

/* The SaveGame capture sequence into caller-supplied scratch + packed buffers. */
static void capture(U8 *src, U8 *scratch, U8 *packed, U32 w, U32 h) {
    ScaleBox(0, 0, (S32)w - 1, (S32)h - 1, src, 0, 0, THUMB_W - 1, THUMB_H - 1, scratch);
    SaveBlock(scratch, packed, 0, 0, THUMB_W - 1, THUMB_H - 1);
}

/* First thumbnail row where two packed 160x120 buffers differ, or -1. */
static int firstDivergentRow(const U8 *a, const U8 *b) {
    for (int y = 0; y < THUMB_H; y++)
        if (memcmp(a + y * THUMB_W, b + y * THUMB_W, THUMB_W) != 0)
            return y;
    return -1;
}

/* Capture with an oversized dedicated scratch -- the overrun-free reference. */
static void captureGolden(U8 *src, U8 *packed, U32 w, U32 h) {
    U8 *scratch = (U8 *)malloc((size_t)THUMB_H * w + 64u);
    capture(src, scratch, packed, w, h);
    free(scratch);
}

/* The fix: capture with an ImageStaging-sized scratch. */
static void captureFix(U8 *src, U8 *packed, U32 w, U32 h) {
    U8 *scratch = (U8 *)malloc(IMAGE_STAGING_BYTES_LOCAL);
    capture(src, scratch, packed, w, h);
    free(scratch);
}

/* The historical layout: ScaleBox -> buf+50000, SaveBlock -> buf+150000. */
static void captureCramped(U8 *src, U8 *packed_out, U32 w, U32 h) {
    U8 *buf = (U8 *)malloc(CRAMPED_PACKED_OFF + THUMB_BYTES + 64u);
    capture(src, buf + CRAMPED_SCRATCH_OFF, buf + CRAMPED_PACKED_OFF, w, h);
    memcpy(packed_out, buf + CRAMPED_PACKED_OFF, THUMB_BYTES);
    free(buf);
}

/* The fix produces the same thumbnail as an oversized scratch at every
   supported width -- i.e. ImageStaging is large enough, no overrun. */
static void test_fix_matches_golden(U32 w, U32 h) {
    setRes(w, h);
    U8 *src = (U8 *)malloc((size_t)w * h);
    fillSource(src, w, h);

    U8 golden[THUMB_BYTES], fixed[THUMB_BYTES];
    captureGolden(src, golden, w, h);
    captureFix(src, fixed, w, h);

    CHECK(memcmp(golden, fixed, THUMB_BYTES) == 0,
          "fix thumbnail diverges from oversized-scratch reference at %ux%u", w, h);
    free(src);
}

/* The cramped 100 KB layout matches the reference only while it fits. */
static void test_cramped_ok_when_it_fits(U32 w, U32 h) {
    setRes(w, h);
    U8 *src = (U8 *)malloc((size_t)w * h);
    fillSource(src, w, h);

    U8 golden[THUMB_BYTES], cramped[THUMB_BYTES];
    captureGolden(src, golden, w, h);
    captureCramped(src, cramped, w, h);

    CHECK(firstDivergentRow(golden, cramped) == -1,
          "cramped layout unexpectedly corrupts at %ux%u (should still fit)", w, h);
    free(src);
}

/* The bug: at 856 wide the cramped layout overruns the packed region and the
   bottom rows of the thumbnail are corrupted. */
static void test_cramped_overruns_at_856(void) {
    const U32 w = 856, h = 480;
    setRes(w, h);
    U8 *src = (U8 *)malloc((size_t)w * h);
    fillSource(src, w, h);

    U8 golden[THUMB_BYTES], cramped[THUMB_BYTES];
    captureGolden(src, golden, w, h);
    captureCramped(src, cramped, w, h);

    int row = firstDivergentRow(golden, cramped);
    CHECK(row >= 0, "expected cramped overrun corruption at 856x480, found none");
    /* 120*856 = 102720 > 100000, overrun starts ~row floor(100000/856)=116, so
       only the bottom handful of rows are hit -- the top of the thumbnail is
       fine, which is exactly the reported symptom. */
    CHECK(row >= 100, "overrun corruption should be confined to the bottom rows, got row %d", row);
    free(src);
}

int main(void) {
    /* ImageStaging must hold the ScaleBox scratch at the widest supported mode. */
    CHECK((U32)THUMB_H * 1920u <= IMAGE_STAGING_BYTES_LOCAL,
          "ImageStaging too small for 120*1920 scratch");

    test_fix_matches_golden(640, 480);
    test_fix_matches_golden(768, 480);
    test_fix_matches_golden(856, 480);
    test_fix_matches_golden(1920, 1080);

    test_cramped_ok_when_it_fits(640, 480);
    test_cramped_ok_when_it_fits(768, 480);

    test_cramped_overruns_at_856();

    if (g_failures == 0) {
        printf("PASS: test_save_thumbnail — all cases\n");
        return 0;
    }
    fprintf(stderr, "FAIL: test_save_thumbnail — %d failure(s)\n", g_failures);
    return 1;
}
