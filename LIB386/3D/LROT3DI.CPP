#include <3D/LROT3D.H>

#include <3D/CAMERA.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// A 64-bit sum of 2.14 products back to an integer: the ASM's shrd 14 then
// adc 0, which adds the last bit shifted out (bit 13) and keeps the low 32 bits.
static inline S32 Shr14Round(S64 v) {
    return (S32)(U32)((U64)(v >> 14) + (U64)((v >> 13) & 1));
}

/*
 * Port of LROT3DI.ASM, the 1997 integer path's LongRotatePoint: x, y, z
 * through the 2.14 matrix that InitMatrixStdI builds, each row summed in 64
 * bits and rounded at bit 13. Writes X0, Y0, Z0.
 */
void LongRotatePointI(TYPE_MAT *Mat, S32 x, S32 y, S32 z) {
    const S64 rx = (S64)Mat->I.M11 * x + (S64)Mat->I.M12 * y + (S64)Mat->I.M13 * z;
    const S64 ry = (S64)Mat->I.M21 * x + (S64)Mat->I.M22 * y + (S64)Mat->I.M23 * z;
    const S64 rz = (S64)Mat->I.M31 * x + (S64)Mat->I.M32 * y + (S64)Mat->I.M33 * z;

    X0 = Shr14Round(rx);
    Y0 = Shr14Round(ry);
    Z0 = Shr14Round(rz);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
