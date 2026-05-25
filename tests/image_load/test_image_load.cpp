/*
 * Unit tests for SOURCES/IMAGE_LOAD.CPP — the stride-aware helpers that
 * blit 640x480 HQR bitmaps into a (ModeDesiredX × ModeDesiredY) framebuffer.
 *
 * Pins the contract end-to-end without requiring retail data:
 *   - At 640x480, both helpers degenerate to a flat memcpy (byte-identical
 *     to the original Load_HQR call).
 *   - At wider/taller framebuffers, Image_*Centered offsets the image and
 *     leaves the sidebar/topbar pixels untouched (caller pre-clears them).
 *   - Image_*Stretched fills the entire destination via nearest-neighbour.
 *   - Image_LoadCentered's failure path leaves the destination untouched
 *     and propagates 0.
 *   - Image_RenderCenteredFromStaging is byte-identical to a fresh
 *     Image_LoadCentered for the same staging contents (no double-load).
 *
 * Synthetic per-pixel sentinels expose stride bugs loudly: an off-by-one
 * row shift produces "row N contains row (N±1)'s bytes" rather than a
 * subtly-shifted palette image that's easy to miss by eye.
 */

#include "IMAGE_LOAD.H"

#include <SYSTEM/ADELINE_TYPES.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Re-declared engine globals so the host test links without SDL3 or the
   rest of the engine.  Image_LoadCentered etc. read these at runtime. */
U32 ModeDesiredX = IMAGE_AUTHORED_WIDTH;
U32 ModeDesiredY = IMAGE_AUTHORED_HEIGHT;

/* Stub for Load_HQR.  Image_LoadCentered/Stretched call this to fill the
   ImageStaging buffer; in the test we control what gets returned and what
   gets written so we can exercise both the helper math and the wrapper. */
static U32 g_load_hqr_return = 0;
static const U8 *g_load_hqr_pattern = NULL;
static U32 g_load_hqr_pattern_size = 0;

extern "C" U32 Load_HQR(const char * /*file*/, void *dest, S32 /*index*/) {
    if (g_load_hqr_pattern && g_load_hqr_pattern_size > 0) {
        memcpy(dest, g_load_hqr_pattern, g_load_hqr_pattern_size);
    }
    return g_load_hqr_return;
}

/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            ++g_failures;                                           \
        }                                                           \
    } while (0)

/* Authored image is IMAGE_AUTHORED_WIDTH * IMAGE_AUTHORED_HEIGHT = 307200. */
static U8 g_staging_storage[IMAGE_STAGING_BYTES];

/* Destination scratch sized for the largest case we exercise (1024x768).
   Pre-filled with a sidebar sentinel; helpers must not touch the sentinel
   bytes outside the inscribed 640x480 image rect. */
#define MAX_DEST_W 1024
#define MAX_DEST_H 768
#define SIDEBAR_SENTINEL 0xCD
static U8 g_dest[MAX_DEST_W * MAX_DEST_H];

/* Each pixel of the authored 640x480 staging gets a per-pixel sentinel.
   The 8-bit byte means duplicates are unavoidable, but the (x * 11 + y * 7)
   recipe is dense enough that any one-row stride error shifts the visible
   pattern by 11 bytes — easy to spot in a debugger. */
static U8 staging_pixel(int x, int y) {
    return (U8)((x * 11 + y * 7) & 0xFF);
}

static void setup_staging_pattern(void) {
    ImageStaging = g_staging_storage;
    for (int y = 0; y < IMAGE_AUTHORED_HEIGHT; ++y) {
        for (int x = 0; x < IMAGE_AUTHORED_WIDTH; ++x) {
            ImageStaging[y * IMAGE_AUTHORED_WIDTH + x] = staging_pixel(x, y);
        }
    }
}

static void reset_dest(void) {
    memset(g_dest, SIDEBAR_SENTINEL, sizeof(g_dest));
}

/* ------------------------------------------------------------------ */
/* Image_CopyCenteredFromStaging tests (via Image_RenderCenteredFromStaging,
   which is the public alias).  At 640x480 the helper degenerates to a flat
   memcpy; at wider/taller destinations it centres and leaves the borders
   alone. */

static void test_centered_640x480_is_flat_memcpy(void) {
    setup_staging_pattern();
    reset_dest();
    ModeDesiredX = 640;
    ModeDesiredY = 480;

    Image_RenderCenteredFromStaging(g_dest);

    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < 640; ++x) {
            if (g_dest[y * 640 + x] != staging_pixel(x, y)) {
                fprintf(stderr,
                        "FAIL: centered 640x480 mismatch at (%d,%d): "
                        "expected 0x%02X got 0x%02X\n",
                        x, y, staging_pixel(x, y),
                        g_dest[y * 640 + x]);
                ++g_failures;
                return;
            }
        }
    }
}

static void test_centered_768x480_offsets_x_by_64(void) {
    setup_staging_pattern();
    reset_dest();
    ModeDesiredX = 768;
    ModeDesiredY = 480;
    /* xOffset = (768 - 640) / 2 = 64; yOffset = 0. */
    const int x_off = 64;

    Image_RenderCenteredFromStaging(g_dest);

    for (int y = 0; y < 480; ++y) {
        /* Left sidebar [0, 64) untouched. */
        for (int x = 0; x < x_off; ++x) {
            CHECK(g_dest[y * 768 + x] == SIDEBAR_SENTINEL,
                  "left sidebar clobbered");
        }
        /* Image content shifted right by x_off. */
        for (int x = 0; x < 640; ++x) {
            if (g_dest[y * 768 + (x_off + x)] != staging_pixel(x, y)) {
                fprintf(stderr,
                        "FAIL: centered 768x480 content (%d,%d): "
                        "expected 0x%02X got 0x%02X\n",
                        x, y, staging_pixel(x, y),
                        g_dest[y * 768 + (x_off + x)]);
                ++g_failures;
                return;
            }
        }
        /* Right sidebar [704, 768) untouched. */
        for (int x = x_off + 640; x < 768; ++x) {
            CHECK(g_dest[y * 768 + x] == SIDEBAR_SENTINEL,
                  "right sidebar clobbered");
        }
    }
}

static void test_centered_1024x768_offsets_both_axes(void) {
    setup_staging_pattern();
    reset_dest();
    ModeDesiredX = 1024;
    ModeDesiredY = 768;
    /* xOffset = (1024 - 640) / 2 = 192; yOffset = (768 - 480) / 2 = 144. */
    const int x_off = 192;
    const int y_off = 144;

    Image_RenderCenteredFromStaging(g_dest);

    /* Top letterbox rows [0, 144) entirely untouched. */
    for (int y = 0; y < y_off; ++y) {
        for (int x = 0; x < 1024; ++x) {
            if (g_dest[y * 1024 + x] != SIDEBAR_SENTINEL) {
                fprintf(stderr,
                        "FAIL: top letterbox (%d,%d) clobbered: 0x%02X\n",
                        x, y, g_dest[y * 1024 + x]);
                ++g_failures;
                return;
            }
        }
    }
    /* Image rows [144, 624): borders untouched, content placed at x_off. */
    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < x_off; ++x) {
            CHECK(g_dest[(y_off + y) * 1024 + x] == SIDEBAR_SENTINEL,
                  "left sidebar (1024) clobbered");
        }
        for (int x = 0; x < 640; ++x) {
            if (g_dest[(y_off + y) * 1024 + (x_off + x)] != staging_pixel(x, y)) {
                fprintf(stderr,
                        "FAIL: 1024x768 content (%d,%d): "
                        "expected 0x%02X got 0x%02X\n",
                        x, y, staging_pixel(x, y),
                        g_dest[(y_off + y) * 1024 + (x_off + x)]);
                ++g_failures;
                return;
            }
        }
        for (int x = x_off + 640; x < 1024; ++x) {
            CHECK(g_dest[(y_off + y) * 1024 + x] == SIDEBAR_SENTINEL,
                  "right sidebar (1024) clobbered");
        }
    }
    /* Bottom letterbox rows [624, 768) entirely untouched. */
    for (int y = y_off + 480; y < 768; ++y) {
        for (int x = 0; x < 1024; ++x) {
            if (g_dest[y * 1024 + x] != SIDEBAR_SENTINEL) {
                fprintf(stderr,
                        "FAIL: bottom letterbox (%d,%d) clobbered: 0x%02X\n",
                        x, y, g_dest[y * 1024 + x]);
                ++g_failures;
                return;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Image_LoadStretched tests.  Sanity at 640x480; nearest-neighbour math
   at 768x480 (the lowest-common widescreen step). */

static void test_stretched_640x480_is_flat_memcpy(void) {
    setup_staging_pattern();
    reset_dest();
    ModeDesiredX = 640;
    ModeDesiredY = 480;

    /* Use Load_HQR-backed entrypoint to also cover the wrapper. */
    g_load_hqr_pattern = ImageStaging; /* already filled by setup */
    g_load_hqr_pattern_size = IMAGE_STAGING_BYTES;
    g_load_hqr_return = IMAGE_STAGING_BYTES;

    U32 ret = Image_LoadStretched("dummy", g_dest, 0);
    CHECK(ret == IMAGE_STAGING_BYTES, "Image_LoadStretched return value");

    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < 640; ++x) {
            if (g_dest[y * 640 + x] != staging_pixel(x, y)) {
                fprintf(stderr,
                        "FAIL: stretched 640x480 mismatch at (%d,%d): "
                        "expected 0x%02X got 0x%02X\n",
                        x, y, staging_pixel(x, y),
                        g_dest[y * 640 + x]);
                ++g_failures;
                return;
            }
        }
    }
}

static void test_stretched_768x480_nearest_neighbour(void) {
    setup_staging_pattern();
    reset_dest();
    ModeDesiredX = 768;
    ModeDesiredY = 480;

    Image_LoadStretched("dummy", g_dest, 0);

    /* For every dest pixel, srcX = (destX * 640) / 768, srcY = destY. */
    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < 768; ++x) {
            int srcX = (x * 640) / 768;
            U8 expected = staging_pixel(srcX, y);
            if (g_dest[y * 768 + x] != expected) {
                fprintf(stderr,
                        "FAIL: stretched 768x480 at (%d,%d): expected "
                        "0x%02X (srcX=%d) got 0x%02X\n",
                        x, y, expected, srcX, g_dest[y * 768 + x]);
                ++g_failures;
                return;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Image_LoadCentered wrapper-level tests: success path returns Load_HQR's
   byte count and writes the staged image into the destination; failure
   path returns 0 and leaves the destination alone. */

static void test_load_centered_success_uses_load_hqr_pattern(void) {
    /* Pre-fill a *different* pattern into the storage that Load_HQR will
       overwrite — proves the helper sources from the post-Load_HQR
       staging contents, not from whatever was there before. */
    ImageStaging = g_staging_storage;
    for (int i = 0; i < (int)IMAGE_STAGING_BYTES; ++i) {
        ImageStaging[i] = 0xEE; /* "wrong" pre-load contents */
    }
    /* Set up the pattern Load_HQR will write. */
    static U8 stub_pattern[IMAGE_STAGING_BYTES];
    for (int y = 0; y < IMAGE_AUTHORED_HEIGHT; ++y)
        for (int x = 0; x < IMAGE_AUTHORED_WIDTH; ++x)
            stub_pattern[y * IMAGE_AUTHORED_WIDTH + x] = staging_pixel(x, y);
    g_load_hqr_pattern = stub_pattern;
    g_load_hqr_pattern_size = IMAGE_STAGING_BYTES;
    g_load_hqr_return = IMAGE_STAGING_BYTES;

    reset_dest();
    ModeDesiredX = 768;
    ModeDesiredY = 480;

    U32 ret = Image_LoadCentered("dummy", g_dest, 0);
    CHECK(ret == IMAGE_STAGING_BYTES, "Image_LoadCentered return value");
    /* Image content should be the staged pattern, centred at x=64. */
    for (int y = 0; y < 480; ++y) {
        if (g_dest[y * 768 + (64 + 320)] != staging_pixel(320, y)) {
            fprintf(stderr,
                    "FAIL: load_centered did not use Load_HQR contents "
                    "at row %d\n",
                    y);
            ++g_failures;
            return;
        }
    }
}

static void test_load_centered_failure_returns_zero_and_no_write(void) {
    setup_staging_pattern();
    reset_dest();
    ModeDesiredX = 768;
    ModeDesiredY = 480;

    g_load_hqr_pattern = NULL;
    g_load_hqr_pattern_size = 0;
    g_load_hqr_return = 0; /* Load_HQR reports failure. */

    U32 ret = Image_LoadCentered("dummy", g_dest, 0);
    CHECK(ret == 0, "Image_LoadCentered must return 0 on Load_HQR failure");
    /* Destination must be entirely untouched. */
    for (size_t i = 0; i < sizeof(g_dest); ++i) {
        if (g_dest[i] != SIDEBAR_SENTINEL) {
            fprintf(stderr,
                    "FAIL: load_centered failure path clobbered dest at %zu\n",
                    i);
            ++g_failures;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Image_RenderCenteredFromStaging vs a fresh Image_LoadCentered: same
   staging contents → byte-identical output.  Pins the "re-render without
   reload" alias contract used by fade transitions. */

static void test_render_centered_matches_load_centered(void) {
    setup_staging_pattern();
    ModeDesiredX = 768;
    ModeDesiredY = 480;

    U8 dest_a[768 * 480];
    U8 dest_b[768 * 480];
    memset(dest_a, SIDEBAR_SENTINEL, sizeof(dest_a));
    memset(dest_b, SIDEBAR_SENTINEL, sizeof(dest_b));

    /* Path A: via Load_HQR wrapper.  Have the stub leave staging alone. */
    g_load_hqr_pattern = NULL;
    g_load_hqr_pattern_size = 0;
    g_load_hqr_return = IMAGE_STAGING_BYTES;
    Image_LoadCentered("dummy", dest_a, 0);

    /* Path B: direct render from the existing staging. */
    Image_RenderCenteredFromStaging(dest_b);

    if (memcmp(dest_a, dest_b, sizeof(dest_a)) != 0) {
        fprintf(stderr,
                "FAIL: render_centered output diverges from "
                "load_centered output\n");
        ++g_failures;
    }
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_centered_640x480_is_flat_memcpy();
    test_centered_768x480_offsets_x_by_64();
    test_centered_1024x768_offsets_both_axes();
    test_stretched_640x480_is_flat_memcpy();
    test_stretched_768x480_nearest_neighbour();
    test_load_centered_success_uses_load_hqr_pattern();
    test_load_centered_failure_returns_zero_and_no_write();
    test_render_centered_matches_load_centered();

    if (g_failures == 0) {
        printf("PASS: test_image_load — all 8 cases\n");
        return 0;
    }
    fprintf(stderr, "FAIL: test_image_load — %d failure(s)\n", g_failures);
    return 1;
}
