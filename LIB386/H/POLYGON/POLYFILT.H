#ifndef POLYFILT_H
#define POLYFILT_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Texture filtering for the software rasterizer.
 *
 * The fillers work in palette-index space, so texels cannot be averaged
 * arithmetically (the mean of index 3 and index 200 is index 101, an unrelated
 * colour). Instead the answer is precomputed: PolyBlendLUT[q][a<<8|b] holds the
 * palette index whose RGB is closest to the weighted mix of palette[a] and
 * palette[b], for q = 0,1,2 meaning 25%, 50% and 75% toward b.
 *
 * Blending must happen AFTER the CLUT/shade lookup, on final display-palette
 * indices — blending raw texture indices could land in a different 16-entry
 * ramp whose shading behaves differently.
 *
 * The weight comes from the sub-texel fraction the fillers already carry. An
 * unweighted 50/50 blend would be useless under magnification: every screen
 * pixel inside one texel span would produce the same value, leaving the blocks
 * exactly as large, merely shifted half a texel.
 */

#define POLY_TEX_FILTER_OFF 0
#define POLY_TEX_FILTER_HORIZONTAL 1
#define POLY_TEX_FILTER_BILINEAR 2

extern S32 PolyTexFilter;         /* one of POLY_TEX_FILTER_* */
extern U8 PolyBlendLUT[3][65536]; /* [weight][a<<8 | b] */

/* Adopt a 768-byte RGB palette and mark the tables stale. Call once per
   palette load, never per frame; the palette is copied, so the caller's
   buffer may be freed or reused afterwards. */
void Poly_BuildBlendLUT(const U8 *pal768);

/* Build the tables if filtering is on and they are stale. One predictable
   branch, so it is cheap to call once per frame -- which is what makes
   turning the filter on at runtime work: the tables are not built while it
   is off, and a palette load while it is off only records the palette. */
void Poly_EnsureBlendLUT(void);

/* Blend final palette indices a and b; q is a quantised weight 0..3 where
   0 means "pure a" and 3 means "75% toward b". */
#define POLY_BLEND(a, b, q) \
    ((q) ? PolyBlendLUT[(q) - 1][(((U32)(U8)(a)) << 8) | (U8)(b)] : (U8)(a))

#ifdef __cplusplus
}
#endif

#endif /* POLYFILT_H */
