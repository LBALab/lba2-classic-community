
#include <3D/ROTRALIS.H>

#include <SYSTEM/UTILS.H>

/*
;void	RotTransListF(MTYPE_MAT *Mat, S32 *dst, S32 *src, S32 n)	;

;#pragma aux RotTransListF			\
;	parm 		[ebx] [edi] [esi] [ecx]	\
;	modify exact	[ecx esi edi]
*/

void RotTransListF(TYPE_MAT *Mat, TYPE_VT16 *Dst, TYPE_VT16 *Src,
									 S32 NbPoints)
{
	for (S32 i = 0; i < NbPoints; i++)
	{
		Dst[i].X = ROUND(Src[i].X * Mat->F.M11 + Src[i].Y * Mat->F.M12 + Src[i].Z * Mat->F.M13 + Mat->F.TX);
		Dst[i].Y = ROUND(Src[i].X * Mat->F.M21 + Src[i].Y * Mat->F.M22 + Src[i].Z * Mat->F.M23 + Mat->F.TY);
		Dst[i].Z = ROUND(Src[i].X * Mat->F.M31 + Src[i].Y * Mat->F.M32 + Src[i].Z * Mat->F.M33 + Mat->F.TZ);
	}
}

// -----------------------------------------------------------------------------
Func_RotTransList *RotTransList = RotTransListF;

