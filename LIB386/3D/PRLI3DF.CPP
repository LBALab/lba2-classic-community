#include <3D/LPROJ.H>

#include <math.h>
#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * Batch perspective projection matching the ASM ProjectList3DF.
 */
void ProjectList3DF(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt,
                    S32 OrgX, S32 OrgY, S32 OrgZ)
{
	LIB386_TRACE_CPP("ProjectList3DF", "1", "Dst=%p Src=%p n=%d", (void*)Dst, (void*)Src, NbPt);
	for (S32 i = 0; i < NbPt; i++)
	{
		S32 z = OrgZ - (S32)Src[i].Z;

		if (z < NearClip)
		{
			Dst[i].X = -32768;
			Dst[i].Y = -32768;
			continue;
		}

		S32 x = (S32)Src[i].X - OrgX;
		S32 y = (S32)Src[i].Y - OrgY;

		X0 = x;
		Y0 = y;
		Z0 = z;

		long double factor = (long double)FRatioX / (long double)z;

		S32 Xe = (S32)lrintl((long double)x * factor);
		S32 Ye = (S32)lrintl((long double)y * factor * (long double)FRatioY);

		Xp = XCentre + Xe;
		Yp = YCentre + Ye;

		if (Xp > 32767 || Xp < -32767 || Yp > 32767 || Yp < -32767)
		{
			Dst[i].X = -32768;
			Dst[i].Y = -32768;
			continue;
		}

		if (Xp < ScreenXMin) ScreenXMin = Xp;
		if (Xp > ScreenXMax) ScreenXMax = Xp;
		if (Yp < ScreenYMin) ScreenYMin = Yp;
		if (Yp > ScreenYMax) ScreenYMax = Yp;

		Dst[i].X = (S16)Xp;
		Dst[i].Y = (S16)Yp;
	}
}

Func_ProjectList *ProjectList3D = ProjectList3DF;
Func_ProjectList *ProjectList = ProjectList3DF;
