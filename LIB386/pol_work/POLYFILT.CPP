#include <POLYGON/POLYFILT.H>

#include <stdlib.h>
#include <string.h>

S32 PolyTexFilter = POLY_TEX_FILTER_OFF;
U8 PolyBlendLUT[3][65536];

static U8 s_palette[768];
static S32 s_paletteValid = 0;
static S32 s_tablesBuilt = 0;

/* Self-contained nearest-palette-entry search. SOURCES/FUNC.CPP has an
   equivalent (SearchBoundColRGB) but LIB386 must not depend upward on
   SOURCES. */
static U8 NearestIndex(S32 r, S32 g, S32 b, const U8 *pal768) {
    S32 best = 0;
    S32 bestDist = 0x7FFFFFFF;
    S32 i;
    for (i = 0; i < 256; i++) {
        const S32 dr = (S32)pal768[i * 3 + 0] - r;
        const S32 dg = (S32)pal768[i * 3 + 1] - g;
        const S32 db = (S32)pal768[i * 3 + 2] - b;
        const S32 d = dr * dr + dg * dg + db * db;
        if (d < bestDist) {
            bestDist = d;
            best = i;
            if (d == 0)
                break;
        }
    }
    return (U8)best;
}

void Poly_BuildBlendLUT(const U8 *pal768) {
    if (pal768 == NULL)
        return;

    memcpy(s_palette, pal768, sizeof(s_palette));
    s_paletteValid = 1;
    s_tablesBuilt = 0;
}

void Poly_EnsureBlendLUT(void) {
    const U8 *pal = s_palette;
    S32 a, b;

    if (PolyTexFilter == POLY_TEX_FILTER_OFF || s_tablesBuilt || !s_paletteValid)
        return;

    for (a = 0; a < 256; a++) {
        const S32 ar = pal[a * 3 + 0];
        const S32 ag = pal[a * 3 + 1];
        const S32 ab = pal[a * 3 + 2];
        for (b = a; b < 256; b++) {
            const S32 br = pal[b * 3 + 0];
            const S32 bg = pal[b * 3 + 1];
            const S32 bb = pal[b * 3 + 2];

            /* 25% toward b and its mirror, which is 75% with the pair
               swapped; 50% is symmetric. */
            const U8 q25 = NearestIndex((ar * 3 + br) >> 2, (ag * 3 + bg) >> 2,
                                        (ab * 3 + bb) >> 2, pal);
            const U8 q50 = NearestIndex((ar + br) >> 1, (ag + bg) >> 1,
                                        (ab + bb) >> 1, pal);
            const U8 q75 = NearestIndex((ar + br * 3) >> 2, (ag + bg * 3) >> 2,
                                        (ab + bb * 3) >> 2, pal);

            PolyBlendLUT[0][(a << 8) | b] = q25;
            PolyBlendLUT[0][(b << 8) | a] = q75;
            PolyBlendLUT[1][(a << 8) | b] = q50;
            PolyBlendLUT[1][(b << 8) | a] = q50;
            PolyBlendLUT[2][(a << 8) | b] = q75;
            PolyBlendLUT[2][(b << 8) | a] = q25;
        }
    }

    s_tablesBuilt = 1;
}
