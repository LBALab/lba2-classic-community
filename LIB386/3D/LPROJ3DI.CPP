#include <3D/LPROJ.H>

#include <3D/CAMERA.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Port of LPROJ3DI.ASM, the 1997 integer path's LongProjectPoint: the
 * perspective divide in integers, -(x - CameraXr) * LFactorX / (z - CameraZr),
 * truncated toward zero like idiv. Subtractions and the negation wrap at 32
 * bits as in the ASM. Domain: z <= CameraZrClip keeps the divisor at or below
 * -NearClip; a quotient that does not fit 32 bits faults in the ASM (#DE).
 */
S32 LongProjectPoint3DI(S32 x, S32 y, S32 z) {
    if (z > CameraZrClip) {
        Xp = -2147483647 - 1;
        Yp = -2147483647 - 1;
        return 0;
    }

    const S32 dx = (S32)(0u - ((U32)x - (U32)CameraXr));
    const S32 dy = (S32)((U32)y - (U32)CameraYr);
    const S32 dz = (S32)((U32)z - (U32)CameraZr);

    Xp = (S32)((U32)(S32)((S64)dx * LFactorX / dz) + (U32)XCentre);
    Yp = (S32)((U32)(S32)((S64)dy * LFactorY / dz) + (U32)YCentre);
    return 1;
}

// n / d to the nearest integer, ties to even: what lrintl does to the float
// path's quotient in round-to-nearest mode. d is never 0 here.
static S32 DivRoundNearestEven(S64 n, S64 d) {
    S64 q = n / d;
    const S64 r = n % d;
    const S64 twiceR = r < 0 ? -2 * r : 2 * r;
    const S64 absD = d < 0 ? -d : d;

    if (twiceR > absD || (twiceR == absD && (q & 1)))
        q += ((n < 0) != (d < 0)) ? -1 : 1;
    return (S32)q;
}

/*
 * lba2cc, not in 1997: LongProjectPoint3DI against an explicit camera, with
 * the quotient rounded to nearest instead of truncated. The exterior terrain,
 * sea and sky take the integer path while objects stay on the float one, whose
 * projection rounds; truncating here would sit the ground one pixel off the
 * objects standing on it at about three contact points in four. Same clip,
 * same Xp/Yp outputs, same return value as LongProjectPoint3DI.
 */
S32 LongProjectPointIRound(S32 x, S32 y, S32 z, S32 camXr, S32 camYr, S32 camZr) {
    if (z > (S32)((U32)camZr - (U32)NearClip)) {
        Xp = -2147483647 - 1;
        Yp = -2147483647 - 1;
        return 0;
    }

    const S32 dx = (S32)(0u - ((U32)x - (U32)camXr));
    const S32 dy = (S32)((U32)y - (U32)camYr);
    const S32 dz = (S32)((U32)z - (U32)camZr);

    Xp = (S32)((U32)DivRoundNearestEven((S64)dx * LFactorX, dz) + (U32)XCentre);
    Yp = (S32)((U32)DivRoundNearestEven((S64)dy * LFactorY, dz) + (U32)YCentre);
    return 1;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
