#include <3D/DISTANCE.H>

#include <math.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/* sqrtl here was a software 128-bit sqrt on 64-bit ARM (long double is quad
   precision there), and these distances run per object per frame via SORT,
   AMBIANCE, GERETRAK and BUGGY.  double sqrt is a hardware instruction on
   every supported target.  The integer correction pins the result to
   floor(sqrt(sum)) - exactly what the old (U32)sqrtl truncation produced
   (the double result can round one ulp above the true sqrt, so the loops
   re-establish the exact floor).  The sum is computed in 64-bit so distant
   objects cannot wrap the 32-bit squares; in-range values are unchanged. */
static U32 SqrtFloorU64(U64 n) {
    U32 r = (U32)sqrt((double)n);
    while (r > 0 && (U64)r * r > n)
        r--;
    while ((U64)(r + 1) * (r + 1) <= n)
        r++;
    return r;
}

U32 Distance2D(S32 x0, S32 y0, S32 x1, S32 y1) {
    S64 dx = (S64)x0 - x1;
    S64 dy = (S64)y0 - y1;
    S64 n = dx * dx + dy * dy;

    if (n <= 0)
        return 0;
    return SqrtFloorU64((U64)n);
}

U32 Distance3D(S32 x0, S32 y0, S32 z0, S32 x1, S32 y1, S32 z1) {
    S64 dx = (S64)x0 - x1;
    S64 dy = (S64)y0 - y1;
    S64 dz = (S64)z0 - z1;
    S64 n = dx * dx + dy * dy + dz * dz;

    if (n <= 0)
        return 0;
    return SqrtFloorU64((U64)n);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
