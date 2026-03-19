#include <3D/PROJ.H>
#include <3D/CAMERA.H>
#include <3D/LPROJ.H>
#include <stdio.h>
#include <SYSTEM/DEBUG_TRACES.H>

void SetProjection(S32 xcentre, S32 ycentre, S32 clip, S32 factorx,
                   S32 factory) {
    LIB386_TRACE_CPP("SetProjection", "1", "xc=%d yc=%d clip=%d fx=%d fy=%d", xcentre, ycentre, clip, factorx, factory);
    XCentre = xcentre;
    YCentre = ycentre;

    NearClip = clip;

    LFactorX = factorx;
    LFactorY = factory;

    TypeProj = TYPE_3D;

    LongProjectPoint = LongProjectPoint3D;
    ProjectList = ProjectList3D;

    FRatioX = (float)LFactorX;
    FRatioY = -(float)LFactorY / FRatioX;
}

void SetIsoProjection(S32 xcentre, S32 ycentre) {
    /*
;void	SetIsoProjection(xc, yc)

;#pragma aux SetIsoProjection	"*"	\
;	parm caller [eax] [ebx]		\
;	modify	[ecx]

SetIsoProjection PROC \
			uses eax ebx ecx edx edi esi ebp\
			xc: DWORD, yc: DWORD
			mov eax, xc
			mov ebx, yc

		mov	[XCentre], eax
		mov	[YCentre], ebx

		mov	[TypeProj], TYPE_ISO

		mov	eax, offset LongProjectPointIso
		mov	ebx, offset ProjectListIso
		mov	LongProjectPoint, eax
		mov	ProjectList, ebx

		ret

SetIsoProjection ENDP
	*/
    LIB386_TRACE_CPP("SetIsoProjection", "1", "xc=%d yc=%d", xcentre, ycentre);
    XCentre = xcentre;
    YCentre = ycentre;

    TypeProj = TYPE_ISO;

    LongProjectPoint = LongProjectPointIso;
    ProjectList = ProjectListIso;
}
