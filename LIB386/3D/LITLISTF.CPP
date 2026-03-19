#include <3D/LITLISTF.H>

#include <math.h>
#include <string.h>
#include <3D/LIGHT.H>
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * Per-vertex lighting.  Matches ASM LightListF.
 *
 * The ASM pipeline:
 * 1. Inverse-rotates the camera light vector → X0,Y0,Z0 (integers)
 * 2. Pre-multiplies by FactorLight → stored as floats in X0,Y0,Z0
 * 3. Inner loop: dot(light_scaled, vertex) → fistp → clamp to [0,∞)
 *
 * Step 2 reinterprets X0/Y0/Z0 as float storage.  We replicate this
 * with local float variables and lrintl for the final rounding.
 */
void LightList(TYPE_MAT *Mat, U16 *dst, TYPE_VT16 *src, S32 n) {
    LIB386_TRACE_CPP("LightListF", "1", "Mat=%p dst=%p src=%p n=%d", (void *)Mat, (void *)dst, (void *)src, n);
    if (n == 0)
        return;

    LongInverseRotatePoint(Mat, CameraXLight, CameraYLight, CameraZLight);

    /* Pre-scale the rotated light vector by FactorLight (stored as float,
	   matching the ASM's fstp [X0] / fmul [X0] pipeline). */
    float lx = (float)X0 * FactorLight;
    float ly = (float)Y0 * FactorLight;
    float lz = (float)Z0 * FactorLight;
    memcpy(&X0, &lx, sizeof(lx));
    memcpy(&Y0, &ly, sizeof(ly));
    memcpy(&Z0, &lz, sizeof(lz));

    for (S32 i = 0; i < n; i++) {
        long double dot = (long double)lx * src[i].X + (long double)ly * src[i].Y + (long double)lz * src[i].Z;
        S32 value = (S32)lrintl(dot);
        dst[i] = (value < 0) ? 0 : (U16)value;
    }
}

Func_LightList *LightListPtr = LightList;
