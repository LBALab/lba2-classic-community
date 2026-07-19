/* Host test for the texture-filter blend tables (LIB386/pol_work/POLYFILT.CPP).
 *
 * The tables are what makes filtering possible at all in a palette-indexed
 * rasterizer: indices cannot be averaged arithmetically, so the nearest index
 * to each blended colour is precomputed instead. Three properties have to hold
 * or the filter degrades in ways that are hard to spot in a screenshot:
 *
 *   - the mix has to be positional. If the 25/50/75 entries collapse to one
 *     value, every screen pixel inside a texel span gets the same colour and
 *     the blocks stay exactly as large -- the filter appears to do nothing;
 *   - the table has to be an exact mirror across the diagonal, because the
 *     fillers pick the operand order from whichever texel the sub-texel
 *     fraction is walking toward. Asymmetry would make a surface shimmer as
 *     the camera crosses a texel boundary;
 *   - the chosen index must be the closest one available, not merely a close
 *     one, or blends drift off the artist's ramp and tint the surface.
 *
 * Pure data in, data out, with no engine state, so it runs in the host suite.
 */

#include <POLYGON/POLYFILT.H>

#include <cstdio>
#include <cstdlib>

static int failures = 0;

static void fail(const char *what) {
    std::printf("FAIL %s\n", what);
    failures++;
}

/* A palette shaped like the game's: 16 banks of 16 monotone steps. */
static void MakeRampPalette(U8 *pal768) {
    for (int bank = 0; bank < 16; bank++) {
        for (int step = 0; step < 16; step++) {
            const int i = bank * 16 + step;
            const int v = step * 17; /* 0..255 across the ramp */
            pal768[i * 3 + 0] = (U8)((bank & 1) ? v : v / 2);
            pal768[i * 3 + 1] = (U8)((bank & 2) ? v : v / 3);
            pal768[i * 3 + 2] = (U8)((bank & 4) ? v : v / 4);
        }
    }
}

static int NearestReference(int r, int g, int b, const U8 *pal768) {
    int best = 0;
    long bestDist = 0x7FFFFFFFL;
    for (int i = 0; i < 256; i++) {
        const long dr = (long)pal768[i * 3 + 0] - r;
        const long dg = (long)pal768[i * 3 + 1] - g;
        const long db = (long)pal768[i * 3 + 2] - b;
        const long d = dr * dr + dg * dg + db * db;
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

int main(void) {
    U8 pal[768];
    MakeRampPalette(pal);

    /* Adopting a palette does not build anything while filtering is off; that
       is what keeps the cost off players who never enable it. */
    PolyTexFilter = POLY_TEX_FILTER_OFF;
    Poly_BuildBlendLUT(pal);
    Poly_EnsureBlendLUT();

    PolyTexFilter = POLY_TEX_FILTER_BILINEAR;
    Poly_EnsureBlendLUT();

    /* 1. Symmetric: blending a toward b at 25% is blending b toward a at 75%. */
    for (int a = 0; a < 256; a += 7) {
        for (int b = 0; b < 256; b += 5) {
            const int ab = (a << 8) | b;
            const int ba = (b << 8) | a;
            if (PolyBlendLUT[0][ab] != PolyBlendLUT[2][ba]) {
                fail("25% table is not the mirror of the 75% table");
                a = 256;
                break;
            }
            if (PolyBlendLUT[1][ab] != PolyBlendLUT[1][ba]) {
                fail("50% table is not symmetric");
                a = 256;
                break;
            }
        }
    }

    /* 2. Positional: within one ramp the three weights must not collapse to a
          single index, or the blend carries no sub-texel information. */
    {
        const int a = 16 * 3 + 2;  /* two steps into bank 3 */
        const int b = 16 * 3 + 13; /* eleven steps further along the same ramp */
        const U8 q25 = PolyBlendLUT[0][(a << 8) | b];
        const U8 q50 = PolyBlendLUT[1][(a << 8) | b];
        const U8 q75 = PolyBlendLUT[2][(a << 8) | b];
        if (q25 == q50 && q50 == q75)
            fail("the three weights collapse to one index on a clean ramp");
        if (!(q25 < q50 && q50 < q75))
            fail("blend indices do not advance monotonically along the ramp");
    }

    /* 3. Nearest, not merely near: every off-diagonal entry matches an
          exhaustive search. The diagonal is deliberately exempt -- check 4
          holds it to a stronger rule. */
    for (int a = 0; a < 256; a += 11) {
        for (int b = 0; b < 256; b += 13) {
            if (a == b)
                continue;
            const int ar = pal[a * 3 + 0], ag = pal[a * 3 + 1], ab_ = pal[a * 3 + 2];
            const int br = pal[b * 3 + 0], bg = pal[b * 3 + 1], bb = pal[b * 3 + 2];
            const int want50 = NearestReference((ar + br) >> 1, (ag + bg) >> 1,
                                                (ab_ + bb) >> 1, pal);
            if (PolyBlendLUT[1][(a << 8) | b] != (U8)want50) {
                fail("50% entry is not the nearest palette index");
                a = 256;
                break;
            }
        }
    }

    /* 4. Identity: blending an index with itself must return that exact index,
          not merely one that happens to share its colour. Palettes routinely
          contain duplicate entries -- this test palette has sixteen blacks --
          and a no-op blend that silently moves a pixel to a different index
          would break effects keyed to index ranges, such as fades that walk a
          bank. */
    for (int level = 0; level < 3; level++) {
        for (int i = 0; i < 256; i++) {
            if (PolyBlendLUT[level][(i << 8) | i] != (U8)i) {
                fail("blending an index with itself did not return that index");
                level = 3;
                break;
            }
        }
    }

    if (failures == 0) {
        std::printf("test_polyfilt: all checks passed\n");
        return EXIT_SUCCESS;
    }
    std::printf("test_polyfilt: %d failure(s)\n", failures);
    return EXIT_FAILURE;
}
