		.386p

		.model	SMALL, C

		.code

		public	C	RegleTrois
		public	C	BoundRegleTrois

BoundRegleTrois proc	\
		Valeur1:DWORD, Valeur2:DWORD, \
		NbStep:DWORD, Step:DWORD

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

tropgrand:	ret

troppetit:	mov	eax, ecx
		ret

BoundRegleTrois endp

RegleTrois	proc	\
		Valeur1:DWORD, Valeur2:DWORD, \
		NbStep:DWORD, Step:DWORD

		mov	ecx, NbStep
		mov	eax, Valeur2
		test	ecx, ecx
		jle	short erreur

		sub	eax, Valeur1
		imul	Step
		idiv	ecx	
		add	eax, Valeur1

erreur: 	ret

RegleTrois	endp

END