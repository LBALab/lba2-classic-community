;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

		include CPU.INC

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	ProcessorSignature:DWORD
		extrn	C	ProcessorFeatureFlags:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		public	C	Chooser_Result

Chooser_Result	dd	0	; Result

		public	C	Chooser_RtnMF
		public	C	Chooser_RtnM
		public	C	Chooser_RtnF
		public	C	Chooser_RtnI

Chooser_RtnMF	dd	0	; MMX and FPU
Chooser_RtnM	dd	0	; MMX
Chooser_RtnF	dd	0	; FPU
Chooser_RtnI	dd	0	; Integer

		ASSUME DS:SEG Chooser_RtnI

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		public	C	Chooser

;*══════════════════════════════════════════════════════════════════════════*
Chooser		proc

		pushad

		mov	ebx, [ProcessorSignature]
		mov	ecx, [ds:Chooser_Result]

		shr	ebx, 8				; remove model/stepping
		mov	eax, [ProcessorFeatureFlags]

		and	ebx, 0F4Fh			; keep manufact/FPU/family
		xor	esi, esi			; esi = 0

		cmp	bl, 45h				; 4 = FPU; 5 = Pentium
		;yaz jb 	noFPU				; FPU+Pentium or better?

		test	eax, 1 SHL 23			; MMX flag ?
		jz	noMMX

		cmp	bh, 1				; Manufacturer should be
		jne	noMMXFPU			; Intel to use MMX+FPU

		mov	edx, [ds:Chooser_RtnMF]		; try MMX+FPU

		test	edx, edx			; MMX Rtn exist?
		jnz	ok
noMMXFPU:
		mov	edx, [ds:Chooser_RtnM]		; try MMX

		test	edx, edx			; MMX Rtn exist?
		jnz	ok
noMMX:
		cmp	bh, 1				; Manufacturer should be
		;yaz jne	noFPU				; Intel to use FPU

		mov	edx, [ds:Chooser_RtnF]		; try FPU

		test	edx, edx
		jnz	ok				; FPU Rtn exist?
noFPU:
		mov	edx, [ds:Chooser_RtnI]		; default to integer
ok:
		mov	[ds:Chooser_RtnMF], esi		; 0
		mov	[ds:Chooser_RtnM], esi		; 0

		mov	[ds:Chooser_RtnF], esi		; 0
		mov	[ds:Chooser_RtnI], esi		; 0

		mov	[ds:Chooser_Result], edx
		mov	[ecx], edx

		popad

		jmp	[ds:Chooser_Result]

Chooser		endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
