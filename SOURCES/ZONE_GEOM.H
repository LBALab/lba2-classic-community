#ifndef ZONE_GEOM_H
#define ZONE_GEOM_H

#include <SYSTEM/ADELINE_TYPES.H>

/* Point-in-zone-box test.
 *
 * Extracted verbatim from the inline the engine repeated in CheckZoneSce,
 * CheckValidObjPos and CheckBuggyZoneGiver (SOURCES/OBJECT.CPP) so the bounds
 * live in one place. The test is deliberately ASYMMETRIC:
 *
 *   X, Z : half-open  [X0, X1) / [Z0, Z1)   -- upper bound is '<', not '<='
 *   Y    : closed      [Y0, Y1]
 *
 * The '<' on X1/Z1 is load-bearing: with '<=' a point sitting exactly on the
 * shared edge of two abutting zones matched BOTH at once (the original
 * "on se retrouvait dans 2 zones en même temps" bug documented at the call
 * sites). Keep this the single source of truth; the host test in
 * tests/zone/test_zone_geom.cpp locks the asymmetry down.
 */
static inline int ZonePointInBox(S32 x, S32 y, S32 z, S32 x0, S32 y0, S32 z0, S32 x1, S32 y1,
                                 S32 z1) {
    return x >= x0 && x < x1 && y >= y0 && y <= y1 && z >= z0 && z < z1;
}

#endif /* ZONE_GEOM_H */
