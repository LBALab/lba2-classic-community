;*══════════════════════════════════════════════════════════════════════════*
		.586p
		.MMX

		.model SMALL, C

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

		extrn	C	printf:NEAR

fmt_setproj_1	DB	"[ASM][SetProjection][1] xc=%d yc=%d clip=%d fx=%d fy=%d", 10, 0
fmt_setisop_1	DB	"[ASM][SetIsoProjection][1] xc=%d yc=%d", 10, 0

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

		 ASSUME DS:SEG XCentre

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
;void	SetProjection(xc, yc, clip, factorx, factory)

;#pragma aux SetProjection	"*"			\
;	parm caller [eax] [ebx] [ecx] [edx] [esi]	\
;	modify exact [eax ebx ecx edx]

SetProjection	PROC \
			uses eax ebx ecx edx edi esi ebp\
			xc: DWORD, yc: DWORD, clip:DWORD, factorx: DWORD, factory: DWORD
			mov eax, xc
			mov ebx, yc
			mov ecx, clip
			mov edx, factorx
			mov esi, factory

		; --- debug trace [ASM][SetProjection][1] params ---
		pushad
		push	esi
		push	edx
		push	ecx
		push	ebx
		push	eax
		push	offset fmt_setproj_1
		call	printf
		add	esp, 24
		popad
		; --- end debug trace ---

		mov	[LFactorX], edx
		mov	[LFactorY], esi

		fild	[LFactorX]	; fx
		fild	[LFactorY]	; fy fx
		fxch	st(1)		; fx fy
		fst	[FRatioX]	; fx fy
		fchs			;-fx fy
		fdivp	st(1), st(0)	; -fy/fx

		mov	[XCentre], eax
		mov	[YCentre], ebx

		mov	[NearClip], ecx
		mov	[TypeProj], TYPE_3D

		mov	eax, offset LongProjectPoint3DF
		mov	ebx, offset ProjectList3DF
		mov	LongProjectPoint, eax
		mov	ProjectList, ebx

		fstp	[FRatioY]

		ret

SetProjection	ENDP

;*══════════════════════════════════════════════════════════════════════════*
;void	SetIsoProjection(xc, yc)

;#pragma aux SetIsoProjection	"*"	\
;	parm caller [eax] [ebx]		\
;	modify	[ecx]

SetIsoProjection PROC \
			uses eax ebx ecx edx edi esi ebp\
			xc: DWORD, yc: DWORD
			mov eax, xc
			mov ebx, yc

		; --- debug trace [ASM][SetIsoProjection][1] params ---
		pushad
		push	ebx
		push	eax
		push	offset fmt_setisop_1
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---

		mov	[XCentre], eax
		mov	[YCentre], ebx

		mov	[TypeProj], TYPE_ISO

		mov	eax, offset LongProjectPointIso
		mov	ebx, offset ProjectListIso
		mov	LongProjectPoint, eax
		mov	ProjectList, ebx

		ret

SetIsoProjection ENDP

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
