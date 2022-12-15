;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C CameraX:DWORD
		extrn	C CameraY:DWORD
		extrn	C CameraZ:DWORD

		extrn	C XCentre:DWORD
		extrn	C YCentre:DWORD

		extrn	C	Xp:DWORD
		extrn	C	Yp:DWORD

		ASSUME  DS:SEG CameraX

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	LongProjectPointIso
		public	C	LongWorldRotateProjectPointIso

;*══════════════════════════════════════════════════════════════════════════*
;void	LongProjectPointIso(S32 x, S32 y, S32 z)		;

;#pragma aux LongProjectPointIso		\
;	parm		[eax] [ebx] [ecx]	\
;	modify		[edx]

LongProjectPointIso PROC 


LongWorldRotateProjectPointIso::

		sub	eax, CameraX
		sub	ebx, CameraY

		mov	edx, eax		; save x
		sub	ecx, CameraZ

		sub	eax, ecx		; x + zrot
		add	ecx, edx		; -(x - zrot)

		mov	edx, ebx		; save y
		add	ecx, ecx		; *2

		shl	ebx, 4			; *16
		lea	eax, [eax+eax*2]	; *3

		sub	ebx, edx		; y*15
		lea	ecx, [ecx+ecx*2]	; *3

		sar	eax, 6			; /64 IsoScale
		mov	edx, [XCentre]

		add	eax, edx
		sub	ecx, ebx		; - (x-zrot) * 6

		sar	ecx, 8			; /256 IsoScale
		mov	edx, [YCentre]

		mov	[Xp], eax
		lea	ecx, [ecx+edx+1]

		mov	eax, -1
		mov	[Yp], ecx

		ret

LongProjectPointIso endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
