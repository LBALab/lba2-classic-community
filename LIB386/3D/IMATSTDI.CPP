#include <3D/IMATSTD.H>

#include <3D/SINTAB.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// A 2.14 product back to 2.14, rounded half up: the ASM's sar 14 then adc 0,
// or lea +(1 SHL 13) then sar 14, which give the same value.
static inline S32 Round14(S32 v) {
    return (v + (1 << 13)) >> 14;
}

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

    const S32 sinGcosA = Round14(sinG * cosA);
    const S32 sinGsinA = Round14(sinG * sinA);

    MatDst->I.M11 = Round14(cosB * cosG);
    MatDst->I.M12 = -sinG;
    MatDst->I.M13 = Round14(cosG * sinB);

    MatDst->I.M21 = Round14(sinB * sinA + sinGcosA * cosB);
    MatDst->I.M22 = Round14(cosG * cosA);
    MatDst->I.M23 = Round14(sinB * sinGcosA - sinA * cosB);

    MatDst->I.M31 = Round14(sinGsinA * cosB - cosA * sinB);
    MatDst->I.M32 = Round14(cosG * sinA);
    MatDst->I.M33 = Round14(sinB * sinGsinA + cosA * cosB);

    MatDst->I.TX = 0;
    MatDst->I.TY = 0;
    MatDst->I.TZ = 0;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
