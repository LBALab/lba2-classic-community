/*
;*══════════════════════════════════════════════════════════════════════════*
		.586p
		.MMX

		.model SMALL, C

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	ProcessorSignature:DWORD
		extrn	C	ProcessorFeatureFlags:DWORD

		 ASSUME DS:SEG ProcessorFeatureFlags

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	XCentre:	DWORD
		extrn	C	YCentre:	DWORD

		extrn	C	NearClip:	DWORD

		extrn	C	LFactorX:	DWORD
		extrn	C	LFactorY:	DWORD

		extrn	C	TypeProj:	DWORD

		extrn	C	FRatioX:	DWORD
		extrn	C	FRatioY:	DWORD

;*══════════════════════════════════════════════════════════════════════════*
		extrn   C LongProjectPoint:		DWORD
		extrn   C ProjectList:			DWORD

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn   C LongProjectPoint3DF:	PROC
		extrn   C LongProjectPointIso:	PROC
		extrn   C ProjectList3DF:		PROC
		extrn   C ProjectListIso:		PROC

;*══════════════════════════════════════════════════════════════════════════*
		public	C	SetProjection
		public	C	SetIsoProjection

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
*/

#include <3D/PROJ.H>
#include <3D/CAMERA.H>
#include <3D/LPROJ.H>
#include <stdio.h>

void SetProjection(S32 xcentre, S32 ycentre, S32 clip, S32 factorx,
                   S32 factory)
{
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

void SetIsoProjection(S32 xcentre, S32 ycentre)
{
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
	XCentre = xcentre;
	YCentre = ycentre;

	TypeProj = TYPE_ISO;

	LongProjectPoint = LongProjectPointIso;
	ProjectList = ProjectListIso;
}

