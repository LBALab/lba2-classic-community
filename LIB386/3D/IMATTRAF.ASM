;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

		public	C	InitMatrixTrans

		InitMatrixTrans	dd	offset InitMatrixTransF

		extrn	C	printf:NEAR

fmt_imattra_1	DB	"[ASM][InitMatrixTransF][1] Dst=%p tx=%d ty=%d tz=%d", 10, 0

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	InitMatrixTransF

;*══════════════════════════════════════════════════════════════════════════*
;void	InitMatrixTransF(S32 *MatDst, S32 tx, S32 ty, S32 tz) ;

;#pragma aux InitMatrixTransF	"*"		\
;	parm		[edi] [eax] [ebx] [ecx]	\
;	modify exact	[]

InitMatrixTransF	PROC

		; --- debug trace [ASM][InitMatrixTransF][1] params ---
		pushad
		push	ecx
		push	ebx
		push	eax
		push	edi
		push	offset fmt_imattra_1
		call	printf
		add	esp, 20
		popad
		; --- end debug trace ---

		push	eax
		push	ebx

		push	ecx

		fild	dword ptr[esp]		; Z

		fild	dword ptr[esp+4]	; Y Z

		fild	dword ptr[esp+8]	; X Y Z

		add	esp, 4*3

		fstp	[edi].Struc_MatriceMAT.MAT_MTX

		fstp	[edi].Struc_MatriceMAT.MAT_MTY

		fstp	[edi].Struc_MatriceMAT.MAT_MTZ

		ret

InitMatrixTransF	ENDP

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
