;*══════════════════════════════════════════════════════════════════════════*

		.386p
		OPTION	LANGUAGE:C
		OPTION	SCOPED

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*
;DGROUP		GROUP	_DATA

;_DATA		SEGMENT USE32 PAGE PUBLIC 'DATA'

		.model SMALL, C
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	MatriceWorld:DWORD

		extrn	C	InitMatrix:DWORD
		extrn	C	LongRotatePoint:DWORD
		extrn	C	LongInverseRotatePoint:DWORD

;*══════════════════════════════════════════════════════════════════════════*
;ALIGN	32					; already aligned
		public	C Xp
		public	C Yp

Xp		dd	0000
Yp		dd	0000

		public	C XCentre
		public	C YCentre

XCentre		dd	0000
YCentre		dd	0000

		public	C X0
		public	C Y0
		public	C Z0

X0		dd	0000
Y0		dd	0000
Z0		dd	0000

		public	C NearClip
NearClip	dd	1

;ALIGN	32					; already aligned
		public	C LFactorX
		public	C LFactorY

LFactorX	dd	0000
LFactorY	dd	0000

		public	C FRatioX
		public	C FRatioY

FRatioX		dd	0000
FRatioY		dd	0000

		public	C CameraXr
		public	C CameraYr
		public	C CameraZr
CameraXr	dd	0000
CameraYr	dd	0000
CameraZr	dd	0000

		public	C TypeProj
TypeProj	dd	0000

;ALIGN	32					; already aligned
		public	C CameraX
		public	C CameraY
		public	C CameraZ
CameraX		dd	0000
CameraY		dd	0000
CameraZ		dd	0000

		public	C CameraZrClip
CameraZrClip	dd	0000

		public	C CameraAlpha
		public	C CameraBeta
		public	C CameraGamma

CameraAlpha	dd	0000
CameraBeta	dd	0000
CameraGamma	dd	0000

		; One DWORD Available HERE
;ALIGN	32
		public	C MMX_DEMI
MMX_DEMI	dd	1 SHL 13
		dd	1 SHL 13

		public	C MMX_DEMI2
MMX_DEMI2	dd	1 SHL 12
		dd	1 SHL 12

;*══════════════════════════════════════════════════════════════════════════*
_DATA		ENDS

;*══════════════════════════════════════════════════════════════════════════*
.CODE
;_TEXT		SEGMENT	USE32 PARA PUBLIC 'CODE'
       		ASSUME	CS:FLAT, DS:FLAT, ES:FLAT, SS:FLAT

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	GetAngleVector3D:PROC
		extrn	C	RecomputeLight:PROC

;*══════════════════════════════════════════════════════════════════════════*
		public	C	SetTargetCamera
		public	C	SetAngleCamera
		public	C	SetPosCamera
		public	C	SetFollowCamera

;*══════════════════════════════════════════════════════════════════════════*
;		public	C	SetCamera	; for use in LIB ONLY

;*══════════════════════════════════════════════════════════════════════════*
;void	InitCamera(	S32 camx, S32 camy, S32 camz,
;			S32 tgtx, S32 tgty, S32 tgtz,
;			S32 gamma)			;

;#pragma aux InitCamera	"*"					\
;	parm		[edx] [esi] [edi] [eax] [ecx] [ebx]

; WARNING: Gamma goes ON THE STACK

InitCamera	PROC \
			uses eax ebx ecx edx edi esi ebp\
			camx: DWORD, camy: DWORD, camz: DWORD, \
			tgtx: DWORD, tgty: DWORD, tgtz: DWORD, \
			gamma: DWORD

			mov edx, camx
			mov esi, camy
			mov edi, camz
			mov eax, tgtx
			mov ecx, tgty
			mov ebx, tgtz

		mov	CameraX, edx
		mov	CameraY, esi
		mov	CameraZ, edi

		mov edx, gamma

		push tgtx
		push tgty
		push tgtz
		push gamma
		call SetTargetCamera
		add esp, 4*4

		ret

;		WARNING continue below

InitCamera endp

;*══════════════════════════════════════════════════════════════════════════*
;void	SetTargetCamera(S32 x, S32 y, S32 z, S32 gamma)	;

;#pragma aux SetTargetCamera	"*"		\
;	parm		[eax] [ecx] [ebx] [edx]	\

;	modify		[esi edi]

SetTargetCamera PROC \
			uses eax ebx ecx edx edi esi ebp\
			x: DWORD, y: DWORD, z: DWORD, gamma: DWORD
			mov eax, x
			mov ecx, y
			mov ebx, z
			mov edx, gamma

		push	edx

		sub	ecx, CameraZ
		sub	ebx, CameraY
		sub	eax, CameraX

		call	GetAngleVector3D

		mov	eax, [X0]
		mov	ebx, [Y0]
		pop	ecx

;		WARNING continue below

		push ecx
		push ebx
		push eax

		call SetAngleCamera
		add esp, 3*4
		ret

SetTargetCamera endp

;*══════════════════════════════════════════════════════════════════════════*
;void	SetAngleCamera(S32 alpha, S32 beta, S32 gamma)	;

;#pragma aux SetAngleCamera	"*"		\
;	parm		[eax] [ebx] [ecx]	\
;	modify		[edx esi edi]

SetAngleCamera PROC \
			uses eax ebx ecx edx edi esi ebp\
			alpha: DWORD, beta: DWORD, gamma: DWORD
			mov eax, alpha
			mov ebx, beta
			mov ecx, gamma

		cmp	[TypeProj], TYPE_3D
		jne	tiso

		and	eax, 4095
		and	ebx, 4095
		and	ecx, 4095
tback:
		mov	edi, offset MatriceWorld

		mov	CameraAlpha, eax
		mov	CameraBeta, ebx

		mov	CameraGamma, ecx
		call	[InitMatrix]

		call	RecomputeLight

		mov     eax, CameraX
		mov     ebx, CameraY
		mov     ecx, CameraZ

				; for use in lib
						; see SetPosCamera
		cmp	[TypeProj], TYPE_3D
		jne	iso

		mov	esi, offset MatriceWorld

		call    [LongRotatePoint]

		mov     eax, X0
		mov     ebx, Y0
		mov     ecx, Z0

		mov     CameraXr, eax
		mov     edx, ecx
		mov     CameraYr, ebx
		sub	edx, NearClip
		mov     CameraZr, ecx
		mov     CameraZrClip, edx

		ret
iso:
		mov	CameraXr, eax
		xor	eax, eax
		mov	CameraYr, ebx
		mov	CameraZr, ecx
		mov	CameraZrClip, ecx
		mov	NearClip, eax

		ret
tiso:
		xor	eax, eax
		xor	ebx, ebx
		xor	ecx, ecx
		jmp	tback

SetAngleCamera	endp

;*══════════════════════════════════════════════════════════════════════════*
;void	SetPosCamera(S32 x, S32 y, S32 z)	;

;#pragma aux SetAngleCamera	"*"		\
;	parm		[eax] [ebx] [ecx]	\
;	modify		[edx esi edi]

SetPosCamera	PROC \
			uses eax ebx ecx edx edi esi ebp\
			x: DWORD, y: DWORD, z: DWORD
			mov eax, x
			mov ebx, y
			mov ecx, z

		mov	CameraX, eax
		mov	CameraY, ebx
		mov	CameraZ, ecx

		cmp	[TypeProj], TYPE_3D
		jne	iso

		mov	esi, offset MatriceWorld

		call    [LongRotatePoint]

		mov     eax, X0
		mov     ebx, Y0
		mov     ecx, Z0

		mov     CameraXr, eax
		mov     edx, ecx
		mov     CameraYr, ebx
		sub	edx, NearClip
		mov     CameraZr, ecx
		mov     CameraZrClip, edx

		ret
iso:
		mov	CameraXr, eax
		xor	eax, eax
		mov	CameraYr, ebx
		mov	CameraZr, ecx
		mov	CameraZrClip, ecx
		mov	NearClip, eax

		ret

SetPosCamera	endp

;*══════════════════════════════════════════════════════════════════════════*
;void	SetFollowCamera(S32 targetx, S32 targety, S32 targetz,
;			S32 alpha, S32 beta, S32 gamma,
;			S32 camzoom)	;

;#pragma aux SetAngleCamera	"*"			\
;	parm		[edx] [esi] [edi] [eax] [ebx] [ecx]

; WARNING : last param "camzoom" passed on the stack!

SetFollowCamera	PROC \
			uses eax ebx ecx edx edi esi ebp\
			targetx: DWORD, targety: DWORD, targetz: DWORD, alpha: DWORD, beta: DWORD, gamma: DWORD, camzoom: DWORD
			
			mov edx, targetx
			mov esi, targety
			mov edi, targetz
			mov eax, alpha
			mov ebx, beta
			mov ecx, gamma

		mov     [CameraX], edx
		mov     [CameraY], esi
		mov     [CameraZ], edi

		push ecx
		push ebx
		push eax

		call	SetAngleCamera

		add esp, 4*3

		cmp	[TypeProj], TYPE_3D
		jne	iso

		mov	esi, camzoom
		mov	eax, [CameraXr]
		mov	ebx, [CameraYr]
		mov	ecx, [CameraZr]
		mov	edx, [CameraZrClip]
		add	ecx, esi
		add	edx, esi
		mov	[CameraZr], ecx
		mov	[CameraZrClip], edx

		mov	esi, offset MatriceWorld

		call	[LongInverseRotatePoint]

		mov	eax, [X0]
		mov	ebx, [Y0]
		mov	ecx, [Z0]
		mov	[CameraX], eax
		mov	[CameraY], ebx
		mov	[CameraZ], ecx
iso:
		ret

SetFollowCamera	endp

;*══════════════════════════════════════════════════════════════════════════*
_TEXT		ENDS

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
