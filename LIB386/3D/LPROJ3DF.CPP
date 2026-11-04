#include <3D/LPROJ.H>

#include <math.h>
#include <3D/CAMERA.H>
#include <3D/PROJREC.H>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * Perspective projection of a 3D point onto the screen.
 *
 * The original ASM uses x87 extended precision throughout and fistp
 * (round-to-nearest-even) for the final integer conversion.  We match
 * this by using double arithmetic and lrint().
 */
S32 LongProjectPoint3D(S32 x, S32 y, S32 z) {
    LIB386_TRACE_CPP("LongProjectPoint3DF", "1", "x=%d y=%d z=%d", x, y, z);
    if (z > CameraZrClip) {
        /* INT_MIN; avoid -2147483648 (decimal literal is unsigned in ISO C90 rules) */
        Xp = -2147483647 - 1;
        Yp = -2147483647 - 1;

        LIB386_TRACE_CPP("LongProjectPoint3DF", "2", "ret=%d Xp=%d Yp=%d", 0, Xp, Yp);
        Projrec_LongProjectPoint3D(x, y, z, 0, Xp, Yp);
        return 0;
    }

    double factor = (double)FRatioX / (double)(CameraZr - z);

    Xp = XCentre + StereoProjShiftX + (S32)lrint((double)(x - CameraXr) * factor);
    Yp = YCentre + (S32)lrint((double)(y - CameraYr) * factor * (double)FRatioY);

    LIB386_TRACE_CPP("LongProjectPoint3DF", "2", "ret=%d Xp=%d Yp=%d", 1, Xp, Yp);
    Projrec_LongProjectPoint3D(x, y, z, 1, Xp, Yp);
    return 1;
}

Func_LongProjectPoint *LongProjectPoint = LongProjectPoint3D;
