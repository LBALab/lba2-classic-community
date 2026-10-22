
#include <3D/ROTRALIS.H>

#include <3D/X87REAL.H>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * Batch rotate+translate matching ASM RotTransListF.
 * X87_REAL + X87Round match the x87 chain and its fistp (3D/X87REAL.H).
 */
void RotTransListF(TYPE_MAT *Mat, TYPE_VT16 *Dst, TYPE_VT16 *Src,
                   S32 NbPoints) {
    LIB386_TRACE_CPP("RotTransListF", "1", "Mat=%p dst=%p src=%p n=%d", (void *)Mat, (void *)Dst, (void *)Src, NbPoints);
    for (S32 i = 0; i < NbPoints; i++) {
        X87_REAL x = (X87_REAL)Src[i].X;
        X87_REAL y = (X87_REAL)Src[i].Y;
        X87_REAL z = (X87_REAL)Src[i].Z;

        Dst[i].X = (S16)X87Round(x * Mat->F.M11 + y * Mat->F.M12 + z * Mat->F.M13 + (X87_REAL)Mat->F.TX);
        Dst[i].Y = (S16)X87Round(x * Mat->F.M21 + y * Mat->F.M22 + z * Mat->F.M23 + (X87_REAL)Mat->F.TY);
        Dst[i].Z = (S16)X87Round(x * Mat->F.M31 + y * Mat->F.M32 + z * Mat->F.M33 + (X87_REAL)Mat->F.TZ);
    }
}

// -----------------------------------------------------------------------------
Func_RotTransList *RotTransList = RotTransListF;
