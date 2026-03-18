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

		extrn	C	printf:NEAR

fmt_lprojiso_1	DB	"[ASM][LongProjectPointIso][1] x=%d y=%d z=%d", 10, 0
fmt_lprojiso_2	DB	"[ASM][LongProjectPointIso][2] ret=%d Xp=%d Yp=%d", 10, 0

		ASSUME  DS:SEG CameraX

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	LongProjectPointIso

;*══════════════════════════════════════════════════════════════════════════*
;void	LongProjectPointIso(S32 x, S32 y, S32 z)		;

;#pragma aux LongProjectPointIso		\
;	parm		[eax] [ebx] [ecx]	\
;	modify		[edx]

LongProjectPointIso PROC 

		; --- debug trace [ASM][LongProjectPointIso][1] params ---
		pushad
		push	ecx
		push	ebx
		push	eax
		push	offset fmt_lprojiso_1
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---

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

		; --- debug trace [ASM][LongProjectPointIso][2] return ---
		pushad
		push	dword ptr [Yp]
		push	dword ptr [Xp]
		push	eax
		push	offset fmt_lprojiso_2
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---

		ret

LongProjectPointIso endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
