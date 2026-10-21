#include <3D/IMATSTD.H>

#include <3D/INT14.H>
#include <3D/SINTAB.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Port of IMATSTDI.ASM, the 1997 integer path's InitMatrixStd: the camera
 * matrix in 2.14 fixed point from SinTab/CosTab, M = M(Alpha) * M(Gamma) *
 * M(Beta) as in IMATSTDF.ASM. Each product is rounded as soon as it is made,
 * including the two shared terms sin(g)*cos(a) and sin(g)*sin(a), so the
 * result is the ASM's to the bit, not the float matrix rounded.
 */
void InitMatrixStdI(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma) {
    alpha &= 4095;
    beta &= 4095;
    gamma &= 4095;

    const S32 sinA = SinTab[alpha];
    const S32 cosA = CosTab[alpha];
    const S32 sinB = SinTab[beta];
    const S32 cosB = CosTab[beta];
    const S32 sinG = SinTab[gamma];
    const S32 cosG = CosTab[gamma];

    const S32 sinGcosA = Int14_Round(sinG * cosA);
    const S32 sinGsinA = Int14_Round(sinG * sinA);

    MatDst->I.M11 = Int14_Round(cosB * cosG);
    MatDst->I.M12 = -sinG;
    MatDst->I.M13 = Int14_Round(cosG * sinB);

    MatDst->I.M21 = Int14_Round(sinB * sinA + sinGcosA * cosB);
    MatDst->I.M22 = Int14_Round(cosG * cosA);
    MatDst->I.M23 = Int14_Round(sinB * sinGcosA - sinA * cosB);

    MatDst->I.M31 = Int14_Round(sinGsinA * cosB - cosA * sinB);
    MatDst->I.M32 = Int14_Round(cosG * sinA);
    MatDst->I.M33 = Int14_Round(sinB * sinGsinA + cosA * cosB);

    MatDst->I.TX = 0;
    MatDst->I.TY = 0;
    MatDst->I.TZ = 0;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
