#include <3D/LIROT3D.H>

#include <3D/CAMERA.H>
#include <3D/INT14.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Port of LIROT3DI.ASM, the 1997 integer path's LongInverseRotatePoint: x, y, z
 * through the transpose of the 2.14 matrix that InitMatrixStdI builds, each
 * column summed in 64 bits and rounded at bit 13, as LongRotatePointI does its
 * rows. Writes X0, Y0, Z0. SetFollowCamera uses it to place the integer camera.
 */
void LongInverseRotatePointI(TYPE_MAT *Mat, S32 x, S32 y, S32 z) {
    const S64 rx = (S64)Mat->I.M11 * x + (S64)Mat->I.M21 * y + (S64)Mat->I.M31 * z;
    const S64 ry = (S64)Mat->I.M12 * x + (S64)Mat->I.M22 * y + (S64)Mat->I.M32 * z;
    const S64 rz = (S64)Mat->I.M13 * x + (S64)Mat->I.M23 * y + (S64)Mat->I.M33 * z;

    X0 = Int14_Shr14Round(rx);
    Y0 = Int14_Shr14Round(ry);
    Z0 = Int14_Shr14Round(rz);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
