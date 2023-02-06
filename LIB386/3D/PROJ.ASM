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
		extrn   C LongProjectPointInit:           PROC
		extrn   C LongProjectPointIso:		PROC
		extrn   C ProjectListInit:		PROC
		extrn   C ProjectListIso:			PROC

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

		mov	[LFactorX], edx
		mov	[LFactorY], esi

		mov	edx, [ProcessorSignature]

		and	edx, 400h
		jz	noFPU

		test	[ProcessorFeatureFlags], 1 SHL 23
		jz	noMMX

		emms
noMMX:
		fild	[LFactorX]	; fx
		fild	[LFactorY]	; fy fx
		fxch	st(1)		; fx fy
		fst	[FRatioX]	; fx fy
		fchs			;-fx fy
		fdivp	st(1), st(0)	; -fy/fx
noFPU:
		mov	[XCentre], eax
		mov	[YCentre], ebx

		mov	[NearClip], ecx
		mov	[TypeProj], TYPE_3D

		mov	eax, offset LongProjectPointInit
		mov	ebx, offset ProjectListInit
		mov	LongProjectPoint, eax
		mov	ProjectList, ebx

		test	edx, edx
		jz	noFPU2

		fstp	[FRatioY]
noFPU2:
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
