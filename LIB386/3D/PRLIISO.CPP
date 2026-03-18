#include <3D/LPROJ.H>

#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>

#include <stdio.h>
#include <SYSTEM/DEBUG_TRACES.H>

void ProjectListIso(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX,
								S32 OrgY, S32 OrgZ)
{
	LIB386_TRACE_CPP("ProjectListIso", "1", "Dst=%p Src=%p n=%d", (void*)Dst, (void*)Src, NbPt);
	for (int i = 0; i < NbPt; i++)
	{
		TYPE_VT16 *v = &Src[i];

		S16 x = v->X - OrgX;
		S16 y = v->Y - OrgY;
		S16 z = v->Z - OrgZ;

		v->Z = (v->X + v->Z) >> 1;

		S32 newX = XCentre + (((x - z) * 3) >> 6); // IsoScale
		S32 newY = YCentre + 1 + (((x + z) * 6 - y * 15) >> 8); // IsoScale

		if (newX > 32767 || newX < -32767 || newY > 32767 || newY < -32767)
		{
			TYPE_PT *p = &Dst[i];
			p->X = 0x8000;
			p->Y = 0x8000;
			continue;
		}

		if (newX < ScreenXMin)
		{
			ScreenXMin = newX;
		}
		if (newX > ScreenXMax)
		{
			ScreenXMax = newX;
		}

		if (newY < ScreenYMin)
		{
			ScreenYMin = newY;
		}
		if (newY > ScreenYMax)
		{
			ScreenYMax = newY;
		}

		TYPE_PT *p = &Dst[i];
		p->X = newX;
		p->Y = newY;
	}
}
