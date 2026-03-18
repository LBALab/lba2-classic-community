#include <3D/LPROJ.H>

#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>

#include <string.h>
#include <stdio.h>
#include <SYSTEM/DEBUG_TRACES.H>

void ProjectListIso(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX,
								S32 OrgY, S32 OrgZ)
{
	LIB386_TRACE_CPP("ProjectListIso", "1", "Dst=%p Src=%p n=%d", (void*)Dst, (void*)Src, NbPt);
	/* The original ASM walks the packed TYPE_VT16 array in place and assumes
	   NbPt > 0. Tests intentionally keep to that valid ASM input domain. */
	for (int i = 0; i < NbPt; i++)
	{
		TYPE_VT16 *v = &Src[i];

		/* The ASM sign-extends X/Y/Z to 32-bit registers immediately and keeps
		   the rest of the projection arithmetic in 32-bit integer ops. Using
		   16-bit temporaries here changes overflow and shift behavior. */
		S32 originalX = (S32)v->X;
		S32 originalY = (S32)v->Y;
		S32 originalZ = (S32)v->Z;
		S32 x = originalX - OrgX;
		S32 y = originalY - OrgY;
		S32 z = originalZ - OrgZ;
		S32 sortZ = (originalX + originalZ) >> 1;

		/* The ASM does a 32-bit store at the Z field offset:
		   mov [vertex+4], edx
		   Since TYPE_VT16 is packed as X,Y,Z,Grp, this intentionally overwrites
		   both Z and Grp with the computed sort key. Keep the full-width write to
		   preserve byte-for-byte behavior. */
		memcpy(&v->Z, &sortZ, sizeof(sortZ));

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
