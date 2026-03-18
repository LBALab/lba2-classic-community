;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C


;*══════════════════════════════════════════════════════════════════════════*
		.DATA

		extrn	C MatrixLib2:DWORD
		extrn	C InitMatrixStd:DWORD
		extrn	C LongRotatePoint:DWORD
		extrn	C printf:NEAR

fmt_rotvect_1	DB	"[ASM][RotateVector][1] norme=%d a=%d b=%d g=%d", 10, 0

		ASSUME  DS:SEG LongRotatePoint

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	RotateVector

;*══════════════════════════════════════════════════════════════════════════*

;*══════════════════════════════════════════════════════════════════════════*
;void RotateVector(S32 norme, S32 alpha, S32 beta, S32 gamma)	;

;#pragma aux RotateVector			\
;	parm		[edx] [eax] [ebx] [ecx]	\
;	modify		[esi edi]

RotateVector	proc

		; --- debug trace [ASM][RotateVector][1] params ---
		pushad
		push	ecx
		push	ebx
		push	eax
		push	edx
		push	offset fmt_rotvect_1
		call	printf
		add	esp, 20
		popad
		; --- end debug trace ---

		mov	edi, offset MatrixLib2
		push	edx

		call	[InitMatrixStd]

		pop	ecx
		xor	eax, eax
		xor	ebx, ebx
		mov	esi, offset MatrixLib2

		jmp	[LongRotatePoint]

RotateVector	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

