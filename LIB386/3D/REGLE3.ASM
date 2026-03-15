		.386p

		.model	SMALL, C

		.DATA

fmt_bregle_1	DB	"[ASM][BoundRegleTrois][1] V1=%d V2=%d NbStep=%d Step=%d", 10, 0
fmt_bregle_2	DB	"[ASM][BoundRegleTrois][2] return=%d", 10, 0
fmt_regle_1	DB	"[ASM][RegleTrois][1] V1=%d V2=%d NbStep=%d Step=%d", 10, 0
fmt_regle_2	DB	"[ASM][RegleTrois][2] return=%d", 10, 0

		extrn	C	printf:NEAR

		.code

		public	C	RegleTrois
		public	C	BoundRegleTrois

BoundRegleTrois proc	\
		Valeur1:DWORD, Valeur2:DWORD, \
		NbStep:DWORD, Step:DWORD

		; --- debug trace [ASM][BoundRegleTrois][1] params ---
		pushad
		push	Step
		push	NbStep
		push	Valeur2
		push	Valeur1
		push	offset fmt_bregle_1
		call	printf
		add	esp, 20
		popad
		; --- end debug trace ---

		mov	edx, Step
		mov	eax, Valeur2
		mov	ecx, Valeur1

		test	edx, edx
		jle	troppetit

		cmp	edx, NbStep
		jge	tropgrand

		sub	eax, ecx
		imul	edx
		idiv	NbStep
		add	eax, ecx

tropgrand:
		; --- debug trace [ASM][BoundRegleTrois][2] return ---
		pushad
		push	eax
		push	offset fmt_bregle_2
		call	printf
		add	esp, 8
		popad
		; --- end debug trace ---

		ret

troppetit:	mov	eax, ecx

		; --- debug trace [ASM][BoundRegleTrois][2] return ---
		pushad
		push	eax
		push	offset fmt_bregle_2
		call	printf
		add	esp, 8
		popad
		; --- end debug trace ---

		ret

BoundRegleTrois endp

RegleTrois	proc	\
		Valeur1:DWORD, Valeur2:DWORD, \
		NbStep:DWORD, Step:DWORD

		; --- debug trace [ASM][RegleTrois][1] params ---
		pushad
		push	Step
		push	NbStep
		push	Valeur2
		push	Valeur1
		push	offset fmt_regle_1
		call	printf
		add	esp, 20
		popad
		; --- end debug trace ---

		mov	ecx, NbStep
		mov	eax, Valeur2
		test	ecx, ecx
		jle	short erreur

		sub	eax, Valeur1
		imul	Step
		idiv	ecx	
		add	eax, Valeur1

erreur:
		; --- debug trace [ASM][RegleTrois][2] return ---
		pushad
		push	eax
		push	offset fmt_regle_2
		call	printf
		add	esp, 8
		popad
		; --- end debug trace ---

		ret

RegleTrois	endp

END