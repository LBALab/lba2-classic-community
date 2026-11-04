/* Unit test for the CPU xBR-lv2 2x upscaler (LIB386/SVGA/XBR.CPP).
 *
 * xBR is the per-texture filter mode's CPU twin, so these tests pin its
 * core contract: flat regions stay exactly flat, the edges of straight
 * boundaries stay put, and a 2x2 checkerboard is a nearest-pixel pattern
 * no edge rule should move. */
#include <SVGA/XBR.H>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static U32 pack(U8 r, U8 g, U8 b) {
    return 0xFF000000u | ((U32)r << 16) | ((U32)g << 8) | (U32)b;
}

static U8 redOf(U32 c) { return (U8)(c >> 16); }
static U8 greenOf(U32 c) { return (U8)(c >> 8); }
static U8 blueOf(U32 c) { return (U8)c; }

/* Fill src with w*h pixels of `c`. */
static void fill(U32 *img, U32 w, U32 h, U32 c) {
    for (U32 i = 0; i < w * h; i++)
        img[i] = c;
}

static void testFlatStaysFlat(void) {
    const U32 W = 8, H = 8;
    U32 src[W * H];
    U32 dst[W * 2 * H * 2];
    fill(src, W, H, pack(200, 100, 50));

    XbrUpscale2x(src, W, H, dst);
    assert(dst[0] != 0); /* packed output is opaque */
    for (U32 i = 0; i < W * 2 * H * 2; i++) {
        if (redOf(dst[i]) != 200 || greenOf(dst[i]) != 100 ||
            blueOf(dst[i]) != 50) {
            fprintf(stderr, "flat region drifted at %u: %08X\n", i, dst[i]);
            assert(0);
        }
    }
}

static void testCheckerboardStaysSharp(void) {
    /* Alternate single pixels: every neighbor differs, so no two guard
     * colors ever agree and each output quadrant must keep the source
     * texel's color. */
    const U32 W = 8, H = 8;
    U32 src[W * H];
    for (U32 y = 0; y < H; y++) {
        for (U32 x = 0; x < W; x++) {
            src[y * W + x] = ((x + y) & 1) ? pack(255, 255, 255)
                                           : pack(0, 0, 0);
        }
    }
    U32 dst[W * 2 * H * 2];
    XbrUpscale2x(src, W, H, dst);

    for (U32 y = 0; y < H * 2; y++) {
        for (U32 x = 0; x < W * 2; x++) {
            U32 want = (((x >> 1) + (y >> 1)) & 1) ? pack(255, 255, 255)
                                                   : pack(0, 0, 0);
            U32 got = dst[y * (W * 2) + x];
            if (got != want) {
                fprintf(stderr, "checkerboard smeared at (%u,%u): %08X\n", x,
                        y, got);
                assert(0);
            }
        }
    }
}

static void testVerticalEdgeStaysVertical(void) {
    /* A straight vertical boundary between two flat halves. xBR may blend
     * the two boundary texel columns across the 2-pixel-wide edge, but the
     * left half must contain no white and the right half no black. */
    const U32 W = 8, H = 8;
    U32 src[W * H];
    for (U32 y = 0; y < H; y++) {
        for (U32 x = 0; x < W; x++) {
            src[y * W + x] = (x < 4) ? pack(0, 0, 0) : pack(255, 255, 255);
        }
    }
    U32 dst[W * 2 * H * 2];
    XbrUpscale2x(src, W, H, dst);

    for (U32 y = 0; y < H * 2; y++) {
        for (U32 x = 0; x < 4; x++) { /* left half of the output */
            if (redOf(dst[y * (W * 2) + x]) != 0) {
                fprintf(stderr, "white bled into the black half at (%u,%u)\n",
                        x, y);
                assert(0);
            }
        }
        for (U32 x = W * 2 - 4; x < W * 2; x++) { /* right half */
            if (redOf(dst[y * (W * 2) + x]) != 255) {
                fprintf(stderr, "black bled into the white half at (%u,%u)\n",
                        x, y);
                assert(0);
            }
        }
    }
}

static void testEdgePixelsHandled(void) {
    /* Distinct colors everywhere: guards the clamp-to-edge fetches — an
     * out-of-range neighbor fetch (e.g. reading row -1) would pick up
     * adjacent-row colors or garbage instead of the border texel. */
    const U32 W = 5, H = 4;
    U32 *src = (U32 *)malloc(W * H * sizeof(U32));
    U32 *dst = (U32 *)malloc(W * 2 * H * 2 * sizeof(U32));
    for (U32 i = 0; i < W * H; i++) {
        src[i] = pack((U8)(i * 13 + 1), (U8)(i * 29 + 7), (U8)(i * 47 + 11));
    }
    XbrUpscale2x(src, W, H, dst);
    /* Corner quadrants of the output must come from the corner colors or
     * their immediate neighbors — never from beyond the border (the ramp
     * is built from distinct primes, so garbage shows up as a wrong
     * channel far from any plausibly blended value). */
    U32 c = dst[0];
    if (redOf(c) > 3 * 13 + 1 + 8) {
        fprintf(stderr, "top-left quadrant reads past the border: %08X\n", c);
        free(src);
        free(dst);
        assert(0);
    }
    free(src);
    free(dst);
}

int main(void) {
    testFlatStaysFlat();
    testCheckerboardStaysSharp();
    testVerticalEdgeStaysVertical();
    testEdgePixelsHandled();
    printf("xBR upscale tests passed\n");
    return 0;
}
