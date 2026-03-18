;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		extrn	C	InitMatrixStd:DWORD
		extrn	C	CopyMatrix:DWORD
		extrn	C	MulMatrix:DWORD

		extrn	C	MatrixLib2:DWORD
		extrn	C	printf:NEAR

fmt_rotmatu_1	DB	"[ASM][RotateMatrixU][1] Dst=%p Src=%p a=%d b=%d g=%d", 10, 0

		public	C	RotateMatrix

		RotateMatrix	dd	offset RotateMatrixU

		ASSUME  DS:SEG MatrixLib2

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	RotateMatrixU

;*══════════════════════════════════════════════════════════════════════════*
;void	RotateMatrixU(S32 *MatDst, S32 *MatSrc, S32 alpha, S32 beta, S32 gamma) ;

;#pragma aux RotateMatrixU				\
;	parm		[edi] [esi] [eax] [ebx] [ecx]
;	modify exact	[eax ebx ecx edx esi]

RotateMatrixU	proc

		; --- debug trace [ASM][RotateMatrixU][1] params ---
		pushad
		push	ecx
		push	ebx
		push	eax
		push	esi
		push	edi
		push	offset fmt_rotmatu_1
		call	printf
		add	esp, 24
		popad
		; --- end debug trace ---

		push	edi
		push	esi

		mov	edi, offset MatrixLib2	; because InitMatrixStd use MatrixLib1
		call	[InitMatrixStd]		; MatrixLib2 = Alpha * Gamma * Beta

		mov	ebx, edi
		pop	esi
		pop	edi

		jmp	[MulMatrix]		; MatDst = MatSrc * Alpha * Gamma * Beta

RotateMatrixU	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
