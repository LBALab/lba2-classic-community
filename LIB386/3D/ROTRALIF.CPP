
#include <3D/ROTRALIS.H>

#include <math.h>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * Batch rotate+translate matching ASM RotTransListF.
 * Uses double + lrint to match x87 fistp round-to-nearest-even.
 */
void RotTransListF(TYPE_MAT *Mat, TYPE_VT16 *Dst, TYPE_VT16 *Src,
                   S32 NbPoints) {
    LIB386_TRACE_CPP("RotTransListF", "1", "Mat=%p dst=%p src=%p n=%d", (void *)Mat, (void *)Dst, (void *)Src, NbPoints);
    for (S32 i = 0; i < NbPoints; i++) {
        double x = (double)Src[i].X;
        double y = (double)Src[i].Y;
        double z = (double)Src[i].Z;

        Dst[i].X = (S16)lrint(x * Mat->F.M11 + y * Mat->F.M12 + z * Mat->F.M13 + (double)Mat->F.TX);
        Dst[i].Y = (S16)lrint(x * Mat->F.M21 + y * Mat->F.M22 + z * Mat->F.M23 + (double)Mat->F.TY);
        Dst[i].Z = (S16)lrint(x * Mat->F.M31 + y * Mat->F.M32 + z * Mat->F.M33 + (double)Mat->F.TZ);
    }
}

// -----------------------------------------------------------------------------
Func_RotTransList *RotTransList = RotTransListF;
