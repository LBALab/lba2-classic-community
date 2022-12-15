;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		public	C InitMatrix
InitMatrix	dd	offset InitMatrixInitStd

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	InitMatrixStd:DWORD
		extrn	C	InitMatrix3DS:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	MatrixStd
		public	C	Matrix3DS

;*══════════════════════════════════════════════════════════════════════════*
InitMatrixInitStd proc

		call	[ds:InitMatrixStd]

		push	eax
		mov	eax, [ds:InitMatrixStd]

		mov	[ds:InitMatrix], eax
		pop	eax

		ret

InitMatrixInitStd endp

;*══════════════════════════════════════════════════════════════════════════*
InitMatrixInit3DS proc

		call	[ds:InitMatrix3DS]

		push	eax
		mov	eax, [ds:InitMatrix3DS]

		mov	[ds:InitMatrix], eax
		pop	eax

		ret

InitMatrixInit3DS endp

;*══════════════════════════════════════════════════════════════════════════*
;extern  void    MatrixS3td();

;#pragma aux MatrixStd   "*"     \
;        modify exact []

MatrixStd	proc

		mov	[ds:InitMatrix], offset InitMatrixInitStd
		ret

MatrixStd	endp

;*══════════════════════════════════════════════════════════════════════════*
;extern  void    Matrix3DS();

;#pragma aux Matrix3DS   "*"     \
;        modify exact []

Matrix3DS	proc

		mov	[ds:InitMatrix], offset InitMatrixInitStd
		ret

Matrix3DS	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
