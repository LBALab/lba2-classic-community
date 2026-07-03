#include <3D/LPROJ.H>
#include <3D/CAMERA.H>
#include <3D/PROJREC.H>
#include <3D/SINTAB.H>
#include <3D/CAMERA.H>
#include <stdio.h>
#include <SYSTEM/DEBUG_TRACES.H>

S32 LongProjectPointIso(S32 x, S32 y, S32 z) {
    S32 origX = x, origY = y, origZ = z;
    LIB386_TRACE_CPP("LongProjectPointIso", "1", "x=%d y=%d z=%d", x, y, z);
    x -= CameraX;
    y -= CameraY;
    z -= CameraZ;
    int tmpx = x;
    x = x - z;
    z = z + tmpx;

    int tmpy = y;
    z = z * 2;
    y = y << 4; // *16
    x = x * 3;

    y = y - tmpy;
    z = z * 3;

    // Apply the runtime iso view scale. At the default ISO_SCALE_ONE this is
    // (v * 2^SHIFT) >> (n + SHIFT) == v >> n, bit-for-bit the original /64 and
    // /256 shifts; the long long intermediate keeps the scaled path from
    // overflowing. The screen centre is added after the scale, so zoom is
    // about XCentre/YCentre (the camera/hero point).
    x = (S32)(((long long)x * IsoScale) >> (6 + ISO_SCALE_SHIFT)); // /64 IsoScale
    x = x + XCentre;
    z = z - y;

    z = (S32)(((long long)z * IsoScale) >> (8 + ISO_SCALE_SHIFT)); // /256 IsoScale
    z = z + YCentre + 1;

    Xp = x;
    Yp = z;

    LIB386_TRACE_CPP("LongProjectPointIso", "2", "ret=%d Xp=%d Yp=%d", -1, Xp, Yp);
    Projrec_LongProjectPointIso(origX, origY, origZ, Xp, Yp);
    return -1;
}
