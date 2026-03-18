			.386P
			.model FLAT, C

;----------------------------------------------------------------------------
			.data

;----------------------------------------------------------------------------

			public	C		MySortCompFunc

;----------------------------------------------------------------------------
ifdef	REG_CALL
			extrn	C		strcmp_	:PROC
else
			extrn	C		strcmp	:PROC
endif

;----------------------------------------------------------------------------

		.DATA

		EXTRN	C	printf	:NEAR

fmt_mysortco_1	DB	"[ASM][MySortCompFunc][1] a=%p b=%p", 10, 0

			.code

;//-------------------------------------------------------------------------
ifdef	REG_CALL

;//-------------------------------------------------------------------------
MySortCompFunc 		proc


		; --- debug trace [ASM][MySortCompFunc][1] params ---
		pushad
		push	ptrb
		push	ptra
		push	offset fmt_mysortco_1
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---

					mov	eax, [eax]
			mov	edx, [edx]

			jmp	strcmp_

MySortCompFunc		endp

;//-------------------------------------------------------------------------
else

;//-------------------------------------------------------------------------
MySortCompFunc 		proc	,\
			ptra:DWORD, ptrb:DWORD

			mov	eax, [ptrb]
			mov	edx, [ptra]

			mov	eax, [eax]
			mov	edx, [edx]

			push	eax
			push	edx

			call	strcmp

			add	esp, 8

			ret

MySortCompFunc		endp

;//-------------------------------------------------------------------------
endif

;----------------------------------------------------------------------------
;				The
				End
